/*
 * $Id: mxf_harmony.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Samba VFS module that exports the raw essence data within MXF files as virtual files.
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
 
 
// TODO: Opaque or Transparent VFS?


#include "includes.h"
#include <mxf_essence.h>

static int vfs_mxfh_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_mxfh_debug_level


static const char* MXFH_MODULE_NAME = "mxf_harmony";
static const char* MXF_SUFFIX = ".mxf";
static const size_t MXF_SUFFIX_LEN = 4;
static const char* VIRTUAL_MXF_SUFFIX = "._v_.";
static const size_t VIRTUAL_MXF_SUFFIX_LEN = 5;


typedef struct _mxfh_virtual_mxf_file
{
    struct _mxfh_virtual_mxf_file* next;
    struct _mxfh_virtual_mxf_file* prev;
    int fd;
    SMB_OFF_T offset;
    SMB_OFF_T length;
} mxfh_virtual_mxf_file;

typedef struct _mxfh_private_data 
{
    mxfh_virtual_mxf_file* virtualFiles;
} mxfh_private_data;

typedef struct _mxfh_dirinfo
{
    pstring dirpath;
    SMB_STRUCT_DIRENT* prevDirent;
    fstring prevDirentName;
    DIR* dirstream;
} mxfh_dirinfo;


static int mxfh_add_virtual_file(vfs_handle_struct* handle, 
    mxfh_virtual_mxf_file* newVirtualFile)
{
    if (!SMB_VFS_HANDLE_TEST_DATA(handle))
    {
        return 0;
    }
    
    mxfh_private_data* pd = NULL;
    SMB_VFS_HANDLE_GET_DATA(handle, pd, mxfh_private_data, );
    
    // first in list
    if (pd->virtualFiles == NULL)
    {
        pd->virtualFiles = newVirtualFile;
        newVirtualFile->prev = NULL;
        newVirtualFile->next = NULL;
    }
    // append to end of list
    else
    {
        mxfh_virtual_mxf_file* last = pd->virtualFiles;
        while (last->next != NULL)
        {
            last = last->next;
        }
        last->next = newVirtualFile;
        newVirtualFile->prev = last;
        newVirtualFile->next = NULL;
    }

    syslog(LOG_NOTICE, "Added mxf virtual file %d\n", newVirtualFile->fd);
    return 1;
}

static void mxfh_free_virtual_file(mxfh_virtual_mxf_file** vf)
{
    SAFE_FREE(*vf);
    *vf = NULL;
}

static void mxfh_remove_virtual_file(vfs_handle_struct* handle, int fd)
{
    if (!SMB_VFS_HANDLE_TEST_DATA(handle))
    {
        return;
    }
    
    mxfh_private_data* pd = NULL;
    SMB_VFS_HANDLE_GET_DATA(handle, pd, mxfh_private_data, );
    
    mxfh_virtual_mxf_file* vf = pd->virtualFiles;
    while (vf != NULL)
    {
        if (vf->fd == fd)
        {
            // remove from list
            if (vf->prev != NULL)
            {
                vf->prev->next = vf->next;
            }
            if (vf->next != NULL)
            {
                vf->next->prev = vf->prev;
            }
            // reassign list root if vf is the root
            if (vf == pd->virtualFiles)
            {
                pd->virtualFiles = vf->next;
            }
            mxfh_free_virtual_file(&vf);
            syslog(LOG_NOTICE, "Removed mxf virtual file %d\n", fd);
            break;
        }
        vf = vf->next;
    }
}

static mxfh_virtual_mxf_file* mxfh_get_virtual_file(vfs_handle_struct* handle, 
    int fd)
{
    if (!SMB_VFS_HANDLE_TEST_DATA(handle))
    {
        return NULL;
    }
    
    mxfh_private_data* pd = NULL;
    SMB_VFS_HANDLE_GET_DATA(handle, pd, mxfh_private_data, );
    
    mxfh_virtual_mxf_file* vf = pd->virtualFiles;
    while (vf != NULL)
    {
        if (vf->fd == fd)
        {
            break;
        }
        vf = vf->next;
    }
    
    return vf;
}

static void mxfh_free_dirinfo(mxfh_dirinfo** dirinfo)
{
    SAFE_FREE(*dirinfo);
    *dirinfo = NULL;
}

static void mxfh_init_private_data(mxfh_private_data* privateData)
{
    privateData->virtualFiles = NULL;
}

static void mxfh_free_private_data(void **p_data)
{
    mxfh_private_data *pd = *(mxfh_private_data **)p_data;

    mxfh_virtual_mxf_file* vf = pd->virtualFiles;
    while (vf != NULL)
    {
        mxfh_virtual_mxf_file* tmp = vf;
        vf = vf->next;
        mxfh_free_virtual_file(&tmp);
    }
    
    SAFE_FREE(pd);
    *p_data = NULL;
}

// a files is deemed to be a virtual file if it has a suffix
// MXF_SUFFIX (.mxf) followed by VIRTUAL_MXF_SUFFIX (._v_.)
static BOOL is_virtual_mxf_file(const char* path, pstring* realPath)
{
    BOOL isVirtual = False;
    const char* suffix = path;
    while (suffix != NULL)
    {
        if ((suffix = strstr(suffix, MXF_SUFFIX)) != NULL)
        {
            suffix += MXF_SUFFIX_LEN;
            if (strncmp(suffix, VIRTUAL_MXF_SUFFIX, VIRTUAL_MXF_SUFFIX_LEN) == 0)
            {
                if (strchr(suffix, '/') == NULL)
                {
                    pstrcpy(*realPath, path);
                    (*realPath)[suffix - path] = '\0';
                    return True;
                }
            }
        }
    }

    pstrcpy(*realPath, path);
    return False;    
}


static int mxfh_connect(vfs_handle_struct *handle, connection_struct *conn,
             const char *svc, const char *user)
{
    int result;
    mxfh_private_data* pd = NULL;

    if (!handle) 
    {
        return -1;
    }
         
    pd = SMB_MALLOC_P(mxfh_private_data);
    if (!pd) 
    {
         return -1;
    }
    ZERO_STRUCTP(pd);
    mxfh_init_private_data(pd);

    SMB_VFS_HANDLE_SET_DATA(handle, pd, mxfh_free_private_data,
                         mxfh_private_data, return -1);

    return SMB_VFS_NEXT_CONNECT(handle, conn, svc, user);
}

static void mxfh_disconnect(vfs_handle_struct *handle,
                 connection_struct *conn)
{
    SMB_VFS_NEXT_DISCONNECT(handle, conn);
}

static DIR *mxfh_opendir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    mxfh_dirinfo* dirInfo = SMB_MALLOC_P(mxfh_dirinfo);
    if (!dirInfo)
    {
        DEBUG(0, ("mxfh_opendir: Out of memory. Failed to malloc dirinfo.\n"));
        return NULL;
    }
    pstrcpy(dirInfo->dirpath, path);
    dirInfo->prevDirent = NULL;
    
    dirInfo->dirstream = SMB_VFS_NEXT_OPENDIR(handle, conn, path);
        
    return (DIR*)dirInfo;
}

static SMB_STRUCT_DIRENT *mxfh_readdir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp)
{
    mxfh_dirinfo* dirInfo = (mxfh_dirinfo*)dirp;

    SMB_STRUCT_DIRENT* d = NULL;
    if (dirInfo->prevDirent != NULL)
    {
        // return the previous entry which was delayed for an associated virtual entry
        d = dirInfo->prevDirent;
        fstrcpy(d->d_name, dirInfo->prevDirentName);
        dirInfo->prevDirent = NULL;
    }
    else
    {
        d = SMB_VFS_NEXT_READDIR(handle, conn, dirInfo->dirstream);
        
        if (d != NULL)
        {
            size_t len = strlen(d->d_name);
            // only check files that end inf MXF_SUFFIX
            if (len >= MXF_SUFFIX_LEN && strcmp(&d->d_name[len - MXF_SUFFIX_LEN], MXF_SUFFIX) == 0)
            {
                pstring fpath;
                pstrcpy(fpath, dirInfo->dirpath);
                pstrcat(fpath, "/");
                pstrcat(fpath, d->d_name);
                FILE* f;
                if ((f = fopen(fpath, "rb")) != NULL)
                {
                    mxfe_EssenceType type;
                    const char* suffix = NULL;
                    if (mxfe_get_essence_type(f, &type) && 
                        type != MXFE_UNKNOWN &&
                        mxfe_get_essence_suffix(type, &suffix))
                    {
                        // save this dir entry for the next readdir
                        dirInfo->prevDirent = d;
                        fstrcpy(dirInfo->prevDirentName, d->d_name);
                        
                        // the dir entry becomes the virtual file
                        fstrcat(d->d_name, VIRTUAL_MXF_SUFFIX);
                        fstrcat(d->d_name, suffix);
                    }
                    fclose(f);
                }
                else
                {
                    DEBUG(0, ("mxfh_readdir: Failed to open file %s.\n", fpath));
                }
            }
        }
    }

    return d;
}

static void mxfh_seekdir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp, long offset)
{
    ((mxfh_dirinfo*)dirp)->prevDirent = NULL; // no more previous
    
    return SMB_VFS_NEXT_SEEKDIR(handle, conn, ((mxfh_dirinfo*)dirp)->dirstream, offset);
}

static long mxfh_telldir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp)
{
    return SMB_VFS_NEXT_TELLDIR(handle, conn, ((mxfh_dirinfo*)dirp)->dirstream);
}

static void mxfh_rewinddir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp)
{
    ((mxfh_dirinfo*)dirp)->prevDirent = NULL; // no more previous
    
    return SMB_VFS_NEXT_REWINDDIR(handle, conn, ((mxfh_dirinfo*)dirp)->dirstream);
}

static int mxfh_closedir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp)
{
    DIR* realdirp = ((mxfh_dirinfo*)dirp)->dirstream;
    mxfh_free_dirinfo((mxfh_dirinfo**)&dirp);
    
    return SMB_VFS_NEXT_CLOSEDIR(handle, conn, realdirp);
}

static int mxfh_open(vfs_handle_struct *handle, connection_struct *conn, const char *path, int flags, mode_t mode)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        mxfh_virtual_mxf_file* vf = SMB_MALLOC_P(mxfh_virtual_mxf_file);
        if (!vf)
        {
            DEBUG(0, ("mxfh_open: Out of memory. Failed to malloc mxfh_virtual_mxf_file.\n"));
            return -1;
        }
        
        // get the offset and length of the essence data
        FILE* f;
        if ((f = fopen(realPath, "rb")) != NULL)
        {
            uint64_t offset;
            uint64_t length;
            if (!mxfe_get_essence_element_info(f, &offset, &length))
            {
                DEBUG(0, ("mxfh_open: Failed to get essence data info from %s\n", realPath));
                mxfh_free_virtual_file(&vf);
                fclose(f);
                return -1;
            }
            vf->offset = offset;
            vf->length = length;
            fclose(f);
        }
        else
        {
            DEBUG(0, ("mxfh_open: Failed to open file %s.\n", realPath));
            mxfh_free_virtual_file(&vf);
            return -1;
        }

        // add virtual file entry if all succeeds        
        int fd = SMB_VFS_NEXT_OPEN(handle, conn, realPath, flags, mode);
        if (fd >= 0)
        {
            vf->fd = fd;
            if (!mxfh_add_virtual_file(handle, vf))
            {
                mxfh_free_virtual_file(&vf);
            }
        }
        else
        {
            mxfh_free_virtual_file(&vf);
        }
        
        return fd;
    }
    else
    {
        return SMB_VFS_NEXT_OPEN(handle, conn, path, flags, mode);
    }
}

static int mxfh_close(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
    mxfh_remove_virtual_file(handle, fd);

    return SMB_VFS_NEXT_CLOSE(handle, fsp, fd);
}

static ssize_t mxfh_read(vfs_handle_struct *handle, files_struct *fsp, int fd, void *data, size_t n)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // if file position is beyond essence data then return EOF
        if (fsp->pos >= vf->offset + vf->length)
        {
            return 0;
        }
        
        // if file position is below essence data offset then move it to the offset
        // this situation occurs in the first read 
        if (fsp->pos < vf->offset)
        {
            SMB_OFF_T offset = SMB_VFS_NEXT_LSEEK(handle, fsp, fd, vf->offset, SEEK_SET);
            if (offset != vf->offset)
            {
                DEBUG(0, ("mxfh_read: Failed to position stream at start of essence data\n"));
                errno = EIO; // TODO: is this the correct error?
                return -1;
            }
        }
        // read
        SMB_OFF_T origPos = fsp->pos;
        ssize_t numRead = SMB_VFS_NEXT_READ(handle, fsp, fd, data, n);
        
        // if we have read past the essence data then adjust the returned numRead
        // and position the file at the end of the essence data
        if (fsp->pos > vf->offset + vf->length)
        {
            numRead = vf->offset + vf->length - origPos;
            SMB_OFF_T offset = SMB_VFS_NEXT_LSEEK(handle, fsp, fd, vf->offset + vf->length, SEEK_SET);
            if (offset != vf->offset + vf->length)
            {
                DEBUG(0, ("mxfh_read: Failed to position stream at end of essence data\n"));
                errno = EIO; // TODO: is this the correct error?
                return -1;
            }
        }
        return numRead;
    }
    else
    {
        return SMB_VFS_NEXT_READ(handle, fsp, fd, data, n);
    }
}

static ssize_t mxfh_pread(vfs_handle_struct *handle, files_struct *fsp, int fd, void *data, size_t n, SMB_OFF_T offset)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // if file position is beyond essence data then return EOF
        if (offset >= vf->length)
        {
            return 0;
        }
        
        // if we have read past the essence data then adjust the returned numRead
        // and position the file at the end of the essence data
        ssize_t numRead = SMB_VFS_NEXT_PREAD(handle, fsp, fd, data, n, offset + vf->offset);
        if (offset + numRead > vf->length)
        {
            numRead = vf->length - offset;
            SMB_OFF_T offset = SMB_VFS_NEXT_LSEEK(handle, fsp, fd, vf->offset + vf->length, SEEK_SET);
            if (offset != vf->offset + vf->length)
            {
                DEBUG(0, ("mxfh_read: Failed to position stream at end of essence data\n"));
                errno = EIO; // TODO: is this the correct error?
                return -1;
            }
        }
        return numRead;
    }
    else
    {
        return SMB_VFS_NEXT_PREAD(handle, fsp, fd, data, n, offset);
    }
}

static ssize_t mxfh_write(vfs_handle_struct *handle, files_struct *fsp, int fd, const void *data, size_t n)
{
    if (mxfh_get_virtual_file(handle, fd) != NULL)
    {
        // write not supported for virtual files
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_WRITE(handle, fsp, fd, data, n);
    }
}

static ssize_t mxfh_pwrite(vfs_handle_struct *handle, files_struct *fsp, int fd, const void *data, size_t n, SMB_OFF_T offset)
{
    if (mxfh_get_virtual_file(handle, fd) != NULL)
    {
        // write not supported for virtual files
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_PWRITE(handle, fsp, fd, data, n, offset);
    }
}

static SMB_OFF_T mxfh_lseek(vfs_handle_struct *handle, files_struct *fsp, int filedes, SMB_OFF_T offset, int whence)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, filedes);
    if (vf != NULL)
    {
        if (whence == SEEK_SET)
        {
            return SMB_VFS_NEXT_LSEEK(handle, fsp, filedes, offset + vf->offset, whence);
        }
        else if (whence == SEEK_END)
        {
            // END is end of the essence data and not end of the file
            // using a SEEK_SET
            return SMB_VFS_NEXT_LSEEK(handle, fsp, filedes, offset + vf->offset + vf->length, SEEK_SET);
        }
        else
        {
            return SMB_VFS_NEXT_LSEEK(handle, fsp, filedes, offset, whence);
        }
    }
    else
    {
        return SMB_VFS_NEXT_LSEEK(handle, fsp, filedes, offset, whence);
    }
}

static int mxfh_rename(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring oldRealPath;
    pstring newRealPath;
    if (is_virtual_mxf_file(oldpath, &oldRealPath) || is_virtual_mxf_file(newpath, &newRealPath))
    {
        // renaming virtual files (oldpath or newpath)
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_RENAME(handle, conn, oldpath, newpath);
    }
}

static int mxfh_fsync(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // fsync virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_FSYNC(handle, fsp, fd);
    }
}

static int mxfh_stat(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_STRUCT_STAT *sbuf)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        int statResult = SMB_VFS_NEXT_STAT(handle, conn, realPath, sbuf);
        if (statResult != 0)
        {
            return statResult;
        }
        
        // set file size equal to length of essence data and set to read only
        FILE* f;
        if ((f = fopen(realPath, "rb")) != NULL)
        {
            uint64_t offset;
            uint64_t len;
            if (!mxfe_get_essence_element_info(f, &offset, &len))
            {
                fclose(f);
                return statResult;
            }
            sbuf->st_size = len;
            sbuf->st_mode &= 077444; // read only
            fclose(f);
        }
        else
        {
            return statResult;
        }
    }
    else
    {
        return SMB_VFS_NEXT_STAT(handle, conn, path, sbuf);
    }
}

static int mxfh_fstat(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_STRUCT_STAT *sbuf)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        int statResult = SMB_VFS_NEXT_FSTAT(handle, fsp, fd, sbuf);
        if (statResult != 0)
        {
            return statResult;
        }
        
        // set file size equal to length of essence data and set to read only
        sbuf->st_size = vf->length;
        sbuf->st_mode &= 077444; // read only
        return statResult;
    }
    else
    {
        return SMB_VFS_NEXT_FSTAT(handle, fsp, fd, sbuf);
    }
}

static int mxfh_lstat(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_STRUCT_STAT *sbuf)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        int statResult = SMB_VFS_NEXT_LSTAT(handle, conn, realPath, sbuf);
        if (statResult != 0)
        {
            return statResult;
        }
        
        // set file size equal to length of essence data and set to read only
        FILE* f;
        if ((f = fopen(realPath, "rb")) != NULL)
        {
            uint64_t offset;
            uint64_t len;
            if (!mxfe_get_essence_element_info(f, &offset, &len))
            {
                fclose(f);
                return statResult;
            }
            sbuf->st_size = len;
            sbuf->st_mode &= 077444; // read only
            fclose(f);
        }
        else
        {
            return statResult;
        }
    }
    else
    {
        return SMB_VFS_NEXT_LSTAT(handle, conn, path, sbuf);
    }
}

static int mxfh_unlink(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        // unlinking virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_UNLINK(handle, conn, path);
    }    
}

static int mxfh_chmod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        // chmod virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_CHMOD(handle, conn, path, mode);
    }
}

static int mxfh_fchmod(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // chmod virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_FCHMOD(handle, fsp, fd, mode);
    }
}

static int mxfh_chown(vfs_handle_struct *handle, connection_struct *conn, const char *path, uid_t uid, gid_t gid)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        // chown virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_CHOWN(handle, conn, path, uid, gid);
    }
}

static int mxfh_fchown(vfs_handle_struct *handle, files_struct *fsp, int fd, uid_t uid, gid_t gid)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // chown virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_FCHOWN(handle, fsp, fd, uid, gid);
    }
}

static int mxfh_chdir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_CHDIR(handle, conn, realPath);
    }
    else
    {
        return SMB_VFS_NEXT_CHDIR(handle, conn, path);
    }
}

static int mxfh_utime(vfs_handle_struct *handle, connection_struct *conn, const char *path, struct utimbuf *times)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_UTIME(handle, conn, realPath, times);
    }
    else
    {
        return SMB_VFS_NEXT_UTIME(handle, conn, path, times);
    }
}

static int mxfh_ftruncate(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_OFF_T offset)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // ftruncate virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_FTRUNCATE(handle, fsp, fd, offset);
    }
}

// TODO: check lock bounds?
static BOOL mxfh_lock(vfs_handle_struct *handle, files_struct *fsp, int fd, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        return SMB_VFS_NEXT_LOCK(handle, fsp, fd, op, offset + vf->offset, count, type);
    }
    else
    {
        return SMB_VFS_NEXT_LOCK(handle, fsp, fd, op, offset, count, type);
    }
}

static BOOL mxfh_symlink(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring oldRealPath;
    pstring newRealPath;
    if (is_virtual_mxf_file(oldpath, &oldRealPath) || is_virtual_mxf_file(newpath, &newRealPath))
    {
        errno = ENOTSUP;
        return False;
    }
    else
    {
        return SMB_VFS_NEXT_SYMLINK(handle, conn, oldpath, newpath);
    }
}

static BOOL mxfh_readlink(vfs_handle_struct *handle, connection_struct *conn, const char *path, char *buf, size_t bufsiz)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return False;
    }
    else
    {
        return SMB_VFS_NEXT_READLINK(handle, conn, path, buf, bufsiz);
    }
}

static int mxfh_link(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring oldRealPath;
    pstring newRealPath;
    if (is_virtual_mxf_file(oldpath, &oldRealPath) || is_virtual_mxf_file(newpath, &newRealPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_LINK(handle, conn, oldpath, newpath);
    }
}

static int mxfh_mknod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode, SMB_DEV_T dev)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_MKNOD(handle, conn, path, mode, dev);
    }
}

static char *mxfh_realpath(vfs_handle_struct *handle, connection_struct *conn, const char *path, char *resolved_path)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_REALPATH(handle, conn, realPath, resolved_path);
    }
    else
    {
        return SMB_VFS_NEXT_REALPATH(handle, conn, path, resolved_path);
    }
}

// TODO: remove this?
static size_t mxfh_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, uint32 security_info, struct security_descriptor_info **ppdesc)
{
    return SMB_VFS_NEXT_FGET_NT_ACL(handle, fsp, fd, security_info, ppdesc);
}

static size_t mxfh_get_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *path, uint32 security_info, struct security_descriptor_info **ppdesc)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, realPath, security_info, ppdesc);
    }
    else
    {
        return SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, path, security_info, ppdesc);
    }
}

static BOOL mxfh_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, uint32 security_info_sent, struct security_descriptor_info *psd)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // fset_nt_acl virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_FSET_NT_ACL(handle, fsp, fd, security_info_sent, psd);
    }
}

static BOOL mxfh_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *path, uint32 security_info_sent, struct security_descriptor_info *psd)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_SET_NT_ACL(handle, fsp, path, security_info_sent, psd);
    }
}

static int mxfh_chmod_acl(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_CHMOD_ACL(handle, conn, path, mode);
    }
}

static int mxfh_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // fchmod_acl virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, fd, mode);
    }
}

static SMB_ACL_T mxfh_sys_acl_get_file(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_ACL_TYPE_T type)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, conn, realPath, type);
    }
    else
    {
        return SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, conn, path, type);
    }
}

// TODO: remove this?
static SMB_ACL_T mxfh_sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
    return SMB_VFS_NEXT_SYS_ACL_GET_FD(handle, fsp, fd);
}

static int mxfh_sys_acl_set_file(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, conn, path, acltype, theacl);
    }
}

static int mxfh_sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp, int fd, SMB_ACL_T theacl)
{
    mxfh_virtual_mxf_file* vf = mxfh_get_virtual_file(handle, fd);
    if (vf != NULL)
    {
        // sys_acl_set_fd virtual files not supported
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_SYS_ACL_SET_FD(handle, fsp, fd, theacl);
    }
}

static int mxfh_sys_acl_delete_def_file(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, conn, path);
    }
}

static ssize_t mxfh_getxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, void *value, size_t size)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_GETXATTR(handle, conn, realPath, name, value, size);
    }
    else
    {
        return SMB_VFS_NEXT_GETXATTR(handle, conn, path, name, value, size);
    }
}

static ssize_t mxfh_lgetxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, void *value, size_t
size)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_LGETXATTR(handle, conn, realPath, name, value, size);
    }
    else
    {
        return SMB_VFS_NEXT_LGETXATTR(handle, conn, path, name, value, size);
    }
}

static ssize_t mxfh_listxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, char *list, size_t size)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_LISTXATTR(handle, conn, realPath, list, size);
    }
    else
    {
        return SMB_VFS_NEXT_LISTXATTR(handle, conn, path, list, size);
    }
}

static ssize_t mxfh_llistxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, char *list, size_t size)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        return SMB_VFS_NEXT_LLISTXATTR(handle, conn, realPath, list, size);
    }
    else
    {
        return SMB_VFS_NEXT_LLISTXATTR(handle, conn, path, list, size);
    }
}

static int mxfh_removexattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_REMOVEXATTR(handle, conn, path, name);
    }
}

static int mxfh_lremovexattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_LREMOVEXATTR(handle, conn, path, name);
    }
}

static int mxfh_setxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, const void *value, size_t size, int flags)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_SETXATTR(handle, conn, path, name, value, size, flags);
    }
}

static int mxfh_lsetxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, const void *value, size_t size, int flags)
{
    pstring realPath;
    if (is_virtual_mxf_file(path, &realPath))
    {
        errno = ENOTSUP;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_LSETXATTR(handle, conn, path, name, value, size, flags);
    }
}





/* VFS operations structure */

