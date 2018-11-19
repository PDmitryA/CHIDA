#ifndef ListNode_H
#define ListNode_H
#include <iostream>
#include <climits>
#include <random>
#include <chrono>
#include <thread>
#include <pthread.h>

using namespace std;

template <typename T>
struct ListNode {

    T val;
    ListNode *next;
    pthread_mutex_t mutex;

    ListNode(T x) : val(x), next(NULL) {
        pthread_mutex_init(&mutex, NULL);
    }
    ~ListNode() {
        pthread_mutex_destroy(&mutex);
    }

};
#endif