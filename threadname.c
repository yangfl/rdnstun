#define _GNU_SOURCE

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "macro.h"
#include "threadname.h"


int threadname_get (char *buf, int size) {
  return pthread_getname_np(pthread_self(), buf, size);
}


int threadname_set (const char *name) {
  return pthread_setname_np(pthread_self(), name);
}


int threadname_format (const char *format, ...) {
  char name[THREADNAME_SIZE];

  va_list ap;
  va_start(ap, format);
  vsnprintf(name, sizeof(name), format, ap);
  va_end(ap);

  return threadname_set(name);
}


int threadname_append (const char *format, ...) {
  pthread_t thread = pthread_self();

  char name[THREADNAME_SIZE];
  return_nonzero (pthread_getname_np(thread, name, sizeof(name)));
  int len = strlen(name);

  va_list ap;
  va_start(ap, format);
  vsnprintf(name + len, sizeof(name) - len, format, ap);
  va_end(ap);

  return pthread_setname_np(thread, name);
}
