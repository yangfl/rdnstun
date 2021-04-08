#include "utils.h"


extern inline int argtoi (const char s[], int *res, int min, int max);
extern void *irealloc (void **ptr, size_t size);


const char *Struct_strerror (int errnum) {
  switch (errnum) {
    case -1:
      return "out of memory";
    default:
      return NULL;
  }
}
