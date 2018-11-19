//
// Created by pda on 13.11.18.
//

#include "Server.h"

Server::Server(int port, bool debug): port(port), DEBUG_MODE(debug) {}

std::string Server::create_response(const std::string& status,
                                   const std::string& content,
                                   const std::string& content_type,
                                   const std::string& content_length) {
    static std::string server_name = "CHIDA";
    static std::string protocol = "HTTP/1.1";
    static std::string connection = "close";

    return HttpParser::create_response(
            server_name,
            protocol,
            status,
            connection,
            content,
            content_type,
            content_length
    );
}

bool Server::handle_get(
    int client,
    HttpParser const* clientRequest,
    std::string const& dir,
    std::string& responseStr,
    ao_read_file*& read_start_push) {
    bool to_close = false;
    if (dir.find("../") != std::string::npos) {
        responseStr = this->create_response("403 Forbidden");
        to_close = true;
    } else {
        int file = open(dir.c_str(), O_RDONLY, 0);
        if (file == -1) {
            std::string path = clientRequest->get_path();
            if (path[path.length() - 1] == '/') {
                responseStr = this->create_response("403 Forbidden");
                to_close = true;
            } else {
                responseStr = this->create_response("404 Not Found");
                to_close = true;
            }
        } else {
            char* buffer = new char[SIZE_TO_READ];
            memset(buffer, 0, strlen(buffer));
            // create the control block structure
            auto read_start = new ao_read_file();
            read_start->epoll_client_fd = client;
            read_start->cb->aio_nbytes = SIZE_TO_READ;
            read_start->cb->aio_fildes = file;
            read_start->cb->aio_offset = 0;
            read_start->cb->aio_buf = buffer;
            // read!
            if (aio_read(read_start->cb) == -1) {
                close(file);
                delete[] buffer;
                delete read_start;
                responseStr = this->create_response("500 Server Error");
                to_close = true;
            } else {
                struct stat file_stat{};
                fstat(file, &file_stat);
                read_start->fileSize = file_stat.st_size;
                if (DEBUG_MODE) {
                    std::cerr << "START PUSH READ_START" << std::endl;
                }
                read_start_push = read_start;
                if (DEBUG_MODE) {
                    std::cerr << "PUSHED READ_START" << std::endl;
                }

                auto mime = MimeType::Instance();
                responseStr = this->create_response(
                        "200 OK",
                        "",
                        mime->get_mime_type(dir),
                        std::to_string(file_stat.st_size)
                );
            }
        }
    }

    return to_close;
}

bool Server::handle_head(int client, HttpParser const* clientRequest, std::string const& dir, std::string& responseStr) {
    if (dir.find("../") != std::string::npos) {
        responseStr = this->create_response("403 Forbidden");
    } else {
        int file = open(dir.c_str(), O_RDONLY, 0);
        if (file == -1) {
            responseStr = this->create_response("404 Not Found");
        } else {
            struct stat file_stat{};
            fstat(file, &file_stat);

            auto mime = MimeType::Instance();
            responseStr = this->create_response(
                    "200 OK",
                    "",
                    mime->get_mime_type(dir),
                    std::to_string(file_stat.st_size)
            );
        }
    }

    return true;
}

