// uci.h
#ifndef UCI_H
#define UCI_H

#include "engine.h"
#include "game.h"
#include <string>
#include <sstream>
#include <iostream>

class UCI {
public:
    UCI(Engine& engine, Game& game);
    void loop();

    void handleCommand(const std::string& line);
    void handlePosition(std::istringstream& iss);
    void handleGo(std::istringstream& iss);
    void handleSetOption(std::istringstream& iss);

private:
    Engine* engine;
    Game* game;

    int wtime = 0, btime = 0, winc = 0, binc = 0, movetime = -1, depth = -1;
};

#endif
