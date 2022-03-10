#include <errno.h>
#include <execinfo.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "macro.h"
#include "color.h"
#define __LOG_BUILD
#include "log.h"


extern struct Logger app_logger;


extern inline int LogLevel_clamp (int level);

extern inline bool Logger_would_log (
  const struct Logger * __restrict self, int level);

extern inline int LoggerEvent_log_va_func (
  const struct LoggerEvent * __restrict self,
  const char * __restrict format, va_list ap);
extern inline int LoggerEvent_write_func (
  const struct LoggerEvent * __restrict self,
  const char * __restrict str, size_t len);
extern inline int LoggerEvent_puts_func (
  const struct LoggerEvent * __restrict self, const char * __restrict str);
extern inline int LoggerEvent_init_func (
  struct LoggerEvent * __restrict self,
  const struct Logger * __restrict logger, int level,
  const char * __restrict file, int line, const char * __restrict func);

extern inline int Logger_log_va_func (
  const struct Logger * __restrict self, int level,
  const char * __restrict file, int line, const char * __restrict func,
  const char * __restrict format, va_list ap);


#define WRITE(fd, str) ((void) (write(fd, str, sizeof(str) - 1) == sizeof(str) - 1))


void print_backtrace (int fd, int skip, bool with_header) {
  void *symbols[64];
  register const int symbols_size = arraysize(symbols);

  // skip itself
  skip++;
  if unlikely (skip >= symbols_size) {
    WRITE(fd, "(too many frames skipped)\n");
    return;
  }

  // get backtrace
  int size = backtrace(symbols, symbols_size);
  should (size > 0) otherwise {
    WRITE(fd, "(no available backtrace)\n");
    return;
  }

  // write header
  if (with_header) {
    WRITE(fd, "Backtrace:\n");
  }

  int skipped = size;
  // skip libc init
  if likely (size < symbols_size) {
    skipped -= 2;
  }
  // skip topmost frame
  skipped -= skip;
  if likely (skipped > 0) {
    backtrace_symbols_fd(symbols + skip, skipped, fd);
  }

  // detect if all frames are printed
  if (size == symbols_size) {
    WRITE(fd, "...and possibly more\n");
  }
}


static bool wait_debugger (void) {
  // attached variable
  volatile bool attached = false;

  // set non buffered
  struct termios stdin_termios;
  tcgetattr(STDIN_FILENO, &stdin_termios);
  {
    struct termios termios = stdin_termios;
    termios.c_lflag &= ~(ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &termios);
  }

  // allow Ctrl-C to work
  struct sigaction sigint_sa;
  {
    static const struct sigaction default_sa = {.sa_handler = SIG_DFL};
    sigaction(SIGINT, &default_sa, &sigint_sa);
  }

  // wait
  fprintf(stderr, "(%d) Start debugging? [y/N] ", getpid());
  fflush(stderr);
  struct pollfd pollfd = {.fd = STDIN_FILENO, .events = POLLIN};
  for (int i = 0; i < 5; i++) {
    goto_if (poll(&pollfd, 1, 1000) > 0) char_inputed;
    fputc('.', stderr);
    fflush(stderr);
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &stdin_termios);
  fputc('\n', stderr);
  return false;

char_inputed:
  tcsetattr(STDIN_FILENO, TCSANOW, &stdin_termios);
  char input = getchar();
  if (input != '\n') {
    fputc('\n', stderr);
  }
  return_if_not (input == 'Y' || input == 'y') false;

  // prepare being attached
  if (!attached) {
    fprintf(
      stderr, "Wait debugger to attach... Enter 'fg' to restore program...\n");
    raise(SIGSTOP);
  }

  // test if attached
  return_if_not (attached) false;
  fprintf(stderr, "Attached!\n");

  // restore
  sigaction(SIGINT, &sigint_sa, NULL);
  return true;
}


static void exit_debug (int status, bool debug) {
  if (debug) {
    return_if (wait_debugger());
    fprintf(stderr, "terminated\n");
  }
  _exit(status);
}


static bool sigsegv_debug;
static int sigsegv_exit_status;


static void sigsegv_handler (int sig) {
  (void) sig;
  LOGEVENT (LOG_LEVEL_CRITICAL) {
    LOGEVENT_LOG("segmentation fault");
    event.skip = 2;
    event.exit_status = sigsegv_exit_status;
    event.fatal = true;
    event.backtrace = true;
    event.debug = sigsegv_debug;
  }
}


int diagnose_sigsegv (
    bool enable_debug, int exit_status, struct sigaction *oldact) {
  sigsegv_debug = enable_debug;
  sigsegv_exit_status = exit_status == 0 ? EXIT_FAILURE : exit_status;
  static const struct sigaction sigsegv_sa = {.sa_handler = sigsegv_handler};
  return sigaction(SIGSEGV, &sigsegv_sa, oldact);
}


/******************************************************************************
 * LogLevel
 ******************************************************************************/

const char * const LogLevel_names[9] = {
  "EMERGENCY", "ALERT", "CRITICAL", "ERROR",
  "WARNING", "NOTICE", "INFO", "DEBUG", "VERBOSE",
};


/******************************************************************************
 * Logger
 ******************************************************************************/


