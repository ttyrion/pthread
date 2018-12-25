#include "Crew.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        LOG_RETURN(1, "main need more arguments!");
    }

    CrewFactory factory;
    factory.GetCrew()->Init(5);
    factory.GetCrew()->Start(argv[1], argv[2]);
    factory.GetCrew()->Report();
    factory.GetCrew()->JoinWorkers();

    return 0;
}