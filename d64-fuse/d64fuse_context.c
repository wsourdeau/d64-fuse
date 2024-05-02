#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fuse.h>

#include "diskimage.h"

#include "utils.h"

#include "d64fuse_context.h"

d64fuse_context *d64fuse_get_context ()
{
  struct fuse_context *fuse_context = fuse_get_context ();
  if (is_null (fuse_context))
    {
      fprintf (stderr, "d64fuse %s: missing fuse_context\n", __func__);
      return NULL;
    }

  d64fuse_context *context = fuse_context->private_data;
  if (is_null (context))
      fprintf (stderr, "d64fuse %s: missing d64fuse_context\n", __func__);

  return context;
}

typedef void (*for_each_file_cb_t) (off_t file_nbr, d64fuse_context *context, RawDirEntry *rde);

static inline bool is_of_file_type (unsigned char type)
{
  return (type & 0x07) < 7;
}

static inline bool is_valid_rawname (const unsigned char *rawname)
{
    return !(rawname[0] == 0xa || rawname[0] == 0);
}

static void for_each_file (for_each_file_cb_t cb, d64fuse_context *context)
{
  ssize_t current_file_nbr = 0;

  TrackSector ts = di_get_dir_ts (context->disk_image);
  while (ts.track)
    {
      unsigned char *di_buffer = di_get_ts_addr(context->disk_image, ts);
      for (off_t offset = 0; offset < 8; offset++)
        {
          RawDirEntry *rde = (RawDirEntry *) (di_buffer + (offset * 32));
          if (is_of_file_type (rde->type) && is_valid_rawname (rde->rawname))
            {
              cb (current_file_nbr, context, rde);
              current_file_nbr++;
            }
        }
      ts = next_ts_in_chain(context->disk_image, ts);
    }
}

static void count_files (off_t file_nbr, d64fuse_context *context, RawDirEntry *rde)
{
  unused_arg (rde);
  context->nbr_files = file_nbr + 1;
}

static size_t get_exact_file_size(struct diskimage *disk_image, unsigned char * rawname, unsigned char file_type)
{
  size_t file_size = 0;
  unsigned char buffer[65536];

  ImageFile *image_file = di_open (disk_image, rawname, file_type, "rb");
  while (true)
    {
      int data_len = di_read (image_file, buffer, 65536);
      if (data_len == 0)
        break;
      file_size += data_len;
    }
  di_close (image_file);

  return file_size;
}

static void fill_file_data (off_t file_nbr, d64fuse_context *context, RawDirEntry *rde)
{
  unsigned char type = rde->type & 0x07;
  d64fuse_file_data *current_stat = context->file_data + file_nbr;
  current_stat->filename[16] = 0;
  current_stat->file_type = type;
  current_stat->rawname = rde->rawname;
  di_name_from_rawname (current_stat->filename, rde->rawname);
  size_t fn_len = strlen (current_stat->filename);
  if (fn_len == 0)
    {
      fprintf (stderr, "empty name?");
      return;
    }

  // size_t file_size = 254 * ((size_t) rde->sizehi << 8 | rde->sizelo);
  size_t file_size = get_exact_file_size (context->disk_image, rde->rawname, type);
  current_stat->file_size = file_size;
  current_stat->dir_file_nbr = file_nbr;
}

void ensure_disk_image_loaded (d64fuse_context *context)
{
  if (is_not_null (context->disk_image))
    return;
  context->disk_image = di_load_image (context->image_filename);

  unsigned char *title = di_title (context->disk_image);
  di_name_from_rawname (context->disk_label, title);
}

void ensure_stats_initialized (d64fuse_context *context)
{
  if (context->nbr_files > -1)
    return;

  ensure_disk_image_loaded (context);
  if (stat (context->image_filename, &context->image_stat) == -1)
    fprintf (stderr, "d64fuse %s: error executing stat on image file: %s\n", __func__, strerror (errno));

  context->nbr_files = 0;
  for_each_file (count_files, context);
  context->file_data = calloc (context->nbr_files, sizeof (d64fuse_file_data));
  for_each_file (fill_file_data, context);
}

d64fuse_file_data *find_file_data (d64fuse_context *context, const char *filename)
{
  ensure_stats_initialized (context);

  if (filename[0] == '/')
    for (ssize_t i = 0; i < context->nbr_files; i++)
      {
        d64fuse_file_data *current_stat = context->file_data + i;
        if (strcmp (filename + 1, current_stat->filename) == 0)
          return current_stat;
      }

  return NULL;
}

