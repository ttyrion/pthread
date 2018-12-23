#include "Crew.h"
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <string.h>

Crew::Crew() {
    LOG( "Constructor Crew.");
}

Crew::~Crew() {
    LOG( "Destructor Crew.");
}

bool Crew::Init(const int workers_count) {
    int status = pthread_mutex_init (&mutex_, NULL);
    if (status != 0) {
        LOG_RETURN(false, " init crew mutex failed.");
    }
    status = pthread_cond_init (&done_, NULL);
    if (status != 0) {
        LOG_RETURN(false, " init crew done failed.");
    }
    status = pthread_cond_init (&go_, NULL);
    if (status != 0) {
        LOG_RETURN(false, " init crew go failed.");
    }

    for (int i = 0; i < workers_count; ++i) {
        std::tr1::shared_ptr<Worker> shared_w(new Worker(shared_from_this(), i));
        if(!shared_w->Activate()) {
            LOG_RETURN(false, " init crew go failed.");
        }
        workers_.push_back(shared_w);
    }
}

int Crew::Start(const std::string &file, const std::string &pattern) {
    if (workers_.size() <= 0) {
        return -1;
    }

    int status = pthread_mutex_lock (&mutex_);
    if (status != 0) {
        LOG_RETURN(status , " lock crew mutex failed.");
    }

    /*
    * If the crew is busy, wait for them to finish.
    */
    while (works_.size() > 0) {
        status = pthread_cond_wait (&done_, &mutex_);
        if (status != 0) {
            pthread_mutex_unlock (&mutex_);
            LOG_RETURN(status , " wait crew done failed.");
        }
    }

    Work wk;
    wk.path = file;
    wk.pattern = pattern;

    works_.push_back(wk);
    status = pthread_cond_signal (&go_);
    if (status != 0) {
        works_.pop_back();
        pthread_mutex_unlock (&mutex_);
        LOG_RETURN(status , " signal crew go failed.");
    }

    while (works_.size() > 0) {
        status = pthread_cond_wait (&done_, &mutex_);
        if (status != 0) {
            LOG_RETURN(status , " wait crew done failed.");
        }
    }

    status = pthread_mutex_unlock (&mutex_);
    if (status != 0) {
        LOG_RETURN(status , " unlock crew mutex failed.");
    }

    return 0;
}

void Crew::Report() {
    int i = 0;
    for (auto &worker : workers_) {
        worker->Report();
    }
}

Crew::Worker::Worker(std::tr1::shared_ptr<Crew> crew, const int id) {
    crew_belongs_to_ = crew;
    id_ = id;
}

bool Crew::Worker::Activate() {
    int status = pthread_create(&thread_, NULL, WorkerRoutine<Worker>, this);
    if (status != 0) {
        LOG_RETURN(false, id_ <<  " create pthread failed @" << status << ".");
    }

    return true;
}

