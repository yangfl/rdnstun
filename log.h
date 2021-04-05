#ifndef LOG_H
#define LOG_H


enum LogLevelFlags {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO,
  LOG_LEVEL_MESSAGE,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_CRITICAL,
  LOG_LEVEL_ERROR,
} ;
extern enum LogLevelFlags effective_log_level;

__attribute__((const, warn_unused_result))
int LogLevel_tocolor (enum LogLevelFlags self);
__attribute__((returns_nonnull, const, warn_unused_result))
const char *LogLevel_tostring_c (enum LogLevelFlags self);

#define should_log(log_level) (effective_log_level >= (log_level))

__attribute__((
  access(read_only, 1), access(read_only, 3), access(read_only, 5),
  access(read_only, 6), format(printf, 6, 7)))
void logger (
  const char log_domain[], enum LogLevelFlags log_level,
  const char file[], int line, const char func[], const char format[], ...);
#define LOGGER(log_domain, log_level, ...) \
  if (!should_log(log_level)) {} else \
    logger(log_domain, log_level, __FILE__, __LINE__, __func__, __VA_ARGS__)


#endif /* LOG_H */
