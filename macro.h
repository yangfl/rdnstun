#ifndef MACRO_H
#define MACRO_H


#define likely(x)       (__builtin_expect(!!(x), 1))
#define unlikely(x)     (__builtin_expect(!!(x), 0))

#define should(test) if likely (test)
#define otherwise ; else
#define do_once switch (1) case 1:

#define goto_if_fail(test) if unlikely (!(test)) goto

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))


#endif /* MACRO_H */
