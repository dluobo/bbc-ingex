/*
 * $Id: media_harmony.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Samba VFS module supporting multiple AVID clients sharing media.
 *
 * Copyright (C) 2005  Philip de Nier <philipn@users.sourceforge.net>
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
 
 
/*
 * Media Harmony is a Samba VFS module that allows multiple AVID clients to 
 * share media. Each client sees their own copy of the AVID msmMMOB.mdb and 
 * msmFMID.pmr files and Creating directories.
 *
 * Add this module to the vfs objects option in your Samba share configuration.
 * eg.
 *
 *   [avid_win]
 *        path = /video
 *        vfs objects = media_harmony
 *      ...
 *
 * It is recommended that you separate out Samba shares for Mac and Windows 
 * clients, and add the following options to the shares for Windows clients  
 * (NOTE: replace @ with *):
 *
 *        veto files = /.DS_Store/._@/.Trash@/.Spotlight@/.hidden/.hotfiles@/.vol/
 *        delete veto files = yes
 *
 * This prevents hidden files from Mac clients interfering with Windows 
 * clients. If you find any more problem hidden files then add them to the list. 
 *
 *
 *
 * Notes:
 * - This module is designed to work with AVID editing applications that look
 *   in the Avid MediaFiles or OMFI MediaFiles directory for media. It is 
 *   not designed to work as expected in all circumstances for general use. For 
 *   example: it is possibly to open client specific files such as 
 *   msmMMOB.mdb_192.168.1.10_userx even though is doesn't show up in a 
 *   directory listing.
 *
 */
 

#include "includes.h"

static int vfs_mh_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_mh_debug_level



static const char* MH_MODULE_NAME = "media_harmony";
static const char* MDB_FILENAME = "msmMMOB.mdb";
static const size_t MDB_FILENAME_LEN = 11;
static const char* PMR_FILENAME = "msmFMID.pmr";
static const size_t PMR_FILENAME_LEN = 11;
static const char* CREATING_DIRNAME = "Creating";
static const size_t CREATING_DIRNAME_LEN = 8;
static const char* AVID_MEDIAFILES_DIRNAME = "Avid MediaFiles";
static const size_t AVID_MEDIAFILES_DIRNAME_LEN = 15;
static const char* OMFI_MEDIAFILES_DIRNAME = "OMFI MediaFiles";
static const size_t OMFI_MEDIAFILES_DIRNAME_LEN = 15;
static const char* APPLE_DOUBLE_PREFIX = "._";
static const size_t APPLE_DOUBLE_PREFIX_LEN = 2;



// supplements the directory list stream
typedef struct mh_dirinfo_struct
{
    pstring dirpath;
    BOOL isInMediaFiles;
    fstring clientMDBFilename;
    fstring clientPMRFilename;
    fstring clientCreatingDirname;
    DIR* dirstream;
} mh_dirinfo_struct;


// Add "_<ip address>_<user name>" suffix to path 
static void pcat_client_suffix(vfs_handle_struct *handle, char* path)
{
    pstrcat(path, "_");
    pstrcat(path, handle->conn->client_address);
    pstrcat(path, "_");
    pstrcat(path, handle->conn->user);
}

// Add "_<ip address>_<user name>" suffix to filename 
static void fcat_client_suffix(vfs_handle_struct *handle, char* fname)
{
    fstrcat(fname, "_");
    fstrcat(fname, handle->conn->client_address);
    fstrcat(fname, "_");
    fstrcat(fname, handle->conn->user);
}

// Simple check for client suffix - 2 underscores means it probably is.
static BOOL has_client_suffix(const char* fname)
{
    const char* cptr;
    if ((cptr = strchr(fname, '_')) != NULL && strchr(cptr + 1, '_') != NULL)
    {
        return True;
    }
    return False; 
}

static BOOL is_apple_double(const char* fname)
{
    if (strncmp(APPLE_DOUBLE_PREFIX, fname, APPLE_DOUBLE_PREFIX_LEN) == 0)
    {
        return True;
    }
    else
    {
        return False;
    }
}

