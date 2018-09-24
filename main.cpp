#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<netdb.h>
#include<string.h>
#include<sys/sendfile.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<errno.h>
#include <iostream>
#include <list>
#include <cstring>
#include <aio.h>

#include "declare.h"
#include "HttpParser.h"
#include "MimeType.h"

int handle_message(int client);

// Setup nonblocking socket
int setnonblocking(int sockfd) {
    CHK(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK));
    return 0;
}

struct ao_read_file {
    int epoll_client_fd;
    aiocb* cb;
    __off_t fileSize;

    ao_read_file() {
        epoll_client_fd = 0;
        cb = new aiocb();
        fileSize = 0;
    }

    ~ao_read_file() {
        delete cb;
    }
};

// To store client's socket list
std::list<int> clients_list;
// To store client`s files requests
std::list<ao_read_file*> serve_files_list;
std::list<ao_read_file*> next_time_serve_list;

// Message buffer
char message[BUF_SIZE];

// for debug mode
int DEBUG_MODE = 0;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "usage: epoll web server need more arguments" << std::endl;
        return 1;
    }
    if (argc > 2) {
        DEBUG_MODE = 1;
        std::cerr << "DEBUG MODE IS ON" << std::endl;
        std::cerr << "MAIN: argc=" << argc << " argv=";
        for(int i = 0; i < argc; i++) {
            std::cerr << argv[i] << " ";
        }
        std::cerr << std::endl;
    }

    int listener;
    struct sockaddr_in addr, their_addr;
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

    while(true) {
        int epoll_wait_timeout = EPOLL_RUN_TIMEOUT;
        if (!next_time_serve_list.empty()) {
            epoll_wait_timeout = 0;
        }
        CHK2(epoll_events_count, epoll_wait(epfd, events, EPOLL_SIZE, epoll_wait_timeout));
        if(DEBUG_MODE) {
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
                clients_list.push_back(client); // add new connection to list of clients
                if (DEBUG_MODE) {
                    std::cerr <<
                    "Add new client(fd = " <<
                    client <<
                    ") to epoll and now clients_list.size = " <<
                    clients_list.size();
                }
            } else {
                // EPOLLIN event for others(new incoming message from client)
                CHK2(res, handle_message(events[i].data.fd));
            }
        }
        std::swap(next_time_serve_list, serve_files_list);
        for (auto &ao_serve_files : serve_files_list) {
            if (aio_error(ao_serve_files->cb) == EINPROGRESS) {
                next_time_serve_list.push_back(&(*ao_serve_files));
                continue;
            }

            __ssize_t numBytes = aio_return(ao_serve_files->cb);
            if (numBytes != -1) {
                char* buffer = (char*) ao_serve_files->cb->aio_buf;
                /*for (int sent = 0; sent < numBytes; sent += send(
                        ao_serve_files->epoll_client_fd,
                        buffer+sent,
                        (size_t)(numBytes - sent),
                        0)
                                );*/
                if(send(ao_serve_files->epoll_client_fd,
                        buffer,
                        (size_t)(numBytes),
                        MSG_NOSIGNAL) == -1) {
                    delete[] buffer;
                    clients_list.remove(ao_serve_files->epoll_client_fd);
                    CHK(close(ao_serve_files->cb->aio_fildes));
                    CHK(close(ao_serve_files->epoll_client_fd));
                    if (DEBUG_MODE) {
                        std::cerr << "Client with fd:" <<
                                  ao_serve_files->epoll_client_fd << " closed! And now clients_list.size = " <<
                                  clients_list.size() << std::endl;
                    }
                };

                // add new task if it is not the end of file
                auto read_continue = new ao_read_file();
                read_continue->cb->aio_nbytes = SIZE_TO_READ;
                read_continue->cb->aio_fildes = ao_serve_files->cb->aio_fildes;
                read_continue->cb->aio_offset = ao_serve_files->cb->aio_offset + SIZE_TO_READ;
                memset(buffer, 0, SIZE_TO_READ);
                read_continue->cb->aio_buf = buffer;
                if (ao_serve_files->fileSize < read_continue->cb->aio_offset || aio_read(read_continue->cb) == -1) {
                    clients_list.remove(ao_serve_files->epoll_client_fd);
                    CHK(close(ao_serve_files->cb->aio_fildes));
                    CHK(close(ao_serve_files->epoll_client_fd));
                    if (DEBUG_MODE) {
                        std::cerr << "Client with fd:" <<
                            ao_serve_files->epoll_client_fd << " closed! And now clients_list.size = " <<
                            clients_list.size() << std::endl;
                    }
                    delete[] buffer;
                    delete read_continue;
                } else {
                    int fd = ao_serve_files->epoll_client_fd;
                    lseek(fd, 0, SEEK_SET);
                    read_continue->epoll_client_fd = fd;
                    read_continue->fileSize = ao_serve_files->fileSize;
                    next_time_serve_list.push_back(read_continue);
                }
            }
            delete &(*ao_serve_files);
        }
        serve_files_list.clear();
    }
}

