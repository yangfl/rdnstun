#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "macro.h"
#include "color.h"
#include "log.h"


LogLevelFlags effective_log_level = LOG_LEVEL_MESSAGE;


void logger (
    const char log_domain[], LogLevelFlags log_level,
    const char file[], int line, const char func[], const char format[], ...) {
  if unlikely (log_level > effective_log_level) {
    return;
  }

  FILE *stream =
    log_level == LOG_LEVEL_ERROR || log_level == LOG_LEVEL_CRITICAL ?
      stderr : stdout;
  if likely (format != NULL) {
    time_t curtime = time(NULL);
    struct tm timeinfo;
    localtime_r(&curtime, &timeinfo);
    char timebuf[64];
    strftime(
      timebuf, sizeof(timebuf),
      COLOR_SEQ(COLOR_FOREGROUND_GREEN) "[%c]" RESET_SEQ " ", &timeinfo);
    fputs(timebuf, stream);

    if likely (log_domain != NULL) {
      fputs(log_domain, stream);
    }
    if likely (file != NULL) {
      fprintf(stream, " (%s:%d)", file, line);
    }
    fputs(": ", stream);

    if likely (func != NULL) {
      fprintf(stream, "%s-", func);
    }
    fprintf(
      stream, COLOR_SEQ_FORMAT,
      COLOR_FOREGROUND + (log_level == LOG_LEVEL_WARNING ? COLOR_YELLOW :
        log_level == LOG_LEVEL_INFO ? COLOR_WHITE :
        log_level == LOG_LEVEL_DEBUG ? COLOR_BLUE :
        log_level == LOG_LEVEL_CRITICAL ? COLOR_YELLOW :
        log_level == LOG_LEVEL_ERROR ? COLOR_RED :
        COLOR_WHITE));
    fprintf(
      stream, "%s" RESET_SEQ " **: ",
      log_level == LOG_LEVEL_WARNING ? "WARNING" :
        log_level == LOG_LEVEL_INFO ? "INFO" :
        log_level == LOG_LEVEL_DEBUG ? "DEBUG" :
        log_level == LOG_LEVEL_CRITICAL ? "CRITICAL" :
        log_level == LOG_LEVEL_ERROR ? "ERROR" :
        "UNKNOWN");

    va_list ap;
    va_start(ap, format);
    vfprintf(stream, format, ap);
    va_end(ap);
  }
  fputc('\n', stream);
}
