#include <stdlib.h>
#include <stdio.h>

#include "declare.h"
#include "Server.h"

// for debug mode
bool DEBUG_MODE = true;
int port = 3345;

int main(int argc, char *argv[]) {

    if (argc > 2) {
        DEBUG_MODE = true;
        std::cerr << "DEBUG MODE IS ON" << std::endl;
    }

    auto * server = new Server(port, DEBUG_MODE);
    server->start();
}