int Server::handle_client(int client) {
    // get raw message from client(buf)
    char buf[BUF_SIZE];
    bzero(buf, BUF_SIZE);

    // to keep different results
    int len;

    // try to get new raw message from client
    if (DEBUG_MODE) {
        std::cerr << "Try to read from fd(" << client << ")" << std::endl;
    }
    CHK2(len, recv(client, buf, BUF_SIZE, 0));
    if (DEBUG_MODE) {
        std::cerr << "recv" << std::endl;
    }
    // zero size of len mean the client closed connection
    if (len == 0) {
        CHK(close(client));
        clients_list.remove(client);
        if (DEBUG_MODE) {
            std::cerr << "Client with fd:" <<
                      client << " closed!" << std::endl;
        }
    } else {
        if(DEBUG_MODE) {
            std::cerr << "parse clientRequest" << std::endl;
        }
        HttpParser* clientRequest = new HttpParser(std::string(buf));
        if(DEBUG_MODE) {
            std::cerr << "HttpParser created and client fd(" << client << ") parsed" << std::endl;
        }

        bool to_close = false;
        std::string responseStr;
        ao_read_file* read_file_push = nullptr;

        if (clientRequest->is_bad_query()) {
            responseStr = this->create_response("400 Bad Request");
            to_close = true;
        } else {

            std::string dir = get_current_dir_name();
            dir += clientRequest->get_path();
            if (dir[dir.length() - 1] == '/') {
                dir += "index.html";
            }

            if (clientRequest->get_method() == "GET") {
                to_close = this->handle_get(client, clientRequest, dir, responseStr, read_file_push);
            } else if (clientRequest->get_method() == "HEAD") {
                to_close = this->handle_head(client, clientRequest, dir, responseStr);
            } else {
                // not allowed method
                responseStr = this->create_response("405 Method Not Allowed");
                to_close = true;
            };
        }
        if (DEBUG_MODE) {
            std::cerr << "METHOD: " << clientRequest->get_method() << '\n'
                      << "PATH: " << clientRequest->get_path() << '\n'
                      << "QUERY: " << clientRequest->get_query() << '\n';
        }
        char* response = new char[responseStr.length() + 1];
        std::strcpy(response, responseStr.c_str());
        for (int sent = 0; sent < strlen(response); sent += send(client, response+sent, strlen(response)-sent, 0));
        if (to_close) {
            CHK(close(client));
            clients_list.remove(client);
            if (DEBUG_MODE) {
                std::cerr << "Client with fd:" <<
                          client << " closed!" << std::endl;
            }
        }
        if (read_file_push) {
            this->client_queue.push(read_file_push);
        }
        delete[] response;
        delete clientRequest;
    }

    return len;
}

void* Server::handle_file(void* to_pthread_void) {
    auto * to_pthread_struct = (to_pthread*) to_pthread_void;
    auto * client_queue = to_pthread_struct->client_queue;
    bool DEBUG_MODE = to_pthread_struct->debug;
    auto * clients_list = to_pthread_struct->clients_list;

    while (true) {
        std::cerr << "THREAD ... Popping" << std::endl;
        auto* ao_serve_file = client_queue->pop();
        std::cerr << "THREAD ... POPPED" << std::endl;

        if (aio_error(ao_serve_file->cb) == EINPROGRESS) {
            std::cerr << "THREAD ... EINPROGRESS" << std::endl;
            client_queue->push(ao_serve_file);
            continue;
        }

        std::cerr << "THREAD ... AIO READY" << std::endl;

        __ssize_t numBytes = aio_return(ao_serve_file->cb);
        if (numBytes != -1) {
            char* buffer = (char*) ao_serve_file->cb->aio_buf;
            if(send(ao_serve_file->epoll_client_fd,
                    buffer,
                    (size_t)(numBytes),
                    MSG_NOSIGNAL) == -1) {
                delete[] buffer;
                clients_list->remove(ao_serve_file->epoll_client_fd);
                CHK(close(ao_serve_file->cb->aio_fildes));
                CHK(close(ao_serve_file->epoll_client_fd));
                if (DEBUG_MODE) {
                    std::cerr << "Client with fd:" <<
                              ao_serve_file->epoll_client_fd << " closed!" << std::endl;
                }
            };

            // add new task if it is not the end of file
            auto read_continue = new ao_read_file();
            read_continue->cb->aio_nbytes = SIZE_TO_READ;
            read_continue->cb->aio_fildes = ao_serve_file->cb->aio_fildes;
            read_continue->cb->aio_offset = ao_serve_file->cb->aio_offset + SIZE_TO_READ;
            memset(buffer, 0, SIZE_TO_READ);
            read_continue->cb->aio_buf = buffer;
            if (ao_serve_file->fileSize < read_continue->cb->aio_offset || aio_read(read_continue->cb) == -1) {
                clients_list->remove(ao_serve_file->epoll_client_fd);
                CHK(close(ao_serve_file->cb->aio_fildes));
                CHK(close(ao_serve_file->epoll_client_fd));
                if (DEBUG_MODE) {
                    std::cerr << "Client with fd:" <<
                              ao_serve_file->epoll_client_fd << " closed!" ;
                }
                delete[] buffer;
                delete read_continue;
            } else {
                int fd = ao_serve_file->epoll_client_fd;
                lseek(fd, 0, SEEK_SET);
                read_continue->epoll_client_fd = fd;
                read_continue->fileSize = ao_serve_file->fileSize;
                client_queue->push(read_continue);
            }
        }
        delete &(*ao_serve_file);
    }
    return nullptr;
}

