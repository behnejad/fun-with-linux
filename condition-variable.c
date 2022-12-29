#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "queue.h"
 
pthread_t tid1, tid2, tid3; 
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
 
typedef struct Task
{
    SIMPLEQ_ENTRY(Task) simpleq;
    int value;
} Task;

typedef SIMPLEQ_HEAD(Task_Q, Task) Task_Q;
static Task_Q queue;
int loop = 1;

void destroy_task_queues()
{
    Task * task;
    while ((task = SIMPLEQ_FIRST(&queue)) != NULL)
    {
        SIMPLEQ_REMOVE_HEAD(&queue, simpleq);
        free(task);
    }
}

void* foo()
{  
    Task * task;
    pthread_mutex_lock(&lock);
    while (loop)
    {
        task = SIMPLEQ_FIRST(&queue);
        if (task)
        {
            SIMPLEQ_REMOVE_HEAD(&queue, simpleq);
            printf("getting: %d\n", task->value);
            free(task);
        }
        else
        {
            printf("waiting\n");
            pthread_cond_wait(&cond1, &lock);
        }
    }
    pthread_mutex_unlock(&lock);
    printf("consumer end\n");
    return NULL;
}
 
void * goo(void * arg)
{
    for (int i = 0; i < 40; ++i)
    {
        pthread_mutex_lock(&lock);
        Task * t = malloc(sizeof(Task));
        t->value = ((int) arg) + i;
        // printf("Signaling %d\n", t->value);
        SIMPLEQ_INSERT_TAIL(&queue, t, simpleq);
        pthread_cond_signal(&cond1);
        pthread_mutex_unlock(&lock);
        // sleep(1);
    }

    printf("producer %d end\n", (int) arg);
    return NULL;
}

int main()
{
    SIMPLEQ_INIT(&queue);
    pthread_create(&tid1, NULL, foo, NULL);
    pthread_create(&tid2, NULL, goo, 100);
    pthread_create(&tid3, NULL, goo, 200);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    sleep(1);
    loop = 0;
    pthread_cond_signal(&cond1);
    printf("main end\n");
    pthread_join(tid1, NULL);
    destroy_task_queues();
    return 0;
}