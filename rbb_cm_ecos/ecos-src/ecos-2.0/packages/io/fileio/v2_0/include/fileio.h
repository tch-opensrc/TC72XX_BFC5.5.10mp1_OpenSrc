#ifndef CYGONCE_FILEIO_H
#define CYGONCE_FILEIO_H
//=============================================================================
//
//      fileio.h
//
//      Fileio header
//
//=============================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 or (at your option) any later version.
//
// eCos is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with eCos; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
//
// As a special exception, if other files instantiate templates or use macros
// or inline functions from this file, or you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on
// this file might be covered by the GNU General Public License.
//
// Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
// at http://sources.redhat.com/ecos/ecos-license/
// -------------------------------------------
//####ECOSGPLCOPYRIGHTEND####
//=============================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):     nickg
// Contributors:  nickg
// Date:          2000-05-25
// Purpose:       Fileio header
// Description:   This header contains the external definitions of the general file
//                IO subsystem for POSIX and EL/IX compatability.
//              
// Usage:
//              #include <fileio.h>
//              ...
//              
//
//####DESCRIPTIONEND####
//
//=============================================================================

#include <pkgconf/hal.h>
#include <pkgconf/kernel.h>
#include <pkgconf/io_fileio.h>

#include <cyg/infra/cyg_type.h>
#include <cyg/hal/hal_tables.h>

#include <stddef.h>             // NULL, size_t
#include <limits.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

//=============================================================================
// forward definitions

struct cyg_mtab_entry;
typedef struct cyg_mtab_entry cyg_mtab_entry;

struct  cyg_fstab_entry;
typedef struct  cyg_fstab_entry  cyg_fstab_entry;

struct CYG_FILEOPS_TAG;
typedef struct CYG_FILEOPS_TAG cyg_fileops;

struct CYG_FILE_TAG;
typedef struct CYG_FILE_TAG cyg_file;

struct CYG_IOVEC_TAG;
typedef struct CYG_IOVEC_TAG cyg_iovec;

struct CYG_UIO_TAG;
typedef struct CYG_UIO_TAG cyg_uio;

struct CYG_SELINFO_TAG;
typedef struct CYG_SELINFO_TAG cyg_selinfo;

//=============================================================================
// Directory pointer

typedef CYG_ADDRWORD cyg_dir;

#define CYG_DIR_NULL 0

//=============================================================================
// Filesystem table entry

typedef int     cyg_fsop_mount    ( cyg_fstab_entry *fste, cyg_mtab_entry *mte );
typedef int     cyg_fsop_umount   ( cyg_mtab_entry *mte );
typedef int     cyg_fsop_open     ( cyg_mtab_entry *mte, cyg_dir dir, const char *name,
                                    int mode,  cyg_file *fte );
typedef int     cyg_fsop_unlink   ( cyg_mtab_entry *mte, cyg_dir dir, const char *name );
typedef int     cyg_fsop_mkdir    ( cyg_mtab_entry *mte, cyg_dir dir, const char *name );
typedef int     cyg_fsop_rmdir    ( cyg_mtab_entry *mte, cyg_dir dir, const char *name );
typedef int     cyg_fsop_rename   ( cyg_mtab_entry *mte, cyg_dir dir1, const char *name1,
                                    cyg_dir dir2, const char *name2 );
typedef int     cyg_fsop_link     ( cyg_mtab_entry *mte, cyg_dir dir1, const char *name1,
                                    cyg_dir dir2, const char *name2, int type );
typedef int     cyg_fsop_opendir  ( cyg_mtab_entry *mte, cyg_dir dir, const char *name,
                                    cyg_file *fte );
typedef int     cyg_fsop_chdir    ( cyg_mtab_entry *mte, cyg_dir dir, const char *name,
                                    cyg_dir *dir_out );
typedef int     cyg_fsop_stat     ( cyg_mtab_entry *mte, cyg_dir dir, const char *name,
                                    struct stat *buf);
