#ifndef UTILS_H
#define UTILS_H

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>


__attribute__((nonnull, access(read_only, 1)))
static inline int argtoi (const char s[], int *res, int min, int max) {
  char *s_end;
  *res = strtol(s, &s_end, 10);
  return !isdigit(*s_end) && min <= *res && *res <= max ? 0 : 1;
}

__attribute__((nonnull))
static inline void *irealloc (void *ptr, size_t size) {
  void *ret = realloc(*(void **) ptr, size);
  if (size > 0 && ret != NULL) {
    *(void **) ptr = ret;
  }
  return ret;
}

__attribute__((const, warn_unused_result))
const char *Struct_strerror (int errnum);


#endif /* UTILS_H */
