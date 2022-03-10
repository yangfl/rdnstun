#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
// #include <signal.h>
struct sigaction;
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

/**
 * @file
 * Log utility functions.
 */


__attribute__((noinline))
/**
 * @brief Print backtrace to file descriptor, skipping libc init frames.
 *
 * @param fd File descriptor.
 * @param skip Number of topmost frames to skip (This function itself is always
 *  skipped).
 * @param with_header Whether to print "Backtrace:" header.
 * @param write @c write() function.
 */
void print_backtrace (int fd, int skip, bool with_header);

__attribute__((access(write_only, 3)))
/**
 * @brief Print backtrace when @c SIGSEGV.
 *
 * @param enable_debug Allow waiting debugger attachment.
 * @param exit_status Exit status when @c SIGSEGV. If 0, @c EXIT_FAILURE is
 *  used.
 * @param[out] oldact Previously associated action. Can be @c NULL.
 * @return 0 on success, -1 if @c sigaction() error.
 */
int diagnose_sigsegv (
  bool enable_debug, int exit_status, struct sigaction *oldact);


/******************************************************************************
 * LogLevel
 ******************************************************************************/

/// Level of log messages, compatible with syslog's priority concept, see RFC 5424.
enum LogLevel {
  /// system is unusable
  LOG_LEVEL_EMERG    = 0,
  /// action must be taken immediately
  LOG_LEVEL_ALERT    = 1,
  /// fatal conditions
  LOG_LEVEL_CRITICAL = 2,
  /// error conditions
  LOG_LEVEL_ERROR    = 3,
  /// warning conditions
  LOG_LEVEL_WARNING  = 4,
  /// normal but significant condition
  LOG_LEVEL_NOTICE   = 5,
  /// informational messages
  LOG_LEVEL_INFO     = 6,
  /// debug-level messages
  LOG_LEVEL_DEBUG    = 7,
  /// more debug messages
  LOG_LEVEL_VERBOSE  = 8,
};

/// number of log levels
#define LOG_LEVEL_COUNT 9

/// name of log levels
extern const char * const LogLevel_names[LOG_LEVEL_COUNT];

__attribute__((const, warn_unused_result))
/**
 * @brief Clamp a log level to a valid value.
 *
 * @param level Log level.
 * @return Clamped log level.
 */
inline int LogLevel_clamp (int level) {
  return level < LOG_LEVEL_EMERG ? LOG_LEVEL_EMERG :
         level > LOG_LEVEL_VERBOSE ? LOG_LEVEL_VERBOSE : level;
}


/******************************************************************************
 * LoggerStream
 ******************************************************************************/

struct LoggerEvent;

/**
 * @memberof LoggerStream
 * @brief general type of log output function
 */
typedef int (*LoggerStreamFunc) ();

/// Log stream.
struct LoggerStream {
  /// output file descriptor
  int fd;
  /// user data
  void *userdata;
  __attribute__((
    access(read_only, 2), access(read_only, 4),
    access(read_only, 6), access(read_only, 7)))
  /**
   * @brief begin output function
   *
   * @param self Log event.
   * @param domain Log domain.
   * @param level Log level.
   * @param file File name at which logger is called. Can be @c NULL.
   * @param line Line number at which logger is called.
   * @param func Function name at which logger is called. Can be @c NULL.
   * @param sgr Select Graphic Rendition parameter.
   * @return Number of bytes written.
   */
  int (*begin) (
    struct LoggerEvent * __restrict self, const char * __restrict domain,
    unsigned char level, const char * __restrict file, int line,
    const char * __restrict func, const char * __restrict sgr);
  __attribute__((access(read_only, 1)))
  /**
   * @brief end output function
   *
   * @param self Log event.
   * @return Number of bytes written.
   */
  int (*end) (const struct LoggerEvent * __restrict self);
};

/// log stream for @c stdout
extern const struct LoggerStream logger_stream_stdout;
/// log stream for @c stderr
extern const struct LoggerStream logger_stream_stderr;
/// log stream for @c stdout without color
extern const struct LoggerStream logger_stream_stdout_nocolor;
/// log stream for @c stderr without color
extern const struct LoggerStream logger_stream_stderr_nocolor;


/******************************************************************************
 * LoggerFormattedStream
 ******************************************************************************/

