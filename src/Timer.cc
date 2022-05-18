#include "Timer.h"

void Timer::start(){
    startTime = high_resolution_clock::now();
}

int Timer::stop(){
    endTime = high_resolution_clock::now();
    return duration_cast<milliseconds>(endTime - startTime).count();
}

void Timer::pStop(std::string event){
    int ms = stop();
    std::cout << event << ": elapsed time: " << ms << " milliseconds." << std::endl;
}
