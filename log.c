#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "macro.h"
#include "color.h"
#include "log.h"


extern inline int logger_set_level (int lvl);
extern inline bool logger_would_log (int lvl);


int logger_level = LOG_LEVEL_MESSAGE;
static FILE *log_streams[2] = {0};


int LogLevel_tocolor (int self) {
  switch (self) {
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
      return self <= 0 ? COLOR_BLUE : COLOR_WHITE;
  }
}


const char *LogLevel_tostring_c (int self) {
  switch (self) {
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
      return self <= 0 ? "DEBUG" : "UNKNOWN";
  }
}


FILE **logger_set_stream (FILE *out, FILE *err) {
  if (out != NULL) {
    log_streams[0] = out;
  }
  if (err != NULL) {
    log_streams[1] = err;
  }
  return log_streams;
}


FILE *logger_get_stream (int lvl) {
  if unlikely (log_streams[0] == NULL) {
    log_streams[0] = stdout;
    log_streams[1] = stderr;
  }
  return log_streams[lvl == LOG_LEVEL_ERROR || lvl == LOG_LEVEL_CRITICAL];
}


#define LOGGER_BEGIN \
  if unlikely (!logger_would_log(log_level)) { \
    return; \
  } \
  __attribute__((unused)) FILE *stream = logger_get_stream(log_level)


void logger_start_va (
    const char log_domain[], int log_level,
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
    const char log_domain[], int log_level,
    const char file[], int line, const char func[], const char format[], ...) {
  LOGGER_BEGIN;

  va_list arg;
  va_start(arg, format);
  logger_start_va(log_domain, log_level, file, line, func, format, arg);
  va_end(arg);
}


void logger_continue_va (
    int log_level, const char format[], va_list arg) {
  LOGGER_BEGIN;

  vfprintf(stream, format, arg);
}


void logger_continue (int log_level, const char format[], ...) {
  LOGGER_BEGIN;

  va_list arg;
  va_start(arg, format);
  vfprintf(stream, format, arg);
  va_end(arg);
}


void logger_continue_literal (
    int log_level, const char format[]) {
  LOGGER_BEGIN;

  fputs(format, stream);
}


void logger_end (int log_level) {
  LOGGER_BEGIN;

  fputc('\n', stream);
}


void logger (
    const char log_domain[], int log_level,
    const char file[], int line, const char func[], const char format[], ...) {
  LOGGER_BEGIN;

  va_list arg;
  va_start(arg, format);
  logger_start_va(log_domain, log_level, file, line, func, format, arg);
  va_end(arg);

  fputc('\n', stream);
}


void logger_perror (
    const char log_domain[], int log_level,
    const char file[], int line, const char func[], const char format[], ...) {
  LOGGER_BEGIN;
  int err = errno;

  va_list arg;
  va_start(arg, format);
  logger_start_va(log_domain, log_level, file, line, func, format, arg);
  va_end(arg);

  static locale_t locale = 0;
  if unlikely (locale == 0) {
    locale = newlocale(LC_ALL_MASK, "", 0);
  }
  fputs(": ", stream);
  fputs(likely (locale != 0) ? strerror_l(err, locale) : strerror(err), stream);

  fputc('\n', stream);
}
