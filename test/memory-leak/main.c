#include <stdio.h>
#include <stdlib.h>

#define SIZE 256

// belief
void good1() {
	void *ptr = malloc(SIZE);
	if (ptr)
		free(ptr);
}

void good2() {
	void *ptr = malloc(SIZE);
	if (ptr)
		free(ptr);
}

void good3() {
	void *ptr = malloc(SIZE);
	if (ptr)
		free(ptr);
}

void good4() {
	void *ptr = malloc(SIZE);
	if (ptr)
		free(ptr);
}

void good5() {
	void *ptr = malloc(SIZE);
	if (ptr)
		free(ptr);
}

int bad(int arg) {
  void *ptr = malloc(SIZE);
  if (ptr) {
    if (arg == 1) {
      free(ptr);
      return 0;

    }
    else {
      return 1;
    }
  }
  return 0;
}
