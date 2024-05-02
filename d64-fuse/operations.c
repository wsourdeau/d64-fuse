#include <stdlib.h>

#include "d64fuse_context.h"
#include "file_operations.h"
#include "dir_operations.h"
#include "common_operations.h"
#include "utils.h"

static void d64fuse_destroy (void *private_data)
{
  if (is_null (private_data))
      return;

  d64fuse_context *context = private_data;
  if (is_null (context->disk_image))
      return;

  free (context->disk_image);
  context->disk_image = NULL;
}

const struct fuse_operations operations = {
  .open = d64fuse_open,
  .read = d64fuse_read,
  .release = d64fuse_release,

  .opendir = d64fuse_opendir,
  .readdir = d64fuse_readdir,
  .releasedir = d64fuse_releasedir,

  .access = d64fuse_access,
  .getattr = d64fuse_getattr,
  .getxattr = d64fuse_getxattr,
  .listxattr = d64fuse_listxattr,

  .destroy = d64fuse_destroy
};
