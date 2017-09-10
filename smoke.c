#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 500

int paper_avail = 0;								       	// global variables that can be updated once a resource is available
int tobacco_avail = 0;
int matches_avail = 0;

uthread_cond_t signal_paper;							 // condition variables to signal when a resource can be smoked
uthread_cond_t signal_tobacco;
uthread_cond_t signal_matches;

struct Agent {
  	uthread_mutex_t mutex;
  	uthread_cond_t  matches;							// condition variables to signal when agent makes a resource available
  	uthread_cond_t  paper;
  	uthread_cond_t  tobacco;
  	uthread_cond_t  smoke;								// condition variable to signal when a smoker consumes the resources
};

struct Agent* createAgent() {
  	struct Agent* agent = malloc (sizeof (struct Agent));
  	agent->mutex   = uthread_mutex_create();
  	agent->paper   = uthread_cond_create (agent->mutex);
  	agent->matches = uthread_cond_create (agent->mutex);
  	agent->tobacco = uthread_cond_create (agent->mutex);
  	agent->smoke   = uthread_cond_create (agent->mutex);
  	return agent;
}

enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};

int signal_count [5];  									// number of times resource signalled (used for assertion)
int smoke_count  [5];  									// number of times smoker with resource smoked (used for assertion)

void* agent (void* av) {
  	struct Agent* a = av;
  	static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  	static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};

  	uthread_mutex_lock (a->mutex);					 		     // agent acquires mutex lock

  	for (int i = 0; i < NUM_ITERATIONS; i++) {
      		int r = rand() % 3;
      		signal_count [matching_smoker [r]] ++;
      		int c = choices [r];							        // agent randomly selects two resources to make available
      		if (c & MATCH) {
        		uthread_cond_signal (a->matches);				// agent signals that matches are available
      		}
      		if (c & PAPER) {
        		uthread_cond_signal (a->paper);					// agent signals that paper is available
      		}
      		if (c & TOBACCO) {
        		uthread_cond_signal (a->tobacco);			  // agent signals that tobacco is available
      		}
      		uthread_cond_wait (a->smoke);						 // agent waits for a smoker to consume the resources
    	}
  	uthread_mutex_unlock (a->mutex);						   // agent releases mutex lock
}

int resources_avail(){
	return matches_avail + paper_avail + tobacco_avail;				// returns number of resources currently available
}											                      // doesn't need lock since only called from within the lock

void* update_matches(void* av) {							// thread that will wait for matches to be made available,
	struct Agent* a = av;								      // then update global variable
	uthread_mutex_lock(a->mutex);							// acquires mutex lock
	while(1){
		uthread_cond_wait(a->matches);						// waits for matches to be signaled
		matches_avail = 1;							         // updates global variable
		int sum = resources_avail();
		if (sum > 1) {								        // if two resources are now available, signal the correct smoker
			if (matches_avail) {
				if(paper_avail) {
					uthread_cond_signal(signal_tobacco);
				}
				if(tobacco_avail) {
					uthread_cond_signal(signal_paper);
				}
			}
			if (paper_avail) {
				if(tobacco_avail){
					uthread_cond_signal(signal_matches);
				}
			}
		}
	}
	uthread_mutex_unlock(a->mutex);
}

void* update_paper(void* av) {								// thread that will wait for paper to be made available,
	struct Agent* a = av;								// then update global variable
	uthread_mutex_lock(a->mutex);							// acquires mutex lock
	while(1){
		uthread_cond_wait(a->paper);						// waits for paper to be signaled
		paper_avail = 1;							// updates global variable
		int sum = resources_avail();
		if (sum > 1) {								// if two resources are now available, signal the correct smoker
			if (matches_avail) {
				if(paper_avail) {
					uthread_cond_signal(signal_tobacco);
				}
				if(tobacco_avail) {
					uthread_cond_signal(signal_paper);
				}
			}
			if (paper_avail) {
				if(tobacco_avail){
					uthread_cond_signal(signal_matches);
				}
			}
		}
	}
	uthread_mutex_unlock(a->mutex);
}

