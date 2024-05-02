#ifndef CONTEXT
#define CONTEXT 1

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct d64fuse_file_data
{
  char filename[20];
  unsigned char *rawname;
  int file_type;
  bool splat_file;
  bool locked_file;
  size_t use_count;
  size_t dir_file_nbr;
  off_t file_size;
  unsigned char *contents;
} d64fuse_file_data;

typedef struct d64fuse_context
{
  char * image_filename;
  struct stat image_stat;
  struct diskimage * disk_image;
  char disk_label[17];
  ssize_t nbr_files; /* -1 indicates that dir and file stats have not been loaded */
  d64fuse_file_data *file_data;
} d64fuse_context;

d64fuse_context *d64fuse_get_context ();

void ensure_disk_image_loaded (d64fuse_context *);
void ensure_stats_initialized (d64fuse_context *);
d64fuse_file_data *find_file_data (d64fuse_context *, const char *);

#endif /* CONTEXT */
