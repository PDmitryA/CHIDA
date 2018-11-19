#include <aio.h>


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