// Returns True if the file or directory referenced by the path is below
// the AVID_MEDIAFILES_DIRNAME or OMFI_MEDIAFILES_DIRNAME directory
// The AVID_MEDIAFILES_DIRNAME and OMFI_MEDIAFILES_DIRNAME are assumed to be in the
// root directory
static BOOL is_in_media_files(const char* path)
{
    if (strncmp(AVID_MEDIAFILES_DIRNAME, path, AVID_MEDIAFILES_DIRNAME_LEN) == 0 &&
        (path[AVID_MEDIAFILES_DIRNAME_LEN] == '\0' || path[AVID_MEDIAFILES_DIRNAME_LEN] == '/'))
    {
        return True;
    }
    else if (strncmp(OMFI_MEDIAFILES_DIRNAME, path, OMFI_MEDIAFILES_DIRNAME_LEN) == 0 &&
        (path[OMFI_MEDIAFILES_DIRNAME_LEN] == '\0' || path[OMFI_MEDIAFILES_DIRNAME_LEN] == '/'))
    {
        return True;
    }
    return False;
}

// Add client suffix to paths to MDB_FILENAME, PMR_FILENAME and CREATING_SUBDIRNAME
static void get_client_path(vfs_handle_struct *handle, const char* path, char* newPath)
{
    pstring intermPath;
    
    // replace /CREATING_DIRNAME/ directory in path with suffixed name
    // or replace /._CREATING_DIRNAME/ directory in path with suffixed name
    char* pathPtr;
    if ((pathPtr = strstr(path, CREATING_DIRNAME)) != NULL &&
        (*(pathPtr + CREATING_DIRNAME_LEN) == '\0' || *(pathPtr + CREATING_DIRNAME_LEN) == '/') &&
        (pathPtr - path > 0 && *(pathPtr - 1) == '/' ||
            pathPtr - path > APPLE_DOUBLE_PREFIX_LEN && 
            *(pathPtr - APPLE_DOUBLE_PREFIX_LEN - 1) == '/' &&
            is_apple_double(pathPtr - APPLE_DOUBLE_PREFIX_LEN)))
    {
        pstrcpy(intermPath, path);
        intermPath[pathPtr - path + CREATING_DIRNAME_LEN] = '\0';
        pcat_client_suffix(handle, intermPath);
        pstrcat(intermPath, pathPtr + CREATING_DIRNAME_LEN);
    }
    else
    {
        pstrcpy(intermPath, path);
    }
    
    // replace /MDB_FILENAME and /PMR_FILENAME in path end with suffixed name
    // or replace /._MDB_FILENAME and /._PMR_FILENAME in path end with suffixed name
    size_t intermPathLen = strlen(intermPath);
    if (intermPathLen > MDB_FILENAME_LEN && 
        strcmp(&intermPath[intermPathLen - MDB_FILENAME_LEN], MDB_FILENAME) == 0 &&
        (intermPath[intermPathLen - MDB_FILENAME_LEN - 1] == '/' ||
            (intermPathLen > MDB_FILENAME_LEN + APPLE_DOUBLE_PREFIX_LEN &&
            intermPath[intermPathLen - MDB_FILENAME_LEN - APPLE_DOUBLE_PREFIX_LEN - 1] == '/' &&
            is_apple_double(&intermPath[intermPathLen - MDB_FILENAME_LEN - APPLE_DOUBLE_PREFIX_LEN]))))
    {
        pstrcpy(newPath, intermPath);
        pcat_client_suffix(handle, newPath);
    }
    else if (intermPathLen > PMR_FILENAME_LEN && 
        strcmp(&intermPath[intermPathLen - PMR_FILENAME_LEN], PMR_FILENAME) == 0 &&
        (intermPath[intermPathLen - PMR_FILENAME_LEN - 1] == '/' ||
            (intermPathLen > PMR_FILENAME_LEN + APPLE_DOUBLE_PREFIX_LEN &&
            intermPath[intermPathLen - PMR_FILENAME_LEN - APPLE_DOUBLE_PREFIX_LEN - 1] == '/' &&
            is_apple_double(&intermPath[intermPathLen - PMR_FILENAME_LEN - APPLE_DOUBLE_PREFIX_LEN]))))
    {
        pstrcpy(newPath, intermPath);
        pcat_client_suffix(handle, newPath);
    }
    else
    {
        pstrcpy(newPath, intermPath);
    }
}



