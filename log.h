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
extern LogLevelFlags cur_level;


void logger (const char log_domain[], LogLevelFlags log_level,
             const char format[], ...) __attribute__((format(printf, 3, 4)));


#endif /* LOG_H */
