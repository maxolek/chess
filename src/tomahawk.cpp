#include <UCI.h>
#include <session.h>
#include <engine.h>
#include <board.h>
#include <PrecomputedMoveData.h>
#include <iostream>
#include <atomic>
#include <thread>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <pthread.h>
#endif

// version name set at compile time
#ifndef ENGINE_ID
#define ENGINE_ID "unknown"
#endif

// ---------------------
// CPU affinity
// ---------------------
void pin_to_pcores() {
#ifdef _WIN32
    // Windows example: P-cores (0,1,6,7,8,9,18,19)
    DWORD_PTR mask = 0;
    mask |= (1ULL << 0);
    HANDLE hProc = GetCurrentProcess();
    if (!SetProcessAffinityMask(hProc, mask)) 
        std::cerr << "Failed to set CPU affinity, error " << GetLastError() << "\n";
#else
    // macOS/Linux: pin to core 0 for now (more complex topologies need sysconf/CPU_SETSIZE)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    pthread_t thread = pthread_self();
    int s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
        std::cerr << "Failed to set CPU affinity\n";
#endif
}

// ---------------------
// UCI listening loop
// ---------------------
std::atomic<bool> keepListening(true);

void listenLoop(UCI& uci) {
    std::string line;
    while (keepListening) {
        if (!std::getline(std::cin, line)) {
            keepListening = false;
            break;
        }
        uci.handleCommand(line);
        if (line == "quit") {
            keepListening = false;
            break;
        }
    }
}

// ---------------------
// main
// ---------------------
int main() {
    // environment
    pin_to_pcores();
    initInstance();      // PID-based instance id
    startNewSession();   // session=1

    // inits
    Logging::initFiles();
    PrecomputedMoveData::init();
    Magics::initMagics();

    // engine
    Engine engine;
    UCI uci(engine);

    // uci loop
    std::thread listener([&uci](){
        uci.loop(); // loop internally
    });
    listener.join();

    return 0;
}
