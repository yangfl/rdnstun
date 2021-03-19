#include <stdarg.h>
#include <stdio.h>

#include "macro.h"

#include "log.h"


LogLevelFlags effective_log_level = LOG_LEVEL_MESSAGE;


void logger (const char log_domain[], LogLevelFlags log_level,
             const char format[], ...) {
  if (log_level <= effective_log_level) {
    va_list ap;
    va_start(ap, format);
    if unlikely (log_level == LOG_LEVEL_ERROR ||
                 log_level == LOG_LEVEL_CRITICAL) {
      vfprintf(stderr, format, ap);
      fputc('\n', stderr);
    } else {
      vprintf(format, ap);
      putchar('\n');
    }
    va_end(ap);
  }
}