// returns a mh_dirinfo_struct cast as a DIR
static DIR *mh_opendir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    mh_dirinfo_struct* dirInfo = SMB_MALLOC_P(mh_dirinfo_struct);
    if (!dirInfo)
    {
        DEBUG(0, ("mh_opendir: Out of memory. Failed to malloc dirinfo.\n"));
        return NULL;
    }
    pstrcpy(dirInfo->dirpath, path);
    
    if (is_in_media_files(path))
    {
        dirInfo->isInMediaFiles = True;
        fstrcpy(dirInfo->clientMDBFilename, MDB_FILENAME);
        fcat_client_suffix(handle, dirInfo->clientMDBFilename);
        fstrcpy(dirInfo->clientPMRFilename, PMR_FILENAME);
        fcat_client_suffix(handle, dirInfo->clientPMRFilename);
        fstrcpy(dirInfo->clientCreatingDirname, CREATING_DIRNAME);
        fcat_client_suffix(handle, dirInfo->clientCreatingDirname);
        
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        dirInfo->dirstream = SMB_VFS_NEXT_OPENDIR(handle, conn, clientPath);
        
        return (DIR*)dirInfo;
    }
    else
    {
        dirInfo->isInMediaFiles = False;
        dirInfo->dirstream = SMB_VFS_NEXT_OPENDIR(handle, conn, path);
        
        return (DIR*)dirInfo;
    }
}

// skip MDB_FILENAME and PMR_FILENAME filenames and CREATING_DIRNAME directory, 
// skip other client's suffixed MDB_FILENAME and PMR_FILENAME filenames and CREATING_DIRNAME directory, 
// replace this client's suffixed MDB_FILENAME and PMR_FILENAME filenames and CREATING_DIRNAME directory
//   with non suffixed
static SMB_STRUCT_DIRENT *mh_readdir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp)
{
    mh_dirinfo_struct* dirInfo = (mh_dirinfo_struct*)dirp;
    
    if (dirInfo->isInMediaFiles)
    {
        int skip = False;
        SMB_STRUCT_DIRENT* d = NULL;
        do
        {
            skip = False;
            d = SMB_VFS_NEXT_READDIR(handle, conn, dirInfo->dirstream);
            if (d != NULL)
            {
                const char* dname;
                BOOL isAppleDouble;
                
                // ignore apple double prefix for logic below
                if (is_apple_double(d->d_name))
                {
                    dname = &d->d_name[APPLE_DOUBLE_PREFIX_LEN];
                    isAppleDouble = True;
                }
                else
                {
                    dname = d->d_name;
                    isAppleDouble = False;
                }
                
                // skip non client-suffixed files
                if (strcmp(dname, MDB_FILENAME) == 0 ||
                    strcmp(dname, PMR_FILENAME) == 0 ||
                    strcmp(dname, CREATING_DIRNAME) == 0)
                {
                    skip = True;
                }
                // rename this client's suffixed files
                else if (strcmp(dname, dirInfo->clientMDBFilename) == 0)
                {
                    if (isAppleDouble)
                    {
                        fstrcpy(d->d_name, APPLE_DOUBLE_PREFIX);
                        fstrcat(d->d_name, MDB_FILENAME);
                    }
                    else
                    {
                        fstrcpy(d->d_name, MDB_FILENAME);
                    }
                }
                else if (strcmp(dname, dirInfo->clientPMRFilename) == 0)
                {
                    if (isAppleDouble)
                    {
                        fstrcpy(d->d_name, APPLE_DOUBLE_PREFIX);
                        fstrcat(d->d_name, PMR_FILENAME);
                    }
                    else
                    {
                        fstrcpy(d->d_name, PMR_FILENAME);
                    }
                }
                else if (strcmp(dname, dirInfo->clientCreatingDirname) == 0)
                {
                    if (isAppleDouble)
                    {
                        fstrcpy(d->d_name, APPLE_DOUBLE_PREFIX);
                        fstrcat(d->d_name, CREATING_DIRNAME);
                    }
                    else
                    {
                        fstrcpy(d->d_name, CREATING_DIRNAME);
                    }
                }
                // skip other client's suffixed files
                else if ((strncmp(MDB_FILENAME, dname, MDB_FILENAME_LEN) == 0 ||
                        strncmp(PMR_FILENAME, dname, PMR_FILENAME_LEN) == 0 ||
                        strncmp(CREATING_DIRNAME, dname, CREATING_DIRNAME_LEN) == 0) &&
                    has_client_suffix(dname))
                {
                    skip = True;
                }
            }
        }
        while (d != NULL && skip);
        
        return d;
    }
    else
    {
        return SMB_VFS_NEXT_READDIR(handle, conn, dirInfo->dirstream);
    }
}

