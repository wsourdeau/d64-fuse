#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>

#include "diskimage.h"

#include "d64fuse_context.h"
#include "utils.h"

static void load_file_contents (d64fuse_file_data * file_data, struct diskimage * disk_image)
{
  if (file_data->use_count == 0)
    {
      ImageFile * image_file = di_open (disk_image, file_data->rawname, file_data->file_type, "rb");
      file_data->contents = malloc (file_data->file_size);
      ssize_t bytes_read = 0;
      while (bytes_read < file_data->file_size)
        {
          size_t remaining = file_data->file_size - bytes_read;
          int data_len = di_read (image_file, file_data->contents + bytes_read, remaining);
          bytes_read += data_len;
        }
      di_close (image_file);
    }
  file_data->use_count++;
}

static void unload_file_contents (d64fuse_file_data * file_data)
{
  if (file_data->use_count == 0)
    {
      fprintf (stderr, "d64fuse %s: inconsistency: the use_count is already 0\n", __func__);
      return;
    }

  file_data->use_count--;
  if (file_data->use_count == 0)
    {
      free (file_data->contents);
      file_data->contents = NULL;
    }
}

/* d64fuse_operations */

int d64fuse_open (const char *filename, struct fuse_file_info *fi)
{
  unused_arg (fi);

  if (is_null (filename))
    return -EINVAL;

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  ensure_disk_image_loaded (context);
  if (is_null (context->disk_image))
    return -EINVAL;

  d64fuse_file_data *file_data = find_file_data (context, filename);
  if (is_null (file_data))
    return -ENOENT;

  load_file_contents (file_data, context->disk_image);

  return 0;
}

int d64fuse_read (const char *filename, char *buffer, size_t buffer_size, off_t offset, struct fuse_file_info *fi)
{
  unused_arg (fi);

  if (is_null (filename))
    return -EINVAL;

  if (is_null (buffer))
    return -EINVAL;

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  const d64fuse_file_data *file_data = find_file_data (context, filename);
  if (is_null (file_data))
    return -ENOENT;

  ssize_t copy_size = buffer_size;
  if (copy_size + offset > file_data->file_size)
    copy_size = file_data->file_size - offset;
  memcpy (buffer, file_data->contents + offset, copy_size);

  return copy_size;
}

int d64fuse_release (const char *filename, struct fuse_file_info *fi)
{
  unused_arg (fi);

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  d64fuse_file_data * file_data = find_file_data (context, filename);
  if (is_null (file_data))
    return -ENOENT;

  unload_file_contents (file_data);

  return 0;
}