/// Log stream with format.
struct LoggerFormattedStream {
  /// output stream
  const struct LoggerStream *stream;
  /// Select Graphic Rendition parameter
  const char *sgr;
};


/******************************************************************************
 * Logger
 ******************************************************************************/

/// Log controller.
struct Logger {
  /// log level, see ::LogLevel
  unsigned char level;
  /// fatal log level above which program will be terminated, see ::LogLevel
  unsigned char fatal;
  union {
    struct {
      /// unused
      bool _unused : 1;
      /// whether to print backtrace on fatal log
      bool backtrace : 1;
      /// whether to allow debug on fatal log
      bool debug : 1;
    };
    /// all flags
    unsigned char flags;
  };

  /// log domain
  char *domain;
  /// output streams
  struct LoggerFormattedStream formatted[LOG_LEVEL_COUNT];
};

/// Logger initializer with proper defaults
#define LOGGER_INIT \
  .level = LOG_LEVEL_NOTICE, \
  .fatal = LOG_LEVEL_CRITICAL, \
  .backtrace = true, \
  .formatted = { \
    {&logger_stream_stderr, \
     "5;1;" COLOR_CODE(COLOR_BACKGROUND_RED) ";" COLOR_CODE(COLOR_FOREGROUND_BRIGHT_WHITE)}, \
    {&logger_stream_stderr, \
     "1;" COLOR_CODE(COLOR_BACKGROUND_RED) ";" COLOR_CODE(COLOR_FOREGROUND_BRIGHT_WHITE)}, \
    {&logger_stream_stderr, "1;" COLOR_CODE(COLOR_FOREGROUND_RED)}, \
    {&logger_stream_stdout, "1;" COLOR_CODE(COLOR_FOREGROUND_RED)}, \
    {&logger_stream_stdout, "1;" COLOR_CODE(COLOR_FOREGROUND_YELLOW)}, \
    {&logger_stream_stdout, "1;" COLOR_CODE(COLOR_FOREGROUND_GREEN)}, \
    {&logger_stream_stdout, COLOR_CODE(COLOR_FOREGROUND_BLUE)}, \
    {&logger_stream_stdout, COLOR_CODE(COLOR_FOREGROUND_MAGENTA)}, \
    {&logger_stream_stdout, COLOR_CODE(COLOR_FOREGROUND_CYAN)}, \
  }, \

#ifndef __LOG_BUILD
/// log controller of this program
extern const struct Logger app_logger;
#endif

#ifndef CURRENT_LOGGER
/// current log controller to use, you may define it before include this header
# define CURRENT_LOGGER app_logger
#endif

/// set attritube of a log controller
#define Logger_set_attribute(self, name, value) \
  (((struct Logger *) &(self))->name = (value))

/// set attritube of #CURRENT_LOGGER
#define LOGGER_SET_ATTRIBUTE(name, value) \
  Logger_set_attribute(CURRENT_LOGGER, name, value)

__attribute__((nonnull, access(write_only, 1), access(none, 3)))
/**
 * @memberof Logger
 * @brief Set output streams below the log level.
 *
 * @param[out] self Log controller.
 * @param level Log level.
 * @param stream Log stream.
 */
void Logger_set_stream (
  struct Logger * __restrict self, int level,
  const struct LoggerStream * __restrict stream);

__attribute__((pure, warn_unused_result, nonnull, access(read_only, 1)))
/**
 * @memberof Logger
 * @brief Test whether a log level will be logged.
 *
 * @param self Log controller.
 * @param level Log level.
 * @return @c true if will.
 */
inline bool Logger_would_log (
    const struct Logger * __restrict self, int level) {
  return level <= self->level;
}
/**
 * @relates Logger
 * @brief Test whether a log level will be logged in #CURRENT_LOGGER.
 *
 * @param level Log level.
 * @return @c true if will.
 */
#define LOG_WOULD_LOG(level) Logger_would_log(&CURRENT_LOGGER, level)


/******************************************************************************
 * LoggerEvent
 ******************************************************************************/

/// Log event.
struct LoggerEvent {
  /// output stream
  const struct LoggerStream *stream;
  union {
    /// user data
    void *userdata;
    /// user data
    unsigned char data[8];
  };

