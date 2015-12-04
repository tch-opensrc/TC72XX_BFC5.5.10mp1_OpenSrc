/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 * $Id: os-ecos.h,v 1.5 2003/01/21 18:14:27 dwmw2 Exp $
 *
 */

#ifndef __JFFS2_OS_ECOS_H__
#define __JFFS2_OS_ECOS_H__

#include <cyg/io/io.h>
#include <sys/types.h>
#include <asm/atomic.h>
#include <linux/stat.h>
#include "jffs2port.h"

#define CONFIG_JFFS2_FS_DEBUG 0

static inline uint32_t os_to_jffs2_mode(uint32_t osmode)
{
	uint32_t jmode = ((osmode & S_IRUSR)?00400:0) |
		((osmode & S_IWUSR)?00200:0) |
		((osmode & S_IXUSR)?00100:0) |
		((osmode & S_IRGRP)?00040:0) |
		((osmode & S_IWGRP)?00020:0) |
		((osmode & S_IXGRP)?00010:0) |
		((osmode & S_IROTH)?00004:0) |
		((osmode & S_IWOTH)?00002:0) |
		((osmode & S_IXOTH)?00001:0);

	switch (osmode & S_IFMT) {
	case S_IFSOCK:
		return jmode | 0140000;
	case S_IFLNK:
		return jmode | 0120000;
	case S_IFREG:
		return jmode | 0100000;
	case S_IFBLK:
		return jmode | 0060000;
	case S_IFDIR:
		return jmode | 0040000;
	case S_IFCHR:
		return jmode | 0020000;
	case S_IFIFO:
		return jmode | 0010000;
	case S_ISUID:
		return jmode | 0004000;
	case S_ISGID:
		return jmode | 0002000;
#ifdef S_ISVTX
	case S_ISVTX:
		return jmode | 0001000;
#endif
	}
	printf("os_to_jffs2_mode() cannot convert 0x%x\n", osmode);
	BUG();
	return 0;
}

static inline uint32_t jffs2_to_os_mode (uint32_t jmode)
{
	uint32_t osmode = ((jmode & 00400)?S_IRUSR:0) |
		((jmode & 00200)?S_IWUSR:0) |
		((jmode & 00100)?S_IXUSR:0) |
		((jmode & 00040)?S_IRGRP:0) |
		((jmode & 00020)?S_IWGRP:0) |
		((jmode & 00010)?S_IXGRP:0) |
		((jmode & 00004)?S_IROTH:0) |
		((jmode & 00002)?S_IWOTH:0) |
		((jmode & 00001)?S_IXOTH:0);

	switch(jmode & 00170000) {
	case 0140000:
		return osmode | S_IFSOCK;
	case 0120000:
		return osmode | S_IFLNK;
	case 0100000:
		return osmode | S_IFREG;
	case 0060000:
		return osmode | S_IFBLK;
	case 0040000:
		return osmode | S_IFDIR;
	case 0020000:
		return osmode | S_IFCHR;
	case 0010000:
		return osmode | S_IFIFO;
	case 0004000:
		return osmode | S_ISUID;
	case 0002000:
		return osmode | S_ISGID;
#ifdef S_ISVTX
	case 0001000:
		return osmode | S_ISVTX;
#endif
	}
	printf("jffs2_to_os_mode() cannot convert 0x%x\n", osmode);
	BUG();
	return 0;
}

 /* Read-only operation not currently implemented on eCos */
#define jffs2_is_readonly(c) (0)

/* NAND flash not currently supported on eCos */
#define jffs2_can_mark_obsolete(c) (1)

#define JFFS2_INODE_INFO(i) (&(i)->jffs2_i)
#define OFNI_EDONI_2SFFJ(f)  ((struct inode *) ( ((char *)f) - ((char *)(&((struct inode *)NULL)->jffs2_i)) ) )
 
#define JFFS2_F_I_SIZE(f) (OFNI_EDONI_2SFFJ(f)->i_size)
#define JFFS2_F_I_MODE(f) (OFNI_EDONI_2SFFJ(f)->i_mode)
#define JFFS2_F_I_UID(f) (OFNI_EDONI_2SFFJ(f)->i_uid)
#define JFFS2_F_I_GID(f) (OFNI_EDONI_2SFFJ(f)->i_gid)
#define JFFS2_F_I_CTIME(f) (OFNI_EDONI_2SFFJ(f)->i_ctime)
#define JFFS2_F_I_MTIME(f) (OFNI_EDONI_2SFFJ(f)->i_mtime)
#define JFFS2_F_I_ATIME(f) (OFNI_EDONI_2SFFJ(f)->i_atime)

/* FIXME: eCos doesn't hav a concept of device major/minor numbers */
#define JFFS2_F_I_RDEV_MIN(f) (MINOR(to_kdev_t(OFNI_EDONI_2SFFJ(f)->i_rdev)))
#define JFFS2_F_I_RDEV_MAJ(f) (MAJOR(to_kdev_t(OFNI_EDONI_2SFFJ(f)->i_rdev)))


