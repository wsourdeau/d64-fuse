#define FUSE_USE_VERSION 35
#include <errno.h>
#include <fuse.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diskimage.h"
extern TrackSector next_ts_in_chain (DiskImage *di, TrackSector ts);

#include "context.h"
#include "utils.h"

static const char *type_labels[] = { "DEL", "SEQ", "PRG", "USR", "REL", "CBM", "DIR" };

/* actual functions */
static d64fuse_context *d64fuse_get_context ()
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

static void load_image_stats (d64fuse_context *context)
{
  struct stat common_stat = {.st_nlink = 1,
                             .st_uid = context->image_stat.st_uid,
                             .st_gid = context->image_stat.st_gid,
                             .st_atim = context->image_stat.st_atim,
                             .st_mtim = context->image_stat.st_mtim,
                             .st_ctim = context->image_stat.st_ctim};

  unsigned char *di_buffer;
  TrackSector ts;
  size_t fn_len;
  ssize_t current_file_nbr = 0;
  size_t dir_size = 0;

  context->nbr_files = 0;
  ts = di_get_dir_ts (context->disk_image);
  while (ts.track)
    {
      di_buffer = di_get_ts_addr(context->disk_image, ts);
      for (off_t offset = 0; offset < 256; offset += 32) {
        RawDirEntry *rde = (RawDirEntry *)(di_buffer + offset);
        unsigned char type = rde->type & 0x07;
        if (type < 7 && rde->rawname[0] != 0xa0)
          context->nbr_files++;
      }
      ts = next_ts_in_chain(context->disk_image, ts);
    }

  context->file_data = calloc (context->nbr_files, sizeof (d64fuse_file_data));

  ts = di_get_dir_ts (context->disk_image);
  while (ts.track)
    {
      di_buffer = di_get_ts_addr (context->disk_image, ts);
      for (off_t offset = 0; offset < 256; offset += 32)
        {
          RawDirEntry *rde = (RawDirEntry *) (di_buffer + offset);
          unsigned type = rde->type & 0x07;
          if (type < 7 && rde->rawname[0] != 0xa0)
            {
              d64fuse_file_data *current_stat = context->file_data + current_file_nbr;
              current_stat->filename[16] = 0;
              current_stat->file_type = type;
              current_stat->rawname = rde->rawname;
              di_name_from_rawname (current_stat->filename, rde->rawname);
              fn_len = strlen (current_stat->filename);
              if (fn_len > 0)
                {
                  // size_t filesize = 254 * ((size_t) rde->sizehi << 8 | rde->sizelo);
                  size_t filesize = get_exact_file_size (context->disk_image, rde->rawname, type);
                  dir_size += filesize;
                  // sprintf (current_stat->filename + fn_len, ".%s", type_labels[type]);
                  current_stat->stat = common_stat;
                  current_stat->stat.st_ino = current_file_nbr + 1;
                  current_stat->stat.st_mode = S_IFREG | S_IRUSR;
                  current_stat->stat.st_size = filesize;
              }
              current_file_nbr++;
            }
        }

      /* todo: add sanity checking */
      ts = next_ts_in_chain (context->disk_image, ts);
    }

  context->dir_stat = common_stat;
  context->dir_stat.st_ino = 0;
  context->dir_stat.st_mode = S_IFDIR | (context->image_stat.st_mode & 0777);
  context->dir_stat.st_size = dir_size;
}

static d64fuse_file_data *find_file_data (const d64fuse_context *context, const char *filename)
{
  if (filename[0] == '/')
    for (ssize_t i = 0; i < context->nbr_files; i++)
      {
        d64fuse_file_data *current_stat = context->file_data + i;
        if (strcmp (filename + 1, current_stat->filename) == 0)
          return current_stat;
      }

  return NULL;
}

