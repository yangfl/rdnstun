#ifndef MACRO_H
#define MACRO_H


#define likely(x)       (__builtin_expect(!!(x), 1))
#define unlikely(x)     (__builtin_expect(!!(x), 0))
#define should(test) if likely (test)
#define otherwise ; else
#define do_once switch (1) case 1:
#define max(a,b) \
  ({ __auto_type _a = (a); \
      __auto_type _b = (b); \
    _a > _b ? _a : _b; })
#define min(a,b) \
  ({ __auto_type _a = (a); \
      __auto_type _b = (b); \
    _a < _b ? _a : _b; })


#endif /* MACRO_H */