static int logger_stream_std_begin (
    struct LoggerEvent * __restrict self, const char * __restrict domain,
    unsigned char level, const char * __restrict file, int line,
    const char * __restrict func, const char * __restrict sgr) {
  int ret;
  int fd = self->stream->fd;

  time_t curtime = time(NULL);
  struct tm timeinfo;
  localtime_r(&curtime, &timeinfo);
  char timebuf[64];
  strftime(
    timebuf, sizeof(timebuf), sgr != NULL ?
      COLOR_SEQ(COLOR_FOREGROUND_GREEN) "[%b %e %T]" RESET_SEQ : "[%b %e %T]",
    &timeinfo);

  if unlikely (file == NULL) {
    ret = dprintf(fd, "%s: ", timebuf);
  } else if unlikely (func == NULL) {
    ret = dprintf(fd, "%s (%s:%d): ", timebuf, file, line);
  } else {
    ret = dprintf(fd, "%s (%s:%d %s): ", timebuf, file, line, func);
  }

  if likely (domain != NULL) {
    ret += dprintf(fd, "%s-", domain);
  }
  const char *level_name = LogLevel_names[level];
  if likely (sgr != NULL) {
    ret += dprintf(fd, SGR_FORMAT_SEQ_START "%s" RESET_SEQ " **: ",
                   sgr, level_name);
  } else {
    ret += dprintf(fd, "%s **: ", level_name);
  }

  return ret;
}


static int logger_stream_std_begin_nocolor (
    struct LoggerEvent * __restrict self, const char * __restrict domain,
    unsigned char level, const char * __restrict file, int line,
    const char * __restrict func, const char * __restrict sgr) {
  (void) sgr;
  return logger_stream_std_begin(self, domain, level, file, line, func, NULL);
}


const struct LoggerStream logger_stream_stdout = {
  .fd = STDOUT_FILENO, .begin = logger_stream_std_begin,
};

const struct LoggerStream logger_stream_stderr = {
  .fd = STDERR_FILENO, .begin = logger_stream_std_begin,
};

const struct LoggerStream logger_stream_stdout_nocolor = {
  .fd = STDOUT_FILENO, .begin = logger_stream_std_begin_nocolor,
};

const struct LoggerStream logger_stream_stderr_nocolor = {
  .fd = STDERR_FILENO, .begin = logger_stream_std_begin_nocolor,
};

struct Logger app_logger = {LOGGER_INIT};


void Logger_set_stream (
    struct Logger * __restrict self, int level,
    const struct LoggerStream * __restrict stream) {
  for (int i = 0; i <= LogLevel_clamp(level); i++) {
    self->formatted[i].stream = stream;
  }
}


/******************************************************************************
 * LoggerEvent
 ******************************************************************************/


int LoggerEvent_log_func (
    const struct LoggerEvent * __restrict self,
    const char * __restrict format, ...) {
  va_list ap;
  va_start(ap, format);
  int res = LoggerEvent_log_va_func(self, format, ap);
  va_end(ap);
  return res;
}


int LoggerEvent_perror_func (
    const struct LoggerEvent * __restrict self, int errnum, bool colon) {
  if unlikely (errnum == 0) {
    errnum = errno;
  }

  int res = colon ? LoggerEvent_write_func(self, ": ", 2) : 0;

  static locale_t locale = 0;
  if unlikely (locale == 0) {
    locale = newlocale(LC_ALL_MASK, "", 0);
  }
  if likely (locale != 0) {
    res += LoggerEvent_puts_func(self, strerror_l(errnum, locale));
  } else {
    char buf[1024];
    strerror_r(errnum, buf, sizeof(buf));
    res += LoggerEvent_puts_func(self, buf);
  }

  return res;
}


int LoggerEvent_backtrace_func (
    const struct LoggerEvent * __restrict self, int skip) {
  print_backtrace(self->stream->fd, self->skip + skip + 1, true);
  return 0;
}


int LoggerEvent_destroy_func (const struct LoggerEvent * __restrict self) {
  int res = write(self->stream->fd, "\n", 1);
  if unlikely (self->backtrace) {
    print_backtrace(self->stream->fd, self->skip + 1, true);
  }
  if unlikely (self->stream->end != NULL) {
    res += self->stream->end(self);
  }
  if unlikely (self->fatal) {
    exit_debug(
      self->exit_status == 0 ? EXIT_FAILURE : self->exit_status, self->debug);
  }
  return res;
}


/******************************************************************************
 * Log
 ******************************************************************************/


int Logger_log_func (
    const struct Logger * __restrict self, int level,
    const char * __restrict file, int line, const char * __restrict func,
    const char * __restrict format, ...) {
  return_if (!Logger_would_log(self, level)) -1;

  struct LoggerEvent event;
  int res = LoggerEvent_init_func(&event, self, level, file, line, func);
  event.skip = 1;

  LOGGER_EVENT_GUARD_BEGIN(&event);

  va_list ap;
  va_start(ap, format);
  res += LoggerEvent_log_va_func(&event, format, ap);
  va_end(ap);

  LOGGER_EVENT_GUARD_END(&event);

  res += LoggerEvent_destroy_inline(&event);
  LOGGER_EVENT_GUARD_END(&event);
  return res;
}


int Logger_log_perror_func (
    const struct Logger * __restrict self, int level,
    const char * __restrict file, int line, const char * __restrict func,
    const char * __restrict format, ...) {
  return_if (!Logger_would_log(self, level)) -1;
  int errnum = errno;

  struct LoggerEvent event;
  int res = LoggerEvent_init_func(&event, self, level, file, line, func);
  event.skip = 1;

  LOGGER_EVENT_GUARD_BEGIN(&event);

  va_list ap;
  va_start(ap, format);
  res += LoggerEvent_log_va_func(&event, format, ap);
  va_end(ap);

  res += LoggerEvent_perror_func(&event, errnum, true);

  LOGGER_EVENT_GUARD_END(&event);

  res += LoggerEvent_destroy_inline(&event);
  LOGGER_EVENT_GUARD_END(&event);
  return res;
}