void Server::serve_files() {
    pthread_t thread;
    auto * to_pthread_struct = new to_pthread();
    to_pthread_struct->clients_list = &this->clients_list;
    to_pthread_struct->debug = DEBUG_MODE;
    to_pthread_struct->client_queue = &this->client_queue;

    if (pthread_create(&thread, nullptr, Server::handle_file, (void*)to_pthread_struct) != 0) {
        std::cerr << "Couldn`t create thread" << std::endl;
        return;
    }
    if (DEBUG_MODE) {
        std::cerr << "Thread created!" << std::endl;
    }
}

// Setup nonblocking socket
int setnonblocking(int sockfd) {
    CHK(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK));
    return 0;
}

int Server::start() {
    int listener;
    struct sockaddr_in addr = {}, their_addr = {};
    //     configure ip & port for listen
    addr.sin_family = PF_INET;
    addr.sin_port = htons(SERVER_PORT);
    CHK(inet_aton(SERVER_HOST, &addr.sin_addr));

    //     size of address
    socklen_t socklen;
    socklen = sizeof(struct sockaddr_in);

    //     event template for epoll_ctl(ev)
    //     storage array for incoming events from epoll_wait(events)
    //        and maximum events count could be EPOLL_SIZE
    static struct epoll_event ev, events[EPOLL_SIZE];
    //     watch just incoming(EPOLLIN)
    //     and Edge Trigged(EPOLLET) events
    ev.events = EPOLLIN | EPOLLET;

    //     epoll descriptor to watch events
    int epfd;

    // other values:
    //     new client descriptor(client)
    //     to keep the results of different functions(res)
    //     to keep incoming epoll_wait's events count(epoll_events_count)
    int client, res, epoll_events_count;

    // *** Setup server listener
    //     create listener with PF_INET(IPv4) and
    //     SOCK_STREAM(sequenced, reliable, two-way, connection-based byte stream)
    CHK2(listener, socket(PF_INET, SOCK_STREAM, 0));
    std::cout << "Main listener(fd=" << listener << ") created!" << std::endl;
    int reuse = 1;
    CHK(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)))

    //    setup nonblocking socket
    setnonblocking(listener);

    //    bind listener to address(addr)
    CHK(bind(listener, (struct sockaddr *)&addr, sizeof(addr)));
    std::cout << "Listener binded to: " << SERVER_HOST << std::endl;

    //    start to listen connections
    CHK(listen(listener, 1));
    std::cout << "Start to listen: " << SERVER_HOST <<"!" << std::endl;

    // *** Setup epoll
    //     create epoll descriptor
    //     and backup store for EPOLL_SIZE of socket events
    CHK2(epfd,epoll_create(EPOLL_SIZE));
    std::cout << "Epoll(fd=" << epfd << ") created!" << std::endl;

    //     set listener to event template
    ev.data.fd = listener;

    //     add listener to epoll
    CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev));
    std::cout << "Main listener(" << epfd << ") added to epoll!" << std::endl;

    serve_files();

    while(true) {
        int epoll_wait_timeout = EPOLL_RUN_TIMEOUT;
        CHK2(epoll_events_count, epoll_wait(epfd, events, EPOLL_SIZE, epoll_wait_timeout));
        if (DEBUG_MODE) {
            std::cerr << "Epoll events count: " << epoll_events_count << std::endl;
        }

        for(int i = 0; i < epoll_events_count; i++) {
            if (events[i].data.fd == listener) {
                CHK2(client, accept(listener, (struct sockaddr *) &their_addr, &socklen));
                if (DEBUG_MODE) {
                    std::cerr <<
                              "connection from:" <<
                              inet_ntoa(their_addr.sin_addr) <<
                              ":" <<
                              ntohs(their_addr.sin_port) <<
                              ", socket assigned to: " <<
                              client << std::endl;
                }
                // setup nonblocking socket
                setnonblocking(client);

                // set new client to event template
                ev.data.fd = client;

                // add new client to epoll
                CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev));

                // save new descriptor to further use
                clients_list.add(client); // add new connection to list of clients
                if (DEBUG_MODE) {
                    std::cerr <<
                              "Add new client(fd = " <<
                              client <<
                              ")" << std::endl;
                }
            } else {
                // EPOLLIN event for others(new incoming message from client)
                CHK2(res, handle_client(events[i].data.fd));
            }
        }
    }
}