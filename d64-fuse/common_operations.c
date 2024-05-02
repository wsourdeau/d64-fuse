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

#include "d64fuse_context.h"
#include "utils.h"

static const char *type_labels[] = {"DEL", "SEQ", "PRG", "USR", "REL", "CBM", "DIR"};
static const char *type_mime_types[] = {"application/x-c64-file",
                                        "application/x-c64-seq-file",
                                        "application/x-c64-prg-file",
                                        "application/x-c64-usr-file",
                                        "application/x-c64-rel-file",
                                        "application/x-c64-cbm-file",
                                        "application/x-c64-dir"};

#define XATTR_VALUE_IMAGE_FILENAME "d64fuse.image_filename"
#define XATTR_VALUE_DISK_LABEL "d64fuse.disk_label"
#define XATTR_VALUE_FILE_TYPE "d64fuse.file_type"
#define XATTR_VALUE_IS_SPLAT "d64fuse.is_splat"
#define XATTR_VALUE_IS_LOCKED "d64fuse.is_locked"
#define XATTR_VALUE_MIME_TYPE "user.mime_type"

/* d64fuse_operations */

int d64fuse_access (const char *filename, int perms)
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

static void fill_directory_stat (struct stat *entry_stat, d64fuse_context *context)
{
  entry_stat->st_ino = 1;
  entry_stat->st_nlink = 2 + context->nbr_files;
  entry_stat->st_mode = S_IFDIR | (context->image_stat.st_mode & 0777);
  if (entry_stat->st_mode & S_IRUSR)
    entry_stat->st_mode |= S_IXUSR;
  if (entry_stat->st_mode & S_IRGRP)
    entry_stat->st_mode |= S_IXGRP;
  if (entry_stat->st_mode & S_IROTH)
    entry_stat->st_mode |= S_IXOTH;
  entry_stat->st_size = context->image_stat.st_size;
}

static void fill_file_stat (struct stat *entry_stat, const d64fuse_file_data *file_data, d64fuse_context *context)
{
  entry_stat->st_ino = file_data->dir_file_nbr + 2;
  entry_stat->st_nlink = 1;
  entry_stat->st_mode = S_IFREG | (context->image_stat.st_mode & 0666);
  entry_stat->st_size = file_data->file_size;
}

int d64fuse_getattr (const char *filename, struct stat *entry_stat, struct fuse_file_info *fi)
{
  unused_arg (fi);

  if (is_null (filename))
    return -EINVAL;

  fprintf (stderr, "d64fuse %s: filename='%s', ....\n", __func__, filename);

  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  ensure_stats_initialized (context);
  *entry_stat = (struct stat) {.st_uid = context->image_stat.st_uid,
                               .st_gid = context->image_stat.st_gid,
                               .st_atim = context->image_stat.st_atim,
                               .st_mtim = context->image_stat.st_mtim,
                               .st_ctim = context->image_stat.st_ctim};

  if (is_root_directory (filename))
    {
      fill_directory_stat (entry_stat, context);
      return 0;
    }

  const d64fuse_file_data *file_data = find_file_data (context, filename);
  if (is_null (file_data))
    return -ENOENT;
  fill_file_stat (entry_stat, file_data, context);

  return 0;
}

int d64fuse_getxattr (const char *filename, const char *attr_name, char *attr_value, size_t attr_value_size)
{
  d64fuse_context *context = d64fuse_get_context ();
  if (is_null (context))
    return -EINVAL;

  const char *value = NULL;

  if (is_root_directory (filename))
    {
      if (strcmp(attr_name, XATTR_VALUE_IMAGE_FILENAME) == 0)
        value = context->image_filename;
      else if (strcmp(attr_name, XATTR_VALUE_DISK_LABEL) == 0)
        value = context->disk_label;
      else if (strcmp(attr_name, XATTR_VALUE_MIME_TYPE) == 0)
        value = type_mime_types[T_DIR];
    }
  else
    {
      d64fuse_file_data *file_data = find_file_data (context, filename);
      if (is_null (file_data))
        return -ENOENT;

      if (strcmp(attr_name, XATTR_VALUE_FILE_TYPE) == 0)
        value = type_labels[file_data->file_type];
      else if (strcmp(attr_name, XATTR_VALUE_MIME_TYPE) == 0)
        value = type_mime_types[file_data->file_type];
      else if (strcmp(attr_name, XATTR_VALUE_IS_SPLAT) == 0)
        value = file_data->splat_file ? "true" : "false";
      else if (strcmp(attr_name, XATTR_VALUE_IS_LOCKED) == 0)
        value = file_data->locked_file ? "true" : "false";
    }

  if (!value)
    return -ENODATA;

  if (attr_value_size == 0)
    return strlen (value) + 1;

  return snprintf (attr_value, attr_value_size, "%s", value);
}

int d64fuse_listxattr (const char *filename, char *list, size_t list_size)
{
  static const char dir_attr_list_str[] = XATTR_VALUE_IMAGE_FILENAME "\0" XATTR_VALUE_DISK_LABEL "\0" XATTR_VALUE_MIME_TYPE;
  static const char file_attr_list_str[] = XATTR_VALUE_FILE_TYPE "\0" XATTR_VALUE_MIME_TYPE "\0" XATTR_VALUE_IS_SPLAT "\0" XATTR_VALUE_IS_LOCKED;
  const char *attr_list_str;
  size_t attr_list_len;

  if (is_root_directory (filename))
    {
      attr_list_str = dir_attr_list_str;
      attr_list_len = sizeof (dir_attr_list_str);
    }
  else
    {
      d64fuse_context *context = d64fuse_get_context ();
      if (is_null (context))
        return -EINVAL;

      d64fuse_file_data * file_data = find_file_data (context, filename);
      if (is_null (file_data))
        return -ENOENT;

      attr_list_str = file_attr_list_str;
      attr_list_len = sizeof (file_attr_list_str);
    }

  if (list_size == 0)
    return attr_list_len;

  size_t max_size = attr_list_len;
  if (max_size > list_size)
    max_size = list_size;
  if (max_size > 0)
    memcpy (list, attr_list_str, max_size);

  return max_size;
}