  /// number of frames to skip when printing backtrace
  unsigned char skip;
  /// exit status when this event is fatal
  unsigned char exit_status;
  union {
    struct {
      /// whether this event is fatal
      bool fatal : 1;
      /// whether to print backtrace
      bool backtrace : 1;
      /// whether to allow debug
      bool debug : 1;
    };
    /// all flags
    unsigned char flags;
  };
};

#ifndef LOGGER_EVENT_VAR_NAME
/// default log event variable name, you may define it before include this header
# define LOGGER_EVENT_VAR_NAME event
#endif
#ifndef LOGGER_EVENT_FILE_NAME
/// file name for log event
# define LOGGER_EVENT_FILE_NAME __FILE__
#endif
#ifndef LOGGER_EVENT_FUNC_NAME
/// function name for log event
# define LOGGER_EVENT_FUNC_NAME __func__
#endif

/// @cond GARBAGE
#ifndef LOGGER_NO_OPTIMIZATION
# define LOGGER_EVENT_GUARD_LEVEL(self, level) \
  if ((level) >= LOG_LEVEL_WARNING) if ((self)->flags) __builtin_unreachable()
# define LOGGER_EVENT_GUARD_BEGIN(self) \
  const struct LoggerStream *__logger_event_saved_stream = (self)->stream; \
  int __logger_event_saved_fd = (self)->stream->fd; \
  unsigned char __logger_event_saved_flags = (self)->flags
# define LOGGER_EVENT_GUARD_END(self) \
  if ((self)->stream != __logger_event_saved_stream) __builtin_unreachable(); \
  if ((self)->stream->fd != __logger_event_saved_fd) __builtin_unreachable(); \
  if ((self)->flags != __logger_event_saved_flags) __builtin_unreachable()
#else
# define LOGGER_EVENT_GUARD_LEVEL(self, level)
# define LOGGER_EVENT_GUARD_BEGIN(self)
# define LOGGER_EVENT_GUARD_END(self)
#endif
/// @endcond

__attribute__((nonnull, access(read_only, 1), access(read_only, 2)))
/**
 * @memberof LoggerEvent
 * @brief Append message in a log event.
 *
 * You should use #LoggerEvent_log_va() instead.
 *
 * @param self Log event.
 * @param format Format string.
 * @param ap Variable argument list.
 * @return Number of bytes written.
 */
inline int LoggerEvent_log_va_func (
    const struct LoggerEvent * __restrict self,
    const char * __restrict format, va_list ap) {
  return vdprintf(self->stream->fd, format, ap);
}
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates LoggerEvent
 * @brief Append message in a log event.
 *
 * @param self Log event.
 * @param format Format string.
 * @param ap Variable argument list.
 * @return Number of bytes written.
 */
# define LoggerEvent_log_va(self, format, ap) __extension__ ({ \
  LOGGER_EVENT_GUARD_BEGIN(self); \
  int res = LoggerEvent_log_va_func(self, format, ap); \
  LOGGER_EVENT_GUARD_END(self); res; })
#else
# define LoggerEvent_log_va LoggerEvent_log_va_func
#endif
/**
 * @relates LoggerEvent
 * @brief Append message in log event #LOGGER_EVENT_VAR_NAME.
 *
 * @param format Format string.
 * @param ap Variable argument list.
 * @return Number of bytes written.
 */
#define LOGEVENT_LOG_VA(format, ap) LoggerEvent_log_va( \
  &LOGGER_EVENT_VAR_NAME, format, ap)

__attribute__((nonnull, access(read_only, 1), access(read_only, 2),
               format(printf, 2, 3)))
/**
 * @memberof LoggerEvent
 * @brief Append message in a log event.
 *
 * You should use #LoggerEvent_log() instead.
 *
 * @param self Log event.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written.
 */
int LoggerEvent_log_func (
  const struct LoggerEvent * __restrict self,
  const char * __restrict format, ...);
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates LoggerEvent
 * @brief Append message in a log event.
 *
 * @param self Log event.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written.
 */
# define LoggerEvent_log(self, ...) __extension__ ({ \
  LOGGER_EVENT_GUARD_BEGIN(self); \
  int res = LoggerEvent_log_func(self, __VA_ARGS__); \
  LOGGER_EVENT_GUARD_END(self); res; })