typedef int     cyg_fsop_getinfo  ( cyg_mtab_entry *mte, cyg_dir dir, const char *name,
                                    int key, void *buf, int len );
typedef int     cyg_fsop_setinfo  ( cyg_mtab_entry *mte, cyg_dir dir, const char *name,
                                    int key, void *buf, int len );


struct cyg_fstab_entry
{
    const char          *name;          // filesystem name
    CYG_ADDRWORD        data;           // private data value
    cyg_uint32          syncmode;       // synchronization mode
    
    cyg_fsop_mount      *mount;
    cyg_fsop_umount     *umount;
    cyg_fsop_open       *open;
    cyg_fsop_unlink     *unlink;
    cyg_fsop_mkdir      *mkdir;
    cyg_fsop_rmdir      *rmdir;
    cyg_fsop_rename     *rename;
    cyg_fsop_link       *link;
    cyg_fsop_opendir    *opendir;
    cyg_fsop_chdir      *chdir;
    cyg_fsop_stat       *stat;
    cyg_fsop_getinfo    *getinfo;
    cyg_fsop_setinfo    *setinfo;
} CYG_HAL_TABLE_TYPE;

//-----------------------------------------------------------------------------
// Keys for getinfo() and setinfo()

#define FS_INFO_CONF            1       /* pathconf() */
#define FS_INFO_ACCESS          2       /* access() */
#define FS_INFO_GETCWD          3       /* getcwd() */

//-----------------------------------------------------------------------------
// Types for link()

#define CYG_FSLINK_HARD         1       /* form a hard link */
#define CYG_FSLINK_SOFT         2       /* form a soft link */

//-----------------------------------------------------------------------------
// getinfo() and setinfo() buffers structures.

struct cyg_getcwd_info
{
    char        *buf;           /* buffer for cwd string */
    size_t      size;           /* size of buffer */
};

//-----------------------------------------------------------------------------
// Macro to define an initialized fstab entry

#define FSTAB_ENTRY( _l, _name, _data, _syncmode, _mount, _umount,      \
                     _open, _unlink,  _mkdir, _rmdir, _rename, _link,   \
                     _opendir, _chdir, _stat, _getinfo, _setinfo)       \
struct cyg_fstab_entry _l CYG_HAL_TABLE_ENTRY(fstab) =                  \
{                                                                       \
    _name,                                                              \
    _data,                                                              \
    _syncmode,                                                          \
    _mount,                                                             \
    _umount,                                                            \
    _open,                                                              \
    _unlink,                                                            \
    _mkdir,                                                             \
    _rmdir,                                                             \
    _rename,                                                            \
    _link,                                                              \
    _opendir,                                                           \
    _chdir,                                                             \
    _stat,                                                              \
    _getinfo,                                                           \
    _setinfo                                                            \
};

//=============================================================================
// Mount table entry

struct cyg_mtab_entry
{
    const char          *name;          // name of mount point
    const char          *fsname;        // name of implementing filesystem
    const char          *devname;       // name of hardware device
    CYG_ADDRWORD        data;           // private data value
    
    // The following are filled in after a successful mount operation
    cyg_bool            valid;          // Valid entry?
    cyg_fstab_entry     *fs;            // pointer to fstab entry
    cyg_dir             root;           // root directory pointer
} CYG_HAL_TABLE_TYPE;


// This macro defines an initialized mtab entry

#define MTAB_ENTRY( _l, _name, _fsname, _devname, _data )       \
struct cyg_mtab_entry _l CYG_HAL_TABLE_ENTRY(mtab) =            \
{                                                               \
    _name,                                                      \
    _fsname,                                                    \
    _devname,                                                   \
    _data,                                                      \
    false,                                                      \
    NULL,                                                       \
    CYG_DIR_NULL                                                \
};

//=============================================================================
// IO vector descriptors

struct CYG_IOVEC_TAG
{
    void           *iov_base;           /* Base address. */
    ssize_t        iov_len;             /* Length. */
};

enum	cyg_uio_rw { UIO_READ, UIO_WRITE };

