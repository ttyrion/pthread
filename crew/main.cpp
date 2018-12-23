#include "Crew.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        LOG_RETURN(1, "main need more arguments!");
    }

    CrewProxy proxy;
    proxy.GetCrew()->Init(5);
    proxy.GetCrew()->Start(argv[1], argv[2]);

    return 0;
}