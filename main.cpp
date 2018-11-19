#include <stdlib.h>
#include <stdio.h>

#include "declare.h"
#include "Server.h"
#include "parse/parse_config.h"

// for debug mode
bool DEBUG_MODE = true;

int main(int argc, char *argv[]) {

    if (argc > 2) {
        DEBUG_MODE = true;
        std::cerr << "DEBUG MODE IS ON" << std::endl;
    }

    struct configf config = {};
    int status = parse_config(&config);
    std::cerr << "config parsed" << std::endl;
    if (status) {
        return -1;
    }

    std::cout << config.port << ' ' << config.path << ' ' << config.thread << std::endl;

    auto * server = new Server(config.port, DEBUG_MODE, config.thread, config.path);
    server->start();
}