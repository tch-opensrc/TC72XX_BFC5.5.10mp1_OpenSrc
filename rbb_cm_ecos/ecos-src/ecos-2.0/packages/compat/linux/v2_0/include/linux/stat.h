#ifndef __LINUX_STAT_H__
#define __LINUX_STAT_H__


#include <sys/stat.h>

/* FIXME: eCos doesn't define bits for symlinks or sockets. In fact,
   since the inode types are mutually exclusive, it's a bit of a waste
   of space to have separate bits for each type. */
#ifndef __stat_mode_LNK
#define __stat_mode_LNK (1<<19)
#define S_ISLNK(__mode)  ((__mode) & __stat_mode_LNK)
#endif
#ifndef __stat_mode_SOCK
#define __stat_mode_SOCK (1<<20)
#define S_ISSOCK(__mode) ((__mode) & __stat_mode_SOCK)
#endif

#define S_IFMT 0x18001F

#define S_IFDIR __stat_mode_DIR
#define S_IFREG __stat_mode_REG
#define S_IFBLK __stat_mode_BLK
#define S_IFCHR __stat_mode_CHR
#define S_IFLNK __stat_mode_LNK
#define S_IFSOCK __stat_mode_SOCK
#define S_IFIFO __stat_mode_FIFO

#define S_IRUGO (S_IRUSR|S_IRGRP|S_IROTH)
#define S_IWUGO (S_IWUSR|S_IWGRP|S_IWOTH)
#define S_IXUGO (S_IXUSR|S_IXGRP|S_IXOTH)
#define S_IRWXUGO (S_IRWXU|S_IRWXG|S_IRWXO)

#endif /* __LINUX_STAT_H__ */
