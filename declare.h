#ifndef HIGHLOAD_DECLARE_H
#define HIGHLOAD_DECLARE_H

#define SERVER_HOST "0.0.0.0"
#define EPOLL_SIZE 10000
#define EPOLL_RUN_TIMEOUT -1
#define BUF_SIZE 1024
#define SIZE_TO_READ 100

// Macros - exit in any error (eval < 0) case
#define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}

// Macros - same as above, but save the result(res) of expression(eval)
#define CHK2(res, eval) if((res = eval) < 0){perror("eval"); exit(-1);}



#endif //HIGHLOAD_DECLARE_H
