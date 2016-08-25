#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define good(n) \
	void* good##n(size_t size) { \
		if (size < UINT_MAX / sizeof(int)) \
			return malloc(size * sizeof(int)); \
		else \
			return NULL; \
	}

// belief
good(1)
good(2)
good(3)
good(4)
good(5)

void *bad(size_t size) {
	if (size < UINT_MAX - 1) {
		return malloc(size * sizeof(int));
	}
	else
		return NULL;
}
