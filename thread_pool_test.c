#include "include/thread_pool.h"

void *process(void *arg)
{
    printf("thread %llu working on task %d\n ",pthread_self(),*(int *)arg);
    sleep(1);
    printf("task %d is end\n",*(int *)arg);
    return NULL;
}
int main()
{
    int i=0;
    threadpool_t *thp = threadpool_create(3,100,12);
    printf("pool inited");

    int *num = (int *)calloc(20, sizeof(int));
    assert(num);
    
    for (i=0; i<10; i++)
    {
        num[i]=i;
        printf("add task %d\n",i);
        threadpool_add(thp,process,(void*)&num[i]);
    }
    sleep(10);
    threadpool_destroy(thp);
}