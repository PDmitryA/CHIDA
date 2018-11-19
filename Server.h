#ifndef HIGHLOAD_SERVER_H
#define HIGHLOAD_SERVER_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <list>
#include <cstring>
#include <aio.h>
#include <pthread.h>

#include "declare.h"
#include <string>
#include "HttpParser.h"
#include "MimeType.h"
#include "queue.cpp"
#include "aoReadFile.cpp"
#include "List/ConcurrentList.cpp"


struct to_pthread {
    Queue<ao_read_file*>* client_queue;
    ConcurrentList<int>* clients_list;
    bool debug;
};


class Server {

    ConcurrentList<int> clients_list;
    int port;
    bool DEBUG_MODE;
    Queue<ao_read_file*> client_queue;
    std::list<pthread_t*> serve_files_thread_list;

    int handle_client(int client);

    std::string create_response(
            const std::string& status,
            const std::string& content = "",
            const std::string& content_type = "",
            const std::string& content_length = "0");

    bool handle_get(
            int client,
            HttpParser const* clientRequest,
            std::string const& dir,
            std::string& responseStr);

    bool handle_head(
            int client,
            HttpParser const* clientRequest,
            std::string const& dir,
            std::string& responseStr);

    static void* handle_file(void* to_pthread_void);

    void serve_files();


public:
    Server(int port, bool debug);
    int start();
};


#endif //HIGHLOAD_SERVER_H
