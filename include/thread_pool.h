#ifndef __THREAD_POOL_H
#define __THREAD_POOL_H

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>

#define DEFAULT_TIME 10 // 领导定时检查队列、线程状态的时间间隔
#define MIN_WAIT_TASK_NUM 10 // 队列中等待的任务数>这个值，便会增加线程
#define DEFAULT_THREAD_VARY 10 //每次线程加减的数目

typedef struct
{
    void *(*function)(void *);
    void *arg;
} threadpool_task_t;

typedef struct
{
    pthread_mutex_t lock;// mutex for the taskpool
    pthread_mutex_t thread_counter;//mutex for count the busy thread
    pthread_cond_t queue_not_full;
    pthread_cond_t queue_not_empty;//任务队列非空的信号
    pthread_t *threads;//执行任务的线程
    pthread_t adjust_tid;//负责管理线程数目的线程
    threadpool_task_t *task_queue;//任务队列
    int min_thr_num;
    int max_thr_num;
    int live_thr_num;//存活的线程，不一定在干活
    int busy_thr_num;//正在干活的线程数
    int wait_exit_thr_num;
    int queue_front;
    int queue_rear;
    int queue_size;
    int queue_max_size;
    bool shutdown;
}threadpool_t;

/**
 * @function void *threadpool_thread(void *threadpool)
 * @desc the worker thread
 * @param threadpool the pool which own the thread
 */
void *threadpool_thread(void *threadpool);

/**
 * @function void *adjust_thread(void *threadpool);
 * @desc manager thread
 * @param threadpool the threadpool
 */
void *adjust_thread(void *threadpool);

/**
 * check a thread is alive
 */
bool is_thread_alive(pthread_t tid);

int threadpool_free(threadpool_t *pool);

threadpool_t *threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);

int threadpool_add(threadpool_t *pool, void*(*function)(void *arg), void *arg);

int threadpool_destroy(threadpool_t *pool);

#endif