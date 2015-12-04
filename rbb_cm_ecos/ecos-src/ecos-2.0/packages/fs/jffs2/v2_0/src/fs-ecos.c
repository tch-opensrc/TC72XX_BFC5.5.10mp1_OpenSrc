/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by Dominic Ostrowski <dominic.ostrowski@3glab.com>
 * Contributors: David Woodhouse, Nick Garnett, Richard Panton.
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 * $Id: fs-ecos.c,v 1.8 2003/01/21 18:13:01 dwmw2 Exp $
 *
 */

#include <linux/types.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include "jffs2port.h"
#include <linux/jffs2.h>
#include <linux/jffs2_fs_sb.h>
#include <linux/jffs2_fs_i.h>
#include <linux/pagemap.h>
#include "nodelist.h"

#include <errno.h>
#include <string.h>
#include <cyg/io/io.h>
#include <cyg/io/config_keys.h>
#include <cyg/io/flash.h>

//==========================================================================
// Forward definitions

// Filesystem operations
static int jffs2_mount(cyg_fstab_entry * fste, cyg_mtab_entry * mte);
static int jffs2_umount(cyg_mtab_entry * mte);
static int jffs2_open(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
		      int mode, cyg_file * fte);
static int jffs2_ops_unlink(cyg_mtab_entry * mte, cyg_dir dir,
			    const char *name);
static int jffs2_ops_mkdir(cyg_mtab_entry * mte, cyg_dir dir, const char *name);
static int jffs2_ops_rmdir(cyg_mtab_entry * mte, cyg_dir dir, const char *name);
static int jffs2_ops_rename(cyg_mtab_entry * mte, cyg_dir dir1,
			    const char *name1, cyg_dir dir2, const char *name2);
static int jffs2_ops_link(cyg_mtab_entry * mte, cyg_dir dir1, const char *name1,
			  cyg_dir dir2, const char *name2, int type);
static int jffs2_opendir(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
			 cyg_file * fte);
static int jffs2_chdir(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
		       cyg_dir * dir_out);
static int jffs2_stat(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
		      struct stat *buf);
static int jffs2_getinfo(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
			 int key, void *buf, int len);
static int jffs2_setinfo(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
			 int key, void *buf, int len);

// File operations
static int jffs2_fo_read(struct CYG_FILE_TAG *fp, struct CYG_UIO_TAG *uio);
static int jffs2_fo_write(struct CYG_FILE_TAG *fp, struct CYG_UIO_TAG *uio);
static int jffs2_fo_lseek(struct CYG_FILE_TAG *fp, off_t * pos, int whence);
static int jffs2_fo_ioctl(struct CYG_FILE_TAG *fp, CYG_ADDRWORD com,
			  CYG_ADDRWORD data);
static int jffs2_fo_fsync(struct CYG_FILE_TAG *fp, int mode);
static int jffs2_fo_close(struct CYG_FILE_TAG *fp);
static int jffs2_fo_fstat(struct CYG_FILE_TAG *fp, struct stat *buf);
static int jffs2_fo_getinfo(struct CYG_FILE_TAG *fp, int key, void *buf,
			    int len);
static int jffs2_fo_setinfo(struct CYG_FILE_TAG *fp, int key, void *buf,
			    int len);

// Directory operations
static int jffs2_fo_dirread(struct CYG_FILE_TAG *fp, struct CYG_UIO_TAG *uio);
static int jffs2_fo_dirlseek(struct CYG_FILE_TAG *fp, off_t * pos, int whence);

//==========================================================================
// Filesystem table entries

// -------------------------------------------------------------------------
// Fstab entry.
// This defines the entry in the filesystem table.
// For simplicity we use _FILESYSTEM synchronization for all accesses since
// we should never block in any filesystem operations.

FSTAB_ENTRY(jffs2_fste, "jffs2", 0,
	    CYG_SYNCMODE_FILE_FILESYSTEM | CYG_SYNCMODE_IO_FILESYSTEM,
	    jffs2_mount,
	    jffs2_umount,
	    jffs2_open,
	    jffs2_ops_unlink,
	    jffs2_ops_mkdir,
	    jffs2_ops_rmdir,
	    jffs2_ops_rename,
	    jffs2_ops_link,
	    jffs2_opendir,
	    jffs2_chdir, jffs2_stat, jffs2_getinfo, jffs2_setinfo);

// -------------------------------------------------------------------------
// File operations.
// This set of file operations are used for normal open files.

static cyg_fileops jffs2_fileops = {
	jffs2_fo_read,
	jffs2_fo_write,
	jffs2_fo_lseek,
	jffs2_fo_ioctl,
	cyg_fileio_seltrue,
	jffs2_fo_fsync,
	jffs2_fo_close,
	jffs2_fo_fstat,
	jffs2_fo_getinfo,
	jffs2_fo_setinfo
};

// -------------------------------------------------------------------------
// Directory file operations.
// This set of operations are used for open directories. Most entries
// point to error-returning stub functions. Only the read, lseek and
// close entries are functional.

static cyg_fileops jffs2_dirops = {
	jffs2_fo_dirread,
	(cyg_fileop_write *) cyg_fileio_enosys,
	jffs2_fo_dirlseek,
	(cyg_fileop_ioctl *) cyg_fileio_enosys,
	cyg_fileio_seltrue,
	(cyg_fileop_fsync *) cyg_fileio_enosys,
	jffs2_fo_close,
	(cyg_fileop_fstat *) cyg_fileio_enosys,
	(cyg_fileop_getinfo *) cyg_fileio_enosys,
	(cyg_fileop_setinfo *) cyg_fileio_enosys
};

//==========================================================================
// STATIC VARIABLES !!!

static char read_write_buffer[PAGE_CACHE_SIZE];	//avoids malloc when user may be under memory pressure
static char gc_buffer[PAGE_CACHE_SIZE];	//avoids malloc when user may be under memory pressure

//==========================================================================
// Directory operations

struct jffs2_dirsearch {
	struct inode *dir;	// directory to search
	const char *path;	// path to follow
	struct inode *node;	// Node found
	const char *name;	// last name fragment used
	int namelen;		// name fragment length
	cyg_bool last;		// last name in path?
};

typedef struct jffs2_dirsearch jffs2_dirsearch;

//==========================================================================
// Ref count and nlink management

// -------------------------------------------------------------------------
// dec_refcnt()
// Decrment the reference count on an inode. If this makes the ref count
// zero, then this inode can be freed.

static int dec_refcnt(struct inode *node)
{
	int err = ENOERR;
	node->i_count--;

	// In JFFS2 inode's are temporary in ram structures that are free'd when the usage i_count drops to 0
	// The i_nlink however is managed by JFFS2 and is unrelated to usage
	if (node->i_count == 0) {
		// This inode is not in use, so delete it.
		iput(node);
	}

	return err;
}

// FIXME: This seems like real cruft. Wouldn't it be better just to do the
// right thing?
static void icache_evict(struct inode *root_i, struct inode *i)
{
	struct inode *cached_inode;
	struct inode *next_inode;

	D2(printf("icache_evict\n"));
	// If this is an absolute search path from the root,
	// remove all cached inodes with i_count of zero (these are only 
	// held where needed for dotdot filepaths)
	if (i == root_i) {
		for (cached_inode = root_i; cached_inode != NULL;
		     cached_inode = next_inode) {
			next_inode = cached_inode->i_cache_next;
			if (cached_inode->i_count == 0) {
				cached_inode->i_cache_prev->i_cache_next = cached_inode->i_cache_next;	// Prveious entry points ahead of us
				if (cached_inode->i_cache_next != NULL)
					cached_inode->i_cache_next->i_cache_prev = cached_inode->i_cache_prev;	// Next entry points behind us
				jffs2_clear_inode(cached_inode);
				D2(printf
				   ("free icache_evict inode %x $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n",
				    cached_inode));
				free(cached_inode);
			}
		}
	}
}