#else
# define LoggerEvent_log LoggerEvent_log_func
#endif
/**
 * @relates LoggerEvent
 * @brief Append message in log event #LOGGER_EVENT_VAR_NAME.
 *
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written.
 */
#define LOGEVENT_LOG(...) LoggerEvent_log(&LOGGER_EVENT_VAR_NAME, __VA_ARGS__)

__attribute__((nonnull, access(read_only, 1), access(read_only, 2, 3)))
/**
 * @memberof LoggerEvent
 * @brief Append message in a log event.
 *
 * You should use #LoggerEvent_write() instead.
 *
 * @param self Log event.
 * @param str Message.
 * @param len Length of Message.
 * @return Number of bytes written.
 */
inline int LoggerEvent_write_func (
    const struct LoggerEvent * __restrict self,
    const char * __restrict str, size_t len) {
  return write(self->stream->fd, str, len);
}
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates LoggerEvent
 * @brief Append message in a log event.
 *
 * @param self Log event.
 * @param str Message.
 * @param len Length of Message.
 * @return Number of bytes written.
 */
# define LoggerEvent_write(self, str, len) __extension__ ({ \
  LOGGER_EVENT_GUARD_BEGIN(self); \
  int res = LoggerEvent_write_func(self, str, len); \
  LOGGER_EVENT_GUARD_END(self); res; })
#else
# define LoggerEvent_write LoggerEvent_write_func
#endif
/**
 * @relates LoggerEvent
 * @brief Append message in log event #LOGGER_EVENT_VAR_NAME.
 *
 * @param str Message.
 * @param len Length of Message.
 * @return Number of bytes written.
 */
#define LOGEVENT_WRITE(str, len) LoggerEvent_write( \
  &LOGGER_EVENT_VAR_NAME, str, len)

__attribute__((nonnull, access(read_only, 1), access(read_only, 2)))
/**
 * @memberof LoggerEvent
 * @brief Append message in a log event.
 *
 * You should use #LoggerEvent_puts() instead.
 *
 * @param self Log event.
 * @param str Message.
 * @return Number of bytes written.
 */
inline int LoggerEvent_puts_func (
    const struct LoggerEvent * __restrict self, const char * __restrict str) {
  return LoggerEvent_write_func(self, str, strlen(str));
}
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates LoggerEvent
 * @brief Append message in a log event.
 *
 * @param self Log event.
 * @param str Message.
 * @return Number of bytes written.
 */
# define LoggerEvent_puts(self, str) __extension__ ({ \
  LOGGER_EVENT_GUARD_BEGIN(self); \
  int res = LoggerEvent_puts_func(self, str); \
  LOGGER_EVENT_GUARD_END(self); res; })
#else
# define LoggerEvent_puts LoggerEvent_puts_func
#endif
/**
 * @relates LoggerEvent
 * @brief Append message in log event #LOGGER_EVENT_VAR_NAME.
 *
 * @param str Message.
 * @return Number of bytes written.
 */
#define LOGEVENT_PUTS(str) LoggerEvent_puts(&LOGGER_EVENT_VAR_NAME, str)

__attribute__((nonnull, access(read_only, 1)))
/**
 * @memberof LoggerEvent
 * @brief Write an error message corresponding to the current value of @c errno
 *  in a log event.
 *
 * You should use #LoggerEvent_perror_func() instead.
 *
 * @param self Log event.
 * @param errnum Error number.
 * @param colon Whether to write a colon and a blank before error message.
 * @return Number of bytes written.
 */
int LoggerEvent_perror_func (
  const struct LoggerEvent * __restrict self, int errnum, bool colon);
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates LoggerEvent
 * @brief Write an error message corresponding to the current value of @c errno
 *  in a log event.
 *
 * @param self Log event.
 * @param errnum Error number.
 * @param colon Whether to write a colon and a blank before error message.
 * @return Number of bytes written.
 */
# define LoggerEvent_perror(self, errnum, colon) __extension__ ({ \
  LOGGER_EVENT_GUARD_BEGIN(self); \
  int res = LoggerEvent_perror_func(self, errnum, colon); \
  LOGGER_EVENT_GUARD_END(self); res; })
