#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

/*
 * Authors: Kunal Pednekar
 *          Abdulellah Abualshour
 *			Christopher Orthmann
 * iLab:	cpp.cs.rutgers.edu
 * This is the .h file for our implementation of the pthread library in the .c file.
 * Contains:
 *      my_pthread_create
 *      my_pthread_yield
 *      pthread_exit
 *      my_pthread_join
 *      my_pthread_mutex_init
 *      my_pthread_mutex
 *      my_pthread_mutex_unlock
 *      my_pthread_mutex_destroy
 *      run_thread
 *      sched_init
 *      sched_add
 *      sched_choose
 *      queue_init
 *      enqueue
 *      dequeue
 *      peek
 *      queueisEmpty
 */
 
//required libraries. Might need more
#include <ucontext.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stddef.h>

//status enumeration
typedef enum status {
	NEW,
	READY, //ready to be scheduled
	RUNNING, //thread is running
	WAITING, //waiting to be scheduled
	TERMINATED, //not running anymore
	YIELD //yeilding to another thread
} status;



//thread struct:
typedef struct my_pthread_t {
	ucontext_t uco; //ucontext object
	//struct my_pthread_t * next_thread; //next thread struct pointer
	status thread_status; //thread status 
	int thread_id; //thread identification
	int num_of_runs; //number of runs
	//int time_of_runs; //time, will need for calculating time when testing. might need timestamps as well 
	int priority; 
	void * retval; //return value
} my_pthread_t;

//queue node struct
typedef struct qNode {
	
	my_pthread_t *thread;
	struct qNode *next;
	
} qNode;

//queue struct
typedef struct {
	
	qNode *head;
	qNode *tail;
	int size;
	
} queue;


//mutex struct
typedef struct {
	int flag; //flag to keep track of mutex status, 0 for unlocked, 1 for locked               
	queue*  waitq;
}my_pthread_mutex_t; //fixed typedef of the struct KP

//scheduler struct
typedef struct {
	queue * pq; //priority queue
	//queue * wait; //pointer for waiting queues
	my_pthread_t * thread_main; //main context
	my_pthread_t * thread_cur; //current context
	int priority_list[16]; //priority list
	long int num_of_scheded; //counter for assigned threads

} sched;

//my_pthread_attr_t struct
typedef struct {
	char *attr1;	
} my_pthread_attr_t; //fixed the definition of typedef struct KP

//declarations for pthread function calls
int my_pthread_create(my_pthread_t * thread, my_pthread_attr_t * attr, void *(*function)(void*), void * arg);
void my_pthread_yield();
void my_pthread_exit(void *value_ptr);
int my_pthread_join(my_pthread_t thread, void **value_ptr);
void sched_destroy();

/*
 * These functions explicitly deal with the mutexes
 */

int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);


//declarations for scheduler function calls
void sched_handle();
void run_thread(my_pthread_t * thr_node, void *(*f)(void *), void * arg); //to run thread using parameters from arg
void sched_init(); //initiate scheduler
void sched_add(my_pthread_t * thr_node, int priority); //add to the scheduler
my_pthread_t * sched_choose(); //for choosing thread
void timer_init();
void sighandler();
void catch_stop ();

//declarations for queue function calls
void queue_init(queue *q);
void print_q();
qNode * qNode_init();
void enqueue(queue *first, my_pthread_t * thread_node);
my_pthread_t * dequeue(queue * first);
my_pthread_t * peek(queue * first);
int queueisEmpty(queue * first);


#endif
