#ifndef D64FUSE_UTILS_H
#define  D64FUSE_UTILS_H 1

#include <stdbool.h>

#define and &&
#define or ||
#define unused_arg(X) { (void) X; }

#define TODO { (void)(1); };

static inline bool is_null(const void *data)
{
  return (data == NULL);
}

static inline bool is_not_null(const void *data)
{
  return !is_null(data);
}

static inline bool is_root_directory(const char *str)
{
  return (str[0] == '/' and str[1] == '\0');
}

#endif /* D64FUSE_UTILS_H */
