#include "uci.h"
#include "engine.h"
#include "game.h"

int main() {
    Engine engine;
    Game game;
    UCI uci(engine, game);

    uci.loop();

    return 0;
}