/* Segment flag values. */
enum cyg_uio_seg
{
    UIO_USERSPACE,		        /* from user data space */
    UIO_SYSSPACE		        /* from system space */
};

struct CYG_UIO_TAG
{
    struct CYG_IOVEC_TAG *uio_iov;	/* pointer to array of iovecs */
    int	                 uio_iovcnt;	/* number of iovecs in array */
    off_t       	 uio_offset;	/* offset into file this uio corresponds to */
    ssize_t     	 uio_resid;	/* residual i/o count */
    enum cyg_uio_seg     uio_segflg;    /* see above */
    enum cyg_uio_rw      uio_rw;        /* see above */
};

// Limits
#define UIO_SMALLIOV	8		/* 8 on stack, else malloc */

//=============================================================================
// Description of open file

typedef int cyg_fileop_readwrite (struct CYG_FILE_TAG *fp, struct CYG_UIO_TAG *uio);
typedef cyg_fileop_readwrite cyg_fileop_read;
typedef cyg_fileop_readwrite cyg_fileop_write;
typedef int cyg_fileop_lseek   (struct CYG_FILE_TAG *fp, off_t *pos, int whence );
typedef int cyg_fileop_ioctl   (struct CYG_FILE_TAG *fp, CYG_ADDRWORD com,
                                CYG_ADDRWORD data);
typedef int cyg_fileop_select  (struct CYG_FILE_TAG *fp, int which, CYG_ADDRWORD info);
typedef int cyg_fileop_fsync   (struct CYG_FILE_TAG *fp, int mode );        
typedef int cyg_fileop_close   (struct CYG_FILE_TAG *fp);
typedef int cyg_fileop_fstat   (struct CYG_FILE_TAG *fp, struct stat *buf );
typedef int cyg_fileop_getinfo (struct CYG_FILE_TAG *fp, int key, void *buf, int len );
typedef int cyg_fileop_setinfo (struct CYG_FILE_TAG *fp, int key, void *buf, int len );

struct CYG_FILEOPS_TAG
{
    cyg_fileop_read     *fo_read;
    cyg_fileop_write    *fo_write;
    cyg_fileop_lseek    *fo_lseek;
    cyg_fileop_ioctl    *fo_ioctl;
    cyg_fileop_select   *fo_select;
    cyg_fileop_fsync    *fo_fsync;
    cyg_fileop_close    *fo_close;
    cyg_fileop_fstat    *fo_fstat;
    cyg_fileop_getinfo  *fo_getinfo;
    cyg_fileop_setinfo  *fo_setinfo;
};

struct CYG_FILE_TAG
{
    cyg_uint32	                f_flag;		/* file state                   */
    cyg_uint16                  f_ucount;       /* use count                    */
    cyg_uint16                  f_type;		/* descriptor type              */
    cyg_uint32                  f_syncmode;     /* synchronization protocol     */
    struct CYG_FILEOPS_TAG      *f_ops;         /* file operations              */
    off_t       	        f_offset;       /* current offset               */
    CYG_ADDRWORD	        f_data;		/* file or socket               */
    CYG_ADDRWORD                f_xops;         /* extra type specific ops      */
    cyg_mtab_entry              *f_mte;         /* mount table entry            */
};

//-----------------------------------------------------------------------------
// File flags

// Allocation here is that bits 0..15 are copies of bits from the open
// flags, bits 16..23 are extra bits that are visible to filesystems but
// are not derived from the open call, and bits 24..31 are reserved for
// the fileio infrastructure.
#define CYG_FREAD       O_RDONLY
#define CYG_FWRITE      O_WRONLY
#define CYG_FNONBLOCK   O_NONBLOCK
#define CYG_FAPPEND     O_APPEND
#define CYG_FASYNC      0x00010000
#define CYG_FDIR        0x00020000

#define CYG_FLOCKED     0x01000000  // Set if file is locked
#define CYG_FLOCK       0x02000000  // Lock during file ops
#define CYG_FALLOC      0x80000000  // File is "busy", i.e. allocated

