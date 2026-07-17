#include <windows.h>
#include <iostream>
#include <chrono>
#include <thread>

VOID CALLBACK TimerCallback(PVOID /*lpParam*/, BOOLEAN /*TimerOrWaitFired*/) {
    std::cout << "Hello World" << std::endl;
}

int main() {
    HANDLE hTimer = NULL;

    // = xTimerCreate + xTimerStart in one call
    CreateTimerQueueTimer(
        &hTimer,            // phNewTimer
        NULL,               // TimerQueue (default)
        TimerCallback,      // callback
        NULL,               // parameter
        500,                // DueTime (ms) - first trigger
        500,                // Period (ms)  - 0 = one-shot
        WT_EXECUTEDEFAULT   // flags
    );

    Sleep(5000);  // keep main thread alive 5s

    // = xTimerStop + xTimerDelete
    DeleteTimerQueueTimer(NULL, hTimer, NULL);
    std::cout << "Timer stopped." << std::endl;
    return 0;
}
