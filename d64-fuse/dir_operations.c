#include <errno.h>
#include <stdio.h>

#include <fuse.h>

#include "d64fuse_context.h"
#include "utils.h"


int d64fuse_opendir (const char *dirname, struct fuse_file_info *fi)
{
  unused_arg (fi);

  if (is_null (dirname))
    return -EINVAL;

  if (!is_root_directory (dirname))
    return -ENOTSUP;

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  ensure_disk_image_loaded (context);

  return 0;
}

int d64fuse_readdir (const char *dirname, void *buffer, fuse_fill_dir_t fill_dir, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
  unused_arg (offset);
  unused_arg (fi);
  unused_arg (flags);

  if (is_null (dirname))
    return -EINVAL;

  if (!is_root_directory (dirname))
    return -ENOTSUP;

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  ensure_stats_initialized (context);

  for (ssize_t i = 0; i < context->nbr_files; i++)
    {
      d64fuse_file_data *current_file_data = context->file_data + i;
      int result = fill_dir (buffer, current_file_data->filename, NULL, 0, 0);
      if (result == 1)
        {
          fprintf (stderr, "d64fuse %s: buffer is full when i = %ld", __func__, i);
          return 0;
        }
    }

  return 0;
}

int d64fuse_releasedir (const char *dirname, struct fuse_file_info *fi)
{
  unused_arg (dirname);
  unused_arg (fi);

  return 0;
}