static void mh_seekdir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp, long offset)
{
    return SMB_VFS_NEXT_SEEKDIR(handle, conn, ((mh_dirinfo_struct*)dirp)->dirstream, offset);
}

static long mh_telldir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp)
{
    return SMB_VFS_NEXT_TELLDIR(handle, conn, ((mh_dirinfo_struct*)dirp)->dirstream);
}

static void mh_rewinddir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp)
{
    return SMB_VFS_NEXT_REWINDDIR(handle, conn, ((mh_dirinfo_struct*)dirp)->dirstream);
}

static int mh_mkdir(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_MKDIR(handle, conn, clientPath, mode);
    }
    else
    {
        return SMB_VFS_NEXT_MKDIR(handle, conn, path, mode);
    }
}

static int mh_rmdir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_RMDIR(handle, conn, clientPath);
    }
    else
    {
        return SMB_VFS_NEXT_RMDIR(handle, conn, path);
    }
}

static int mh_closedir(vfs_handle_struct *handle, connection_struct *conn, DIR *dirp)
{
    DIR* realdirp = ((mh_dirinfo_struct*)dirp)->dirstream;
    SAFE_FREE(dirp);
    
    return SMB_VFS_NEXT_CLOSEDIR(handle, conn, realdirp);
}

static int mh_open(vfs_handle_struct *handle, connection_struct *conn, const char *path, int flags, mode_t mode)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_OPEN(handle, conn, clientPath, flags, mode);
    }
    else
    {
        return SMB_VFS_NEXT_OPEN(handle, conn, path, flags, mode);
    }
}

static int mh_rename(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    if (is_in_media_files(oldpath) || is_in_media_files(newpath))
    {
        pstring oldClientPath;
        pstring newClientPath;
        get_client_path(handle, oldpath, oldClientPath);
        get_client_path(handle, newpath, newClientPath);
        
        return SMB_VFS_NEXT_RENAME(handle, conn, oldClientPath, newClientPath);
    }
    else
    {
        return SMB_VFS_NEXT_RENAME(handle, conn, oldpath, newpath);
    }
}

static int mh_stat(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_STRUCT_STAT *sbuf)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_STAT(handle, conn, clientPath, sbuf);
    }
    else
    {
        return SMB_VFS_NEXT_STAT(handle, conn, path, sbuf);
    }
}

static int mh_lstat(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_STRUCT_STAT *sbuf)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_LSTAT(handle, conn, clientPath, sbuf);
    }
    else
    {
        return SMB_VFS_NEXT_LSTAT(handle, conn, path, sbuf);
    }
}

static int mh_unlink(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_UNLINK(handle, conn, clientPath);
    }
    else
    {
        return SMB_VFS_NEXT_UNLINK(handle, conn, path);
    }    
}

static int mh_chmod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_CHMOD(handle, conn, clientPath, mode);
    }
    else
    {
        return SMB_VFS_NEXT_CHMOD(handle, conn, path, mode);
    }
}

static int mh_chown(vfs_handle_struct *handle, connection_struct *conn, const char *path, uid_t uid, gid_t gid)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_CHOWN(handle, conn, clientPath, uid, gid);
    }
    else
    {
        return SMB_VFS_NEXT_CHOWN(handle, conn, path, uid, gid);
    }
}

static int mh_chdir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_CHDIR(handle, conn, clientPath);
    }
    else
    {
        return SMB_VFS_NEXT_CHDIR(handle, conn, path);
    }
}

static int mh_utime(vfs_handle_struct *handle, connection_struct *conn, const char *path, struct utimbuf *times)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_UTIME(handle, conn, clientPath, times);
    }
    else
    {
        return SMB_VFS_NEXT_UTIME(handle, conn, path, times);
    }
}

static BOOL mh_symlink(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    if (is_in_media_files(oldpath) || is_in_media_files(newpath))
    {
        pstring oldClientPath;
        pstring newClientPath;
        get_client_path(handle, oldpath, oldClientPath);
        get_client_path(handle, newpath, newClientPath);
        
        return SMB_VFS_NEXT_SYMLINK(handle, conn, oldClientPath, newClientPath);
    }
    else
    {
        return SMB_VFS_NEXT_SYMLINK(handle, conn, oldpath, newpath);
    }
}

static BOOL mh_readlink(vfs_handle_struct *handle, connection_struct *conn, const char *path, char *buf, size_t bufsiz)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_READLINK(handle, conn, clientPath, buf, bufsiz);
    }
    else
    {
        return SMB_VFS_NEXT_READLINK(handle, conn, path, buf, bufsiz);
    }
}

