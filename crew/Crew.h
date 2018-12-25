#ifndef CREW_CREW_H
#define CREW_CREW_H

#include <pthread.h>
#include <vector>
#include <list>
#include <tr1/memory>
#include "common.h"

class Crew 
        /* 
        *   Crew cannot be constructed on stack any more. 
        *   So I make constructor protected. 
        */
        : public std::tr1::enable_shared_from_this<Crew> {

        friend class CrewFactory;
public:    
    class Worker {
    public:
        Worker(std::tr1::shared_ptr<Crew> crew, const int id);
        ~Worker();
        bool Activate();
        void *operator()();
        void Stop();
        void Clean();
        void Report();
        void Join();
        void Detach();

    private:
        int            id_ = -1;          /* Thread's index */
        pthread_t      thread_;         /* Thread for stage */
        
        std::tr1::weak_ptr<Crew> crew_belongs_to_;          /* Pointer to crew */
        std::list<std::string> matched_files_;
    };

public:
    ~Crew();
protected:
    Crew();

public:
bool Init(const int workers_count);
int Start(const std::string &file, const std::string &pattern);
void Stop();
void Report();
void JoinWorkers();
void DetachWorkers();

public:
    std::vector<Work>     works_;
    int works_count_ = 0;
    pthread_mutex_t     mutex_;          /* Mutex for crew data */
    pthread_cond_t      done_;           /* Crew Waits for work done */
    pthread_cond_t      go_;             /* Workers Wait for work */

private:
    std::vector<std::tr1::shared_ptr<Worker> > workers_;
};

class CrewFactory {
public:
    std::tr1::shared_ptr<Crew> GetCrew();

private:
std::tr1::shared_ptr<Crew> crew_;
};

#endif /* CREW_CREW_H */