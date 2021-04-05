#ifndef LOG_H
#define LOG_H


typedef enum {
  /* log flags */
  LOG_FLAG_RECURSION          = 1 << 0,
  LOG_FLAG_FATAL              = 1 << 1,

  /* GLib log levels */
  LOG_LEVEL_ERROR             = 1 << 2,       /* always fatal */
  LOG_LEVEL_CRITICAL          = 1 << 3,
  LOG_LEVEL_WARNING           = 1 << 4,
  LOG_LEVEL_MESSAGE           = 1 << 5,
  LOG_LEVEL_INFO              = 1 << 6,
  LOG_LEVEL_DEBUG             = 1 << 7,

  LOG_LEVEL_MASK              = ~(LOG_FLAG_RECURSION | LOG_FLAG_FATAL)
} LogLevelFlags;
extern LogLevelFlags effective_log_level;

#define should_log(log_level) (effective_log_level >= (log_level))

__attribute__((
  access(read_only, 1), access(read_only, 3), access(read_only, 5),
  access(read_only, 6), format(printf, 6, 7)))
void logger (
  const char log_domain[], LogLevelFlags log_level,
  const char file[], int line, const char func[], const char format[], ...);
#define LOGGER(log_domain, log_level, ...) \
  if (!should_log(log_level)) {} else \
    logger(log_domain, log_level, __FILE__, __LINE__, __func__, __VA_ARGS__)


#endif /* LOG_H */