static int mh_link(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    if (is_in_media_files(oldpath) || is_in_media_files(newpath))
    {
        pstring oldClientPath;
        pstring newClientPath;
        get_client_path(handle, oldpath, oldClientPath);
        get_client_path(handle, newpath, newClientPath);
        
        return SMB_VFS_NEXT_LINK(handle, conn, oldClientPath, newClientPath);
    }
    else
    {
        return SMB_VFS_NEXT_LINK(handle, conn, oldpath, newpath);
    }
}

static int mh_mknod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode, SMB_DEV_T dev)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_MKNOD(handle, conn, clientPath, mode, dev);
    }
    else
    {
        return SMB_VFS_NEXT_MKNOD(handle, conn, path, mode, dev);
    }
}

static char *mh_realpath(vfs_handle_struct *handle, connection_struct *conn, const char *path, char *resolved_path)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_REALPATH(handle, conn, clientPath, resolved_path);
    }
    else
    {
        return SMB_VFS_NEXT_REALPATH(handle, conn, path, resolved_path);
    }
}

static size_t mh_get_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *path, uint32 security_info, struct security_descriptor_info **ppdesc)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, clientPath, security_info, ppdesc);
    }
    else
    {
        return SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, path, security_info, ppdesc);
    }
}

static BOOL mh_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *path, uint32 security_info_sent, struct security_descriptor_info *psd)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
    
        return SMB_VFS_NEXT_SET_NT_ACL(handle, fsp, clientPath, security_info_sent, psd);
    }
    else
    {
        return SMB_VFS_NEXT_SET_NT_ACL(handle, fsp, path, security_info_sent, psd);
    }
}

static int mh_chmod_acl(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_CHMOD_ACL(handle, conn, clientPath, mode);
    }
    else
    {
        return SMB_VFS_NEXT_CHMOD_ACL(handle, conn, path, mode);
    }
}

static SMB_ACL_T mh_sys_acl_get_file(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_ACL_TYPE_T type)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, conn, clientPath, type);
    }
    else
    {
        return SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, conn, path, type);
    }
}

static int mh_sys_acl_set_file(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
     
        return SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, conn, clientPath, acltype, theacl);
    }
    else
    {
        return SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, conn, path, acltype, theacl);
    }
}

static int mh_sys_acl_delete_def_file(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, conn, clientPath);
    }
    else
    {
        return SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, conn, path);
    }
}

static ssize_t mh_getxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, void *value, size_t size)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_GETXATTR(handle, conn, clientPath, name, value, size);
    }
    else
    {
        return SMB_VFS_NEXT_GETXATTR(handle, conn, path, name, value, size);
    }
}

static ssize_t mh_lgetxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, void *value, size_t
size)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_LGETXATTR(handle, conn, clientPath, name, value, size);
    }
    else
    {
        return SMB_VFS_NEXT_LGETXATTR(handle, conn, path, name, value, size);
    }
}

static ssize_t mh_listxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, char *list, size_t size)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_LISTXATTR(handle, conn, clientPath, list, size);
    }
    else
    {
        return SMB_VFS_NEXT_LISTXATTR(handle, conn, path, list, size);
    }
}

static ssize_t mh_llistxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, char *list, size_t size)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_LLISTXATTR(handle, conn, clientPath, list, size);
    }
    else
    {
        return SMB_VFS_NEXT_LLISTXATTR(handle, conn, path, list, size);
    }
}

static int mh_removexattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_REMOVEXATTR(handle, conn, clientPath, name);
    }
    else
    {
        return SMB_VFS_NEXT_REMOVEXATTR(handle, conn, path, name);
    }
}

static int mh_lremovexattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_LREMOVEXATTR(handle, conn, clientPath, name);
    }
    else
    {
        return SMB_VFS_NEXT_LREMOVEXATTR(handle, conn, path, name);
    }
}

static int mh_setxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, const void *value, size_t size, int flags)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_SETXATTR(handle, conn, clientPath, name, value, size, flags);
    }
    else
    {
        return SMB_VFS_NEXT_SETXATTR(handle, conn, path, name, value, size, flags);
    }
}