//#define ITIME(sec) (sec)
//#define I_SEC(x) (x)
#define get_seconds cyg_timestamp

struct inode {
	//struct list_head	i_hash;
	//struct list_head	i_list;
	struct list_head	i_dentry;

	cyg_uint32		i_ino;
	atomic_t		i_count;
	//kdev_t			i_dev;
	mode_t			i_mode;
	nlink_t			i_nlink;
	uid_t			i_uid;
	gid_t			i_gid;
	kdev_t			i_rdev;
	off_t			i_size;
	time_t			i_atime;
	time_t			i_mtime;
	time_t			i_ctime;
	unsigned long		i_blksize;
	unsigned long		i_blocks;
	//unsigned long		i_version;
	//struct semaphore	i_sem;
	//struct semaphore	i_zombie;
	struct inode_operations	*i_op;
	struct file_operations	*i_fop;	// former ->i_op->default_file_ops 
	struct super_block	*i_sb;
	//wait_queue_head_t	i_wait;
	//struct file_lock	*i_flock;
	//struct address_space	*i_mapping;
	//struct address_space	i_data;	
	//struct dquot		*i_dquot[MAXQUOTAS];
	//struct pipe_inode_info	*i_pipe;
	//struct block_device	*i_bdev;

	//unsigned long		i_state;

	unsigned int		i_flags;
	//unsigned char		i_sock;

	atomic_t		i_writecount;
	//unsigned int		i_attr_flags;
	//uint32_t			i_generation;
	struct jffs2_inode_info	jffs2_i;

        struct inode *i_parent;

        struct inode *i_cache_prev;
        struct inode *i_cache_next;
};

#define JFFS2_SB_INFO(sb) (&(sb)->jffs2_sb)

#define OFNI_BS_2SFFJ(c)  ((struct super_block *) ( ((char *)c) - ((char *)(&((struct super_block *)NULL)->jffs2_sb)) ) )

struct super_block {
	unsigned long		s_blocksize;
	unsigned char		s_blocksize_bits;
	unsigned char		s_dirt;
	//struct super_operations	*s_op;
	unsigned long		s_flags;
	unsigned long		s_magic;
	//struct dentry		*s_root;
	struct inode		*s_root;
	struct jffs2_sb_info jffs2_sb;
        unsigned long       s_mount_count;
    cyg_io_handle_t     s_dev;
};

#define sleep_on_spinunlock(wq, sl) do { ; } while(0)
#define EBADFD 32767

/* background.c */
static inline void jffs2_garbage_collect_trigger(struct jffs2_sb_info *c)
{
	/* We don't have a GC thread in eCos (yet) */
}

/* dir.c */
extern struct file_operations jffs2_dir_operations;
extern struct inode_operations jffs2_dir_inode_operations;

/* file.c */
extern struct file_operations jffs2_file_operations;
extern struct inode_operations jffs2_file_inode_operations;
extern struct address_space_operations jffs2_file_address_operations;
int jffs2_null_fsync(struct file *, struct dentry *, int);
int jffs2_setattr (struct dentry *dentry, struct iattr *iattr);
int jffs2_do_readpage_nolock (struct inode *inode, struct page *pg);
int jffs2_do_readpage_unlock (struct inode *inode, struct page *pg);
//int jffs2_readpage (struct file *, struct page *);
int jffs2_readpage (struct inode *d_inode, struct page *pg);
//int jffs2_prepare_write (struct file *, struct page *, unsigned, unsigned);
int jffs2_prepare_write (struct inode *d_inode, struct page *pg, unsigned start, unsigned end);
//int jffs2_commit_write (struct file *, struct page *, unsigned, unsigned);
int jffs2_commit_write (struct inode *d_inode, struct page *pg, unsigned start, unsigned end);

#ifndef CONFIG_JFFS2_FS_NAND
#define jffs2_can_mark_obsolete(c) (1)
#define jffs2_cleanmarker_oob(c) (0)
#define jffs2_write_nand_cleanmarker(c,jeb) (-EIO)

#define jffs2_flush_wbuf(c, flag) do { ; } while(0)
#define jffs2_nand_read_failcnt(c,jeb) do { ; } while(0)
#define jffs2_write_nand_badblock(c,jeb) do { ; } while(0)
#define jffs2_flash_writev jffs2_flash_writev
#define jffs2_wbuf_timeout NULL
#define jffs2_wbuf_process NULL
#else
#error no nand yet
#endif
struct inode *jffs2_new_inode (struct inode *dir_i, int mode, struct jffs2_raw_inode *ri);
void jffs2_clear_inode (struct inode *inode);
void jffs2_read_inode (struct inode *inode);

static inline void jffs2_init_inode_info(struct jffs2_inode_info *f)
{
	memset(f, 0, sizeof(*f));
	init_MUTEX_LOCKED(&f->sem);
}

#endif /* __JFFS2_OS_ECOS_H__ */