#else
# define LoggerEvent_perror LoggerEvent_perror_func
#endif
/**
 * @relates LoggerEvent
 * @brief Write an error message corresponding to the current value of @c errno
 *  in log event #LOGGER_EVENT_VAR_NAME.
 *
 * @param self Log event.
 * @param errnum Error number.
 * @param colon Whether to write a colon and a blank before error message.
 * @return Number of bytes written.
 */
#define LOGEVENT_PERROR(errnum, colon) LoggerEvent_perror( \
  &LOGGER_EVENT_VAR_NAME, errnum, colon)

__attribute__((noinline, nonnull, access(read_only, 1)))
/**
 * @memberof LoggerEvent
 * @brief Print backtrace in a log event, skip topmost @p self->skip + @p skip
 *  frames.
 *
 * You should use #LoggerEvent_backtrace() instead.
 *
 * @param self Log event.
 * @param skip Number of additional topmost frames to skip (This function itself
 *  is always skipped).
 * @return 0.
 */
int LoggerEvent_backtrace_func (
  const struct LoggerEvent * __restrict self, int skip);
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates LoggerEvent
 * @brief Print backtrace in a log event, skip topmost @p self->skip + @p skip
 *  frames.
 *
 * @param self Log event.
 * @param skip Number of additional topmost frames to skip (This function itself
 *  is always skipped).
 * @return 0.
 */
# define LoggerEvent_backtrace(self, skip) __extension__ ({ \
  LOGGER_EVENT_GUARD_BEGIN(self); \
  int res = LoggerEvent_backtrace_func(self, skip); \
  LOGGER_EVENT_GUARD_END(self); res; })
#else
# define LoggerEvent_backtrace LoggerEvent_backtrace_func
#endif
/**
 * @relates LoggerEvent
 * @brief Print backtrace in log event #LOGGER_EVENT_VAR_NAME, skip topmost
 *  @p self->skip + @p skip frames.
 *
 * @param skip Number of additional topmost frames to skip (This function itself
 *  is always skipped).
 * @return 0.
 */
#define LOGEVENT_BACKTRACE(skip) LoggerEvent_backtrace( \
  &LOGGER_EVENT_VAR_NAME, skip)

__attribute__((noinline, access(read_only, 1)))
/**
 * @memberof LoggerEvent
 * @brief End a log event, write a new-line, print backtrace if
 *  @p self->backtrace, terminate program if @p self->fatal, start debug if
 *  @p self->debug.
 *
 * You should use #LoggerEvent_destroy() instead.
 *
 * @param self Log event.
 * @return Number of bytes written. If @p self->fatal, this function should not
 *  return, unless @p self->debug and debugger is attached.
 */
int LoggerEvent_destroy_func (const struct LoggerEvent * __restrict self);
__attribute__((always_inline, access(read_only, 1)))
/**
 * @memberof LoggerEvent
 * @brief End a log event, write a new-line, print backtrace if
 *  @p self->backtrace, terminate program if @p self->fatal, start debug if
 *  @p self->debug.
 *
 * @param self Log event.
 * @return Number of bytes written. If @p self->fatal, this function should not
 *  return, unless @p self->debug and debugger is attached.
 */
#define LoggerEvent_destroy_inline(self) __builtin_expect((self)->flags, 0) ? \
  LoggerEvent_destroy_func(self) : \
  write((self)->stream->fd, "\n", 1) + ( \
    __builtin_expect((self)->stream->end != NULL, 0) ? \
      (self)->stream->end(self) : 0)
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates LoggerEvent
 * @brief End a log event, write a new-line, print backtrace if
 *  @p self->backtrace, terminate program if @p self->fatal, start debug if
 *  @p self->debug.
 *
 * @param self Log event.
 * @return Number of bytes written. If @p self->fatal, this function should not
 *  return, unless @p self->debug and debugger is attached.
 */
# define LoggerEvent_destroy(self) __extension__ ({ \
  LOGGER_EVENT_GUARD_BEGIN(self); \
  int res = LoggerEvent_destroy_inline(self); \
  LOGGER_EVENT_GUARD_END(self); res; })
