#include "thread_pool.h"

//创建线程池
threadpool_t *threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size)
{
    threadpool_t *pool = NULL;
    int i=0;
    do{
        pool = (threadpool_t *)calloc(1, sizeof(threadpool_t));
        assert(pool);

        pool->min_thr_num = min_thr_num;
        pool->max_thr_num = max_thr_num;
        pool->busy_thr_num = 0;
        pool->live_thr_num = min_thr_num;
        pool->queue_size = 0;
        pool->queue_max_size = queue_max_size;
        pool->queue_front = 0;
        pool->queue_rear = 0;
        pool->shutdown = false;
        pool->threads = (pthread_t *)calloc(max_thr_num, sizeof(pthread_t));
        assert(pool->threads);
        
        pool->task_queue = (threadpool_task_t *)calloc(queue_max_size, sizeof(threadpool_task_t));
        assert(pool->task_queue);

        if (pthread_mutex_init(&(pool->lock), NULL) != 0
            || pthread_mutex_init(&(pool->thread_counter), NULL) != 0
            || pthread_cond_init(&(pool->queue_not_empty), NULL) != 0
            || pthread_cond_init(&(pool->queue_not_full), NULL) != 0)
        {
            printf("init the lock or cond fail\n");
            break;
        }
        /**
         * start work thread min_thr_num
         */
        for (i=0; i<min_thr_num; i++)
        {
            //启动任务线程
            pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void *)pool);
            printf("start thread 0x%x...\n", pool->threads[i]);
        }
        //启动管理线程
        pthread_create(&(pool->adjust_tid), NULL, adjust_thread, (void *)pool);
        return pool;
    }while(0);
    threadpool_free(pool);
    return NULL;
}

//把任务添加到队列中
int threadpool_add(threadpool_t *pool, void*(*function)(void *arg), void *arg)
{
    assert(pool != NULL);
    assert(function != NULL);
    assert(arg != NULL);

    pthread_mutex_lock(&(pool->lock));
    //队列满的时候，等待
    while ((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
    {
        //queue full wait
        pthread_cond_wait(&(pool->queue_not_full), &(pool->lock));
    }
    if (pool->shutdown)
    {
        pthread_mutex_unlock(&(pool->lock));
    }
    //如下是添加任务到队列，使用循环队列
    if (pool->task_queue[pool->queue_rear].arg != NULL)
    {
        free(pool->task_queue[pool->queue_rear].arg);
        pool->task_queue[pool->queue_rear].arg = NULL;
    }
    pool->task_queue[pool->queue_rear].function = function;
    pool->task_queue[pool->queue_rear].arg = arg;
    pool->queue_rear = (pool->queue_rear + 1)%pool->queue_max_size;
    pool->queue_size++;
    //每次加完任务，发个信号给线程
    //若没有线程处于等待状态，此句则无效，但不影响
    pthread_cond_signal(&(pool->queue_not_empty));
    pthread_mutex_unlock(&(pool->lock));
    return 0;
}

//线程执行任务
void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;
    while(true)
    {
        /* Lock must be taken to wait on conditional variable */
        pthread_mutex_lock(&(pool->lock));
        //任务队列为空的时候，等待
        while ((pool->queue_size == 0) && (!pool->shutdown))
        {
            printf("thread 0x%x is waiting\n", pthread_self());
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));
            //被唤醒后，判断是否是要退出的线程
            if (pool->wait_exit_thr_num > 0)
            {
                pool->wait_exit_thr_num--;
                if (pool->live_thr_num > pool->min_thr_num)
                {
                    printf("thread 0x%x is exiting\n", pthread_self());
                    pool->live_thr_num--;
                    pthread_mutex_unlock(&(pool->lock));
                    pthread_exit(NULL);
                }
            }
        }
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&(pool->lock));
            printf("thread 0x%x is exiting\n", pthread_self());
            pthread_exit(NULL);
        }
        //get a task from queue
        task.function = pool->task_queue[pool->queue_front].function;
        task.arg = pool->task_queue[pool->queue_front].arg;
        pool->queue_front = (pool->queue_front + 1)%pool->queue_max_size;//此处采用循环队列
        pool->queue_size--;
        //now queue must be not full
        pthread_cond_broadcast(&(pool->queue_not_full));//通知队列有空余位置
        pthread_mutex_unlock(&(pool->lock));
        // Get to work
        printf("thread 0x%x start working\n", pthread_self());
        pthread_mutex_lock(&(pool->thread_counter));
        pool->busy_thr_num++;
        pthread_mutex_unlock(&(pool->thread_counter));
        (*(task.function))(task.arg);//执行相应的任务
        // task run over
        printf("thread 0x%x end working\n", pthread_self());
        pthread_mutex_lock(&(pool->thread_counter));
        pool->busy_thr_num--;
        pthread_mutex_unlock(&(pool->thread_counter));
    }
    pthread_exit(NULL);
    return (NULL);
}

