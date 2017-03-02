/*
 * Authors: Kunal Pednekar
 *          Abdulellah Abualshour
 *			Christopher Orthmann
 * iLab:	cpp.cs.rutgers.edu
 * This is the .c file that contains implementations of the functions declared in the .h file
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
 *      
 *      IN ADDITION TO ANY HELPER METHODS NEEDED
 */
#include "my_pthread_t.h"

#define STACKSIZE 4096 //size of each context
#define LEVELS 3 //Number of levels in Multi-level priority queue

static int id = 0;
int init = -1; //-1 means that the scheduler has not been initialized, useful for first pthread_create call
static sched* scheduler;
struct itimerval it;//declare the timer struct
ucontext_t main_ctx;
my_pthread_t Main;
//instead of having an LL just have a table (array) of threads
//threads[0] will be where the main thread is saved
//threads[1] will be first thread created, etc
my_pthread_t threads[1024];

void catch_stop (){};

void sighandler(int sig)
{
	sched_handle(); //hopefully this will handle the thread swapping
  
}
//creates thread and suitable stack for it ----------------FIXES THE ISSUE OF CREATING 2 THREADS WHEN YOU ONLY WANTED 1
int my_pthread_create(my_pthread_t * thread, my_pthread_attr_t * attr, void *(*function)(void*), void * arg){
    if (init == -1) { //scheduler is not initialized
                sched_init(); //initializes static sched * scheduler
                init = 0;
                timer_init();
        }

    //check if context exists
    if(getcontext(&(thread->uco)) == -1) { //getcontext
                printf("Error: getcontext returned -1\n");
                return -1;
        }
        //Here we set the meta data for the new thread.
        thread->uco.uc_stack.ss_size = STACKSIZE;
        thread->uco.uc_stack.ss_sp = malloc(STACKSIZE);
        thread->uco.uc_link = &main_ctx; //so if this thread finishes, resume main
        id++;
        thread->thread_id = id;
        //we also make the context and pass the function and arguments to it.
        makecontext(&(thread->uco), (void *)run_thread, 3, thread, function, arg);
        //then we add the thread to the scheduler after creation.
        sched_add(thread, 0);
        scheduler->thread_cur = NULL;
        sched_handle();
        return 0; // assuming this is the correct return value to send back KP
}

void timer_init(){
	signal(SIGALRM, sighandler);
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 5000;
	it.it_value.tv_sec = 0; //it_value if set to zero disables the alarm
	it.it_value.tv_usec = 5000; //it_value if not zero enables the alarm
	setitimer(ITIMER_REAL, &it, NULL);//starting the itimer 
}

void my_pthread_yield(){
    scheduler->thread_cur->thread_status = YIELD;
    sched_handle();
}

void my_pthread_exit(void *value_ptr){
    scheduler->thread_cur->thread_status = TERMINATED;
    scheduler->thread_cur->retval = value_ptr;
    swapcontext(&scheduler->thread_cur->uco, &Main.uco);
}

int my_pthread_join(my_pthread_t thread, void **value_ptr){
	while(thread.thread_status != TERMINATED){
		my_pthread_yield();
	}
	thread.retval = value_ptr;
	free(thread.uco.uc_stack.ss_sp); // supposed to free the stack (look at line 78 in pthread_create)
    return 0; // assuming this is the correct return value to send back KP
}

int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr){
	int x = 0; //return value
	if(mutex == NULL){
		x = EINVAL;
	}
	mutex->flag = 0;
	mutex->waitq = (queue*) malloc(sizeof(queue));
	queue_init(mutex->waitq);
    return x; 
}

int my_pthread_mutex_lock(my_pthread_mutex_t *mutex){
	if(mutex->flag == 0){
		mutex->flag = 1;
	}
	else{
		scheduler->thread_cur->thread_status = WAITING;
		enqueue(mutex->waitq, scheduler->thread_cur);
		sched_handle();
	}
    return 0; 
}

int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex){
	if(mutex->waitq->size != 0){
		my_pthread_t *holder = dequeue(mutex->waitq);
		sched_add(holder, holder->priority);
	}
	mutex->flag = 0;
    return 0; // assuming this is the correct return value to send back KP
}

int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex){
	mutex->flag = 0;
	if(mutex == NULL){
		return EINVAL;
	}
	free(mutex->waitq);
    return 0; // assuming this is the correct return value to send back KP
}

//run thread takes a thread and executes the function passed with the passed arguments
void run_thread(my_pthread_t * thread_node, void *(*f)(void *), void * arg){
	thread_node->thread_status = RUNNING;
	scheduler->thread_cur = thread_node;
	thread_node = f(arg);
	thread_node->thread_status = TERMINATED;
	sched_handle();
}