//==========================================================================
// Directory search

// -------------------------------------------------------------------------
// init_dirsearch()
// Initialize a dirsearch object to start a search

static void init_dirsearch(jffs2_dirsearch * ds,
			   struct inode *dir, const char *name)
{
	D2(printf("init_dirsearch name = %s\n", name));
	D2(printf("init_dirsearch dir = %x\n", dir));
	ds->dir = dir;
	ds->path = name;
	ds->node = dir;
	ds->name = name;
	ds->namelen = 0;
	ds->last = false;
}

// -------------------------------------------------------------------------
// find_entry()
// Search a single directory for the next name in a path and update the
// dirsearch object appropriately.

static int find_entry(jffs2_dirsearch * ds)
{
	unsigned long hash;
	struct qstr this;
	unsigned int c;
	const char *hashname;

	struct inode *dir = ds->dir;
	const char *name = ds->path;
	const char *n = name;
	char namelen = 0;
	struct inode *d;

	D2(printf("find_entry\n"));

	// check that we really have a directory
	if (!S_ISDIR(dir->i_mode))
		return ENOTDIR;

	// Isolate the next element of the path name. 
	while (*n != '\0' && *n != '/')
		n++, namelen++;

	// If we terminated on a NUL, set last flag.
	if (*n == '\0')
		ds->last = true;

	// update name in dirsearch object
	ds->name = name;
	ds->namelen = namelen;

	if (name[0] == '.')
		switch (namelen) {
		default:
			break;
		case 2:
			// Dot followed by not Dot, treat as any other name 
			if (name[1] != '.')
				break;
			// Dot Dot 
			// Move back up the search path
			D2(printf("find_entry found ..\n"));
			ds->node = ds->dir->i_parent;
			if (ds->dir->i_count == 0) {
				iput(ds->dir);	// This inode may be evicted
				ds->dir = NULL;
			}
			return ENOERR;
		case 1:
			// Dot is consumed
			D2(printf("find_entry found .\n"));
			ds->node = ds->dir;
			return ENOERR;
		}
	// Here we have the name and its length set up.
	// Search the directory for a matching entry

	hashname = name;
	this.name = hashname;
	c = *(const unsigned char *) hashname;

	hash = init_name_hash();
	do {
		hashname++;
		hash = partial_name_hash(c, hash);
		c = *(const unsigned char *) hashname;
	} while (c && (c != '/'));
	this.len = hashname - (const char *) this.name;
	this.hash = end_name_hash(hash);

	D2(printf("find_entry for name = %s\n", ds->path));
	d = jffs2_lookup(dir, &this);
	D2(printf("find_entry got dir = %x\n", d));

	if (d == NULL)
		return ENOENT;

	// The back path for dotdot to follow
	d->i_parent = dir;
	// pass back the node we have found
	ds->node = d;

	return ENOERR;

}

// -------------------------------------------------------------------------
// jffs2_find()
// Main interface to directory search code. This is used in all file
// level operations to locate the object named by the pathname.

static int jffs2_find(jffs2_dirsearch * d)
{
	int err;

	D2(printf("jffs2_find for path =%s\n", d->path));
	// Short circuit empty paths
	if (*(d->path) == '\0')
		return ENOERR;

	// iterate down directory tree until we find the object
	// we want.
	for (;;) {
		err = find_entry(d);

		if (err != ENOERR)
			return err;

		if (d->last)
			return ENOERR;

		// every inode traversed in the find is temporary and should be free'd
		//iput(d->dir);

		// Update dirsearch object to search next directory.
		d->dir = d->node;
		d->path += d->namelen;
		if (*(d->path) == '/')
			d->path++;	// skip dirname separators
	}
}

//==========================================================================
// Pathconf support
// This function provides support for pathconf() and fpathconf().

static int jffs2_pathconf(struct inode *node, struct cyg_pathconf_info *info)
{
	int err = ENOERR;
	D2(printf("jffs2_pathconf\n"));

	switch (info->name) {
	case _PC_LINK_MAX:
		info->value = LINK_MAX;
		break;

	case _PC_MAX_CANON:
		info->value = -1;	// not supported
		err = EINVAL;
		break;

	case _PC_MAX_INPUT:
		info->value = -1;	// not supported
		err = EINVAL;
		break;

	case _PC_NAME_MAX:
		info->value = NAME_MAX;
		break;

	case _PC_PATH_MAX:
		info->value = PATH_MAX;
		break;

	case _PC_PIPE_BUF:
		info->value = -1;	// not supported
		err = EINVAL;
		break;

	case _PC_ASYNC_IO:
		info->value = -1;	// not supported
		err = EINVAL;
		break;

	case _PC_CHOWN_RESTRICTED:
		info->value = -1;	// not supported
		err = EINVAL;
		break;

	case _PC_NO_TRUNC:
		info->value = 0;
		break;

	case _PC_PRIO_IO:
		info->value = 0;
		break;

	case _PC_SYNC_IO:
		info->value = 0;
		break;

	case _PC_VDISABLE:
		info->value = -1;	// not supported
		err = EINVAL;
		break;

	default:
		err = EINVAL;
		break;
	}

	return err;
}

//==========================================================================
// Filesystem operations

// -------------------------------------------------------------------------
// jffs2_mount()
// Process a mount request. This mainly creates a root for the
// filesystem.
static int jffs2_read_super(struct super_block *sb)
{
	struct jffs2_sb_info *c;
	struct inode *root_i;
	Cyg_ErrNo err;
	cyg_uint32 len;
	cyg_io_flash_getconfig_devsize_t ds;
	cyg_io_flash_getconfig_blocksize_t bs;

	D1(printk(KERN_DEBUG "jffs2: read_super\n"));

	c = JFFS2_SB_INFO(sb);

	len = sizeof (ds);
	err = cyg_io_get_config(sb->s_dev,
				CYG_IO_GET_CONFIG_FLASH_DEVSIZE, &ds, &len);
	if (err != ENOERR) {
		D1(printf
		   ("jffs2: cyg_io_get_config failed to get dev size: %d\n",
		    err));
		return err;
	}
	len = sizeof (bs);
	bs.offset = 0;
	err = cyg_io_get_config(sb->s_dev,
				CYG_IO_GET_CONFIG_FLASH_BLOCKSIZE, &bs, &len);
	if (err != ENOERR) {
		D1(printf
		   ("jffs2: cyg_io_get_config failed to get block size: %d\n",
		    err));
		return err;
	}

	c->sector_size = bs.block_size;
	c->flash_size = ds.dev_size;
	c->cleanmarker_size = sizeof(struct jffs2_unknown_node);

	err = jffs2_do_mount_fs(c);
	if (err)
		return -err;

	D1(printk(KERN_DEBUG "jffs2_read_super(): Getting root inode\n"));
	root_i = iget(sb, 1);
	if (is_bad_inode(root_i)) {
		D1(printk(KERN_WARNING "get root inode failed\n"));
		err = EIO;
		goto out_nodes;
	}

	D1(printk(KERN_DEBUG "jffs2_read_super(): d_alloc_root()\n"));
	sb->s_root = d_alloc_root(root_i);
	if (!sb->s_root) {
		err = ENOMEM;
		goto out_root_i;
	}
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = JFFS2_SUPER_MAGIC;

	return 0;

      out_root_i:
	iput(root_i);
      out_nodes:
	jffs2_free_ino_caches(c);
	jffs2_free_raw_node_refs(c);
	free(c->blocks);

	return err;
}

