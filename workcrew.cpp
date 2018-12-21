#include <string>
#include <vector>
#include <list>
#include <pthread.h>

#ifdef DEBUG
# define DPRINT(arg) std::cout <<
#else
# define DPRINT(arg)
#endif

#define LOG(log) std::cout << __FUNCTION__ << ":" << log << std::endl
#define LOG_RETURN(value,log) std::cout << __FUNCTION__ << ":line" << __LINE__ << ":" << log << " @"<< value << std::endl; return value


struct Work {
    std::string path;       /* Directory or file */
    std::string pattern;    /* Search string */
};

/* thread routine template */
template <class T>
void *WorkerRoutine(void *pt) {
    return (*(reinterpret_cast<T*>(pt)))();
}

struct Worker {
    Worker& Activate(T *task) {
        int status = pthread_create(&thread_, NULL, WorkerRoutine<Worker>, this);
        if (status != 0) {
            LOG_RETURN(status, id_ <<  " create pthread failed.");
        }

        return *this;
    }

    void *operator()() {
        if (crew_belongs_to_.expired()) {
            LOG_RETURN(0, id_ <<  " crew expired.");
        }

        status = pthread_mutex_lock (&crew_belongs_to_->mutex);
        if (status != 0) {
            LOG_RETURN(status, id_ <<  " lock crew mutex failed.");
        }

        /* There's no work to do right now, Wait for go command. */
        /*  Recheck predicate here! */
        while(crew_belongs_to_->works.size() == 0) {
            status = pthread_cond_wait(&crew_belongs_to_->go, &crew_belongs_to_->mutex);
            if (status != 0) {
                LOG_RETURN(status, id_ << " wait crew go failed.");
            }
        }

        Work wk = crew_belongs_to_->works.first();
        crew_belongs_to_->works.erase(crew_belongs_to_->works.begin());

        /* Unlock crew mutx, start working now. */
        status = pthread_mutex_unlock (&crew->mutex);
        if (status != 0) {
            LOG_RETURN(status, id_ << " unlock crew mutex failed.");
        }

        struct stat filestat;
        status = lstat(wk.path.c_str(), &filestat);
        if (status != 0) {
            LOG_RETURN(status, id_ << " lstat " << wk.path " failed.");
        }

        if (S_ISLNK (filestat.st_mode)) {
            LOG(id_ << " " << wk.path " is a link.");
        }
        else if (S_ISDIR (filestat.st_mode)){
            DIR *pd = nullptr;
            /* read dir entries */
            struct dirent *pde = nullptr;
            if( (pd = opendir(wk.path.c_str()) == NULL) {
                LOG_RETURN(0, id_ << " open dir " << wk.path << " failed.");
            }

            while((pde = readdir(pd)) != NULL) {
                /* Ignore "." and ".." entries. */
                if (strcmp (entry->d_name, ".") == 0)
                    continue;
                if (strcmp (entry->d_name, "..") == 0)
                    continue;
                
                if(*(wk.path.end()-1) != '/') {
                    wk.path += "/";
                }
                wk.path += entry->d_name;
                
                status = pthread_mutex_lock (&crew_belongs_to_->mutex);
                if (status != 0) {
                    LOG_RETURN(status, id_ <<  " lock crew mutex failed.");
                }

                crew_belongs_to_->works.push_back(wk);

                /* Signal the waiters at first, a waiter would be waken up from the cond and then lock on crew mutex. */
                status = pthread_cond_signal (&crew->go);
                if (status != 0) {
                    LOG_RETURN(status, id_ <<  " signal failed.");
                }
                /* 
                    Unlock the crew mutex, the waiter locked on it would be waken up, 
                    ie. pthread_cond_wait would return to calling thread.
                */
                status = pthread_mutex_unlock (&crew->mutex);
                if (status != 0) {
                    LOG_RETURN(status, id_ <<  " unlock crew mutex failed.");
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

        status = pthread_mutex_lock (&crew_belongs_to_->mutex);
        if (status != 0) {
            LOG_RETURN(status, id_ <<  " lock crew mutex failed.");
        }

        if (crew_belongs_to_.works.size() <= 0) {
            LOG(id_ <<  " all work done.");

            status = pthread_cond_broadcast (&crew->_);
            if (status != 0) {
                LOG_RETURN(status, id_ <<  " broadcast done failed.");
            }
            status = pthread_mutex_unlock (&crew->mutex);
            if (status != 0) {
                LOG_RETURN(status, id_ <<  " unlock crew mutex failed.");
            }

            /* break out the outside loop , return from the thread routine. */
            LOG(id_ <<  " stop.");
            break;
        }
        
        status = pthread_mutex_unlock (&crew->mutex);
        if (status != 0) {
            LOG_RETURN(status, id_ <<  " unlock crew mutex failed.");
        }
    }

    void report() {
        std::cout << "worker " << id_ << ":" << std::endl;
        for (auto &file : matched_files_) {
            std::cout << file << std::endl;
        }
    }

    int            id_ = -1;          /* Thread's index */
    pthread_t      thread_;         /* Thread for stage */
    
    std::weak_ptr<Crew> crew_belongs_to_;          /* Pointer to crew */
    std::list<string> matched_files_;
};

struct Crew {
    bool Init(const int workers_count) {
        status = pthread_mutex_init (&mutex, NULL);
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
            workers_.push_back(Worker().Activate())
        }
    }

    int Start(const std::string &file, const std::string &pattern) {
        if (workers_.size() <= 0) {
            return;
        }

        int status = pthread_mutex_lock (&mutex_);
        if (status != 0) {
            LOG_RETURN(status , " lock crew mutex failed.");
        }

        /*
        * If the crew is busy, wait for them to finish.
        */
        while (works.size() > 0) {
            status = pthread_cond_wait (&done_, &mutex_);
            if (status != 0) {
                pthread_mutex_unlock (&crew->mutex);
                LOG_RETURN(status , " wait crew done failed.");
            }
        }

        Work wk;
        wk.path = file;
        wk.pattern = pattern;

        works.push_back(wk);
        status = pthread_cond_signal (&go_);
        if (status != 0) {
            works.pop_back();
            crew->work_count = 0;
            pthread_mutex_unlock (&mutex_);
            LOG_RETURN(status , " signal crew go failed.");
        }

        while (works.size() > 0) {
            status = pthread_cond_wait (&done_, &mutex_);
            if (status != 0) {
                LOG_RETURN(status , " wait crew done failed.");
            }
        }

        status = pthread_mutex_unlock (&crew->mutex);
        if (status != 0) {
            LOG_RETURN(status , " unlock crew mutex failed.");
        }

        return 0;
    }

    void report() {
        int i = 0;
        for (auto &worker : workers_) {
            worker.report();
        }
    }

    
    std::list<Work>     works;
    pthread_mutex_t     mutex_;          /* Mutex for crew data */
    pthread_cond_t      done_;           /* Crew Waits for work done */
    pthread_cond_t      go_;             /* Workers Wait for work */

private:
    std::vector<Worker> workers_;
};


int main(int argc, char *argv[]) {
    Crew crew;
    crew.init();
    crew.Start(argv[1], argv[2]);

    return 0;
}