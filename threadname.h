#ifndef THREADNAME_H
#define THREADNAME_H

#ifdef __cplusplus
extern "C" {
#endif

/** @file */


// size of thread name, including terminating null
#define THREADNAME_SIZE 16

__attribute__((nonnull, access(write_only, 1, 2)))
/**
 * @brief Get current thread name.
 *
 * @param[out] buf Buffer to store thread name.
 * @param size Size of buffer.
 * @return 0 on success, error otherwise.
 */
int threadname_get (char *buf, int size);
__attribute__((nonnull, access(read_only, 1)))
/**
 * @brief Set current thread name.
 *
 * @param name Thread name.
 * @return 0 on success, error otherwise.
 */
int threadname_set (const char *name);
__attribute__((nonnull, access(read_only, 1)))
/**
 * @brief Set current thread name.
 *
 * @param format Format string.
 * @param ... Format arguments.
 * @return 0 on success, error otherwise.
 */
int threadname_format (const char *format, ...);
__attribute__((nonnull, access(read_only, 1), format(printf, 1, 2)))
/**
 * @brief Append string to current thread name.
 *
 * @param format Format string.
 * @param ... Format arguments.
 * @return 0 on success, error otherwise.
 */
int threadname_append (const char *format, ...);


#ifdef __cplusplus
}
#endif

#endif /* THREADNAME_H */