//管理线程
void *adjust_thread(void *threadpool)
{
    int i=0;
    threadpool_t *pool = (threadpool_t *)threadpool;
    while (!pool->shutdown)
    {
        sleep(DEFAULT_TIME);//每隔一段时间处理一次
        pthread_mutex_lock(&(pool->lock));
        int queue_size = pool->queue_size;
        int live_thr_num = pool->live_thr_num;
        pthread_mutex_unlock(&(pool->lock));
        pthread_mutex_lock(&(pool->thread_counter));
        int busy_thr_num = pool->busy_thr_num;
        pthread_mutex_unlock(&(pool->thread_counter));
        //任务多线程少，增加线程
        if (queue_size >= MIN_WAIT_TASK_NUM
                && live_thr_num < pool->max_thr_num)
        {
            //need add thread
            pthread_mutex_lock(&(pool->lock));
            int add = 0;
            //从pool->threads中找到一个空的或者已经释放的线程位置
            for (i=0; i<pool->max_thr_num && add<DEFAULT_THREAD_VARY
                    && pool->live_thr_num<pool->max_thr_num; i++)
            {
                if (pool->threads[i] == 0 || !is_thread_alive(pool->threads[i]))
                {
                    pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void *)pool);
                    add++;
                    pool->live_thr_num++;
                }
            }
            pthread_mutex_unlock(&(pool->lock));
        }
        //任务少线程多，减少线程
        if ((busy_thr_num * 2) < live_thr_num
                && live_thr_num > pool->min_thr_num)
        {
            //need del thread
            pthread_mutex_lock(&(pool->lock));
            pool->wait_exit_thr_num = DEFAULT_THREAD_VARY;
            pthread_mutex_unlock(&(pool->lock));
            //wake up thread to exit
            for (i = 0; i < DEFAULT_THREAD_VARY; i++)
            {
                pthread_cond_signal(&(pool->queue_not_empty));//这里并不是真的queue_not_empty，被用来做通知
            }
        }
    }
}

int threadpool_destroy(threadpool_t *pool)
{
    int i=0;
    if (pool == NULL)
    {
        return -1;
    }
    pool->shutdown = true;
    //adjust_tid exit first
    pthread_join(pool->adjust_tid, NULL);
    // wake up the waiting thread
    pthread_cond_broadcast(&(pool->queue_not_empty));
    for(i=0; i<pool->max_thr_num; i++){
        if(pool->threads[i] !=0 && is_thread_alive(pool->threads[i])){
            pthread_join(pool->threads[i], NULL);
        }
    }

    threadpool_free(pool);
    return 0;
}

int threadpool_free(threadpool_t *pool)
{
    if (pool == NULL)
    {
        return -1;
    }
    if (pool->task_queue)
    {
        free(pool->task_queue);
        pool->task_queue = NULL;
    }
    if (pool->threads)
    {
        free(pool->threads);
        pool->threads = NULL;
        pthread_mutex_lock(&(pool->lock));//here is for safe
        pthread_mutex_destroy(&(pool->lock));
        pthread_mutex_lock(&(pool->thread_counter));
        pthread_mutex_destroy(&(pool->thread_counter));
        pthread_cond_destroy(&(pool->queue_not_empty));
        pthread_cond_destroy(&(pool->queue_not_full));
    }
    free(pool);
    pool = NULL;
    return 0;
}

int threadpool_all_threadnum(threadpool_t *pool)
{
    int all_threadnum = -1;
    pthread_mutex_lock(&(pool->lock));
    all_threadnum = pool->live_thr_num;
    pthread_mutex_unlock(&(pool->lock));
    return all_threadnum;
}

int threadpool_busy_threadnum(threadpool_t *pool)
{
    int busy_threadnum = -1;
    pthread_mutex_lock(&(pool->thread_counter));
    busy_threadnum = pool->busy_thr_num;
    pthread_mutex_unlock(&(pool->thread_counter));
    return busy_threadnum;
}

bool is_thread_alive(pthread_t tid)
{
    int kill_rc = pthread_kill(tid, 0);//测试线程是否存在
    if (kill_rc == ESRCH)//线程不存在
    {
        return false;
    }
    return true;
}