// Mask for open mode bits stored in file object
#define CYG_FILE_MODE_MASK (CYG_FREAD|CYG_FWRITE|CYG_FNONBLOCK|CYG_FAPPEND)

//-----------------------------------------------------------------------------
// Type of file

#define	CYG_FILE_TYPE_FILE      1	/* file */
#define	CYG_FILE_TYPE_SOCKET	2	/* communications endpoint */
#define	CYG_FILE_TYPE_DEVICE	3	/* device */

//-----------------------------------------------------------------------------
// Keys for getinf() and setinfo()

#define FILE_INFO_CONF          1       /* fpathconf() */

//-----------------------------------------------------------------------------
// Modes for fsync()

#define CYG_FSYNC              1
#define CYG_FDATASYNC          2

//-----------------------------------------------------------------------------
// Get/set info buffer structures

// This is used for pathconf() and fpathconf()
struct cyg_pathconf_info
{
    int         name;           // POSIX defined variable name
    long        value;          // Returned variable value
};

//=============================================================================
// Synchronization modes
// These values are filled into the syncmode fields of the above structures
// and define the synchronization protocol used when accessing the object in
// question.

#define CYG_SYNCMODE_NONE               (0)     // no locking required

#define CYG_SYNCMODE_FILE_FILESYSTEM    0x0002  // lock fs during file ops
#define CYG_SYNCMODE_FILE_MOUNTPOINT    0x0004  // lock mte during file ops
#define CYG_SYNCMODE_IO_FILE            0x0010  // lock file during io ops
#define CYG_SYNCMODE_IO_FILESYSTEM      0x0020  // lock fs during io ops
#define CYG_SYNCMODE_IO_MOUNTPOINT      0x0040  // lock mte during io ops
#define CYG_SYNCMODE_SOCK_FILE          0x0100  // lock socket during socket ops
#define CYG_SYNCMODE_SOCK_NETSTACK      0x0800  // lock netstack during socket ops

#define CYG_SYNCMODE_IO_SHIFT           (4)     // shift for IO to file bits
#define CYG_SYNCMODE_SOCK_SHIFT         (8)     // shift for sock to file bits

//=============================================================================
// Mount and umount functions

__externC int mount( const char *devname,
                     const char *dir,
                     const char *fsname);

__externC int umount( const char *name);

//=============================================================================
// Select support

//-----------------------------------------------------------------------------
// Data structure for embedding in client data structures. A pointer to this
// must be passed to cyg_selrecord() and cyg_selwakeup().

struct CYG_SELINFO_TAG
{
    CYG_ADDRWORD        si_info;        // info passed through from fo_select()
    CYG_ADDRESS         si_thread;      // selecting thread pointer
};

//-----------------------------------------------------------------------------
// Select support functions.

// cyg_selinit() is used to initialize a selinfo structure.
__externC void cyg_selinit( struct CYG_SELINFO_TAG *sip );

// cyg_selrecord() is called when a client device needs to register
// the current thread for selection.
__externC void cyg_selrecord( CYG_ADDRWORD info, struct CYG_SELINFO_TAG *sip );

// cyg_selwakeup() is called when the client device matches the select
// criterion, and needs to wake up a selector.
__externC void cyg_selwakeup( struct CYG_SELINFO_TAG *sip );

//=============================================================================
// Timestamp support

// Provides the current time as a time_t timestamp for use in filesystem
// data strucures.

__externC time_t cyg_timestamp(void);

//=============================================================================
// Default functions.
// Cast to the appropriate type, these functions can be put into any of
// the operation table slots to provide the defined error code.

__externC int cyg_fileio_enosys(void);
__externC int cyg_fileio_erofs(void);
__externC int cyg_fileio_enoerr(void);
__externC int cyg_fileio_enotdir(void);
__externC cyg_fileop_select cyg_fileio_seltrue;

//-----------------------------------------------------------------------------
#endif // ifndef CYGONCE_FILEIO_H
// End of fileio.h
