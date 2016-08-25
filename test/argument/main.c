#include <stdio.h>
#include <string.h>

void good1(char *src) {
  char buf1[0x100];
  char buf2[0x80];
  memcpy(buf2, src, sizeof(buf2));
}

void good2(char *src) {
  char buf1[0x100];
  char buf2[0x80];
  memcpy(buf2, src, sizeof(buf2));
}

void good3(char *src) {
  char buf1[0x100];
  char buf2[0x80];
  memcpy(buf2, src, sizeof(buf2));
}

void good4(char *src) {
  char buf1[0x100];
  char buf2[0x80];
  memcpy(buf2, src, sizeof(buf2));
}

void good5(char *src) {
  char buf1[0x100];
  char buf2[0x80];
  memcpy(buf2, src, sizeof(buf2));
}

void bad(char *src) {
  char buf1[0x100];
  char buf2[0x80];
  memcpy(buf2, src, sizeof(buf1));
}