static vfs_op_tuple mxfh_op_tuples[] = 
{

    /* Disk operations */
    {SMB_VFS_OP(mxfh_connect),            SMB_VFS_OP_CONNECT,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_disconnect),         SMB_VFS_OP_DISCONNECT,  SMB_VFS_LAYER_TRANSPARENT},

    /* Directory operations */

    {SMB_VFS_OP(mxfh_opendir),            SMB_VFS_OP_OPENDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_readdir),            SMB_VFS_OP_READDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_seekdir),            SMB_VFS_OP_SEEKDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_telldir),            SMB_VFS_OP_TELLDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_rewinddir),          SMB_VFS_OP_REWINDDIR,   SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_closedir),           SMB_VFS_OP_CLOSEDIR,    SMB_VFS_LAYER_TRANSPARENT},

    
    /* File operations */

    {SMB_VFS_OP(mxfh_open),               SMB_VFS_OP_OPEN,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_close),              SMB_VFS_OP_CLOSE,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_read),               SMB_VFS_OP_READ,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_pread),              SMB_VFS_OP_PREAD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_write),              SMB_VFS_OP_WRITE,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_pwrite),             SMB_VFS_OP_PWRITE,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_lseek),              SMB_VFS_OP_LSEEK,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_rename),             SMB_VFS_OP_RENAME,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_fsync),              SMB_VFS_OP_FSYNC,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_stat),               SMB_VFS_OP_STAT,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_fstat),              SMB_VFS_OP_FSTAT,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_lstat),              SMB_VFS_OP_LSTAT,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_unlink),             SMB_VFS_OP_UNLINK,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_chmod),              SMB_VFS_OP_CHMOD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_fchmod),             SMB_VFS_OP_FCHMOD,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_chown),              SMB_VFS_OP_CHOWN,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_fchown),             SMB_VFS_OP_FCHOWN,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_chdir),              SMB_VFS_OP_CHDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_utime),              SMB_VFS_OP_UTIME,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_ftruncate),          SMB_VFS_OP_FTRUNCATE,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_lock),               SMB_VFS_OP_LOCK,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_symlink),            SMB_VFS_OP_SYMLINK,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_readlink),           SMB_VFS_OP_READLINK,    SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_link),               SMB_VFS_OP_LINK,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_mknod),              SMB_VFS_OP_MKNOD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_realpath),           SMB_VFS_OP_REALPATH,    SMB_VFS_LAYER_TRANSPARENT},

    /* NT File ACL operations */

    {SMB_VFS_OP(mxfh_fget_nt_acl),        SMB_VFS_OP_FGET_NT_ACL,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_get_nt_acl),         SMB_VFS_OP_GET_NT_ACL,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_fset_nt_acl),        SMB_VFS_OP_FSET_NT_ACL,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_set_nt_acl),         SMB_VFS_OP_SET_NT_ACL,      SMB_VFS_LAYER_TRANSPARENT},

    /* POSIX ACL operations */

    {SMB_VFS_OP(mxfh_chmod_acl),            SMB_VFS_OP_CHMOD_ACL,               SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_fchmod_acl),           SMB_VFS_OP_FCHMOD_ACL,      SMB_VFS_LAYER_TRANSPARENT},

    {SMB_VFS_OP(mxfh_sys_acl_get_file),           SMB_VFS_OP_SYS_ACL_GET_FILE,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_sys_acl_get_fd),             SMB_VFS_OP_SYS_ACL_GET_FD,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_sys_acl_set_file),           SMB_VFS_OP_SYS_ACL_SET_FILE,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_sys_acl_set_fd),             SMB_VFS_OP_SYS_ACL_SET_FD,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_sys_acl_delete_def_file),    SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE, SMB_VFS_LAYER_TRANSPARENT},
    
    /* EA operations. */
    {SMB_VFS_OP(mxfh_getxattr),           SMB_VFS_OP_GETXATTR,            SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_lgetxattr),          SMB_VFS_OP_LGETXATTR,           SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_listxattr),          SMB_VFS_OP_LISTXATTR,           SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_llistxattr),         SMB_VFS_OP_LLISTXATTR,          SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_removexattr),        SMB_VFS_OP_REMOVEXATTR,         SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_lremovexattr),       SMB_VFS_OP_LREMOVEXATTR,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_setxattr),           SMB_VFS_OP_SETXATTR,            SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mxfh_lsetxattr),          SMB_VFS_OP_LSETXATTR,           SMB_VFS_LAYER_TRANSPARENT},

    {NULL,                      SMB_VFS_OP_NOOP,            SMB_VFS_LAYER_NOOP}
};

NTSTATUS init_module(void)
{
    NTSTATUS ret =  smb_register_vfs(SMB_VFS_INTERFACE_VERSION, MXFH_MODULE_NAME, mxfh_op_tuples);

    if (!NT_STATUS_IS_OK(ret))
    {
        return ret;
    }

    vfs_mxfh_debug_level = debug_add_class(MXFH_MODULE_NAME);
    if (vfs_mxfh_debug_level == -1) 
    {
        vfs_mxfh_debug_level = DBGC_VFS;
        DEBUG(0, ("%s: Couldn't register custom debugging class!\n",
            "media_harmony::init_module"));
    } 
    else 
    {
        DEBUG(10, ("%s: Debug class number of '%s': %d\n", 
            "media_harmony::init_module", MXFH_MODULE_NAME, vfs_mxfh_debug_level));
    }

    return ret;
}