#else
# define LoggerEvent_destroy LoggerEvent_destroy_func
#endif
/**
 * @relates LoggerEvent
 * @brief End log event #LOGGER_EVENT_VAR_NAME, write a new-line, print
 *  backtrace if #LOGGER_EVENT_VAR_NAME->backtrace, terminate program if
 *  #LOGGER_EVENT_VAR_NAME->fatal, start debug if #LOGGER_EVENT_VAR_NAME->debug.
 *
 * @return Number of bytes written. If #LOGGER_EVENT_VAR_NAME->fatal, this
 *  function should not return, unless #LOGGER_EVENT_VAR_NAME->debug and
 *  debugger is attached.
 */
#define LOGEVENT_DESTROY() LoggerEvent_destroy(&LOGGER_EVENT_VAR_NAME)

__attribute__((nonnull, access(read_only, 2),
               access(read_only, 4), access(read_only, 6)))
/**
 * @memberof LoggerEvent
 * @brief Initialize a log event and print heading of a log event.
 *
 * You should use #LoggerEvent_init() instead.
 *
 * @note This function assumes the log level will always be logged.
 *
 * @param[out] self Log event.
 * @param logger Log controller.
 * @param level Log level.
 * @param file File name at which logger is called. May be @c NULL.
 * @param line Line number at which logger is called.
 * @param func Function name at which logger is called. May be @c NULL.
 * @return Number of bytes written.
 */
inline int LoggerEvent_init_func (
    struct LoggerEvent * __restrict self,
    const struct Logger * __restrict logger, int level,
    const char * __restrict file, int line, const char * __restrict func) {
  struct LoggerFormattedStream formatted =
    logger->formatted[LogLevel_clamp(level)];

  self->stream = formatted.stream;

  self->skip = 0;
  self->exit_status = 0;
  if (__builtin_expect(level <= LOG_LEVEL_WARNING, 0) &&
      __builtin_expect(level <= logger->fatal, 0)) {
    self->fatal = true;
    self->backtrace = logger->backtrace;
    self->debug = logger->debug;
  } else {
    self->flags = 0;
  }

  LOGGER_EVENT_GUARD_BEGIN(self);
  int res = self->stream->begin == NULL ? 0 : self->stream->begin(
    self, logger->domain, level, file, line, func, formatted.sgr);
  LOGGER_EVENT_GUARD_END(self);

  return res;
}
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates LoggerEvent
 * @brief Initialize a log event and print heading of a log event.
 *
 * @note This function assumes the log level will always be logged.
 *
 * @param[out] self Log event.
 * @param logger Log controller.
 * @param level Log level.
 * @param file File name at which logger is called. May be @c NULL.
 * @param line Line number at which logger is called.
 * @param func Function name at which logger is called. May be @c NULL.
 * @return Number of bytes written.
 */
# define LoggerEvent_init_explicit(self, logger, level, file, line, func) __extension__ ({ \
  int res = LoggerEvent_init_func(self, logger, level, file, line, func); \
  LOGGER_EVENT_GUARD_LEVEL(self, level); res; })
#else
# define LoggerEvent_init_explicit LoggerEvent_begin_func
#endif
/**
 * @relates LoggerEvent
 * @brief Initialize a log event and print heading of a log event.
 *
 * @note This function assumes the log level will always be logged.
 *
 * @param[out] self Log event.
 * @param logger Log controller.
 * @param level Log level.
 * @return Number of bytes written.
 */
#define LoggerEvent_init(self, logger, level) LoggerEvent_init_explicit( \
  self, logger, level, LOGGER_EVENT_FILE_NAME, __LINE__, LOGGER_EVENT_FUNC_NAME)
/**
 * @relates LoggerEvent
 * @brief Initialize log event #LOGGER_EVENT_VAR_NAME and print heading of a log
 *  event.
 *
 * @note This function assumes the log level will always be logged.
 *
 * @param logger Log controller.
 * @param level Log level.
 * @return Number of bytes written.
 */
#define LOGEVENT_INIT(logger, level) \
  LoggerEvent_init(&LOGGER_EVENT_VAR_NAME, logger, level)

/**
 * @relates LoggerEvent
 * @brief Start log event #LOGGER_EVENT_VAR_NAME.
 *
 * @code{.c}
  logevent (logger, level)
    LOGEVENT_LOG("Hello, world!");
 * @endcode
 *
 * @param logger Log controller.
 * @param level Log level.
 */
