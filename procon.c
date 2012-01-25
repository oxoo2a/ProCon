extern "C" {
	#include <pthread.h>
	#include <stdio.h>
	#include <unistd.h>
	#include <stdlib.h>
	#include <assert.h>
	#include <semaphore.h>
}

#include <list>
using namespace std;
	
// ******************************************************************
// Class Semaphore: encapsulates POSIX semaphores
// ******************************************************************

class Semaphore {
	public:
		Semaphore ( int initial_value ) {
			int ret = sem_init(&s,0,initial_value);
			assert(ret == 0);
		}
		~Semaphore () {
			int ret = sem_destroy(&s);
			assert(ret == 0);
		}
		
		void P () {
			int ret = sem_wait(&s);
			assert(ret == 0);
		}
		void V () {
			int ret = sem_post(&s);
			assert(ret == 0);
		}
	
	private:
		sem_t s;
};

// ******************************************************************
// Class Buffer
// ******************************************************************

class Buffer {
	public:
		Buffer ( int size ) : p_mutex(1), c_mutex(1), slots_used(0), slots_free(size) {
			buffer_size = size;
			b = new int[size];
			first_used = 0;
			first_free = 0;
		}
		~Buffer () {
			delete [] b;
		}
		void Produce ( int good ) {
			slots_free.P();
			p_mutex.P();
			b[first_free] = good;
			first_free = (first_free+1) % buffer_size;
			p_mutex.V();
			slots_used.V();
		}
		int Consume () {
			slots_used.P();
			c_mutex.P();
			int good = b[first_used];
			first_used = (first_used+1) % buffer_size;
			c_mutex.V();
			slots_free.V();
			return good;
		}
	private:
		int buffer_size;
		int *b;
		int first_used;
		int first_free;
		Semaphore p_mutex;
		Semaphore c_mutex;
		Semaphore slots_used;
		Semaphore slots_free;
};

static Buffer *the_buffer;

// ******************************************************************
// Class Worker
// ******************************************************************

class Worker {
	public:
		Worker ( void *(*func)(void *), int arg ) {
			int ret = pthread_create(&thread,NULL,func,(void *) arg);
			assert(ret == 0);
		}
		void Join () {
			int ret = pthread_join(thread,NULL);
			assert(ret == 0);
		}
	private:
		pthread_t thread;
};

class Producer: public Worker {
	public:
		Producer ( int id ) : Worker(main,id) { }
	private:
		static void *main ( void *argument ) {
			long id = (long) argument;
			int count = 0;
			while (1) {
				int good = id*10000+count;
				printf("Producer %d: good %d\n",id,good);
				the_buffer->Produce(good);
				count++;
				sleep(2);
			}
		}
};

class Consumer: public Worker {
	public:
		Consumer ( int id) : Worker(main,id) { }
	private:
		static void *main ( void *argument ) {
			long id = (long) argument;
			while (1) {
				int good = the_buffer->Consume();
				printf("Consumer %d: consumed %d\n",id,good);
				sleep(2);
			}
		}
};

// ******************************************************************
// main
// ******************************************************************

int main ( int ac, char **av ) {
	int ret;

	assert(ac == 4);	
	int n_producer = atoi(av[1]);
	assert(n_producer > 0);
	int n_consumer = atoi(av[2]);
	assert(n_consumer > 0);
	int buffer_size = atoi(av[3]);
	assert(buffer_size > 0);
	
	printf("Creating %d producer, %d consumer, and a buffer with %d entries\n",n_producer,n_consumer,buffer_size);
	
	the_buffer = new Buffer(buffer_size);
	
	// A STL generic list is used to store all the worker references
	list<Worker *> worker;
	for (int p=0; p<n_producer; p++)
		worker.push_back(new Producer(p+1));
	for (int c=0; c<n_consumer; c++)
		worker.push_back(new Consumer(c+1));
	
	// Wait for all workers to terminate ... what they will never do ;-)
	// Not blocking the main thread would force the worker threads to terminate immediately
	list<Worker *>::iterator w;
	for (w = worker.begin(); w != worker.end(); ++w) {
		(*w)->Join();
	}
}