//------------------------------------REWORKED TO FIX THE ISSUE OF CREATING 2 THREADS WHEN YOU ONLY WANTED 1
void sched_init(){
        if(init != -1){
			return;
        }
        scheduler = malloc(sizeof(sched));
        getcontext(&main_ctx); //holds the context of main, this is your reference
        Main.uco = main_ctx;
        Main.uco.uc_link = NULL;
        Main.thread_status = READY;
        scheduler->thread_main = &Main;
        scheduler->thread_main->thread_status = READY;
        scheduler->thread_main->thread_id = 0;
        scheduler->thread_cur = NULL;
        scheduler->pq = (queue *)malloc(sizeof(queue)*LEVELS); //so we malloc 3 queues
        int k;
        for(k = 0; k < LEVELS; k++){
          queue_init(&(scheduler->pq[k])); //so we can initialize a queue at each index
        }
}

    
void sched_add(my_pthread_t * thread_node, int priority){
    thread_node->thread_status = READY;
    thread_node->priority = priority;
    enqueue(&(scheduler->pq[priority]), thread_node);
}

my_pthread_t * sched_choose(){
	int i = 0;
	for(i = 0; i < 3; i++){//three levels
		if(scheduler->pq[i].head != NULL){
			return dequeue(&(scheduler->pq[i]));
		}
	}
    return NULL;
}

void sched_handle() {
	struct sigaction setup_action;
	sigset_t block_mask;
	sigemptyset (&block_mask);
	sigaddset (&block_mask, SIGALRM);
	sigaddset (&block_mask, SIGQUIT);
	setup_action.sa_handler = catch_stop;
	setup_action.sa_mask = block_mask;
	setup_action.sa_flags = 0;
	sigaction (SIGALRM, &setup_action, NULL); //I noticed that SIGALRM is never fired.
	my_pthread_t * temp = scheduler->thread_cur;
	if(temp != NULL){
		if(temp->thread_status == YIELD){
			//put back to scheduler
			sched_add(temp, temp->priority);
		}
		else if(temp->thread_status == TERMINATED){
			//do nothing
		}
		else if(temp->thread_status == WAITING){
			//do nothing, already waiting
		}
		else{
			//means it is running, set up new priority
			int np; //new priority
			if((temp->priority)+1 > 2){
				np = 2;
			}
			else{
				np = (temp->priority)+1;
			}
			sched_add(temp, np);
		}

	}
	//now we choose another thread to run
	scheduler->thread_cur = sched_choose();
	
	if(scheduler->thread_cur == NULL){
		run_thread(&Main, NULL, NULL);
	}
	scheduler->thread_cur->thread_status = RUNNING;
	
	//now swap contexts
	if(temp != NULL){
		swapcontext(&(temp->uco), &(scheduler->thread_cur->uco));
	}
	else {
		swapcontext(&main_ctx, &(scheduler->thread_cur->uco));
	}
	sigemptyset(&block_mask);
}

void print_q(){
	int i;
	for(i = 0; i < 3; i++){
		printf("Head at level %d is %d\n\n", i, scheduler->pq[i].size);
	}
}

void queue_init(queue *q) {
	q -> head = NULL;
	q -> tail = NULL;
	q -> size = 0;
}

qNode * qNode_init() {
	qNode *new = (qNode *)malloc(sizeof(qNode));
	//Check malloc return value
	if (new == NULL) {
		
		fprintf(stderr, "Error : fatal error, malloc call failed in %s, line %i; %s \n", __FILE__, __LINE__, strerror(errno));
		exit(EXIT_FAILURE);
			
	}
	if (new != NULL){
		new-> thread = NULL;
		new-> next = NULL;
		return new;
	}
	
	return NULL;
}

void enqueue(queue *first, my_pthread_t * thread_node) {
	
	//Check to see if the queue is empty
	if (first -> size == 0) {
		
		//New thread becomes the head
		//qNode *h = first -> head;
		qNode * h = qNode_init();
		first -> head = h;
		first -> tail = h;
		h -> thread = thread_node;
		(first -> size)++;
		
	}
	
	else {
		
		//Add thread to the end of the queue
		qNode * old_tail = first -> tail;
		qNode * t = qNode_init();
		//first -> tail = t;
		t -> thread = thread_node;
		t -> next = NULL;
		old_tail->next = t;
		
		
		
		//Update tail pointer
		first -> tail = t -> next;
		(first -> size)++;
		
	}
}

my_pthread_t * dequeue(queue * first) {
	
	my_pthread_t *dqThread = NULL;
	
	//Check to see if the queue is empty
	if (first -> size == 0) {
		
		fprintf(stderr, "Error : the list is empty, nothing to dequeue\n");
		return dqThread;
		
	} 
	
	dqThread = (first -> head) -> thread;
	qNode *temp = (first -> head) -> next;
	free(first -> head);
	first -> head = temp;
	(first -> size)--;
	
    return dqThread;
    
}

my_pthread_t * peek(queue * first){
	
	my_pthread_t *peekThread = NULL;
	
	//Check to see if the queue is empty
	if (first -> size == 0) {
		
		return peekThread;
		
	}
	
	peekThread = (first -> head) -> thread;
	
    return peekThread;
    
}

int queueisEmpty(queue * first){
	
	if (first -> size == 0) {
		
		return 1;
		
	}
	
	return 0;

}