#define logevent(logger, level) \
  if (!Logger_would_log(logger, level)) ; else switch (1) \
  for (struct LoggerEvent LOGGER_EVENT_VAR_NAME; \
       LoggerEvent_destroy(&LOGGER_EVENT_VAR_NAME), 0;) case 1: \
  if (LoggerEvent_init(&LOGGER_EVENT_VAR_NAME, logger, level), 0) ; else
/**
 * @relates LoggerEvent
 * @brief Start log event #LOGGER_EVENT_VAR_NAME within #CURRENT_LOGGER.
 *
 * @param level Log level.
 */
#define LOGEVENT(level) logevent(&CURRENT_LOGGER, level)


/******************************************************************************
 * Log
 ******************************************************************************/

__attribute__((nonnull(1, 6), access(read_only, 1), access(read_only, 3),
               access(read_only, 5), access(read_only, 6)))
/**
 * @memberof Logger
 * @brief Write a log message, followed by a new-line.
 *
 * You should use #Logger_log_va() instead.
 *
 * @param self Log controller.
 * @param level Log level.
 * @param file File name at which logger is called. Can be @c NULL.
 * @param line Line number at which logger is called.
 * @param func Function name at which logger is called. Can be @c NULL.
 * @param format Format string.
 * @param ap Variable argument list.
 * @return Number of bytes written, or -1 if no logging.
 */
inline int Logger_log_va_func (
    const struct Logger * __restrict self, int level,
    const char * __restrict file, int line, const char * __restrict func,
    const char * __restrict format, va_list ap) {
  if (!Logger_would_log(self, level)) {
    return -1;
  }

  struct LoggerEvent event;
  int res = LoggerEvent_init_func(&event, self, level, file, line, func);
  event.skip = 1;

  LOGGER_EVENT_GUARD_BEGIN(&event);
  res += LoggerEvent_log_va_func(&event, format, ap);
  LOGGER_EVENT_GUARD_END(&event);
  res += LoggerEvent_destroy_inline(&event);
  LOGGER_EVENT_GUARD_END(&event);
  return res;
}
/**
 * @relates Logger
 * @brief Write a log message, followed by a new-line.
 *
 * @param logger Log controller.
 * @param level Log level.
 * @param format Format string.
 * @param ap Variable argument list.
 * @return Number of bytes written, or -1 if no logging happens.
 */
#define Logger_log_va(logger, level, format, ap) ( \
  !Logger_would_log(logger, level) ? -1 : Logger_log_va_func( \
    logger, level, LOGGER_EVENT_FILE_NAME, __LINE__, LOGGER_EVENT_FUNC_NAME, \
    format, ap))
/**
 * @relates Logger
 * @brief Write a log message using #CURRENT_LOGGER, followed by a new-line.
 *
 * @param level Log level.
 * @param format Format string.
 * @param ap Variable argument list.
 * @return Number of bytes written, or -1 if no logging happens.
 */
#define LOG_VA(level, format, ap) Logger_log_va( \
  &CURRENT_LOGGER, level, format, ap)

__attribute__((
  nonnull(1, 6), access(read_only, 1), access(read_only, 3),
  access(read_only, 5), access(read_only, 6), format(printf, 6, 7)))
/**
 * @memberof Logger
 * @brief Write a log message, followed by a new-line.
 *
 * You should use #Logger_log() instead.
 *
 * @param self Log controller.
 * @param level Log level.
 * @param file File name at which logger is called. Can be @c NULL.
 * @param line Line number at which logger is called.
 * @param func Function name at which logger is called. Can be @c NULL.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written, or -1 if no logging happens.
 */
int Logger_log_func (
  const struct Logger * __restrict self, int level,
  const char * __restrict file, int line, const char * __restrict func,
  const char * __restrict format, ...);
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates Logger
 * @brief Write a log message, followed by a new-line.
 *
 * @param self Log controller.
 * @param level Log level.
 * @param file File name at which logger is called. Can be @c NULL.
 * @param line Line number at which logger is called.
 * @param func Function name at which logger is called. Can be @c NULL.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written, or -1 if no logging happens.
 */
