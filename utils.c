#include "utils.h"


const char *Struct_strerror (int errnum) {
  switch (errnum) {
    case -1:
      return "out of memory";
    default:
      return NULL;
  }
}