// *** Handle incoming message from clients
int handle_message(int client) {
    // get row message from client(buf)
    //     and format message to populate(message)
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
            client << " closed! And now clients_list.size = " <<
            clients_list.size() << std::endl;
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

        if (clientRequest->is_bad_query()) {
            responseStr = HttpParser::create_response(
                    "CHIDA",
                    "HTTP/1.1",
                    "400 Bad Request",
                    "close"
            );
            to_close = true;
        } else {

            std::string dir = get_current_dir_name();
            dir += clientRequest->get_path();
            if (dir[dir.length() - 1] == '/') {
                dir += "index.html";
            }

            if (clientRequest->get_method() == "GET") {

                if (dir.find("../") != std::string::npos) {
                    responseStr = HttpParser::create_response(
                            "CHIDA",
                            "HTTP/1.1",
                            "403 Forbidden",
                            "close"
                    );
                    to_close = true;
                } else {
                    int file = open(dir.c_str(), O_RDONLY, 0);
                    if (file == -1) {
                        responseStr = HttpParser::create_response(
                                "CHIDA",
                                "HTTP/1.1",
                                "404 Not Found",
                                "close"
                        );
                        to_close = true;
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
                            responseStr = HttpParser::create_response(
                                    "CHIDA",
                                    "HTTP/1.1",
                                    "500 Server Error",
                                    "close"
                            );
                            to_close = true;
                        } else {
                            struct stat file_stat{};
                            fstat(file, &file_stat);
                            read_start->fileSize = file_stat.st_size;
                            next_time_serve_list.push_back(read_start);

                            auto mime = MimeType::Instance();
                            responseStr = HttpParser::create_response(
                                    "CHIDA",
                                    "HTTP/1.1",
                                    "200 OK",
                                    "close",
                                    "",
                                    mime->get_mime_type(dir),
                                    std::to_string(file_stat.st_size)
                            );
                        }
                    }
                }
            } else if (clientRequest->get_method() == "HEAD") {
                if (dir.find("../") != std::string::npos) {
                    responseStr = HttpParser::create_response(
                            "CHIDA",
                            "HTTP/1.1",
                            "403 Forbidden",
                            "close"
                    );
                    to_close = true;
                } else {
                    int file = open(dir.c_str(), O_RDONLY, 0);
                    if (file == -1) {
                        responseStr = HttpParser::create_response(
                                "CHIDA",
                                "HTTP/1.1",
                                "404 Not Found",
                                "close"
                        );
                        to_close = true;
                    } else {
                        struct stat file_stat{};
                        fstat(file, &file_stat);

                        auto mime = MimeType::Instance();
                        responseStr = HttpParser::create_response(
                                "CHIDA",
                                "HTTP/1.1",
                                "200 OK",
                                "close",
                                "",
                                mime->get_mime_type(dir),
                                std::to_string(file_stat.st_size)
                        );
                        to_close = true;
                    }
                }
            } else {
                // not allowed method
                responseStr = HttpParser::create_response(
                        "CHIDA",
                        "HTTP/1.1",
                        "405 Method Not Allowed",
                        "close"
                );
                to_close = true;
            };
        }
        if (DEBUG_MODE) {
            std::cerr << "METHOD: " << clientRequest->get_method() << '\n'
                      << "PATH: " << clientRequest->get_path() << '\n'
                      << "QUERY: " << clientRequest->get_query() << '\n';
        }
        std::cerr << responseStr << '\n';
        char* response = new char[responseStr.length() + 1];
        std::strcpy(response, responseStr.c_str());
        for (int sent = 0; sent < strlen(response); sent += send(client, response+sent, strlen(response)-sent, 0));
        if (to_close) {
            CHK(close(client));
            clients_list.remove(client);
            if (DEBUG_MODE) {
                std::cerr << "Client with fd:" <<
                          client << " closed! And now clients_list.size = " <<
                          clients_list.size() << std::endl;
            }
        }
        delete[] response;
        delete clientRequest;
    }

    return len;
}