# define Logger_log_explicit(self, level, file, line, func, ...) ( \
  !Logger_would_log(self, level) ? -1 : __extension__ ({ \
    struct LoggerEvent __logger_event; \
    int res = LoggerEvent_init_func( \
      &__logger_event, self, level, file, line, func); \
    LOGGER_EVENT_GUARD_BEGIN(&__logger_event); \
    res += dprintf(__logger_event.stream->fd, __VA_ARGS__); \
    res += LoggerEvent_destroy_inline(&__logger_event); \
    LOGGER_EVENT_GUARD_END(&__logger_event); \
    res; \
  }))
#else
# define Logger_log_explicit Logger_log_func
#endif
/**
 * @relates Logger
 * @brief Write a log message, followed by a new-line.
 *
 * @param self Log controller.
 * @param level Log level.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written, or -1 if no logging happens.
 */
#define Logger_log(self, level, ...) ( \
  !Logger_would_log(self, level) ? -1 : Logger_log_explicit( \
    self, level, LOGGER_EVENT_FILE_NAME, __LINE__, LOGGER_EVENT_FUNC_NAME, \
    __VA_ARGS__))
/**
 * @relates Logger
 * @brief Write a log message using #CURRENT_LOGGER, followed by a new-line.
 *
 * @param level Log level.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written, or -1 if no logging happens.
 */
#define LOG(level, ...) Logger_log(&CURRENT_LOGGER, level, __VA_ARGS__)

__attribute__((copy(Logger_log_func)))
/**
 * @memberof Logger
 * @brief Write a log message, followed by a colon and a blank, then an error
 *  message corresponding to the current value of @c errno and a new-line.
 *
 * You should use #Logger_log_perror() instead.
 *
 * @param self Log controller.
 * @param level Log level.
 * @param file File name at which logger is called. Can be @c NULL.
 * @param line Line number at which logger is called.
 * @param func Function name at which logger is called. Can be @c NULL.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written, or -1 if no logging happens.
 */
int Logger_log_perror_func (
  const struct Logger * __restrict self, int level,
  const char * __restrict file, int line, const char * __restrict func,
  const char * __restrict format, ...);
#ifndef LOGGER_NO_OPTIMIZATION
/**
 * @relates Logger
 * @brief Write a log message, followed by a colon and a blank, then an error
 *  message corresponding to the current value of @c errno and a new-line.
 *
 * @param self Log controller.
 * @param level Log level.
 * @param file File name at which logger is called. Can be @c NULL.
 * @param line Line number at which logger is called.
 * @param func Function name at which logger is called. Can be @c NULL.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written, or -1 if no logging happens.
 */
# define Logger_log_perror_explicit(self, level, file, line, func, ...) ( \
  !Logger_would_log(self, level) ? -1 : __extension__ ({ \
    struct LoggerEvent __logger_event; \
    int errnum = errno; \
    int res = LoggerEvent_init_func( \
      &__logger_event, self, level, file, line, func); \
    LOGGER_EVENT_GUARD_BEGIN(&__logger_event); \
    res += dprintf(__logger_event.stream->fd, __VA_ARGS__); \
    res += LoggerEvent_perror_func(&__logger_event, errnum, true); \
    LOGGER_EVENT_GUARD_END(&__logger_event); \
    res += LoggerEvent_destroy_inline(&__logger_event); \
    LOGGER_EVENT_GUARD_END(&__logger_event); \
    res; \
  }))
#else
# define Logger_log_perror_explicit Logger_log_perror_func
#endif
/**
 * @relates Logger
 * @brief Write a log message, followed by a colon and a blank, then an error
 *  message corresponding to the current value of @c errno and a new-line.
 *
 * @param self Log controller.
 * @param level Log level.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written, or -1 if no logging happens.
 */
#define Logger_log_perror(self, level, ...) ( \
  !Logger_would_log(self, level) ? -1 : Logger_log_perror_explicit( \
    self, level, LOGGER_EVENT_FILE_NAME, __LINE__, LOGGER_EVENT_FUNC_NAME, \
    __VA_ARGS__))
/**
 * @relates Logger
 * @brief Write a log message using #CURRENT_LOGGER, followed by a colon and a
 *  blank, then an error message corresponding to the current value of @c errno
 *  and a new-line.
 *
 * @param level Log level.
 * @param format Format string.
 * @param ... Format arguments.
 * @return Number of bytes written, or -1 if no logging happens.
 */
#define LOG_PERROR(level, ...) Logger_log_perror( \
  &CURRENT_LOGGER, level, __VA_ARGS__)


#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
