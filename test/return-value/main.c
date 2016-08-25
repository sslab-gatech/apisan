#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define SIZE 256

#define good(n) \
	bool good##n() {\
		void *ptr = malloc(SIZE);\
		if (ptr != NULL)\
			return true;\
		else\
			return false;\
	}

// belief
good(1)
good(2)
good(3)
good(4)
good(5)
good(6)
good(7)
good(8)
good(9)
good(10)

void bad() {
	// no return value validation
	void *ptr = malloc(SIZE);
  free(ptr);
}

void* not_bad() {
  return malloc(SIZE);
}
