#include <pthread.h>
#include <stdbool.h>

pthread_mutex_t lock;

#define good(n) \
  void good##n() { \
  	pthread_mutex_lock(&lock); \
	  pthread_mutex_unlock(&lock); \
  }

// belief
good(1)
good(2)
good(3)
good(4)
good(5)

void bad(bool cond) {
	pthread_mutex_lock(&lock);
	if (cond) return;
	pthread_mutex_unlock(&lock);
}
