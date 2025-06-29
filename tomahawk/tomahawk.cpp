#include "uci.h"
#include "engine.h"
#include "game.h"

int main() {
    Game game = Game();
    Engine engine = Engine(&game.board);
    UCI uci(engine, game);

    uci.loop();

    return 0;
}
