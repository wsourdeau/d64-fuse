#ifndef FILE_OPERATIONS
#define FILE_OPERATIONS 1

struct fuse_file_info;

int d64fuse_open (const char *, struct fuse_file_info *);
int d64fuse_read (const char *, char *, size_t, off_t, struct fuse_file_info *);
int d64fuse_release (const char *, struct fuse_file_info *);

#endif /* FILE_OPERATIONS */
