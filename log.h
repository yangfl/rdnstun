#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>


enum LogLevelFlags {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO,
  LOG_LEVEL_MESSAGE,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_CRITICAL,
  LOG_LEVEL_ERROR,
};

__attribute__((const, warn_unused_result))
int LogLevel_tocolor (int self);
__attribute__((returns_nonnull, const, warn_unused_result))
const char *LogLevel_tostring_c (int self);


extern int logger_level;

static inline int logger_set_level (int lvl) {
  logger_level = lvl;
  return lvl;
}

__attribute__((const, warn_unused_result))
static inline bool logger_would_log (int lvl) {
  return lvl >= logger_level;
}

__attribute__((access(read_only, 1), access(read_only, 2)))
FILE **logger_set_stream (FILE *out, FILE *err);
__attribute__((const, warn_unused_result))
FILE *logger_get_stream (int lvl);

__attribute__((
  nonnull(6), access(read_only, 1), access(read_only, 3), access(read_only, 5),
  access(read_only, 6)))
void logger_start_va (
  const char log_domain[], int log_level,
  const char file[], int line, const char func[], const char format[],
  va_list arg);
#define LOGGER_START_VA(log_domain, log_level, ...) \
  if (!logger_would_log(log_level)) {} else logger_start_va( \
    log_domain, log_level, __FILE__, __LINE__, __func__, __VA_ARGS__)
__attribute__((
  nonnull(6), access(read_only, 1), access(read_only, 3), access(read_only, 5),
  access(read_only, 6), format(printf, 6, 7)))
void logger_start (
  const char log_domain[], int log_level,
  const char file[], int line, const char func[], const char format[], ...);
#define LOGGER_START(log_domain, log_level, ...) \
  if (!logger_would_log(log_level)) {} else logger_start( \
    log_domain, log_level, __FILE__, __LINE__, __func__, __VA_ARGS__)
__attribute__((nonnull, access(read_only, 2)))
void logger_continue_va (
  int log_level, const char format[], va_list arg);
__attribute__((nonnull, access(read_only, 2), format(printf, 2, 3)))
void logger_continue (int log_level, const char format[], ...);
__attribute__((nonnull, access(read_only, 2)))
void logger_continue_literal (
  int log_level, const char format[]);
void logger_end (int log_level);

__attribute__((
  nonnull(6), access(read_only, 1), access(read_only, 3), access(read_only, 5),
  access(read_only, 6)))
void logger_va (
  const char log_domain[], int log_level,
  const char file[], int line, const char func[], const char format[],
  va_list arg);
#define LOGGER_VA(log_domain, log_level, ...) \
  if (!logger_would_log(log_level)) {} else logger_va( \
    log_domain, log_level, __FILE__, __LINE__, __func__, __VA_ARGS__)
__attribute__((
  nonnull(6), access(read_only, 1), access(read_only, 3), access(read_only, 5),
  access(read_only, 6), format(printf, 6, 7)))
void logger (
  const char log_domain[], int log_level,
  const char file[], int line, const char func[], const char format[], ...);
#define LOGGER(log_domain, log_level, ...) \
  if (!logger_would_log(log_level)) {} else logger( \
    log_domain, log_level, __FILE__, __LINE__, __func__, __VA_ARGS__)
__attribute__((
  nonnull(6), access(read_only, 1), access(read_only, 3), access(read_only, 5),
  access(read_only, 6), format(printf, 6, 7)))
void logger_perror (
  const char log_domain[], int log_level,
  const char file[], int line, const char func[], const char format[], ...);
#define LOGGER_PERROR(log_domain, log_level, ...) \
  if (!logger_would_log(log_level)) {} else logger_perror( \
    log_domain, log_level, __FILE__, __LINE__, __func__, __VA_ARGS__)


#endif /* LOG_H */