static int jffs2_mount(cyg_fstab_entry * fste, cyg_mtab_entry * mte)
{
	extern cyg_mtab_entry mtab[], mtab_end;
	struct super_block *jffs2_sb = NULL;
	struct jffs2_sb_info *c;
	cyg_mtab_entry *m;
	cyg_io_handle_t t;
	Cyg_ErrNo err;

	D2(printf("jffs2_mount\n"));

	err = cyg_io_lookup(mte->devname, &t);
	if (err != ENOERR)
		return -err;

	// Iterate through the mount table to see if we're mounted
	// FIXME: this should be done better - perhaps if the superblock
	// can be stored as an inode in the icache.
	for (m = &mtab[0]; m != &mtab_end; m++) {
		// stop if there are more than the configured maximum
		if (m - &mtab[0] >= CYGNUM_FILEIO_MTAB_MAX) {
			m = &mtab_end;
			break;
		}
		if (m->valid && strcmp(m->fsname, "jffs2") == 0 &&
		    strcmp(m->devname, mte->devname) == 0) {
			jffs2_sb = (struct super_block *) m->data;
		}
	}

	if (jffs2_sb == NULL) {
		jffs2_sb = malloc(sizeof (struct super_block));

		if (jffs2_sb == NULL)
			return ENOMEM;

		c = JFFS2_SB_INFO(jffs2_sb);
		memset(jffs2_sb, 0, sizeof (struct super_block));
		jffs2_sb->s_dev = t;

		c->inocache_list = malloc(sizeof(struct jffs2_inode_cache *) * INOCACHE_HASHSIZE);
		if (!c->inocache_list) {
			free(jffs2_sb);
			return ENOMEM;
		}
		memset(c->inocache_list, 0, sizeof(struct jffs2_inode_cache *) * INOCACHE_HASHSIZE);

		err = jffs2_read_super(jffs2_sb);

		if (err) {
			free(jffs2_sb);
			free(c->inocache_list);
			return err;
		}

		jffs2_sb->s_root->i_parent = jffs2_sb->s_root;	// points to itself, no dotdot paths above mountpoint
		jffs2_sb->s_root->i_cache_prev = NULL;	// root inode, so always null
		jffs2_sb->s_root->i_cache_next = NULL;
		jffs2_sb->s_root->i_count = 1;	// Ensures the root inode is always in ram until umount

		D2(printf("jffs2_mount erasing pending blocks\n"));
		jffs2_erase_pending_blocks(c);
	}
	mte->data = (CYG_ADDRWORD) jffs2_sb;

	jffs2_sb->s_mount_count++;
	mte->root = (cyg_dir) jffs2_sb->s_root;
	D2(printf("jffs2_mounted superblock at %x\n", mte->root));

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_umount()
// Unmount the filesystem. 

static int jffs2_umount(cyg_mtab_entry * mte)
{
	struct inode *root = (struct inode *) mte->root;
	struct super_block *jffs2_sb = root->i_sb;
	struct jffs2_sb_info *c = JFFS2_SB_INFO(jffs2_sb);

	D2(printf("jffs2_umount\n"));

	// Decrement the mount count
	jffs2_sb->s_mount_count--;

	// Only really umount if this is the only mount
	if (jffs2_sb->s_mount_count == 0) {

		// Check for open/inuse root or any cached inodes
//if( root->i_count != 1 || root->i_cache_next != NULL) // root icount was set to 1 on mount
		if (root->i_cache_next != NULL)	// root icount was set to 1 on mount
			return EBUSY;

		dec_refcnt(root);	// Time to free the root inode

		//Clear root inode
		//root_i = NULL;

		// Clean up the super block and root inode
		jffs2_free_ino_caches(c);
		jffs2_free_raw_node_refs(c);
		free(c->blocks);
		free(c->inocache_list);
		free(jffs2_sb);
		// Clear root pointer
		mte->root = CYG_DIR_NULL;
		mte->fs->data = 0;	// fstab entry, visible to all mounts. No current mount
		// That's all folks.
		D2(printf("jffs2_umount No current mounts\n"));
	}

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_open()
// Open a file for reading or writing.

static int jffs2_open(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
		      int mode, cyg_file * file)
{

	jffs2_dirsearch ds;
	struct inode *node = NULL;
	int err;

	D2(printf("jffs2_open\n"));

	icache_evict((struct inode *) mte->root, (struct inode *) dir);

	init_dirsearch(&ds, (struct inode *) dir, name);

	err = jffs2_find(&ds);

	if (err == ENOENT) {
		if (ds.last && (mode & O_CREAT)) {
			unsigned long hash;
			struct qstr this;
			unsigned int c;
			const char *hashname;

			// No node there, if the O_CREAT bit is set then we must
			// create a new one. The dir and name fields of the dirsearch
			// object will have been updated so we know where to put it.

			hashname = ds.name;
			this.name = hashname;
			c = *(const unsigned char *) hashname;

			hash = init_name_hash();
			do {
				hashname++;
				hash = partial_name_hash(c, hash);
				c = *(const unsigned char *) hashname;
			} while (c && (c != '/'));
			this.len = hashname - (const char *) this.name;
			this.hash = end_name_hash(hash);

			err = jffs2_create(ds.dir, &this, S_IRUGO|S_IXUGO|S_IWUSR|S_IFREG, &node);

			if (err != 0) {
				//Possible orphaned inode on the flash - but will be gc'd
				return err;
			}

			err = ENOERR;
		}
	} else if (err == ENOERR) {
		// The node exists. If the O_CREAT and O_EXCL bits are set, we
		// must fail the open.

		if ((mode & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
			err = EEXIST;
		else
			node = ds.node;
	}

	if (err == ENOERR && (mode & O_TRUNC)) {
		struct jffs2_inode_info *f = JFFS2_INODE_INFO(node);
		struct jffs2_sb_info *c = JFFS2_SB_INFO(node->i_sb);
		// If the O_TRUNC bit is set we must clean out the file data.

		node->i_size = 0;
		jffs2_truncate_fraglist(c, &f->fragtree, 0);
		// Update file times
		node->i_ctime = node->i_mtime = cyg_timestamp();
	}

	if (err != ENOERR)
		return err;

	// Check that we actually have a file here
	if (S_ISDIR(node->i_mode))
		return EISDIR;

	node->i_count++;	// Count successful open

	// Initialize the file object

	file->f_flag |= mode & CYG_FILE_MODE_MASK;
	file->f_type = CYG_FILE_TYPE_FILE;
	file->f_ops = &jffs2_fileops;
	file->f_offset = (mode & O_APPEND) ? node->i_size : 0;
	file->f_data = (CYG_ADDRWORD) node;
	file->f_xops = 0;

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_ops_unlink()
// Remove a file link from its directory.

static int jffs2_ops_unlink(cyg_mtab_entry * mte, cyg_dir dir, const char *name)
{
	unsigned long hash;
	struct qstr this;
	unsigned int c;
	const char *hashname;
	jffs2_dirsearch ds;
	int err;

	D2(printf("jffs2_ops_unlink\n"));

	icache_evict((struct inode *) mte->root, (struct inode *) dir);

	init_dirsearch(&ds, (struct inode *) dir, name);

	err = jffs2_find(&ds);

	if (err != ENOERR)
		return err;

	// Cannot unlink directories, use rmdir() instead
	if (S_ISDIR(ds.node->i_mode))
		return EPERM;

	// Delete it from its directory

	hashname = ds.name;
	this.name = hashname;
	c = *(const unsigned char *) hashname;

	hash = init_name_hash();
	do {
		hashname++;
		hash = partial_name_hash(c, hash);
		c = *(const unsigned char *) hashname;
	} while (c && (c != '/'));
	this.len = hashname - (const char *) this.name;
	this.hash = end_name_hash(hash);

	err = jffs2_unlink(ds.dir, ds.node, &this);

	return err;
}

// -------------------------------------------------------------------------
// jffs2_ops_mkdir()
// Create a new directory.

static int jffs2_ops_mkdir(cyg_mtab_entry * mte, cyg_dir dir, const char *name)
{
	jffs2_dirsearch ds;
	struct inode *node = NULL;
	int err;

	D2(printf("jffs2_ops_mkdir\n"));

	icache_evict((struct inode *) mte->root, (struct inode *) dir);

	init_dirsearch(&ds, (struct inode *) dir, name);

	err = jffs2_find(&ds);

	if (err == ENOENT) {
		if (ds.last) {
			unsigned long hash;
			struct qstr this;
			unsigned int c;
			const char *hashname;
			// The entry does not exist, and it is the last element in
			// the pathname, so we can create it here.

			hashname = ds.name;
			this.name = hashname;
			c = *(const unsigned char *) hashname;

			hash = init_name_hash();
			do {
				hashname++;
				hash = partial_name_hash(c, hash);
				c = *(const unsigned char *) hashname;
			} while (c && (c != '/'));
			this.len = hashname - (const char *) this.name;
			this.hash = end_name_hash(hash);

			err = jffs2_mkdir(ds.dir, &this, 0, &node);

			if (err != 0)
				return ENOSPC;

		}
		// If this was not the last element, then and intermediate
		// directory does not exist.
	} else {
		// If there we no error, something already exists with that
		// name, so we cannot create another one.

		if (err == ENOERR)
			err = EEXIST;
	}

	return err;
}

// -------------------------------------------------------------------------
// jffs2_ops_rmdir()
// Remove a directory.

static int jffs2_ops_rmdir(cyg_mtab_entry * mte, cyg_dir dir, const char *name)
{
	unsigned long hash;
	struct qstr this;
	unsigned int c;
	const char *hashname;
	jffs2_dirsearch ds;
	int err;

	D2(printf("jffs2_ops_rmdir\n"));

	icache_evict((struct inode *) mte->root, (struct inode *) dir);

	init_dirsearch(&ds, (struct inode *) dir, name);

	err = jffs2_find(&ds);

	if (err != ENOERR)
		return err;

	// Check that this is actually a directory.
	if (!S_ISDIR(ds.node->i_mode))
		return EPERM;

	// Delete the entry. 
	hashname = ds.name;
	this.name = hashname;
	c = *(const unsigned char *) hashname;

	hash = init_name_hash();
	do {
		hashname++;
		hash = partial_name_hash(c, hash);
		c = *(const unsigned char *) hashname;
	} while (c && (c != '/'));
	this.len = hashname - (const char *) this.name;
	this.hash = end_name_hash(hash);

	err = jffs2_rmdir(ds.dir, ds.node, &this);

	return err;

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_ops_rename()
// Rename a file/dir.

static int jffs2_ops_rename(cyg_mtab_entry * mte, cyg_dir dir1,
			    const char *name1, cyg_dir dir2, const char *name2)
{
	unsigned long hash;
	struct qstr this1, this2;
	unsigned int c;
	const char *hashname;
	jffs2_dirsearch ds1, ds2;
	int err;

	D2(printf("jffs2_ops_rename\n"));

	init_dirsearch(&ds1, (struct inode *) dir1, name1);

	err = jffs2_find(&ds1);

	if (err != ENOERR)
		return err;

	init_dirsearch(&ds2, (struct inode *) dir2, name2);

	err = jffs2_find(&ds2);

	// Allow through renames to non-existent objects.
	if (ds2.last && err == ENOENT)
		ds2.node = NULL, err = ENOERR;

	if (err != ENOERR)
		return err;

	// Null rename, just return
	if (ds1.node == ds2.node)
		return ENOERR;

	hashname = ds1.name;
	this1.name = hashname;
	c = *(const unsigned char *) hashname;

	hash = init_name_hash();
	do {
		hashname++;
		hash = partial_name_hash(c, hash);
		c = *(const unsigned char *) hashname;
	} while (c && (c != '/'));
	this1.len = hashname - (const char *) this1.name;
	this1.hash = end_name_hash(hash);

	hashname = ds2.name;
	this2.name = hashname;
	c = *(const unsigned char *) hashname;

	hash = init_name_hash();
	do {
		hashname++;
		hash = partial_name_hash(c, hash);
		c = *(const unsigned char *) hashname;
	} while (c && (c != '/'));
	this2.len = hashname - (const char *) this2.name;
	this2.hash = end_name_hash(hash);

	// First deal with any entry that is at the destination
	if (ds2.node) {
		// Check that we are renaming like-for-like

		if (!S_ISDIR(ds1.node->i_mode) && S_ISDIR(ds2.node->i_mode))
			return EISDIR;

		if (S_ISDIR(ds1.node->i_mode) && !S_ISDIR(ds2.node->i_mode))
			return ENOTDIR;

		// Now delete the destination directory entry

		err = jffs2_unlink(ds2.dir, ds2.node, &this2);

		if (err != 0)
			return err;

	}
	// Now we know that there is no clashing node at the destination,
	// make a new direntry at the destination and delete the old entry
	// at the source.

	err = jffs2_rename(ds1.dir, ds1.node, &this1, ds2.dir, &this2);

	// Update directory times
	if (err == 0)
		ds1.dir->i_ctime =
		    ds1.dir->i_mtime =
		    ds2.dir->i_ctime = ds2.dir->i_mtime = cyg_timestamp();

	return err;
}

// -------------------------------------------------------------------------
// jffs2_ops_link()
// Make a new directory entry for a file.

static int jffs2_ops_link(cyg_mtab_entry * mte, cyg_dir dir1, const char *name1,
			  cyg_dir dir2, const char *name2, int type)
{
	unsigned long hash;
	struct qstr this;
	unsigned int c;
	const char *hashname;
	jffs2_dirsearch ds1, ds2;
	int err;

	D2(printf("jffs2_ops_link\n"));

	// Only do hard links for now in this filesystem
	if (type != CYG_FSLINK_HARD)
		return EINVAL;

	init_dirsearch(&ds1, (struct inode *) dir1, name1);

	err = jffs2_find(&ds1);

	if (err != ENOERR)
		return err;

	init_dirsearch(&ds2, (struct inode *) dir2, name2);

	err = jffs2_find(&ds2);

	// Don't allow links to existing objects
	if (err == ENOERR)
		return EEXIST;

	// Allow through links to non-existing terminal objects
	if (ds2.last && err == ENOENT)
		ds2.node = NULL, err = ENOERR;

	if (err != ENOERR)
		return err;

	// Now we know that there is no existing node at the destination,
	// make a new direntry at the destination.

	hashname = ds2.name;
	this.name = hashname;
	c = *(const unsigned char *) hashname;

	hash = init_name_hash();
	do {
		hashname++;
		hash = partial_name_hash(c, hash);
		c = *(const unsigned char *) hashname;
	} while (c && (c != '/'));
	this.len = hashname - (const char *) this.name;
	this.hash = end_name_hash(hash);

	err = jffs2_link(ds2.dir, ds1.node, &this);

	if (err == 0)
		ds1.node->i_ctime =
		    ds2.dir->i_ctime = ds2.dir->i_mtime = cyg_timestamp();

	return err;
}

// -------------------------------------------------------------------------
// jffs2_opendir()
// Open a directory for reading.

static int jffs2_opendir(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
			 cyg_file * file)
{
	jffs2_dirsearch ds;
	int err;

	D2(printf("jffs2_opendir\n"));

	icache_evict((struct inode *) mte->root, (struct inode *) dir);

	init_dirsearch(&ds, (struct inode *) dir, name);

	err = jffs2_find(&ds);

	if (err != ENOERR)
		return err;

	// check it is really a directory.
	if (!S_ISDIR(ds.node->i_mode))
		return ENOTDIR;

	ds.node->i_count++;	// Count successful open

	// Initialize the file object, setting the f_ops field to a
	// special set of file ops.

	file->f_type = CYG_FILE_TYPE_FILE;
	file->f_ops = &jffs2_dirops;
	file->f_offset = 0;
	file->f_data = (CYG_ADDRWORD) ds.node;
	file->f_xops = 0;

	return ENOERR;

}

// -------------------------------------------------------------------------
// jffs2_chdir()
// Change directory support.

static int jffs2_chdir(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
		       cyg_dir * dir_out)
{
	D2(printf("jffs2_chdir\n"));

	if (dir_out != NULL) {
		// This is a request to get a new directory pointer in
		// *dir_out.

		jffs2_dirsearch ds;
		int err;

		icache_evict((struct inode *) mte->root, (struct inode *) dir);

		init_dirsearch(&ds, (struct inode *) dir, name);

		err = jffs2_find(&ds);

		if (err != ENOERR)
			return err;

		// check it is a directory
		if (!S_ISDIR(ds.node->i_mode))
			return ENOTDIR;

		// Increment ref count to keep this directory in existance
		// while it is the current cdir.
		ds.node->i_count++;

		// Pass it out
		*dir_out = (cyg_dir) ds.node;
	} else {
		// If no output dir is required, this means that the mte and
		// dir arguments are the current cdir setting and we should
		// forget this fact.

		struct inode *node = (struct inode *) dir;

		// Just decrement directory reference count.
		dec_refcnt(node);
	}

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_stat()
// Get struct stat info for named object.

static int jffs2_stat(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
		      struct stat *buf)
{
	jffs2_dirsearch ds;
	int err;

	D2(printf("jffs2_stat\n"));

	icache_evict((struct inode *) mte->root, (struct inode *) dir);

	init_dirsearch(&ds, (struct inode *) dir, name);

	err = jffs2_find(&ds);

	if (err != ENOERR)
		return err;

	// Fill in the status
	buf->st_mode = ds.node->i_mode;
	buf->st_ino = (ino_t) ds.node;
	buf->st_dev = 0;
	buf->st_nlink = ds.node->i_nlink;
	buf->st_uid = 0;
	buf->st_gid = 0;
	buf->st_size = ds.node->i_size;
	buf->st_atime = ds.node->i_atime;
	buf->st_mtime = ds.node->i_mtime;
	buf->st_ctime = ds.node->i_ctime;

	return err;

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_getinfo()
// Getinfo. Currently only support pathconf().

static int jffs2_getinfo(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
			 int key, void *buf, int len)
{
	jffs2_dirsearch ds;
	int err;

	D2(printf("jffs2_getinfo\n"));

	icache_evict((struct inode *) mte->root, (struct inode *) dir);

	init_dirsearch(&ds, (struct inode *) dir, name);

	err = jffs2_find(&ds);

	if (err != ENOERR)
		return err;

	switch (key) {
	case FS_INFO_CONF:
		err = jffs2_pathconf(ds.node, (struct cyg_pathconf_info *) buf);
		break;

	default:
		err = EINVAL;
	}
	return err;

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_setinfo()
// Setinfo. Nothing to support here at present.

static int jffs2_setinfo(cyg_mtab_entry * mte, cyg_dir dir, const char *name,
			 int key, void *buf, int len)
{
	// No setinfo keys supported at present

	D2(printf("jffs2_setinfo\n"));

	return EINVAL;
}

//==========================================================================
// File operations

// -------------------------------------------------------------------------
// jffs2_fo_read()
// Read data from the file.

static int jffs2_fo_read(struct CYG_FILE_TAG *fp, struct CYG_UIO_TAG *uio)
{
	struct inode *inode = (struct inode *) fp->f_data;
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	int i;
	ssize_t resid = uio->uio_resid;
	off_t pos = fp->f_offset;

	down(&f->sem);

	// Loop over the io vectors until there are none left
	for (i = 0; i < uio->uio_iovcnt && pos < inode->i_size; i++) {
		int ret;
		cyg_iovec *iov = &uio->uio_iov[i];
		off_t len = min(iov->iov_len, inode->i_size - pos);

		D2(printf("jffs2_fo_read inode size %d\n", inode->i_size));

		ret =
		    jffs2_read_inode_range(c, f,
					   (unsigned char *) iov->iov_base, pos,
					   len);
		if (ret) {
			D1(printf
			   ("jffs2_fo_read(): read_inode_range failed %d\n",
			    ret));
			uio->uio_resid = resid;
			up(&f->sem);
			return -ret;
		}
		resid -= len;
		pos += len;
	}

	// We successfully read some data, update the node's access time
	// and update the file offset and transfer residue.

	inode->i_atime = cyg_timestamp();

	uio->uio_resid = resid;
	fp->f_offset = pos;

	up(&f->sem);

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_fo_write()
// Write data to file.

static int jffs2_fo_write(struct CYG_FILE_TAG *fp, struct CYG_UIO_TAG *uio)
{
	struct page write_page;
	off_t page_start_pos;
	struct inode *node = (struct inode *) fp->f_data;
	off_t pos = fp->f_offset;
	ssize_t resid = uio->uio_resid;
	int i;

	memset(&read_write_buffer, 0, PAGE_CACHE_SIZE);
	write_page.virtual = &read_write_buffer;

	// If the APPEND mode bit was supplied, force all writes to
	// the end of the file.
	if (fp->f_flag & CYG_FAPPEND)
		pos = fp->f_offset = node->i_size;

	// Check that pos is within current file size, or at the very end.
	if (pos < 0 || pos > node->i_size)
		return EINVAL;

	// Now loop over the iovecs until they are all done, or
	// we get an error.
	for (i = 0; i < uio->uio_iovcnt; i++) {
		cyg_iovec *iov = &uio->uio_iov[i];
		char *buf = (char *) iov->iov_base;
		off_t len = iov->iov_len;

		// loop over the vector writing it to the file until it has
		// all been done.
		while (len > 0) {
			//cyg_uint8 *fbuf;
			//size_t bsize;
			size_t writtenlen;
			off_t l = len;
			int err;

			write_page.index = 0;

			page_start_pos = pos;
			while (page_start_pos >= (PAGE_CACHE_SIZE)) {
				write_page.index++;
				page_start_pos -= PAGE_CACHE_SIZE;
			}

			if (l > PAGE_CACHE_SIZE - page_start_pos)
				l = PAGE_CACHE_SIZE - page_start_pos;

			D2(printf
			   ("jffs2_fo_write write_page.index %d\n",
			    write_page.index));
			D2(printf
			   ("jffs2_fo_write page_start_pos %d\n",
			    page_start_pos));
			D2(printf("jffs2_fo_write transfer size %d\n", l));

			err =
			    jffs2_prepare_write(node, &write_page,
						page_start_pos,
						page_start_pos + l);

			if (err != 0)
				return err;

			// copy data in
			memcpy(&read_write_buffer[page_start_pos], buf, l);

			writtenlen =
			    jffs2_commit_write(node, &write_page,
					       page_start_pos,
					       page_start_pos + l);

			if (writtenlen != l)
				return ENOSPC;

			// Update working vars
			len -= l;
			buf += l;
			pos += l;
			resid -= l;
		}
	}

	// We wrote some data successfully, update the modified and access
	// times of the node, increase its size appropriately, and update
	// the file offset and transfer residue.
	node->i_mtime = node->i_ctime = cyg_timestamp();
	if (pos > node->i_size)
		node->i_size = pos;

	uio->uio_resid = resid;
	fp->f_offset = pos;

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_fo_lseek()
// Seek to a new file position.

static int jffs2_fo_lseek(struct CYG_FILE_TAG *fp, off_t * apos, int whence)
{
	struct inode *node = (struct inode *) fp->f_data;
	off_t pos = *apos;

	D2(printf("jffs2_fo_lseek\n"));

	switch (whence) {
	case SEEK_SET:
		// Pos is already where we want to be.
		break;

	case SEEK_CUR:
		// Add pos to current offset.
		pos += fp->f_offset;
		break;

	case SEEK_END:
		// Add pos to file size.
		pos += node->i_size;
		break;

	default:
		return EINVAL;
	}

	// Check that pos is still within current file size, or at the
	// very end.
	if (pos < 0 || pos > node->i_size)
		return EINVAL;

	// All OK, set fp offset and return new position.
	*apos = fp->f_offset = pos;

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_fo_ioctl()
// Handle ioctls. Currently none are defined.

static int jffs2_fo_ioctl(struct CYG_FILE_TAG *fp, CYG_ADDRWORD com,
			  CYG_ADDRWORD data)
{
	// No Ioctls currenly defined.

	D2(printf("jffs2_fo_ioctl\n"));

	return EINVAL;
}

// -------------------------------------------------------------------------
// jffs2_fo_fsync().
// Force the file out to data storage.

static int jffs2_fo_fsync(struct CYG_FILE_TAG *fp, int mode)
{
	// Data is always permanently where it belongs, nothing to do
	// here.

	D2(printf("jffs2_fo_fsync\n"));

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_fo_close()
// Close a file. We just decrement the refcnt and let it go away if
// that is all that is keeping it here.

static int jffs2_fo_close(struct CYG_FILE_TAG *fp)
{
	struct inode *node = (struct inode *) fp->f_data;

	D2(printf("jffs2_fo_close\n"));

	dec_refcnt(node);

	fp->f_data = 0;		// zero data pointer

	return ENOERR;
}

// -------------------------------------------------------------------------
//jffs2_fo_fstat()
// Get file status.

static int jffs2_fo_fstat(struct CYG_FILE_TAG *fp, struct stat *buf)
{
	struct inode *node = (struct inode *) fp->f_data;

	D2(printf("jffs2_fo_fstat\n"));

	// Fill in the status
	buf->st_mode = node->i_mode;
	buf->st_ino = (ino_t) node;
	buf->st_dev = 0;
	buf->st_nlink = node->i_nlink;
	buf->st_uid = 0;
	buf->st_gid = 0;
	buf->st_size = node->i_size;
	buf->st_atime = node->i_atime;
	buf->st_mtime = node->i_mtime;
	buf->st_ctime = node->i_ctime;

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_fo_getinfo()
// Get info. Currently only supports fpathconf().

static int jffs2_fo_getinfo(struct CYG_FILE_TAG *fp, int key, void *buf,
			    int len)
{
	struct inode *node = (struct inode *) fp->f_data;
	int err;

	D2(printf("jffs2_fo_getinfo\n"));

	switch (key) {
	case FS_INFO_CONF:
		err = jffs2_pathconf(node, (struct cyg_pathconf_info *) buf);
		break;

	default:
		err = EINVAL;
	}
	return err;

	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_fo_setinfo()
// Set info. Nothing supported here.

static int jffs2_fo_setinfo(struct CYG_FILE_TAG *fp, int key, void *buf,
			    int len)
{
	// No setinfo key supported at present

	D2(printf("jffs2_fo_setinfo\n"));

	return ENOERR;
}

//==========================================================================
// Directory operations

// -------------------------------------------------------------------------
// jffs2_fo_dirread()
// Read a single directory entry from a file.

static __inline void filldir(char *nbuf, int nlen, const char *name, int namlen)
{
	int len = nlen < namlen ? nlen : namlen;
	memcpy(nbuf, name, len);
	nbuf[len] = '\0';
}

static int jffs2_fo_dirread(struct CYG_FILE_TAG *fp, struct CYG_UIO_TAG *uio)
{
	struct inode *d_inode = (struct inode *) fp->f_data;
	struct dirent *ent = (struct dirent *) uio->uio_iov[0].iov_base;
	char *nbuf = ent->d_name;
	int nlen = sizeof (ent->d_name) - 1;
	off_t len = uio->uio_iov[0].iov_len;
	struct jffs2_inode_info *f;
	struct jffs2_sb_info *c;
	struct inode *inode = d_inode;
	struct jffs2_full_dirent *fd;
	unsigned long offset, curofs;
	int found = 1;

	if (len < sizeof (struct dirent))
		return EINVAL;

	D1(printk
	   (KERN_DEBUG "jffs2_readdir() for dir_i #%lu\n", d_inode->i_ino));

	f = JFFS2_INODE_INFO(inode);
	c = JFFS2_SB_INFO(inode->i_sb);

	offset = fp->f_offset;

	if (offset == 0) {
		D1(printk
		   (KERN_DEBUG "Dirent 0: \".\", ino #%lu\n", inode->i_ino));
		filldir(nbuf, nlen, ".", 1);
		goto out;
	}
	if (offset == 1) {
		filldir(nbuf, nlen, "..", 2);
		goto out;
	}

	curofs = 1;
	down(&f->sem);
	for (fd = f->dents; fd; fd = fd->next) {

		curofs++;
		/* First loop: curofs = 2; offset = 2 */
		if (curofs < offset) {
			D2(printk
			   (KERN_DEBUG
			    "Skipping dirent: \"%s\", ino #%u, type %d, because curofs %ld < offset %ld\n",
			    fd->name, fd->ino, fd->type, curofs, offset));
			continue;
		}
		if (!fd->ino) {
			D2(printk
			   (KERN_DEBUG "Skipping deletion dirent \"%s\"\n",
			    fd->name));
			offset++;
			continue;
		}
		D2(printk
		   (KERN_DEBUG "Dirent %ld: \"%s\", ino #%u, type %d\n", offset,
		    fd->name, fd->ino, fd->type));
		filldir(nbuf, nlen, fd->name, strlen(fd->name));
		goto out_sem;
	}
	/* Reached the end of the directory */
	found = 0;
      out_sem:
	up(&f->sem);
      out:
	fp->f_offset = ++offset;
	if (found) {
		uio->uio_resid -= sizeof (struct dirent);
	}
	return ENOERR;
}

// -------------------------------------------------------------------------
// jffs2_fo_dirlseek()
// Seek directory to start.

static int jffs2_fo_dirlseek(struct CYG_FILE_TAG *fp, off_t * pos, int whence)
{
	// Only allow SEEK_SET to zero

	D2(printf("jffs2_fo_dirlseek\n"));

	if (whence != SEEK_SET || *pos != 0)
		return EINVAL;

	*pos = fp->f_offset = 0;

	return ENOERR;
}

//==========================================================================
// 
// Called by JFFS2
// ===============
// 
//
//==========================================================================

struct page *read_cache_page(unsigned long index,
			     int (*filler) (void *, struct page *), void *data)
{
	// Only called in gc.c jffs2_garbage_collect_dnode
	// but gets a real page for the specified inode

	int err;
	struct page *gc_page = malloc(sizeof (struct page));

	printf("read_cache_page\n");
	memset(&gc_buffer, 0, PAGE_CACHE_SIZE);

	if (gc_page != NULL) {
		gc_page->virtual = &gc_buffer;
		gc_page->index = index;

		err = filler(data, gc_page);
		if (err < 0) {
			free(gc_page);
			gc_page = NULL;
		}
	}

	return gc_page;
}

void page_cache_release(struct page *pg)
{

	// Only called in gc.c jffs2_garbage_collect_dnode
	// but should free the page malloc'd by read_cache_page

	printf("page_cache_release\n");
	free(pg);
}

struct inode *new_inode(struct super_block *sb)
{

	// Only called in write.c jffs2_new_inode
	// Always adds itself to inode cache

	struct inode *inode;
	struct inode *cached_inode;

	inode = malloc(sizeof (struct inode));
	if (inode == NULL)
		return 0;

	D2(printf
	   ("malloc new_inode %x ####################################\n",
	    inode));

	memset(inode, 0, sizeof (struct inode));
	inode->i_sb = sb;
	inode->i_ino = 1;
	inode->i_count = 0;	//1; // Let ecos manage the open count

	inode->i_nlink = 1;	// Let JFFS2 manage the link count
	inode->i_size = 0;

	inode->i_cache_next = NULL;	// Newest inode, about to be cached

	// Add to the icache
	for (cached_inode = sb->s_root; cached_inode != NULL;
	     cached_inode = cached_inode->i_cache_next) {
		if (cached_inode->i_cache_next == NULL) {
			cached_inode->i_cache_next = inode;	// Current last in cache points to newcomer
			inode->i_cache_prev = cached_inode;	// Newcomer points back to last
			break;
		}
	}

	return inode;
}

struct inode *iget(struct super_block *sb, cyg_uint32 ino)
{

	// Substitute for iget drops straight through to reading the 
	// inode from disk if it is not in the inode cache

	// Called in super.c jffs2_read_super, dir.c jffs2_lookup,
	// and gc.c jffs2_garbage_collect_pass

	// Must first check for cached inode 
	// If this fails let new_inode create one

	struct inode *inode;

	D2(printf("iget\n"));

	// Check for this inode in the cache
	for (inode = sb->s_root; inode != NULL; inode = inode->i_cache_next) {
		if (inode->i_ino == ino)
			return inode;
	}
	inode = NULL;

	// Not cached, so malloc it
	inode = new_inode(sb);
	if (inode == NULL)
		return 0;

	inode->i_ino = ino;
	jffs2_read_inode(inode);

	return inode;
}

void iput(struct inode *i)
{

	// Called in dec_refcnt, jffs2_find 
	// (and jffs2_open and jffs2_ops_mkdir?)
	// super.c jffs2_read_super,
	// and gc.c jffs2_garbage_collect_pass

	struct inode *cached_inode;

	D2(printf
	   ("free iput inode %x $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n", i));
	if (i && i->i_count) {
		/* Added by dwmw2. iget/iput in Linux track the use count,
		   don't just unconditionally free it */
		printf("iput called for used inode\n");
		return;
	}
	if (i != NULL) {
		// Remove from the icache
		for (cached_inode = i->i_sb->s_root; cached_inode != NULL;
		     cached_inode = cached_inode->i_cache_next) {
			if (cached_inode == i) {
				cached_inode->i_cache_prev->i_cache_next = cached_inode->i_cache_next;	// Prveious entry points ahead of us
				if (cached_inode->i_cache_next != NULL)
					cached_inode->i_cache_next->i_cache_prev = cached_inode->i_cache_prev;	// Next entry points behind us
				break;
			}
		}
		// inode has been seperated from the cache
		jffs2_clear_inode(i);
		free(i);
	}
}

static int return_EIO(void)
{
	return -EIO;
}

#define EIO_ERROR ((void *) (return_EIO))

void make_bad_inode(struct inode *inode)
{

	// In readinode.c JFFS2 checks whether the inode has appropriate
	// content for its marked type

	D2(printf("make_bad_inode\n"));

	inode->i_mode = S_IFREG;
	inode->i_atime = inode->i_mtime = inode->i_ctime = cyg_timestamp();
	inode->i_op = EIO_ERROR;
	inode->i_fop = EIO_ERROR;
}

int is_bad_inode(struct inode *inode)
{

	// Called in super.c jffs2_read_super,
	// and gc.c jffs2_garbage_collect_pass

	D2(printf("is_bad_inode\n"));

	return (inode->i_op == EIO_ERROR);
	/*if(i == NULL)
	   return 1;
	   return 0; */
}

cyg_bool jffs2_flash_read(struct jffs2_sb_info * c,
			  cyg_uint32 read_buffer_offset, const size_t size,
			  size_t * return_size, char *write_buffer)
{
	Cyg_ErrNo err;
	cyg_uint32 len = size;
	struct super_block *sb = OFNI_BS_2SFFJ(c);

	//D2(printf("FLASH READ\n"));
	//D2(printf("read address = %x\n", CYGNUM_FS_JFFS2_BASE_ADDRESS + read_buffer_offset));
	//D2(printf("write address = %x\n", write_buffer));
	//D2(printf("size = %x\n", size));
	err = cyg_io_bread(sb->s_dev, write_buffer, &len, read_buffer_offset);

	*return_size = (size_t) len;
	return (err != ENOERR);
}

cyg_bool jffs2_flash_write(struct jffs2_sb_info * c,
			   cyg_uint32 write_buffer_offset, const size_t size,
			   size_t * return_size, char *read_buffer)
{

	Cyg_ErrNo err;
	cyg_uint32 len = size;
	struct super_block *sb = OFNI_BS_2SFFJ(c);

	//    D2(printf("FLASH WRITE ENABLED!!!\n"));
	//    D2(printf("write address = %x\n", CYGNUM_FS_JFFS2_BASE_ADDRESS + write_buffer_offset));
	//    D2(printf("read address = %x\n", read_buffer));
	//    D2(printf("size = %x\n", size));

	err = cyg_io_bwrite(sb->s_dev, read_buffer, &len, write_buffer_offset);
	*return_size = (size_t) len;

	return (err != ENOERR);
}

int
jffs2_flash_writev(struct jffs2_sb_info *c, const struct iovec *vecs,
		   unsigned long count, loff_t to, size_t * retlen)
{
	unsigned long i;
	size_t totlen = 0, thislen;
	int ret = 0;

	for (i = 0; i < count; i++) {
		// writes need to be aligned but the data we're passed may not be
		// Observation suggests most unaligned writes are small, so we
		// optimize for that case.

		if (((vecs[i].iov_len & (sizeof (int) - 1))) ||
		    (((unsigned long) vecs[i].
		      iov_base & (sizeof (unsigned long) - 1)))) {
			// are there iov's after this one? Or is it so much we'd need
			// to do multiple writes anyway?
			if ((i + 1) < count || vecs[i].iov_len > 256) {
				// cop out and malloc
				unsigned long j;
				ssize_t sizetomalloc = 0, totvecsize = 0;
				char *cbuf, *cbufptr;

				for (j = i; j < count; j++)
					totvecsize += vecs[j].iov_len;

				// pad up in case unaligned
				sizetomalloc = totvecsize + sizeof (int) - 1;
				sizetomalloc &= ~(sizeof (int) - 1);
				cbuf = (char *) malloc(sizetomalloc);
				// malloc returns aligned memory
				if (!cbuf) {
					ret = -ENOMEM;
					goto writev_out;
				}
				cbufptr = cbuf;
				for (j = i; j < count; j++) {
					memcpy(cbufptr, vecs[j].iov_base,
					       vecs[j].iov_len);
					cbufptr += vecs[j].iov_len;
				}
				ret =
				    jffs2_flash_write(c, to, sizetomalloc,
						      &thislen, cbuf);
				if (thislen > totvecsize)	// in case it was aligned up
					thislen = totvecsize;
				totlen += thislen;
				free(cbuf);
				goto writev_out;
			} else {
				// otherwise optimize for the common case
				int buf[256 / sizeof (int)];	// int, so int aligned
				size_t lentowrite;

				lentowrite = vecs[i].iov_len;
				// pad up in case its unaligned
				lentowrite += sizeof (int) - 1;
				lentowrite &= ~(sizeof (int) - 1);
				memcpy(buf, vecs[i].iov_base, lentowrite);

				ret =
				    jffs2_flash_write(c, to, lentowrite,
						      &thislen, (char *) &buf);
				if (thislen > vecs[i].iov_len)
					thislen = vecs[i].iov_len;
			}	// else
		} else
			ret =
			    jffs2_flash_write(c, to, vecs[i].iov_len, &thislen,
					      vecs[i].iov_base);
		totlen += thislen;
		if (ret || thislen != vecs[i].iov_len)
			break;
		to += vecs[i].iov_len;
	}
      writev_out:
	if (retlen)
		*retlen = totlen;

	return ret;
}

cyg_bool jffs2_flash_erase(struct jffs2_sb_info * c,
			   struct jffs2_eraseblock * jeb)
{
	cyg_io_flash_getconfig_erase_t e;
	void *err_addr;
	Cyg_ErrNo err;
	cyg_uint32 len = sizeof (e);
	struct super_block *sb = OFNI_BS_2SFFJ(c);

	e.offset = jeb->offset;
	e.len = c->sector_size;
	e.err_address = &err_addr;

	//        D2(printf("FLASH ERASE ENABLED!!!\n"));
	//        D2(printf("erase address = %x\n", CYGNUM_FS_JFFS2_BASE_ADDRESS + jeb->offset));
	//        D2(printf("size = %x\n", c->sector_size));

	err = cyg_io_get_config(sb->s_dev, CYG_IO_GET_CONFIG_FLASH_ERASE,
				&e, &len);

	return (err != ENOERR || e.flasherr != 0);
}

// -------------------------------------------------------------------------
// EOF jffs2.c
void jffs2_clear_inode (struct inode *inode)
{
        /* We can forget about this inode for now - drop all
         *  the nodelists associated with it, etc.
         */
        struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
        struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);

        D1(printk(KERN_DEBUG "jffs2_clear_inode(): ino #%lu mode %o\n", inode->i_ino, inode->i_mode));

        jffs2_do_clear_inode(c, f);
}


/* jffs2_new_inode: allocate a new inode and inocache, add it to the hash,
   fill in the raw_inode while you're at it. */
struct inode *jffs2_new_inode (struct inode *dir_i, int mode, struct jffs2_raw_inode *ri)
{
	struct inode *inode;
	struct super_block *sb = dir_i->i_sb;
	struct jffs2_sb_info *c;
	struct jffs2_inode_info *f;
	int ret;

	D1(printk(KERN_DEBUG "jffs2_new_inode(): dir_i %ld, mode 0x%x\n", dir_i->i_ino, mode));

	c = JFFS2_SB_INFO(sb);
	
	inode = new_inode(sb);
	
	if (!inode)
		return ERR_PTR(-ENOMEM);

	f = JFFS2_INODE_INFO(inode);
	jffs2_init_inode_info(f);

	memset(ri, 0, sizeof(*ri));
	/* Set OS-specific defaults for new inodes */
	ri->uid = ri->gid = cpu_to_je16(0);
	ri->mode =  cpu_to_jemode(mode);
	ret = jffs2_do_new_inode (c, f, mode, ri);
	if (ret) {
		make_bad_inode(inode);
		iput(inode);
		return ERR_PTR(ret);
	}
	inode->i_nlink = 1;
	inode->i_ino = je32_to_cpu(ri->ino);
	inode->i_mode = jemode_to_cpu(ri->mode);
	inode->i_gid = je16_to_cpu(ri->gid);
	inode->i_uid = je16_to_cpu(ri->uid);
	inode->i_atime = inode->i_ctime = inode->i_mtime = cyg_timestamp();
	ri->atime = ri->mtime = ri->ctime = cpu_to_je32(inode->i_mtime);

	inode->i_size = 0;

	insert_inode_hash(inode);

	return inode;
}


void jffs2_read_inode (struct inode *inode)
{
	struct jffs2_inode_info *f;
	struct jffs2_sb_info *c;
	struct jffs2_raw_inode latest_node;
	int ret;

	D1(printk(KERN_DEBUG "jffs2_read_inode(): inode->i_ino == %lu\n", inode->i_ino));

	f = JFFS2_INODE_INFO(inode);
	c = JFFS2_SB_INFO(inode->i_sb);

	jffs2_init_inode_info(f);
	
	ret = jffs2_do_read_inode(c, f, inode->i_ino, &latest_node);

	if (ret) {
		make_bad_inode(inode);
		up(&f->sem);
		return;
	}
	inode->i_mode = jemode_to_cpu(latest_node.mode);
	inode->i_uid = je16_to_cpu(latest_node.uid);
	inode->i_gid = je16_to_cpu(latest_node.gid);
	inode->i_size = je32_to_cpu(latest_node.isize);
	inode->i_atime = je32_to_cpu(latest_node.atime);
	inode->i_mtime = je32_to_cpu(latest_node.mtime);
	inode->i_ctime = je32_to_cpu(latest_node.ctime);

	inode->i_nlink = f->inocache->nlink;
	up(&f->sem);

	D1(printk(KERN_DEBUG "jffs2_read_inode() returning\n"));
}
