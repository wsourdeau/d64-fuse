#ifndef DIR_OPERATIONS
#define DIR_OPERATIONS 1

#include <fuse.h>

int d64fuse_opendir (const char *, struct fuse_file_info *);
int d64fuse_readdir (const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *, enum fuse_readdir_flags);
int d64fuse_releasedir (const char *, struct fuse_file_info *);

#endif /* DIR_OPERATIONS */
