#ifndef D64FUSE_CONTEXT
#define D64FUSE_CONTEXT 1

#include <stdbool.h>
#include <sys/stat.h>

typedef struct d64fuse_file_data
{
  char filename[20];
  unsigned char *rawname;
  int file_type;
  bool splat_file;
  bool locked_file;
  size_t use_count;
  unsigned char *contents;
  struct stat stat;
} d64fuse_file_data;

typedef struct d64fuse_context
{
  const char * image_filename;
  struct diskimage * disk_image;
  struct stat image_stat;
  ssize_t nbr_files; /* -1 indicates that dir and file stats have not been loaded */
  struct stat dir_stat;
  d64fuse_file_data *file_data;
} d64fuse_context;

#endif /* D64FUSE_CONTEXT */
