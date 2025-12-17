#include "UCI.h"
#include "session.h"
#include "engine.h"
#include "board.h"
#include "PrecomputedMoveData.h"
#include <iostream>
#include <atomic>
#include <thread>
#include <windows.h>


// set cpu affinity to ensure pcores not ecores are used for beter performance
void pin_to_pcores() {
    // P-cores (0,1,6,7,8,9,18,19)
    DWORD_PTR mask = 0;
    mask |= (1ULL << 0);

    mask |= (1ULL << 1);
    mask |= (1ULL << 6);
    mask |= (1ULL << 7);
    mask |= (1ULL << 8);
    mask |= (1ULL << 9);
    mask |= (1ULL << 18);
    mask |= (1ULL << 19);

    HANDLE hProc = GetCurrentProcess();

    if (!SetProcessAffinityMask(hProc, mask)) 
        std::cerr << "Failed to set CPU affinity, error " << GetLastError() << "\n";
    
}

// uci loop to allow continuous communication during game/search
std::atomic<bool> keepListening(true);
void listenLoop(UCI& uci) {
    std::string line;
    while (keepListening) {
        if (!std::getline(std::cin, line)) {
            // End of input or error, stop listening
            keepListening = false;
            break;
        }
        // Process the input line here
        uci.handleCommand(line);

        if (line == "quit") {
            keepListening = false;
            break;
        }
    }
}


int main() {
    pin_to_pcores();
    startNewSession();

    PrecomputedMoveData::init();
    Magics::initMagics();

    Engine engine = Engine();
    UCI uci(engine);

    std::thread listener([&uci](){
        uci.loop(); // loop internally
    });
    listener.join();

    return 0;
}