void *Crew::Worker::operator()() {
    while(true) {
        if (crew_belongs_to_.expired()) {
            LOG_RETURN(0, id_ <<  " crew expired.");
        }

        int status = pthread_mutex_lock (&(crew_belongs_to_.lock()->mutex_));
        if (status != 0) {
            LOG_RETURN((void*)status, id_ <<  " lock crew mutex failed.");
        }

        /* There's no work to do right now, Wait for go command. */
        /*  Recheck predicate here! */
        while(crew_belongs_to_.lock()->works_.size() == 0) {
            status = pthread_cond_wait(&crew_belongs_to_.lock()->go_, &(crew_belongs_to_.lock()->mutex_));
            if (status != 0) {
                LOG_RETURN((void*)status, id_ << " wait crew go failed.");
            }
        }

        Work wk = *(crew_belongs_to_.lock()->works_.begin());
        crew_belongs_to_.lock()->works_.erase(crew_belongs_to_.lock()->works_.begin());

        /* Unlock crew mutx, start working now. */
        status = pthread_mutex_unlock (&(crew_belongs_to_.lock()->mutex_));
        if (status != 0) {
            LOG_RETURN((void*)status, id_ << " unlock crew mutex failed.");
        }

        struct stat filestat;
        status = lstat(wk.path.c_str(), &filestat);
        if (status != 0) {
            LOG_RETURN((void*)status, id_ << " lstat " << wk.path << " failed.");
        }

        if (S_ISLNK (filestat.st_mode)) {
            LOG(id_ << " " << wk.path << " is a link.");
        }
        else if (S_ISDIR (filestat.st_mode)){
            DIR *pd = nullptr;
            /* read dir entries */
            struct dirent *pde = nullptr;
            if( (pd = opendir(wk.path.c_str())) == NULL) {
                LOG_RETURN((void*)0, id_ << " open dir " << wk.path << " failed.");
            }

            Work new_work;
            while((pde = readdir(pd)) != NULL) {
                /* Ignore "." and ".." entries. */
                if (strcmp (pde->d_name, ".") == 0)
                    continue;
                if (strcmp (pde->d_name, "..") == 0)
                    continue;
                
                new_work = wk;

                if(*(new_work.path.end()-1) != '/') {
                    new_work.path += "/";
                }
                new_work.path += pde->d_name;
                status = pthread_mutex_lock (&(crew_belongs_to_.lock()->mutex_));
                if (status != 0) {
                    LOG_RETURN((void*)status, id_ <<  " lock crew mutex failed.");
                }

                crew_belongs_to_.lock()->works_.push_back(new_work);

                /* Signal the waiters at first, a waiter would be waken up from the cond and then lock on crew mutex. */
                status = pthread_cond_signal (&(crew_belongs_to_.lock()->go_));
                if (status != 0) {
                    LOG_RETURN((void*)status, id_ <<  " signal failed.");
                }
                /* 
                    Unlock the crew mutex, the waiter locked on it would be waken up, 
                    ie. pthread_cond_wait would return to calling thread.
                */
                status = pthread_mutex_unlock (&(crew_belongs_to_.lock()->mutex_));
                if (status != 0) {
                    LOG_RETURN((void*)status, id_ <<  " unlock crew mutex failed.");
                }
            }
            closedir(pd);
        }
        else if (S_ISREG (filestat.st_mode)) {
            // char buffer[256] = { 0 };
            // FILE *file = fopen(wk.path.c_str(), "r");
            // if (!file) {
            //     LOG_RETURN(0, id_ << "open file " << wk.path << " failed.");
            // }

            std::ifstream infile(wk.path, std::ifstream::in);
            if(infile.good()) {
                int line_num = 0;
                while(!infile.eof()) {
                std::string line;
                std::getline(infile, line);
                    ++line_num;
                    if (line.find_first_of(wk.pattern) != -1) {
                        matched_files_.push_back(wk.path);
                        break;
                    }
                }
            }
        }
        else {
            /* other files */
        }

        status = pthread_mutex_lock (&(crew_belongs_to_.lock()->mutex_));
        if (status != 0) {
            LOG_RETURN((void*)status, id_ <<  " lock crew mutex failed.");
        }

        if (crew_belongs_to_.lock()->works_.size() <= 0) {
            LOG(id_ <<  " all work done.");

            status = pthread_cond_broadcast(&(crew_belongs_to_.lock()->done_));
            if (status != 0) {
                LOG_RETURN((void*)status, id_ <<  " broadcast done failed.");
            }
            status = pthread_mutex_unlock(&(crew_belongs_to_.lock()->mutex_));
            if (status != 0) {
                LOG_RETURN((void*)status, id_ <<  " unlock crew mutex failed.");
            }

            /* break out the outside loop , return from the thread routine. */
            LOG(id_ <<  " stop.");

            return (void*)-1;
        }
        
        status = pthread_mutex_unlock(&(crew_belongs_to_.lock()->mutex_));
        if (status != 0) {
            LOG_RETURN((void*)status, id_ <<  " unlock crew mutex failed.");
        }
    }
}

void Crew::Worker::Report() {
    LOG("worker " << id_ << ":");
    for (auto &file : matched_files_) {
        std::cout << file << std::endl;
    }
}

std::tr1::shared_ptr<Crew> CrewProxy::GetCrew() {
    if (!crew_.get()) {
        crew_.reset(new Crew());
    }

    return crew_;
}