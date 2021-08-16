#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"

// create new threadpool object
threadpool* create_threadpool(int num_threads_in_pool)
{
	if (num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 1) // check for legal num of threads
	{
		perror("create_threadpool : num_threads_in_pool invalid\n");
		return NULL;
	}
	
	threadpool *Tpool = (threadpool*)malloc(sizeof(threadpool)); // allocate memory
	if (Tpool == NULL)
	{
		perror("create_threadpool : Malloc Tpool failed\n");
		return NULL;
	}

    //init values into the struct
    Tpool->num_threads = num_threads_in_pool;
	Tpool->qsize = 0;
    Tpool->shutdown = 0;
    Tpool->dont_accept = 0;   
    Tpool->qhead = NULL;
	Tpool->qtail = NULL;

    //init the conditions and the mutex lock
    pthread_mutex_init(&Tpool->qlock,NULL);
    pthread_cond_init(&Tpool->q_empty,NULL);
    pthread_cond_init(&Tpool->q_not_empty,NULL);
	
    //allocate the array of threads
	Tpool->threads = (pthread_t*)calloc(num_threads_in_pool, sizeof(pthread_t));
	if (Tpool->threads == NULL)
	{
		perror("create_threadpool : Tpool->threads calloc failed.\n");
		return NULL;
	}
	
   //create threads
    for(int i = 0; i<num_threads_in_pool ; i++)
	{
        pthread_create(&Tpool->threads[i],NULL,do_work,Tpool);
	}
	return Tpool;
}

//Dispatch function , do new func everytime into the queue
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{

	if(from_me == NULL || dispatch_to_here == NULL) // check for valid input
        return;

	pthread_mutex_lock(&(from_me->qlock)); // starting critical section
	if(from_me->dont_accept == 1) // if we dont accept anymore new threads
    {
        pthread_mutex_unlock(&(from_me->qlock));
		return;
    }
	pthread_mutex_unlock(&(from_me->qlock)); // we end here, it is important to know that we check each one by one
	

	work_t *work = (work_t*)malloc(sizeof(work_t)); // allocate new work and check
	if (work == NULL) 
	{
		perror("dispatch : work malloc failed\n");
		return;
	}
	
    //init data into struct
	work->routine = dispatch_to_here;
	work->arg = arg;
	work->next = NULL;
	

	pthread_mutex_lock(&(from_me->qlock)); // critical section in case destroy started
	if(from_me->dont_accept == 1)
	{	
		free(work); // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ WAS MISING THIS FREE FOR 2 DAYS@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		return;
	}

	if (from_me->qsize == 0) // if its a new work and the queue is empty
	{
		from_me->qhead = work;
		from_me->qtail = from_me->qhead;
	}
	else // in case there are some works in the queue already
	{
		from_me->qtail->next = work;
		from_me->qtail = from_me->qtail->next;
	}

	from_me->qsize++; // increase the tasks
	pthread_cond_signal(&(from_me->q_not_empty)); // signal that the queue not empty
	pthread_mutex_unlock(&(from_me->qlock));
}

void* do_work(void* p)
{	
	if(p == NULL) // check for valid p
        return NULL;
        
	threadpool *Tpool = (threadpool*)p;
    work_t *tmp;
	while (1)
	{
		pthread_mutex_lock(&(Tpool->qlock));// starting section
		if(Tpool->shutdown == 1)  // check if we at shutdown state , means end
		{
			pthread_mutex_unlock(&(Tpool->qlock));
			return NULL;
		}

		if(Tpool->qsize == 0) 	 /// wait untill the q is not empty
			pthread_cond_wait(&(Tpool->q_not_empty),&(Tpool->qlock));

        if (Tpool->shutdown == 1) 
			{
				pthread_mutex_unlock(&(Tpool->qlock));
				return NULL;
			}
	
		Tpool->qsize--;
		tmp = Tpool->qhead;
		if (Tpool->qsize == 0) 
		{
			Tpool->qhead = NULL;
			Tpool->qtail = NULL;
	
			if (Tpool->dont_accept == 1)
				pthread_cond_signal(&(Tpool->q_empty));
		}
		else 
			Tpool->qhead = Tpool->qhead->next;
	
        pthread_mutex_unlock(&(Tpool->qlock));
        tmp->routine(tmp->arg);
        free(tmp);
	}
}

//destroy the thread pool
void destroy_threadpool(threadpool* destroyme)
{
	if(destroyme == NULL)
        return;

	pthread_mutex_lock(&(destroyme->qlock));

	destroyme->dont_accept = 1;
	
	if(destroyme->qsize != 0)
		pthread_cond_wait(&(destroyme->q_empty),&(destroyme->qlock));

	destroyme->shutdown = 1;
    
	pthread_cond_broadcast(&(destroyme->q_not_empty)); 
	pthread_mutex_unlock(&(destroyme->qlock));
	
	for(int i = 0; i < destroyme->num_threads ; i++)
    {
        pthread_join(destroyme->threads[i],NULL);
        //printf("Destroyed i = %d\n",i);
    }
	
    if(destroyme->threads != NULL)
	    free(destroyme->threads);
    if(destroyme != NULL)
	    free(destroyme);
}


