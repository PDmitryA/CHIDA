//
// Created by pda on 19.11.18.
//

#ifndef ConcurrentList_H
#define ConcurrentList_H
#include <iostream>
#include <climits>
#include <random>
#include <chrono>
#include <thread>
#include <pthread.h>

#include "ListNode.cpp"

template <typename T>
class ConcurrentList {
public:

    ConcurrentList() {
        head = new ListNode<T>(std::numeric_limits<T>::min());
        head->next = new ListNode<T>(std::numeric_limits<T>::max());
    }
    ~ConcurrentList() {
        ListNode<T> *prev, *curr;

        prev = head;
        curr = prev->next;
        while (curr != nullptr) {
            delete prev;
            prev = curr;
            curr = prev->next;
        }
        delete prev;
    }

    virtual bool contain(T val) {
        ListNode<T> *prev, *curr;

        prev = head;
        pthread_mutex_lock(&prev->mutex);
        curr = prev->next;
        while (curr != nullptr) {
            pthread_mutex_lock(&curr->mutex);
            if (curr->val == val) {
                pthread_mutex_unlock(&curr->mutex);
                pthread_mutex_unlock(&prev->mutex);
                return true;
            }
            if (curr->val > val) {
                pthread_mutex_unlock(&curr->mutex);
                pthread_mutex_unlock(&prev->mutex);
                return false;
            }
            pthread_mutex_unlock(&prev->mutex);
            prev = curr;
            curr = prev->next;
        }
        pthread_mutex_unlock(&prev->mutex);
        return false;

    }

    virtual bool add(T val) {
        ListNode<T> *prev, *curr;

        prev = head;
        pthread_mutex_lock(&prev->mutex);
        curr = prev->next;
        while ( (curr != nullptr) ) {
            pthread_mutex_lock(&curr->mutex);
            if (curr->val > val ) {
                // insert
                auto newNode = new ListNode<T>(val);
                pthread_mutex_lock(&newNode->mutex);
                newNode->next = prev->next;
                prev->next = newNode;
                pthread_mutex_unlock(&curr->mutex);
                pthread_mutex_unlock(&newNode->mutex);
                pthread_mutex_unlock(&prev->mutex);
                return true;
            }
            else if (curr->val == val) {
                // gurantee elements are unique
                pthread_mutex_unlock(&curr->mutex);
                pthread_mutex_unlock(&prev->mutex);
                return false;
            }
            pthread_mutex_unlock(&prev->mutex);
            prev = curr;
            curr = prev->next;
        }
        // as add tail node, do not need add in tail
        pthread_mutex_unlock(&prev->mutex);
        return false;
    }

    virtual bool remove(T val) {
        ListNode<T> *prev, *curr;

        prev = head;
        pthread_mutex_lock(&prev->mutex);
        curr = prev->next;
        while (curr != nullptr) {
            pthread_mutex_lock(&curr->mutex);
            if (curr->val == val) {
                prev->next = curr->next;
                pthread_mutex_unlock(&curr->mutex);
                pthread_mutex_unlock(&prev->mutex);
                delete curr;
                return true;
            }
            pthread_mutex_unlock(&prev->mutex);
            prev = curr;
            curr = prev->next;
        }
        pthread_mutex_unlock(&prev->mutex);
        return false;
    }

    virtual int print(int flag = 0){
        ListNode<T>* temp = head;
        int count = 0;
        while(temp != nullptr) {
            if (flag) {
                cout << temp->val << endl;
            }
            temp = temp->next;
            count++;
        }
        return count;
    }


protected:
    ListNode<T>* head;
};
#endif