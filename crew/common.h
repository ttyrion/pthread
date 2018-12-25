#ifndef CREW_COMMON_H
#define CREW_COMMON_H

#include <string>
#include <iostream>

#ifdef DEBUG
# define DPRINT(arg) std::cout <<
#else
# define DPRINT(arg)
#endif

#define LOG(log) std::cout << __FUNCTION__ << ":line" << __LINE__ << ":" << log << std::endl
#define LOG_RETURN(value,log) std::cout << __FUNCTION__ << ":line" << __LINE__ << ":" << log << " @"<< value << std::endl; return value


struct Work {
    std::string path;       /* Directory or file */
    std::string pattern;    /* Search string */
};

/* thread routine template */
template <class T>
void *WorkerRoutine(void *pt) {
    return (*(reinterpret_cast<T*>(pt)))();
};

template <class T>
void WorkerCleanRoutine(void *pt) {
    return ((reinterpret_cast<T*>(pt)->Clean()));
};

#endif /* CREW_COMMON_H */