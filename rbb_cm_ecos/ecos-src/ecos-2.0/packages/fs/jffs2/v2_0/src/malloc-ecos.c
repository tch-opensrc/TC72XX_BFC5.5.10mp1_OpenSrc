/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 * $Id: malloc-ecos.c,v 1.2 2003/01/09 13:53:44 dwmw2 Exp $
 *
 */

#include <linux/kernel.h>
#include "nodelist.h"

struct jffs2_full_dirent *jffs2_alloc_full_dirent(int namesize)
{
	return malloc(sizeof(struct jffs2_full_dirent) + namesize);
}

void jffs2_free_full_dirent(struct jffs2_full_dirent *x)
{
	free(x);
}

struct jffs2_full_dnode *jffs2_alloc_full_dnode(void)
{
	return malloc(sizeof(struct jffs2_full_dnode));
}

void jffs2_free_full_dnode(struct jffs2_full_dnode *x)
{
	free(x);
}

struct jffs2_raw_dirent *jffs2_alloc_raw_dirent(void)
{
	return malloc(sizeof(struct jffs2_raw_dirent));
}

void jffs2_free_raw_dirent(struct jffs2_raw_dirent *x)
{
	free(x);
}

struct jffs2_raw_inode *jffs2_alloc_raw_inode(void)
{
	return malloc(sizeof(struct jffs2_raw_inode));
}

void jffs2_free_raw_inode(struct jffs2_raw_inode *x)
{
	free(x);
}

struct jffs2_tmp_dnode_info *jffs2_alloc_tmp_dnode_info(void)
{
	return malloc(sizeof(struct jffs2_tmp_dnode_info));
}

void jffs2_free_tmp_dnode_info(struct jffs2_tmp_dnode_info *x)
{
	free(x);
}

struct jffs2_raw_node_ref *jffs2_alloc_raw_node_ref(void)
{
	return malloc(sizeof(struct jffs2_raw_node_ref));
}

void jffs2_free_raw_node_ref(struct jffs2_raw_node_ref *x)
{
	free(x);
}

struct jffs2_node_frag *jffs2_alloc_node_frag(void)
{
	return malloc(sizeof(struct jffs2_node_frag));
}

void jffs2_free_node_frag(struct jffs2_node_frag *x)
{
	free(x);
}

struct jffs2_inode_cache *jffs2_alloc_inode_cache(void)
{
	struct jffs2_inode_cache *ret = malloc(sizeof(struct jffs2_inode_cache));
	D1(printk(KERN_DEBUG "Allocated inocache at %p\n", ret));
	return ret;
}

void jffs2_free_inode_cache(struct jffs2_inode_cache *x)
{
	D1(printk(KERN_DEBUG "Freeing inocache at %p\n", x));
	free(x);
}