void* update_tobacco(void* av) {							// thread that will wait for paper to be made available,
	struct Agent* a = av;								// then update global variable
	uthread_mutex_lock(a->mutex);							// acquires mutex lock
	while(1){
		uthread_cond_wait(a->tobacco);						// waits for tobacco to be signaled
		tobacco_avail = 1;							// updates global variable
		int sum = resources_avail();
		if (sum > 1) {								// if two resources are now available, signal the correct smoker
			if (matches_avail) {
				if(paper_avail) {
					uthread_cond_signal(signal_tobacco);
				}
				if(tobacco_avail) {
					uthread_cond_signal(signal_paper);
				}
			}
			if (paper_avail) {
				if(tobacco_avail){
					uthread_cond_signal(signal_matches);
				}
			}
		}
	}
	uthread_mutex_unlock(a->mutex);
}

void * matches(void* av){
	struct Agent * a = av;
	uthread_mutex_lock(a->mutex);							// acquires mutex lock
	while(1){
		uthread_cond_wait(signal_matches);					// wait for tobacco + paper to be available
		tobacco_avail = 0;							// consume resources provided by agent
		paper_avail = 0;
		uthread_cond_signal(a->smoke);						// signal to agent that smoker smoked his resources
		smoke_count[MATCH]++;
	}
	uthread_mutex_unlock(a->mutex);							// release mutex lock
}

void * paper(void* av){
	struct Agent * a = av;
	uthread_mutex_lock(a->mutex);							// acquires mutex lock
	while(1){
		uthread_cond_wait(signal_paper);					// wait for tobacco + matches to be available
		tobacco_avail = 0;							// consume resources provided by agent
		matches_avail = 0;
		uthread_cond_signal(a->smoke);						// signal to agent that smoker smoked his resources
		smoke_count[PAPER]++;
	}
	uthread_mutex_unlock(a->mutex);							// release mutex lock
}

void * tobacco(void* av){
	struct Agent * a = av;
	uthread_mutex_lock(a->mutex);							// acquires mutex lock
	while(1){
		uthread_cond_wait(signal_tobacco);					// wait for matches + paper to be available
		matches_avail = 0;							// consume resources provided by agent
		paper_avail = 0;
		uthread_cond_signal(a->smoke);						// signal to agent that smoker smoked his resources
		smoke_count[TOBACCO]++;
	}
	uthread_mutex_unlock(a->mutex);							// release mutex lock
}


int main (int argc, char** argv) {
	uthread_init (7);
	srand(time(0));
	struct Agent  * a = createAgent();

  	signal_matches = uthread_cond_create(a->mutex);
  	signal_tobacco = uthread_cond_create(a->mutex);
  	signal_paper = uthread_cond_create(a->mutex);
											// create threads to smoke the resources provided by the agent
  	uthread_t matches_thread = uthread_create(matches, (void*) (struct Agent *) a);
  	uthread_t tobacco_thread = uthread_create(tobacco, (void*) (struct Agent *) a);
  	uthread_t paper_thread = uthread_create(paper, (void*) (struct Agent *) a);

											// create threads to update the global variables for resource availability
  	uthread_t update_matches_thread = uthread_create(update_matches, (void*) (struct Agent *) a);
  	uthread_t update_tobacco_thread = uthread_create(update_tobacco, (void*) (struct Agent *) a);
  	uthread_t update_paper_thread = uthread_create(update_paper, (void*) (struct Agent *) a);

  	uthread_join (uthread_create (agent, a), 0);
  	assert (signal_count [MATCH]   == smoke_count [MATCH]);
 	assert (signal_count [PAPER]   == smoke_count [PAPER]);
  	assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  	assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  	printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
	        smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);

	uthread_cond_destroy(a->matches);
	uthread_cond_destroy(a->tobacco);
	uthread_cond_destroy(a->paper);
	free(a);
	uthread_cond_destroy(signal_matches);
	uthread_cond_destroy(signal_tobacco);
	uthread_cond_destroy(signal_paper);
}