static int mh_lsetxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, const void *value, size_t size, int flags)
{
    if (is_in_media_files(path))
    {
        pstring clientPath;
        get_client_path(handle, path, clientPath);
        
        return SMB_VFS_NEXT_LSETXATTR(handle, conn, clientPath, name, value, size, flags);
    }
    else
    {
        return SMB_VFS_NEXT_LSETXATTR(handle, conn, path, name, value, size, flags);
    }
}


/* VFS operations structure */

static vfs_op_tuple mh_op_tuples[] = {

    /* Disk operations */

    /* Directory operations */

    {SMB_VFS_OP(mh_opendir),            SMB_VFS_OP_OPENDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_readdir),            SMB_VFS_OP_READDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_seekdir),            SMB_VFS_OP_SEEKDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_telldir),            SMB_VFS_OP_TELLDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_rewinddir),          SMB_VFS_OP_REWINDDIR,   SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_mkdir),              SMB_VFS_OP_MKDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_rmdir),              SMB_VFS_OP_RMDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_closedir),           SMB_VFS_OP_CLOSEDIR,    SMB_VFS_LAYER_TRANSPARENT},

    
    /* File operations */

    {SMB_VFS_OP(mh_open),               SMB_VFS_OP_OPEN,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_rename),             SMB_VFS_OP_RENAME,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_stat),               SMB_VFS_OP_STAT,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_lstat),              SMB_VFS_OP_LSTAT,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_unlink),             SMB_VFS_OP_UNLINK,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_chmod),              SMB_VFS_OP_CHMOD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_chown),              SMB_VFS_OP_CHOWN,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_chdir),              SMB_VFS_OP_CHDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_utime),              SMB_VFS_OP_UTIME,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_symlink),            SMB_VFS_OP_SYMLINK,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_readlink),           SMB_VFS_OP_READLINK,    SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_link),               SMB_VFS_OP_LINK,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_mknod),              SMB_VFS_OP_MKNOD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_realpath),           SMB_VFS_OP_REALPATH,    SMB_VFS_LAYER_TRANSPARENT},

    /* NT File ACL operations */

    {SMB_VFS_OP(mh_get_nt_acl),         SMB_VFS_OP_GET_NT_ACL,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_set_nt_acl),         SMB_VFS_OP_SET_NT_ACL,      SMB_VFS_LAYER_TRANSPARENT},

    /* POSIX ACL operations */

    {SMB_VFS_OP(mh_chmod_acl),                  SMB_VFS_OP_CHMOD_ACL,               SMB_VFS_LAYER_TRANSPARENT},

    {SMB_VFS_OP(mh_sys_acl_get_file),           SMB_VFS_OP_SYS_ACL_GET_FILE,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_sys_acl_set_file),           SMB_VFS_OP_SYS_ACL_SET_FILE,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_sys_acl_delete_def_file),    SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE, SMB_VFS_LAYER_TRANSPARENT},
    
    /* EA operations. */
    {SMB_VFS_OP(mh_getxattr),           SMB_VFS_OP_GETXATTR,            SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_lgetxattr),          SMB_VFS_OP_LGETXATTR,           SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_listxattr),          SMB_VFS_OP_LISTXATTR,           SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_llistxattr),         SMB_VFS_OP_LLISTXATTR,          SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_removexattr),        SMB_VFS_OP_REMOVEXATTR,         SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_lremovexattr),       SMB_VFS_OP_LREMOVEXATTR,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_setxattr),           SMB_VFS_OP_SETXATTR,            SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mh_lsetxattr),          SMB_VFS_OP_LSETXATTR,           SMB_VFS_LAYER_TRANSPARENT},

    {NULL,                      SMB_VFS_OP_NOOP,            SMB_VFS_LAYER_NOOP}
};

NTSTATUS init_module(void)
{
    NTSTATUS ret =  smb_register_vfs(SMB_VFS_INTERFACE_VERSION, MH_MODULE_NAME, mh_op_tuples);

    if (!NT_STATUS_IS_OK(ret))
    {
        return ret;
    }

    vfs_mh_debug_level = debug_add_class(MH_MODULE_NAME);
    if (vfs_mh_debug_level == -1) 
    {
        vfs_mh_debug_level = DBGC_VFS;
        DEBUG(0, ("%s: Couldn't register custom debugging class!\n",
            "media_harmony::init_module"));
    } 
    else 
    {
        DEBUG(10, ("%s: Debug class number of '%s': %d\n", 
            "media_harmony::init_module", MH_MODULE_NAME, vfs_mh_debug_level));
    }

    return ret;
}


