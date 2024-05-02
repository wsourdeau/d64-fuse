#ifndef COMMON_OPERATIONS
#define COMMON_OPERATIONS 1

struct fuse_file_info;

int d64fuse_access (const char *, int);
int d64fuse_getattr (const char *, struct stat *, struct fuse_file_info *);
int d64fuse_getxattr (const char *, const char *, char *, size_t);
int d64fuse_listxattr (const char *, char *, size_t);

#endif /* COMMON_OPERATIONS */