static void load_file_contents (d64fuse_file_data * file_data, struct diskimage * disk_image)
{
  if (file_data->use_count == 0)
    {
      ImageFile * image_file = di_open (disk_image, file_data->rawname, file_data->file_type, "rb");
      file_data->contents = malloc (file_data->stat.st_size);
      ssize_t bytes_read = 0;
      while (bytes_read < file_data->stat.st_size)
        {
          size_t remaining = file_data->stat.st_size - bytes_read;
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

static int d64fuse_open_diskimage ()
{
  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  if (is_not_null (context->disk_image))
    {
      fprintf (stderr, "d64fuse %s: disk image already open\n", __func__);
      return -EINVAL;
    }

  if (is_null (context->image_filename))
    {
      fprintf (stderr, "d64fuse %s: missing image filename\n", __func__);
      return -EINVAL;
    }

  if (stat (context->image_filename, &context->image_stat) == -1)
    {
      fprintf (stderr, "d64fuse %s: error executing stat on image file\n", __func__);
      return -errno;
    }

  context->disk_image = di_load_image (context->image_filename);
  if (is_null (context->disk_image))
    {
      fprintf (stderr, "d64fuse %s: failure opening disk image; errno=%d; error=%s\n", __func__, errno, strerror (errno));
      return -errno;
    }
  load_image_stats (context);

  return 0;
}



/* d64fuse_operations */

static int d64fuse_getattr (const char *filename, struct stat *file_data, struct fuse_file_info *fi)
{
  unused_arg (fi);

  if (is_null (filename))
    return -EINVAL;

  fprintf (stderr, "d64fuse %s: filename='%s', ....\n", __func__, filename);

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  if (is_root_directory (filename))
    {
      *file_data = context->dir_stat;
      return 0;
    }

  if (context->nbr_files < 0)
    return -EINVAL;

  const d64fuse_file_data *found_file_data = find_file_data (context, filename);
  if (is_null (found_file_data))
    return ENOENT;

  *file_data = found_file_data->stat;

  return 0;
}

static int d64fuse_open (const char *filename, struct fuse_file_info *fi)
{
  unused_arg (fi);
  fprintf (stderr, "d64fuse %s: filename='%s', fuse_file_info=%p\n", __func__, filename, (void *) fi);

  if (is_null (filename))
    return -EINVAL;

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context->disk_image))
    return -EINVAL;

  d64fuse_file_data *file_data = find_file_data (context, filename);
  if (is_null (file_data))
    return -ENOENT;

  load_file_contents (file_data, context->disk_image);

  return 0;
}

static int d64fuse_read (const char *filename, char *buffer, size_t buffer_size, off_t offset, struct fuse_file_info *fi)
{
  unused_arg (fi);

  fprintf (stderr, "d64fuse %s: filename='%s', ....\n", __func__, filename);

  if (is_null (filename))
    return -EINVAL;

  if (is_null (buffer))
    return -EINVAL;

  d64fuse_context *context = d64fuse_get_context ();
  if (!context)
    return -EINVAL;

  const d64fuse_file_data *file_data = find_file_data (context, filename);
  if (is_null (file_data))
    return ENOENT;

  ssize_t copy_size = buffer_size;
  if (copy_size + offset > file_data->stat.st_size)
    copy_size = file_data->stat.st_size - offset;
  memcpy (buffer, file_data->contents + offset, copy_size);

  return copy_size;
}

/*
static int d64fuse_statfs (const char *filename, struct statvfs *vfs_stat)
{
  unused_arg (vfs_stat);

  fprintf (stderr, "d64fuse %s: filename='%s', ....\n", __func__, filename);
  if (is_null (filename))
    return -EINVAL;
  TODO;
  return operation_not_supported;
}
*/

static int d64fuse_opendir (const char *dirname, struct fuse_file_info *fi)
{
  unused_arg (fi);

  if (is_null (dirname))
    return -EINVAL;

  if (!is_root_directory (dirname))
    return operation_not_supported;

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  if (is_null (context->disk_image))
    return d64fuse_open_diskimage ();

  return 0;
}

static int d64fuse_readdir (const char *dirname, void *buffer, fuse_fill_dir_t fill_dir, off_t readdir_offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
  unused_arg (readdir_offset);
  unused_arg (fi);
  unused_arg (flags);

  if (is_null (dirname))
    return -EINVAL;

  if (!is_root_directory (dirname))
    return operation_not_supported;

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  for (ssize_t i = 0; i < context->nbr_files; i++)
    {
      d64fuse_file_data *current_file_data = context->file_data + i;
      fill_dir (buffer, current_file_data->filename, &current_file_data->stat, 0, 0);
    }

  return 0;
}

static int d64fuse_releasedir (const char *dirname, struct fuse_file_info *fi)
{
  unused_arg (dirname);
  unused_arg (fi);

  return 0;
}

static int d64fuse_getxattr (const char *filename, const char *attr_name, char *attr_value, size_t attr_value_size)
{
  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  d64fuse_file_data *file_data = find_file_data (context, filename);
  if (is_null (file_data))
    return -ENOENT;

  const char *value;
  if (strcmp(attr_name, "d64fuse.type") == 0)
    value = type_labels[file_data->file_type];
  else if (strcmp(attr_name, "d64fuse.is_splat") == 0)
    value = file_data->splat_file ? "true" : "false";
  else if (strcmp(attr_name, "d64fuse.is_locked") == 0)
    value = file_data->locked_file ? "true" : "false";
  else
    return -EINVAL;

  if (attr_value_size == 0)
    return strlen (value) + 1;

  return snprintf (attr_value, attr_value_size, "%s", value);
}

/** List extended attributes */
static int d64fuse_listxattr (const char *filename, char *list, size_t list_size)
{
  static const char attr_list_str[] = "d64fuse.type\0d64fuse.is_splat\0d64fuse.is_locked";
  static size_t attr_list_len = sizeof (attr_list_str);

  unused_arg (filename);

  if (list_size == 0)
    return attr_list_len;

  size_t max_size = attr_list_len;
  if (max_size > list_size)
    max_size = list_size;
  if (max_size > 0)
    memcpy (list, attr_list_str, max_size);

  return max_size;
}

static int d64fuse_release (const char *filename, struct fuse_file_info *fi)
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

static void d64fuse_destroy (void *private_data)
{
  d64fuse_context *context;

  if (is_not_null (private_data))
    {
      context = private_data;
      if (is_not_null (context->disk_image))
      {
          free (context->disk_image);
          context->disk_image = NULL;
      }
    }
}

static int d64fuse_access (const char *filename, int perms)
{
  fprintf (stderr, "d64fuse %s: filename='%s', perms=%d\n", __func__, filename, perms);

  if (is_null (filename))
    return -EINVAL;

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  if (perms == F_OK)
    {
      if (is_root_directory (filename))
        return 0;
      const d64fuse_file_data * file_data = find_file_data (context, filename);
      if (is_not_null (file_data))
        return 0;
      return -ENOENT;
    }

  if ((perms & W_OK) == W_OK)
    return -EPERM;

  if (((perms & X_OK) == X_OK) && !is_root_directory (filename))
    return -EPERM;

  if ((perms & R_OK) == R_OK)
    if (access (context->image_filename, R_OK) == -1)
      return -errno;

  return 0;
}

const struct fuse_operations operations = {
  .getattr = d64fuse_getattr,
  .open = d64fuse_open,
  .read = d64fuse_read,
  // .statfs = d64fuse_statfs,
  .opendir = d64fuse_opendir,
  .readdir = d64fuse_readdir,
  .releasedir = d64fuse_releasedir,
  .getxattr = d64fuse_getxattr,
  .listxattr = d64fuse_listxattr,
  .release = d64fuse_release,
  .destroy = d64fuse_destroy,
  .access = d64fuse_access,

  /* unsupported methods:
  .copy_file_range = d64fuse_copy_file_range,
  .read_buf = d64fuse_read_buf,
  .readlink = d64fuse_readlink,
  .mknod = d64fuse_mknod,
  .mkdir = d64fuse_mkdir,
  .unlink = d64fuse_unlink,
  .rmdir = d64fuse_rmdir,
  .symlink = d64fuse_symlink,
  .rename = d64fuse_rename,
  .link = d64fuse_link,
  .chmod = d64fuse_chmod,
  .chown = d64fuse_chown,
  .truncate = d64fuse_truncate,
  .write = d64fuse_write,
  .flush = d64fuse_flush,
  .fsync = d64fuse_fsync,
  .setxattr = d64fuse_setxattr,
  .removexattr = d64fuse_removexattr,
  .fsyncdir = d64fuse_fsyncdir,
  .create = d64fuse_create,
  .lock = d64fuse_lock,
  .utimens = d64fuse_utimens,
  .bmap = d64fuse_bmap,
  .ioctl = d64fuse_ioctl,
  .poll = d64fuse_poll,
  .write_buf = d64fuse_write_buf,
  .flock = d64fuse_flock,
  .fallocate = d64fuse_fallocate,
  .lseek = d64fuse_lseek */
};
