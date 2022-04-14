#ifndef MACRO_H
#define MACRO_H


#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define likely(x)   (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))

#define should(test) if likely (test) {
#define otherwise } else
#define do_once switch (1) case 1:

#define return_if(expr) if (expr) return
#define return_if_not(expr) if (!(expr)) return
#define return_if_fail(expr) if unlikely (!(expr)) return
#define return_nonzero(expr) do { \
  int __res = (expr); \
  return_if_fail (__res == 0) __res; \
} while (0)

#define break_if(expr) if (expr) break
#define break_if_not(expr) if (!(expr)) break
#define break_if_fail(expr) if unlikely (!(expr)) break

#define continue_if(expr) if (expr) continue
#define continue_if_not(expr) if (!(expr)) continue
#define continue_if_fail(expr) if unlikely (!(expr)) continue

#define goto_if(expr) if (expr) goto
#define goto_if_not(expr) if (!(expr)) goto
#define goto_if_fail(expr) if unlikely (!(expr)) goto
#define goto_nonzero(expr) if unlikely ((ret = (expr)) != 0) goto

#define test_goto(expr, errnum) should (expr) otherwise \
  if (ret = (errnum), 1) goto

#define promise(x) if (!(x)) __builtin_unreachable()

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define cmp(a, b) ((a) == (b) ? 0 : (a) < (b) ? -1 : 1)

#define arraysize(a) (sizeof(a) / sizeof(a[0]))


#endif /* MACRO_H */
