#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "macro.h"
#include "color.h"
#include "log.h"


extern inline bool LogLevel_should_log (enum LogLevelFlags self);


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


#define LOGGER_BEGIN \
  if unlikely (!LogLevel_should_log(log_level)) { \
    return; \
  } \
  __attribute__((unused)) FILE *stream = \
    log_level == LOG_LEVEL_ERROR || log_level == LOG_LEVEL_CRITICAL ? \
      stderr : stdout


void logger_start_va (
    const char log_domain[], enum LogLevelFlags log_level,
    const char file[], int line, const char func[], const char format[],
    va_list arg) {
  LOGGER_BEGIN;

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

  vfprintf(stream, format, arg);
}


void logger_start (
    const char log_domain[], enum LogLevelFlags log_level,
    const char file[], int line, const char func[], const char format[], ...) {
  LOGGER_BEGIN;

  va_list arg;
  va_start(arg, format);
  logger_start_va(log_domain, log_level, file, line, func, format, arg);
  va_end(arg);
}


void logger_continue_va (
    enum LogLevelFlags log_level, const char format[], va_list arg) {
  LOGGER_BEGIN;

  vfprintf(stream, format, arg);
}


void logger_continue (enum LogLevelFlags log_level, const char format[], ...) {
  LOGGER_BEGIN;

  va_list arg;
  va_start(arg, format);
  vfprintf(stream, format, arg);
  va_end(arg);
}


void logger_continue_literal (
    enum LogLevelFlags log_level, const char format[]) {
  LOGGER_BEGIN;

  fputs(format, stream);
}


void logger_end (enum LogLevelFlags log_level) {
  LOGGER_BEGIN;

  fputc('\n', stream);
}


void logger (
    const char log_domain[], enum LogLevelFlags log_level,
    const char file[], int line, const char func[], const char format[], ...) {
  LOGGER_BEGIN;

  va_list arg;
  va_start(arg, format);
  logger_start_va(log_domain, log_level, file, line, func, format, arg);
  va_end(arg);

  fputc('\n', stream);
}
