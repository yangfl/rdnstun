#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "macro.h"
#include "color.h"
#include "log.h"


enum LogLevelFlags effective_log_level = LOG_LEVEL_MESSAGE;


int LogLevel_tocolor (enum LogLevelFlags self) {
  switch (self) {
    case LOG_LEVEL_DEBUG:
      return COLOR_BLUE;
    case LOG_LEVEL_INFO:
      return COLOR_WHITE;
    case LOG_LEVEL_MESSAGE:
      return COLOR_GREEN;
    case LOG_LEVEL_WARNING:
      return COLOR_YELLOW;
    case LOG_LEVEL_CRITICAL:
      return COLOR_YELLOW;
    case LOG_LEVEL_ERROR:
      return COLOR_RED;
    default:
      return COLOR_WHITE;
  }
}


const char *LogLevel_tostring_c (enum LogLevelFlags self) {
  switch (self) {
    case LOG_LEVEL_DEBUG:
      return "DEBUG";
    case LOG_LEVEL_INFO:
      return "INFO";
    case LOG_LEVEL_MESSAGE:
      return "MESSAGE";
    case LOG_LEVEL_WARNING:
      return "WARNING";
    case LOG_LEVEL_CRITICAL:
      return "CRITICAL";
    case LOG_LEVEL_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}


void logger (
    const char log_domain[], enum LogLevelFlags log_level,
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
    fprintf(stream, COLOR_SEQ_FORMAT "%s" RESET_SEQ " **: ",
            COLOR_FOREGROUND + LogLevel_tocolor(log_level),
            LogLevel_tostring_c(log_level));

    va_list ap;
    va_start(ap, format);
    vfprintf(stream, format, ap);
    va_end(ap);
  }
  fputc('\n', stream);
}
