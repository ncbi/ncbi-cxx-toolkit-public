#ifndef CORELIB___NCBIFILE__HPP
#define CORELIB___NCBIFILE__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors: Vladimir Ivanov, Denis Vakatov
 *
 *
 */

/// @file ncbifile.hpp
///
/// Defines classes:
///   CDirEntry, CFile, CDir, CSymLink,
///   CMemoryFile,
///   CFileUtil,
///   CFileLock,
///   CFileIO,
///   CFileReader, CFileWriter, CFileReaderWriter,
///   CFileException.
///   Defines different file finding algorithms.

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_mask.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/reader_writer.hpp>

#include <sys/types.h>
#if defined(HAVE_SYS_STAT_H)
#  include <sys/stat.h>
#endif

#include <limits.h>
#include <stdlib.h>
#if defined(NCBI_OS_UNIX)
#  include <sys/param.h>
#endif
#if defined(NCBI_OS_MSWIN)
#  include <stdio.h>       // for FILENAME_MAX
#endif


/** @addtogroup Files
 *
 * @{
 */

BEGIN_NCBI_SCOPE


// Define missing types

// mode_t
#if defined(NCBI_OS_MSWIN)
    typedef unsigned int mode_t;
#endif

// FILENAME_MAX
#if !defined(FILENAME_MAX)
#  if defined(MAXNAMELEN)
#    define FILENAME_MAX MAXNAMELEN    /* in <sys/param.h> on some systems */
#  elif defined(_MAX_FNAME)
#    define FILENAME_MAX _MAX_FNAME    /* MS Windows */
#  else
#    define FILENAME_MAX 256
#  endif
#endif

// PATH_MAX
#if !defined(PATH_MAX)
#  if defined(MAXPATHLEN)
#    define PATH_MAX MAXPATHLEN        /* in <sys/param.h> on some systems */
#  elif defined(_MAX_PATH)
#    define PATH_MAX _MAX_PATH         /* MS Windows */
#  else
#    if FILENAME_MAX > 255
#      define PATH_MAX FILENAME_MAX
#    else
#      define PATH_MAX 1024
#    endif
#  endif
#endif

// File handle
#if defined(NCBI_OS_MSWIN)
    typedef HANDLE TFileHandle;
    const   HANDLE kInvalidHandle = INVALID_HANDLE_VALUE;
#else
    typedef int TFileHandle;
    const   int kInvalidHandle = -1;
#endif

// Forward declaration of struct containing OS-specific lock storage.
struct SLock;

// TNcbiSys_stat type
#if defined(NCBI_OS_MSWIN)
typedef struct _stat64 TNcbiSys_stat;
#else
typedef struct stat    TNcbiSys_stat;
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// CFileException --
///
/// Define exceptions generated for file operations.
///
/// CFileException inherits its basic functionality from CCoreException
/// and defines additional error codes for file operations.

class NCBI_XNCBI_EXPORT CFileException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eMemoryMap,
        eRelativePath,
        eNotExists,
        eFileIO,
        eTmpFile
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CFileException, CCoreException);
};


// File exception with system errno-based message

#if defined(NCBI_OS_MSWIN)
    typedef CErrnoTemplException_Win<CFileException> CFileErrnoException_Base;
#else
    typedef CErrnoTemplException<CFileException> CFileErrnoException_Base;
#endif

class NCBI_XNCBI_EXPORT CFileErrnoException : public CFileErrnoException_Base
{
public:
    /// Error types
    enum EErrCode {
        eFileSystemInfo,
        eFileLock,
        eFileIO
    };
    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;
    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CFileErrnoException, CFileErrnoException_Base);
};


/////////////////////////////////////////////////////////////////////////////
///
/// Global settings related to the file API.
///
///

class NCBI_XNCBI_EXPORT CFileAPI
{
public:
    /// Enable or disable logging of errors.
    ///
    /// @param on_off_default
    ///   Switch between logging enabled (eOn), disabled (eOff),
    ///   or reset to the default state (currently disabled).
    static void SetLogging(ESwitch on_off_default);

    /// Enable or disable honoring umask settings on Unix for
    /// newly created files/directories in the File API.
    ///
    /// @param on_off_default
    ///   When set to eOn, allows read-only files to be deleted.
    ///   Otherwise, an attempt to delete a read-only files will
    ///   return an error (EACCES).
    /// @note
    ///   Unix only.
    static void SetHonorUmask(ESwitch on_off_default);

    /// Specify whether read-only files can be deleted via
    /// CDirEntry::Remove() on Windows.
    ///
    /// @param on_off_default
    ///   When set to eOn, allows read-only files to be deleted.
    ///   Otherwise, an attempt to delete a read-only files will
    ///   return an error (EACCES).
    /// @note
    ///   Windows only.
    static void SetDeleteReadOnlyFiles(ESwitch on_off_default);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDirEntry --
///
/// Base class to work with files and directories.
///
/// Models a directory entry in the file system.  Assumes that
/// the path argument has the following form, where any or
/// all components may be missing:
///
/// <dir><base><ext>
///
/// - dir  - file path             ("/usr/local/bin/"  or  "c:\\windows\\")
/// - base - file name without ext ("autoexec")
/// - ext  - file extension        (".bat" - whatever goes after a last dot)
///
/// Supported filename formats:  MS DOS/Windows, UNIX.

class NCBI_XNCBI_EXPORT CDirEntry
{
public:
    /// Default constructor.
    CDirEntry(void);

    /// Constructor using specified path string.
    CDirEntry(const string& path);

    /// Copy constructor.
    CDirEntry(const CDirEntry& other);

    /// Destructor.
    virtual ~CDirEntry(void);

    /// Get entry path.
    const string& GetPath(void) const;

    /// Reset path string.
    void Reset(const string& path);

    /// Assignment operator.
    CDirEntry& operator= (const CDirEntry& other);


    //
    // Path processing.
    //

    /// Split a path string into its basic components.
    ///
    /// @param path
    ///   Path string to be split.
    /// @param dir
    ///   The directory component that is returned. This will always have
    ///   a terminating path separator (example: "/usr/local/").
    /// @param base
    ///   File name with both directory (if any) and extension (if any)
    ///   parts stripped.
    /// @param ext
    ///   The extension component (if any), always has a leading dot
    ///   (example: ".bat").
    static void SplitPath(const string& path,
                          string* dir = 0, string* base = 0, string* ext = 0);

    /// Split a path string into its basic components.
    ///
    /// Note that the arguments are not OS-specific.
    /// @param path
    ///   Path string to be split.
    /// @param disk
    ///   Disk name if present like "C:" (MS Windows paths only),
    ///   otherwise empty string.
    /// @param dir
    ///   The directory component that is returned. This will always have
    ///   a terminating path separator (example: "/usr/local/").
    /// @param base
    ///   File name with both directory (if any) and extension (if any)
    ///   parts stripped.
    /// @param ext
    ///   The extension component (if any), always has a leading dot
    ///   (example: ".bat").
    static void SplitPathEx(const string& path,
                            string* disk = 0, string* dir = 0,
                            string* base = 0, string* ext = 0);

    /// What GetDir() should return if the dir entry does not contains path.
    /// @sa GetDir
    enum EIfEmptyPath {
        eIfEmptyPath_Empty,    ///< Return empty string
        eIfEmptyPath_Current   ///< Return current dir like "./"
    };

    /// Get the directory component for this directory entry.
    //
    /// @param path
    ///   Flag to control returning value for paths that don't have
    ///   directory name. 
    /// @return
    ///   The directory component for this directory entry, or empty string.
    /// @sa EGetDirMode, SplitPath
    string GetDir(EIfEmptyPath mode = eIfEmptyPath_Current) const;

    /// Get the base entry name with extension (if any).
    string GetName(void) const;

    /// Get the base entry name without extension.
    string GetBase(void) const;

    /// Get extension name.
    string GetExt (void) const;

    /// Assemble a path from basic components.
    ///
    /// @param dir
    ///   The directory component to make the path string. This will always
    ///   have a terminating path separator (example: "/usr/local/").
    /// @param base
    ///   The base name of the file component that is used to make up the path.
    /// @param ext
    ///   The extension component. This will always be added with a leading dot
    ///   (input of either "bat" or ".bat" gets added to the path as ".bat").
    /// @return
    ///   Path built from the components.
    static string MakePath(const string& dir  = kEmptyStr,
                           const string& base = kEmptyStr,
                           const string& ext  = kEmptyStr);

    /// Get path separator symbol specific for the current platform.
    static char GetPathSeparator(void);

    /// Check whether a character "c" is a path separator symbol
    /// specific for the current platform.
    static bool IsPathSeparator(const char c);

    /// Add trailing path separator, if needed.
    static string AddTrailingPathSeparator(const string& path);

    /// Delete trailing path separator, if any.
    static string DeleteTrailingPathSeparator(const string& path);

    /// Convert relative "path" on any OS to the current
    /// OS-dependent relative path. 
    static string ConvertToOSPath(const string& path);

    /// Check if a "path" is absolute for the current OS.
    ///
    /// Note that the "path" must be for the current OS. 
    static bool IsAbsolutePath(const string& path);

    /// Check if the "path" is absolute for any OS.
    ///
    /// Note that the "path" can be for any OS (MSWIN, UNIX).
    static bool IsAbsolutePathEx(const string& path);

    /// Create a relative path between two points in the file
    /// system specified by their absolute paths.
    ///
    /// @param path_from 
    ///   Absolute path that defines start of the relative path.
    /// @param path_to
    ///   Absolute path that defines endpoint of the relative path.
    /// @return
    ///   Relative path (empty string if the paths are the same).
    ///   Throw CFileException on error (e.g. if any of the paths is not
    ///   absolute, or if it is impossible to create a relative path, such
    ///   as in case of different disks on MS-Windows).
    static string CreateRelativePath(const string& path_from, 
                                     const string& path_to);

    /// How to interpret relative paths.
    /// @sa CreateAbsolutePath
    enum ERelativeToWhat {
        eRelativeToCwd, ///< Relative to the current working directory
        /// Relative to the executable's location.  If the executable was
        /// invoked via a symlink, search the directory containing the symlink
        /// before the directory (if different) containing the actual binary.
        eRelativeToExe  
    };

    /// Get an absolute path from some, possibly relative, path.
    ///
    /// @param path 
    ///   Path to resolve, in native syntax; returned as is if absolute.
    /// @param rtw
    ///   Starting point for relative path resolution -- the current directory
    ///   by default, but looking alongside the executable is also an option.
    /// @return
    ///   Corresponding absolute path.  May be the original string (if already
    ///   absolute) or the starting point indicated by rtw (if the input was
    ///   empty or ".").
    /// @sa ERelativeToWhat
    static string CreateAbsolutePath(const string& path,
                                     ERelativeToWhat rtw = eRelativeToCwd);

    /// Concatenate two parts of the path for the current OS.
    ///
    /// Note that the arguments must be OS-specific.
    /// @param first
    ///   First part of the path which can be either absolute or relative.
    /// @param second
    ///   Second part of the path must always be relative.
    /// @return
    ///   The concatenated path.
    static string ConcatPath(const string& first, const string& second);

    /// Concatenate two parts of the path for any OS.
    ///
    /// Note that the arguments are not OS-specific.
    /// @param first
    ///   First part of the path which can be either absolute or relative.
    /// @param second
    ///   Second part of the path must always be relative.
    /// @return
    ///   The concatenated path.
    static string ConcatPathEx(const string& first, const string& second);

    /// Normalize a path.
    ///
    /// Remove from an input "path" all redundancy, and if possible,
    /// convert it to more simple form for the current OS.
    /// Note that "path" must be OS-specific.
    /// @param follow_links
    ///   Whether to follow symlinks (shortcuts, aliases)
    static string NormalizePath(const string& path,
                                EFollowLinks  follow_links = eIgnoreLinks);


    //
    // Checks & manipulations.
    //

    /// Match a "name" against a simple filename "mask".
    static bool MatchesMask(const string& name, const string& mask,
                            NStr::ECase use_case = NStr::eCase);

    /// Match a "name" against a set of "masks"
    /// Note that any name match to empty vector of masks.
    static bool MatchesMask(const string& name, const vector<string>& masks,
                            NStr::ECase use_case = NStr::eCase);

    /// Match a "name" against a set of "masks"
    /// Note that any name match to empty set of masks.
    static bool MatchesMask(const string& name, const CMask& mask,
                            NStr::ECase use_case = NStr::eCase);

    /// Check the entry existence.
    virtual bool Exists(void) const;

    /// Copy flags.
    /// Note that update modification time for directory depends on the OS.
    /// Normally it gets updated when a new directory entry is added/removed.
    /// On the other hand, changing contents of files of that directory
    /// doesn't usually affect the directory modification time.
    enum ECopyFlags {
        /// The following three flags define what to do when the
        /// destination entry already exists:
        /// - Overwrite the destination
        fCF_Overwrite       = (1<< 1), 
        /// - Update older entries only (compare modification times).
        fCF_Update          = (1<< 2) | fCF_Overwrite,
        /// - Backup destination if it exists.
        fCF_Backup          = (1<< 3) | fCF_Overwrite,
        /// All above flags can be applied to the top directory only
        /// (not for every file therein), to process the directory
        /// as a single entity for overwriting, updating or backing up.
        fCF_TopDirOnly      = (1<< 6),
        /// If destination entry exists, it must have the same type as source.
        fCF_EqualTypes      = (1<< 7),
        /// Copy entries following their sym.links, not the links themselves.
        fCF_FollowLinks     = (1<< 8),
        fCF_Verify          = (1<< 9),  ///< Verify data after copying
        fCF_PreserveOwner   = (1<<10),  ///< Preserve owner/group
        fCF_PreservePerm    = (1<<11),  ///< Preserve permissions/attributes
        fCF_PreserveTime    = (1<<12),  ///< Preserve date/times
        fCF_PreserveAll     = fCF_PreserveOwner | fCF_PreservePerm |
                              fCF_PreserveTime,
        fCF_Recursive       = (1<<14),  ///< Copy recursively (for dir only)
        /// Skip all entries for which we don't have Copy() method.
        fCF_SkipUnsupported = (1<<15),
        /// Default flags.
        fCF_Default         = fCF_Recursive | fCF_FollowLinks
    };
    typedef unsigned int TCopyFlags;    ///< Binary OR of "ECopyFlags"

    /// Copy the entry to a location specified by "new_path".
    ///
    /// The Copy() method must be overloaded in derived classes
    /// that support copy operation.
    /// @param new_path
    ///   New path/name of an entry.
    /// @param flags
    ///   Flags specifying how to copy the entry.
    /// @param buf_size
    ///   Buffer size to use while copying the file contents.
    ///   Zero value means using default buffer size. This parameter
    ///   have advisory status and can be overrided, depends from OS
    ///   and size of copied file.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa
    ///   CFile::Copy, CDir::Copy, CLink::Copy, CopyToDir
    virtual bool Copy(const string& new_path, TCopyFlags flags = fCF_Default,
                      size_t buf_size = 0) const;

    /// Copy the entry to a specified directory.
    ///
    /// The target entry name will be "dir/entry".
    /// @param dir
    ///   Directory name to copy into.
    /// @param flags
    ///   Flags specifying how to copy the entry.
    /// @param buf_size
    ///   Buffer size to use while copying the file contents.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa
    ///   Copy
    bool CopyToDir(const string& dir, TCopyFlags flags = fCF_Default,
                   size_t buf_size = 0) const;

    /// Rename flags
    enum ERenameFlags {
        /// Remove destination if it exists.
        fRF_Overwrite   = (1<<1),
        /// Update older entries only (compare modification times).
        fRF_Update      = (1<<2) | fCF_Overwrite,
        /// Backup destination if it exists before renaming.
        fRF_Backup      = (1<<3) | fCF_Overwrite,
        /// If destination entry exists, it must have the same type as source.
        fRF_EqualTypes  = (1<<4),
        /// Rename entries following sym.links, not the links themselves.
        fRF_FollowLinks = (1<<5),
        /// Default flags
        fRF_Default     = 0
    };
    typedef unsigned int TRenameFlags;   ///< Binary OR of "ERenameFlags"

    /// Rename entry.
    ///
    /// @param new_path
    ///   New path/name of an entry.
    /// @param flags
    ///   Flags specifying how to rename the entry.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    ///   NOTE that if flag fRF_Update is set, the function returns TRUE and
    ///   just removes the current entry in case when destination entry
    ///   exists and has its modification time newer than the current entry.
    /// @sa
    ///   ERenameFlags, Copy
    bool Rename(const string& new_path, TRenameFlags flags = fRF_Default);

    /// Move the entry to a specified directory.
    ///
    /// The target entry name will be "dir/entry".
    /// @param dir
    ///   Directory name to move into.
    /// @param flags
    ///   Flags specifying how to move the entry.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa
    ///   CopyToDir, Rename, Copy
    bool MoveToDir(const string& dir, TCopyFlags flags = fRF_Default);

    /// Get backup suffix.
    ///
    /// @sa
    ///   SetBackupSuffix, Backup, Rename, Copy
    static const char* GetBackupSuffix(void);

    /// Set backup suffix.
    ///
    /// @sa
    ///   GetBackupSuffix, Backup, Rename, Copy
    static void SetBackupSuffix(const char* suffix);

    /// Backup modes
    enum EBackupMode {
        eBackup_Copy    = (1<<1),       ///< Copy entry
        eBackup_Rename  = (1<<2),       ///< Rename entry
        eBackup_Default = eBackup_Copy  ///< Default mode
    };

    /// Backup an entry.
    ///
    /// Create a copy of the current entry with the same name and an extension
    /// specified by SetBackupSuffix(). By default this extension is ".bak".
    /// Backups can be automatically created in 'copy' or 'rename' operations.
    /// If an entry with the name of the backup already exists, then it will
    /// be deleted (if possible). The current entry name components are
    /// changed to reflect the backed up copy.
    /// @param suffix
    ///   Extension to add to backup entry. If empty, GetBackupSuffix()
    ///   is be used.
    /// @param mode
    ///   Backup mode. Specifies what to do, copy the entry or just rename it.
    /// @param copyflags
    ///   Flags to copy the entry. Used only if mode is eBackup_Copy,
    /// @param copybufsize
    ///   Buffer size to use while copying the file contents.
    ///   Used only if 'mode' is eBackup_Copy,
    /// @return
    ///   TRUE if backup created successfully; FALSE otherwise.
    /// @sa
    ///   EBackupMode
    bool Backup(const string& suffix      = kEmptyStr, 
                EBackupMode   mode        = eBackup_Default,
                TCopyFlags    copyflags   = fCF_Default,
                size_t        copybufsize = 0);


    /// Entries processing flags.
    /// @note
    ///   Different methods use different sets of flags,
    ///   and ignore all other. Refer to method descriptions for details.
    enum EProcessingFlags {
   
        /// Current entry only
        fEntry         = (1 << 0),

        // --- directory processing flags ---------------------------
        
        fDir_Self      = fEntry,    ///< Top directory entry
        fDir_Files     = (1 << 1),  ///< Non-directory entries
        fDir_Subdirs   = (1 << 2),  ///< Subdirectory entries (non recursive)
        fDir_Recursive = (1 << 3),  ///< Add an recursion
        
        /// Mask to check what should be processed in a directory
        fDir_All = fDir_Self + fDir_Files + fDir_Subdirs,
        
        // --- additional flags -------------------------------------
        
        fIgnoreMissing   = (1 << 4),

        // --- directory processing modes ---------------------------
        // "Enums", retained for backward compatibility.

        /// Directory entry only, no any files or subdirestories
        eOnlyEmpty    = fDir_Self,
        eEntryOnly    = fDir_Self,
        
        /// All files in the top directory only, no subdirectories
        /// and any files in them.
        eTopDirOnly   = fDir_Self | fDir_Files,
        
        /// All files and subdirectories in the top directory,
        /// but no any files in subdirectories
        eNonRecursive = fDir_All,
        
        /// Process all files and subdirectories recursively.
        eRecursive    = fDir_All | fDir_Recursive,
        
        /// Same as eRecursive, but do not report an error for
        /// disappeared entries (e.g. if the same directory is being
        /// removed in a parallel thread for example).
        eRecursiveIgnoreMissing = eRecursive | fIgnoreMissing
    };
    typedef unsigned int TProcessingFlags;  ///< Binary OR of "EProcessingFlags"
    
    /// Flags based on TProcessingFlags
    typedef TProcessingFlags TRemoveFlags;  
    typedef TProcessingFlags TSetModeFlags;

    /// Directory remove mode.
    /// @deprecated  Use TRemoveFlags instead
    NCBI_DEPRECATED typedef TRemoveFlags EDirRemoveMode;
    

    /// Remove a directory entry.
    ///
    /// @flags
    ///   Entry processing flags.
    ///   See EProcessingFlags and CDir::Remove() for details.
    /// @sa
    ///   CDir::Remove(), EProcessingFlags
    virtual bool Remove(TRemoveFlags flags = eRecursive) const;
    
    /// Directory entry type.
    enum EType {
        eFile = 0,                ///< Regular file
        eDir,                     ///< Directory
        ePipe,                    ///< Pipe
        eLink,                    ///< Symbolic link     (UNIX only)
        eSymLink = eLink,         ///< Symbolic link     (UNIX only)
        eSocket,                  ///< Socket            (UNIX only)
        eDoor,                    ///< Door              (UNIX only)
        eBlockSpecial,            ///< Block special     (UNIX only)
        eCharSpecial,             ///< Character special
        //
        eUnknown                  ///< Unknown type
    };

    /// Construct a directory entry object of a specified type.
    ///
    /// An object of specified type will be constructed in memory only,
    /// file system will not be modified.
    /// @param type
    ///   Define a type of the object to create.
    /// @return
    ///   A pointer to newly created entry. If a class for specified type
    ///   is not defined, generic CDirEntry will be returned. Do not forget
    ///   to delete the returned pointer when it is no longer used.
    /// @sa
    ///   CFile, CDir, CSymLink
    static CDirEntry* CreateObject(EType type, const string& path = kEmptyStr);

    /// Get a type of constructed object.
    ///
    /// @return
    ///   Return one of the values in EType. Return "eUnknown" for CDirEntry.
    /// @sa
    ///   CreateObject, GetType
    virtual EType GetObjectType(void) const { return eUnknown; };

    /// Alternate stat structure for use instead of the standard struct stat.
    /// The alternate stat can have useful, but non-posix fields, which
    /// are usually highly platform-dependent, and named differently
    /// in the underlying data structures on different systems.
    struct SStat {
        TNcbiSys_stat orig;  ///< Original stat structure
        // Nanoseconds for dir entry times (if available)
        long  mtime_nsec;  ///< Nanoseconds for modification time
        long  ctime_nsec;  ///< Nanoseconds for creation time
        long  atime_nsec;  ///< Nanoseconds for last access time
    };

    /// Get status information on a dir entry.
    ///
    /// By default have the same behavior as UNIX's lstat().
    /// @param buffer
    ///   Pointer to structure that stores results.
    /// @param follow_links
    ///   Whether to follow symlinks (shortcuts, aliases).
    /// @return
    ///   Return TRUE if the file-status information is obtained,
    ///   FALSE otherwise (errno may be set).
    bool Stat(struct SStat *buffer,
              EFollowLinks follow_links = eIgnoreLinks) const;

    /// Get a type of a directory entry.
    ///
    /// @return
    ///   Return one of the values in EType. If the directory entry does
    ///   not exist, return "eUnknown".
    /// @sa
    ///   IsFile, IsDir, IsLink
    EType GetType(EFollowLinks follow = eIgnoreLinks) const;

    /// Get a type of a directory entry by status information.
    ///
    /// @param st
    ///   Status file information.
    /// @return
    ///   Return one of the values in EType. If the directory entry does
    ///   not exist, return "eUnknown".
    /// @sa
    ///   IsFile, IsDir, IsLink
    static EType GetType(const TNcbiSys_stat& st);

    /// Check whether a directory entry is a file.
    /// @sa
    ///   GetType
    bool IsFile(EFollowLinks follow = eFollowLinks) const;

    /// Check whether a directory entry is a directory.
    /// @sa
    ///   GetType
    bool IsDir(EFollowLinks follow = eFollowLinks) const;

    /// Check whether a directory entry is a symbolic link (alias).
    /// @sa
    ///   GetType
    bool IsLink(void) const;

    /// Get an entry name that a link points to.
    ///
    /// @return
    ///   The real entry name that the link points to. Return an empty
    ///   string if the entry is not a link, or cannot be dereferenced.
    ///   The dereferenced name can be another symbolic link.
    /// @sa 
    ///   GetType, IsLink, DereferenceLink
    string LookupLink(void) const;

    /// Dereference a link.
    ///
    /// If the current entry is a symbolic link, then dereference it
    /// recursively until it is no further a link (but a file, directory,
    /// etc, or does not exist). Replace the entry path string with
    /// the dereferenced path.
    /// @note
    ///   This method dereference only last component of the path.
    ///   To dereference all path components use DereferencePath() method.
    /// @sa 
    ///   DereferencePath, IsLink, LookupLink, NormalizePath
    void DereferenceLink(ENormalizePath normalize = eNormalizePath);

    /// Dereference a path.
    ///
    /// Very similar to DereferenceLink() method, but dereference all
    /// path components recursively until it is no further a link 
    /// in the path (but a file, directory, etc, or does not exist).
    /// Replace the entry path string with the dereferenced path.
    /// @sa 
    ///   DereferenceLink, IsLink, LookupLink
    void DereferencePath(void);

    /// Get time stamp(s) of a directory entry.
    ///
    /// The creation time under MS windows is an actual creation time of the
    /// entry. Under UNIX "creation" time is the time of last entry status
    /// change. If either OS or file system does not support some time type
    /// (modification/last access/creation), then the corresponding CTime
    /// gets returned "empty".
    /// Returned times are always in CTime's time zone format (eLocal/eGMT). 
    /// NOTE that what GetTime returns may not be equal to the the time(s)
    /// previously set by SetTime, as the behavior depends on the OS and
    /// the file system used.
    /// @return
    ///   TRUE if time(s) obtained successfully, FALSE otherwise.
    /// @sa
    ///   GetTimeT, SetTime, Stat
    bool GetTime(CTime* modification,
                 CTime* last_access = 0,
                 CTime* creation    = 0) const;

    /// Get time stamp(s) of a directory entry (time_t version).
    ///
    /// Use GetTime() if you need precision of more than 1 second.
    /// Returned time(s) are always in GMT format. 
    /// @return
    ///   TRUE if time(s) obtained successfully, FALSE otherwise.
    /// @sa
    ///   GetTime, SetTimeT
    bool GetTimeT(time_t* modification,
                  time_t* last_access = 0,
                  time_t* creation    = 0) const;

    /// Set time stamp(s) of a directory entry.
    ///
    /// The process must own the file or have write permissions in order
    /// to change the time(s). Any parameter with a value of zero will
    /// not cause the corresponding time stamp change.
    /// NOTE that what GetTime can later return may not be equal to the
    /// the time(s) set by SetTime, as the behavior depends on the OS and
    /// the file system used.
    /// Also, on UNIX it is impossible to change creation time of an entry
    /// (we silently ignore the "creation" time stamp on that platform).
    /// @param modification
    ///   Entry modification time to set.
    /// @param last_access
    ///   Entry last access time to set. It cannot be less than the entry
    ///   creation time, otherwise it will be set equal to the creation time.
    /// @param creation
    ///   Entry creation time to set. On some platforms it cannot be changed,
    ///   so this parameter will be quietly ignored.
    /// @return
    ///   TRUE if the time(s) changed successfully, FALSE otherwise.
    /// @sa
    ///   SetTimeT, GetTime
    bool SetTime(const CTime* modification = 0,
                 const CTime* last_access  = 0,
                 const CTime* creation     = 0) const;

    /// Set time stamp(s) of a directory entry (time_t version).
    ///
    /// Use SetTime() if you need precision of more than 1 second.
    /// @param modification
    ///   Entry modification time to set.
    /// @param creation
    ///   Entry creation time to set. On some platforms it cannot be changed,
    ///   so this parameter will be quietly ignored.
    /// @param last_access
    ///   Entry last access time to set. It cannot be less than the entry
    ///   creation time, otherwise it will be set equal to the creation time.
    /// @return
    ///   TRUE if the time(s) changed successfully, FALSE otherwise.
    /// @sa
    ///   SetTime, GetTimeT
    bool SetTimeT(const time_t* modification = 0,
                  const time_t* last_access  = 0,
                  const time_t* creation     = 0) const;


    /// What IsNewer() should do if the dir entry does not exist or
    /// is not accessible.
    /// @sa IsNewer
    enum EIfAbsent {
        eIfAbsent_Throw,    ///< Throw an exception
        eIfAbsent_Newer,    ///< Deem absent entry to be "newer"
        eIfAbsent_NotNewer  ///< Deem absent entry to be "older"
    };

    /// Check if the current entry is newer than a specified date/time.
    ///
    /// @param tm
    ///   Time to compare with the current entry modification time.
    /// @param if_absent
    ///   What to do if If the entry does not exist or is not accessible.
    /// @return
    ///   TRUE if the entry's modification time is newer than specified time.
    ///   Return FALSE otherwise.
    /// @sa
    ///   GetTime, EIfAbsent
    bool IsNewer(time_t    tm, 
                 EIfAbsent if_absent /* = eIfAbsent_Throw*/) const;

    /// Check if the current entry is newer than a specified date/time.
    ///
    /// @param tm
    ///   Time to compare with the current entry modification time.
    /// @param if_absent
    ///   What to do if If the entry does not exist or is not accessible.
    /// @return
    ///   TRUE if the entry's modification time is newer than specified time.
    ///   Return FALSE otherwise.
    /// @sa
    ///   GetTime, EIfAbsent
    bool IsNewer(const CTime& tm,
                 EIfAbsent    if_absent /* = eIfAbsent_Throw*/) const;

    /// What path version of IsNewer() should do if the dir entry or specified
    /// path does not exist or is not accessible. Default flags (0) mean
    /// throwing an exceptions if one of dir entries does not exists. 
    /// But if you don't like to have an exception here, you can change IsNewer()
    /// behavior specifying flags. There are three cases: there's a problem only
    /// with the argument (HasThis-NoPath), there's a problem only with original
    /// entry (NoThis-HasPath), or there are problems with both (NoThis-NoPath). 
    /// The EIfAbsent2 lets you independently select what to do in each of those
    /// scenarios: throw an exception (no flags set), return true (f*_Newer),
    /// or return false (f*_NotNewer).
    /// @sa IsNewer
    enum EIfAbsent2 {
        fHasThisNoPath_Newer    = (1 << 0),
        fHasThisNoPath_NotNewer = (1 << 1),
        fNoThisHasPath_Newer    = (1 << 2),
        fNoThisHasPath_NotNewer = (1 << 3),
        fNoThisNoPath_Newer     = (1 << 4),
        fNoThisNoPath_NotNewer  = (1 << 5)
    };
    typedef int TIfAbsent2;   ///< Binary OR of "EIfAbsent2"

    /// Check if the current entry is newer than some other.
    ///
    /// @param path
    ///   An entry name, of which to compare the modification times.
    /// @return
    ///   TRUE if the modification time of the current entry is newer than
    ///   the modification time of the specified entry, or if that entry
    ///   doesn't exist. Return FALSE otherwise.
    /// @sa GetTime, EIfAbsent2
    bool IsNewer(const string& path,
                 TIfAbsent2    if_absent /* = 0 throw*/) const;

    /// Check if the current entry and the given entry_name are identical.
    ///
    /// Note that entries can be checked accurately only on UNIX.
    /// On MS Windows function can check it only by file name.
    /// @param entry_name
    ///   An entry name, of which to compare current entry.
    /// @return
    ///   TRUE if both entries exists and identical. Return FALSE otherwise.
    /// @param follow_links
    ///   Whether to follow symlinks (shortcuts, aliases)
    bool IsIdentical(const string& entry_name,
                     EFollowLinks  follow_links = eIgnoreLinks) const;

    /// Get an entry owner.
    ///
    /// WINDOWS:
    ///   Retrieve the name of the account and the name of the first
    ///   group, which the account belongs to. The obtained group name may
    ///   be an empty string, if we don't have permissions to get it.
    ///   Windows 2000/XP: In addition to looking up for local accounts,
    ///   local domain accounts, and explicitly trusted domain accounts,
    ///   it also can look for any account in any known domain around.
    /// UNIX:
    ///   Retrieve an entry owner:group pair.
    /// @param owner
    ///   Pointer to a string to receive an owner name.
    /// @param group
    ///   Pointer to a string to receive a group name.
    /// @param follow
    ///   Whether to follow symbolic links when getting the information.
    /// @param uid
    ///   Pointer to an unsigned int to receive numeric user id
    ///   (this information is purely supplemental).
    /// @param gid
    ///   Pointer to an unsigned int to receive numeric group id
    ///   (this information is purely supplemental).
    /// @note
    ///   Numeric uid/gid may be the fake ones on Windows (and may be 0).
    /// @note
    ///   The call will fail if neither owner nor group is provided non-NULL
    ///   (even though uid and/or gid may be non-NULL).
    /// @note
    ///   It is in best interest of the caller to clear all strings and
    ///   integers that are expected to get values out of this call prior
    ///   to making the actual call.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   SetOwner
    bool GetOwner(string* owner, string* group = 0,
                  EFollowLinks follow = eFollowLinks,
                  unsigned int* uid = 0, unsigned int* gid = 0) const;

    /// Set an entry owner and/or group.
    ///
    /// You should have administrative (super user) rights to change
    /// an owner or group.
    /// WINDOWS:
    ///   Only administrative privileges (Restore and Take Ownership)
    ///   grant rights to change ownership. Without one of the privileges,
    ///   an administrator cannot take ownership of any file or give ownership
    ///   back to the some user.
    /// UNIX:
    ///   The owner of an entry can change the group to any group of which
    ///   that owner is a member of. The super-user may assign the group
    ///   arbitrarily.
    /// @param owner
    ///   New owner name to set. Use empty string if dont want to change.
    /// @param group
    ///   New group name to set. Use empty string if dont want to change.
    /// @param uid
    ///   Pointer to an unsigned int to receive numeric user id of the
    ///   prospective owner (this information is purely supplemental).
    /// @param gid
    ///   Pointer to an unsigned int to receive numeric group id of the
    ///   prospective owner (this information is purely supplemental).
    /// @note
    ///   Numeric uid/gid can be returned even if the call was unsuccessful;
    ///   they may be the fake ones on Windows (and may be 0).
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   GetOwner
    bool SetOwner(const string& owner, const string& group = kEmptyStr,
                  EFollowLinks follow = eFollowLinks,
                  unsigned int* uid = 0, unsigned int* gid = 0) const;

    //
    // Access permissions.
    //

    /// Directory entry access permissions.
    enum EMode {
        fExecute = 1,       ///< Execute / List(directory) permission
        fWrite   = 2,       ///< Write permission
        fRead    = 4,       ///< Read permission
        fDefault = 8,       ///< Special flag: ignore all other flags, use current default mode
        
        // initial defaults for directories
        fDefaultDirUser  = fRead | fExecute | fWrite,
                            ///< Default user permission for a dir.
        fDefaultDirGroup = fRead | fExecute,
                            ///< Default group permission for a dir.
        fDefaultDirOther = fRead | fExecute,
                            ///< Default other permission for a dir.
        
        // initial defaults for non-dir entries (files, etc.)
        fDefaultUser     = fRead | fWrite,
                            ///< Default user permission for a file
        fDefaultGroup    = fRead,
                            ///< Default group permission for a file
        fDefaultOther    = fRead
                            ///< Default other permission for a file
    };
    typedef unsigned int TMode;  ///< Bitwise OR of "EMode"

    enum ESpecialModeBits {
        fSticky = 1,
        fSetGID = 2,
        fSetUID = 4
    };
    typedef unsigned int TSpecialModeBits; ///< Bitwise OR of ESpecialModeBits

    /// Relative permissions change modes.
    /// Can be combined with TMode or TSpecialModeBits.
    /// Only one of these flags is allowed for each mode.
    enum EModeRelative {
        fModeAdd      = 16,  ///< Adds the argument permission mode to
                             ///< the entry's current mode.
        fModeRemove   = 32,  ///< Removes the argument permission mode
                             ///< from the entriy's current mode.
        fModeNoChange = 64   ///< Do not change permission mode.
    };

    /// Get permission mode(s) of a directory entry.
    ///
    /// On WINDOWS: There is only the "user_mode" permission setting,
    /// and "group_mode" and "other_mode" settings will be ignored.
    /// The "user_mode" will be combined with effective permissions for
    /// the current process owner on specified directory entry.
    /// @return
    ///   TRUE upon successful retrieval of permission mode(s),
    ///   FALSE otherwise.
    /// @sa
    ///   SetMode
    bool GetMode(TMode*            user_mode,
                 TMode*            group_mode = 0,
                 TMode*            other_mode = 0,
                 TSpecialModeBits* special    = 0) const;

    /// Set permission mode(s) of a directory entry.
    ///
    /// Permissions are set as specified by the passed values for
    /// user_mode, group_mode and other_mode. This method rewrites
    /// each user/group/other/special permission by default. 
    /// @note
    ///   Adding one of the EModeRelative flags to mode change default
    ///   behavior that replace modes with passed values and allow
    ///   for relative mode change for any of user/group/other/special.
    ///   It is possble to keep it as is, or add/remove some permission
    ///   modes specified in parameters.
    /// @note
    ///   Passing fDefault will cause the corresponding mode
    ///   to be taken and set from its default setting.
    /// @flags
    ///   Entry processing flags. Affect directories only,
    ///   see CDir::SetMode() for details. For other type entris
    ///   dont have any effect.
    /// @return
    ///   TRUE if permission successfully set;  FALSE, otherwise.
    /// @sa
    ///   SetDefaultMode, SetDefaultModeGlobal, GetMode, EMode,
    ///   EModeRelative, ESpecialModeBits, CDir::SetMode, EProcessingFlags
    virtual bool SetMode(
                 TMode            user_mode, // e.g. fDefault
                 TMode            group_mode = fDefault,
                 TMode            other_mode = fDefault,
                 TSpecialModeBits special    = 0,
                 TSetModeFlags    flags      = eEntryOnly) const;

    /// Set default permission modes globally for all CDirEntry objects.
    ///
    /// The default mode is set globally for all CDirEntry objects except
    /// for those having their own mode set with individual
    /// SetDefaultMode().
    ///
    /// When "fDefault" is passed as a value of the mode parameters,
    /// the default mode takes places for the value as defined in EType:
    ///
    /// If user_mode is "fDefault", then the default is to take
    /// - "fDefaultDirUser" if this is a directory, or
    /// - "fDefaultUser" if this is a file.
    /// 
    /// If group_mode is "fDefault", then the default is to take
    /// - "fDefaultDirGroup" if this is a directory, or
    /// - "fDefaultGroup" if this is a file.
    ///
    /// If other_mode is "fDefault", then the default is to take
    /// - "fDefaultDirOther" if this is a directory, or
    /// - "fDefaultOther" if this is a file.
    static void SetDefaultModeGlobal(
                                EType            entry_type,
                                TMode            user_mode, // e.g. fDefault
                                TMode            group_mode = fDefault,
                                TMode            other_mode = fDefault,
                                TSpecialModeBits special    = 0);

    /// Set default mode(s) for this entry only.
    ///
    /// When "fDefault" is passed as a value of the parameters,
    /// the corresponding mode will be taken and set from the global mode
    /// as specified by SetDefaultModeGlobal().
    virtual void SetDefaultMode(EType            entry_type,
                                TMode            user_mode, // e.g. fDefault
                                TMode            group_mode = fDefault,
                                TMode            other_mode = fDefault,
                                TSpecialModeBits special    = 0);
                                
    /// Check access rights.
    ///
    /// Use effective user ID (or process owner) to check the entry
    /// for accessibility accordingly to specified mask.
    /// @note
    ///   - If an entry is a symbolic link, that the access rights
    ///     will be checked for entry to which the link points.
    ///   - Execute bit means 'search' for directories.
    ///   - Success checking fExecute mode do not guarantee that
    ///     the file will executable at all. This function just check
    ///     permissions and not the file format.
    ///   - On some platforms, if the file is currently open for execution,
    ///     the function reports that it is not writable, regardless
    ///     of the setting of its mode.
    /// @note
    ///   This method may not work correctly
    ///     On MS-Windows:
    ///       - for network shares and mapped network drives.
    ///     On Unix:
    ///       - for entries on NFS file systems.
    /// @note
    ///   Using CheckAccess() to check if a user is authorized to e.g. open
    ///   a file before actually doing so creates a security hole, because
    ///   the user might exploit the short time interval between checking
    ///   and opening the file to manipulate it. Always is better to try to
    ///   open a file with necessary permissions and check result.
    /// @param access_mode
    ///   Checked access mode (any combination of fRead/fWrite/fExecute).
    /// @return
    ///   TRUE if specified permissions granted, FALSE otherwise
    ///   (also returns FALSE if the file doesn't exists or an error occurs).
    /// @sa
    ///   GetMode
    bool CheckAccess(TMode access_mode) const;


    /// Construct mode_t value from permission modes.
    /// Parameters must not have "fDefault" values.
    /// @sa ModeFromModeT
    static mode_t MakeModeT(TMode            user_mode,
                            TMode            group_mode,
                            TMode            other_mode,
                            TSpecialModeBits special);

    /// Convert mode_t to permission mode(s).
    ///
    /// @note
    ///   On WINDOWS: There is only the "user_mode" permission setting,
    ///   and "group_mode" and "other_mode" settings will be ignored.
    ///   The "user_mode" will be combined with effective permissions for
    ///   the current process owner on specified directory entry.
    /// @sa
    ///   GetMode, SetMode, MakeModeT
    static void ModeFromModeT(mode_t            mode, 
                              TMode*            user_mode, 
                              TMode*            group_mode = 0, 
                              TMode*            other_mode = 0,
                              TSpecialModeBits* special    = 0);

    /// Permission mode string format.
    /// @sa 
    ///   StringToMode, ModeToString
    enum EModeStringFormat {
        eModeFormat_Octal,                       ///< Octal format ("664")
        eModeFormat_Symbolic,                    ///< Shell symbolic format ("u=rwx,g=rwx,o=rx")
        eModeFormat_List,                        ///< Shell list 'ls -l' like format ("rwxrwxrwx")
        eModeFormat_Default = eModeFormat_Octal  ///< Default mode
    };


    /// Convert permission modes to string representation using one of predefined formats.
    /// Parameters must not have "fDefault" values.
    /// @sa StringToMode, EModeStringFormat
    static string ModeToString(TMode             user_mode,
                               TMode             group_mode,
                               TMode             other_mode,
                               TSpecialModeBits  special,
                               EModeStringFormat format = eModeFormat_Default);

    /// Convert string (in one of predefined formats, detects automatically) to permission mode(s).
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa 
    ///   ModeToString, EModeStringFormat
    static bool StringToMode(const CTempString& mode, 
                             TMode*            user_mode, 
                             TMode*            group_mode = 0, 
                             TMode*            other_mode = 0,
                             TSpecialModeBits* special    = 0);


    /// Get file/directory mode creation mask.
    /// @note
    ///   umask can be useful on Unix platform only. On Windows only
    ///   C Runtime library honor it, any methods that use Windows API
    ///   ignore its setting.
    /// @sa 
    ///   SetUmask
    static void GetUmask(TMode*            user_mode, 
                         TMode*            group_mode = 0, 
                         TMode*            other_mode = 0,
                         TSpecialModeBits* special    = 0);

    /// Set file/directory mode creation mask.
    ///
    /// @note
    ///   fDefault value for permission mode(s) mean 0.
    /// @note
    ///   umask can be useful on Unix platform only. On Windows only
    ///   C Runtime library honor it, any methods that use Windows API
    ///   ignore its setting.
    /// @sa
    ///   GetUmask
    static void SetUmask(TMode             user_mode,
                         TMode             group_mode = fDefault,
                         TMode             other_mode = fDefault,
                         TSpecialModeBits  special    = 0);

    //
    // Temporary directory entries (and files)
    //

    /// Temporary file creation mode
    enum ETmpFileCreationMode {
        eTmpFileCreate,     ///< Create empty file for each GetTmpName* call.
        eTmpFileGetName     ///< Get name of the file only.
    };

    /// Get temporary file name.
    ///
    /// This class generates a temporary file name in the temporary directory
    /// specified by the OS. But this behavior can be changed -- just set the
    /// desired temporary directory using the global parameter 'TmpDir' in
    /// in the 'NCBI' registry section or the environment (see CParam class
    /// description), and it will used by default in this class.
    ///
    /// @param mode
    ///   Temporary file creation mode. Note, that default mode eTmpFileGetName
    ///   returns only the name of the temporary file, without creating it.
    ///   This method can be faster, but it have potential race condition,
    ///   when other process can leave as behind and create file with the same
    ///   name first. Also, please note, that if you call GetTmpName() again,
    ///   without creating file for previous call of this method, the same
    ///   file name can be returned.
    /// @return
    ///   Name of temporary file, or "kEmptyStr" if there was an error
    ///   getting temporary file name.
    /// @sa
    ///    GetTmpNameEx, ETmpFileCreationMode
    static string GetTmpName(ETmpFileCreationMode mode = eTmpFileGetName);

    /// Get temporary file name.
    ///
    /// @param dir
    ///   Directory in which temporary file is to be created;
    ///   default of kEmptyStr means that function try to get application 
    ///   specific temporary directory name from global parameter (see
    ///   GetTmpName description), then it try to get system temporary
    ///   directory, and all of this fails - current directory will be used.
    /// @param prefix
    ///   Temporary file name will be prefixed by value of this parameter;
    ///   default of kEmptyStr means that, effectively, no prefix value will
    ///   be used.
    /// @param mode
    ///   Temporary file creation mode. 
    ///   If set to "eTmpFileCreate", empty file with unique name will be
    ///   created. Please, do not forget to remove it yourself as soon as it
    ///   is no longer needed. On some platforms "eTmpFileCreate" mode is 
    ///   equal to "eTmpFileGetName".
    ///   If set to "eTmpFileGetName", returns only the name of the temporary
    ///   file, without creating it. This method can be faster, but it have
    ///   potential race condition, when other process can leave as behind and
    ///   create file with the same name first.
    /// @return
    ///   Name of temporary file, or "kEmptyStr" if there was an error
    ///   getting temporary file name.
    /// @sa
    ///    GetTmpName, ETmpFileCreationMode
    static string GetTmpNameEx(const string&        dir    = kEmptyStr,
                               const string&        prefix = kEmptyStr,
                               ETmpFileCreationMode mode   = eTmpFileGetName);


    /// What type of temporary file to create.
    enum ETextBinary {
        eText,          ///< Create text file
        eBinary         ///< Create binary file
    };

    /// Which operations to allow on temporary file.
    enum EAllowRead {
        eAllowRead,     ///< Allow read and write
        eWriteOnly      ///< Allow write only
    };

    /// Create temporary file and return pointer to corresponding stream.
    ///
    /// The temporary file will be automatically deleted after the stream
    /// object is deleted. If the file exists before the function call, then
    /// after the function call it will be removed. Also any previous contents
    /// of the file will be overwritten.
    /// @param filename
    ///   Use this value as name of temporary file. If "kEmptyStr" is passed
    ///   generate a temporary file name.
    /// @param text_binary
    ///   Specifies if temporary filename should be text ("eText") or binary
    ///   ("eBinary").
    /// @param allow_read
    ///   If set to "eAllowRead", read and write are permitted on temporary
    ///   file. If set to "eWriteOnly", only write is permitted on temporary
    ///   file.
    /// @return
    ///   - Pointer to corresponding stream, or
    ///   - NULL if error encountered.
    /// @sa
    ///   CreateTmpFileEx
    static fstream* CreateTmpFile(const string& filename    = kEmptyStr,
                                  ETextBinary   text_binary = eBinary,
                                  EAllowRead    allow_read  = eAllowRead);

    /// Create temporary file and return pointer to corresponding stream.
    ///
    /// Similar to CreateTmpEx() except that you can also specify the directory
    /// in which to create the temporary file and the prefix string to be used
    /// for creating the temporary file.
    ///
    /// The temporary file will be automatically deleted after the stream
    /// object is deleted. If the file exists before the function call, then
    /// after the function call it will be removed. Also any previous contents
    /// of the file will be overwritten.
    /// @param dir
    ///   The directory in which the temporary file is to be created. If not
    ///   specified, the temporary file will be created in the current 
    ///   directory.
    /// @param prefix
    ///   Use this value as the prefix for temporary file name. If "kEmptyStr"
    ///   is passed generate a temporary file name.
    /// @param text_binary
    ///   Specifies if temporary filename should be text ("eText") or binary
    ///   ("eBinary").
    /// @param allow_read
    ///   If set to "eAllowRead", read and write are permitted on temporary
    ///   file. If set to "eWriteOnly", only write is permitted on temporary
    ///   file.
    /// @return
    ///   - Pointer to corresponding stream, or
    ///   - NULL if error encountered.
    /// @sa
    ///   CreateTmpFile
    static fstream* CreateTmpFileEx(const string& dir         = ".",
                                    const string& prefix      = kEmptyStr,
                                    ETextBinary   text_binary = eBinary,
                                    EAllowRead    allow_read  = eAllowRead);

protected:
    /// Get the default global mode.
    ///
    /// For use in derived classes like CDir and CFile.
    static void GetDefaultModeGlobal(EType             entry_type,
                                     TMode*            user_mode,
                                     TMode*            group_mode,
                                     TMode*            other_mode,
                                     TSpecialModeBits* special);

    /// Get the default mode.
    ///
    /// For use by derived classes like CDir and CFile.
    void GetDefaultMode(TMode*            user_mode,
                        TMode*            group_mode,
                        TMode*            other_mode,
                        TSpecialModeBits* special) const;

private:
    string m_Path;    ///< Full path of this directory entry

    /// Which default mode: user, group, or other.
    ///
    /// Used as an index into an array that contains default mode values;
    /// so there is no "fDefault" as an enumeration value for EWho here!
    enum EWho {
        eUser = 0,    ///< User mode
        eGroup,       ///< Group mode
        eOther,       ///< Other mode
        eSpecial      ///< Special bits
    }; 

    /// Holds default mode values
    TMode m_DefaultMode[4/*EWho + Special bits*/];

    /// Holds default mode global values, per entry type
    static TMode m_DefaultModeGlobal[eUnknown][4/*EWho + Special bits*/];

    /// Backup suffix
    static const char* m_BackupSuffix;

private:
    /// Convert permission mode to symbolic string representation.
    static string x_ModeToSymbolicString(EWho who, TMode mode, bool special_bit, char filler);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CFile --
///
/// Define class to work with files.
///
/// Models a file in a file system. Basic functionality is derived from
/// CDirEntry and extended for files.

class NCBI_XNCBI_EXPORT CFile : public CDirEntry
{
    typedef CDirEntry CParent;    ///< CDirEntry is the parent class

public:
    /// Default constructor.
    CFile(void);

    /// Constructor using specified path string.
    CFile(const string& file);

    /// Copy constructor.
    CFile(const CDirEntry& file);

    /// Destructor.
    virtual ~CFile(void);

    /// Get a type of constructed object.
    virtual EType GetObjectType(void) const { return eFile; };

    /// Check existence of file.
    virtual bool Exists(void) const;

    /// Get size of file.
    ///
    /// @return
    ///   Size of the file, if operation successful; -1, otherwise.
    Int8 GetLength(void) const;

    /// Copy a file.
    ///
    /// @param new_path
    ///    Target entry path/name.
    /// @param flags
    ///   Flags specified how to copy entry.
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    /// @sa
    ///   TCopyFlags
    virtual bool Copy(const string& new_path, TCopyFlags flags = fCF_Default,
                      size_t buf_size = 0) const;

    /// Compare files contents in binary form.
    ///
    /// @param file
    ///   File name to compare.
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if files content is equal; FALSE otherwise.
    bool Compare(const string& file, size_t buf_size = 0) const;

    /// Mode to compare streams in text form.
    enum ECompareText {
        /// Skip end-of-line characters ('\r' and '\n')
        eIgnoreEol = eCompareText_IgnoreEol,
        ///< Skip white spaces (in terms of isspace(), including end-of-line)
        eIgnoreWs  = eCompareText_IgnoreWhiteSpace
    };

    /// Compare files contents in text form.
    ///
    /// @param file
    ///   File name to compare.
    /// @param mode
    ///   Type of white space characters to ignore
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if files content is equal; FALSE otherwise.
    bool CompareTextContents(const string& file, ECompareText mode,
                             size_t buf_size = 0) const;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDir --
///
/// Define class to work with directories.
///

class NCBI_XNCBI_EXPORT CDir : public CDirEntry
{
    typedef CDirEntry CParent;  ///< CDirEntry is the parent class

public:
    /// Default constructor.
    CDir(void);

    /// Constructor using specified directory name.
    CDir(const string& dirname);

    /// Copy constructor.
    CDir(const CDirEntry& dir);

    /// Destructor.
    virtual ~CDir(void);

    /// Get a type of constructed object.
    virtual EType GetObjectType(void) const { return eDir; };

    /// Check if directory "dirname" exists.
    virtual bool Exists(void) const;

    /// Get user "home" directory.
    static string GetHome(void);

    /// Get temporary directory.
    static string GetTmpDir(void);

    /// Get the current working directory.
    static string GetCwd(void);

    /// Change the current working directory.
    static bool SetCwd(const string& dir);

    /// Define a list of pointers to directory entries.
    typedef list< AutoPtr<CDirEntry> > TEntries;

    /// Flags for GetEntries()
    /// @sa GetEntries, GetEntriesPtr
    enum EGetEntriesFlags {
        fIgnoreRecursive = (1<<1), ///< Suppress self recursive elements ("..")
        fCreateObjects   = (1<<2), ///< Get objects accordingly to entry type
                                   ///< (CFile,CDir,...), not just a CDirEntry
        fNoCase          = (1<<3), ///< Use case insensitive compare by mask
        fIgnorePath      = (1<<4), ///< Return only entries names, not full path
        // These flags added for backward compatibility and will be removed
        // in the future, so don't use it.
        eAllEntries       = 0,
        eIgnoreRecursive  = fIgnoreRecursive
    };
    typedef int TGetEntriesFlags; ///< Binary OR of "EGetEntriesFlags"

    /// Get directory entries based on the specified "mask".
    ///
    /// @param mask
    ///   Use to select only entries that match this mask.
    ///   Do not use file mask if set to "kEmptyStr".
    /// @param mode
    ///   Defines which entries return.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const string&    mask  = kEmptyStr,
                        TGetEntriesFlags flags = 0) const;

    /// Get directory entries based on the specified set of "masks".
    ///
    /// @param mask
    ///   Use to select only entries that match this set of masks.
    /// @param mode
    ///   Defines which entries return.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const vector<string>& masks,
                        TGetEntriesFlags flags = 0) const;

    /// Get directory entries based on the specified set of "masks".
    ///
    /// @param mask
    ///   Use to select only entries that match this set of masks.
    /// @param mode
    ///   Defines which entries return.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const CMask&     masks, 
                        TGetEntriesFlags flags = 0) const;


    /// Versions of GetEntries() which returns pointer to TEntries.
    /// This methods are faster on big directories than GetEntries().
    /// These methods return NULL on error.
    /// NOTE: Do not forget to release allocated memory using return pointer.
    TEntries* GetEntriesPtr(const string&    mask  = kEmptyStr,
                            TGetEntriesFlags flags = 0) const;

    TEntries* GetEntriesPtr(const vector<string>& masks,
                            TGetEntriesFlags flags = 0) const;

    TEntries* GetEntriesPtr(const CMask&     masks,
                            TGetEntriesFlags flags = 0) const;

    // OBSOLETE functions. Will be deleted soon.
    // Please use versions of GetEntries*() listed above.
    typedef TGetEntriesFlags EGetEntriesMode;
    TEntries GetEntries    (const string&    mask,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries GetEntries    (const vector<string>& masks,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries GetEntries    (const CMask&     masks,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries* GetEntriesPtr(const string&    mask,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries* GetEntriesPtr(const vector<string>& masks,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries* GetEntriesPtr(const CMask&     masks,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;

    /// Create the directory using "dirname" passed in the constructor.
    /// 
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    bool Create(void) const;

    /// Create the directory path recursively possibly more than one at a time.
    ///
    /// @return
    ///   TRUE if operation successful; FALSE otherwise.
    bool CreatePath(void) const;

    /// Copy directory.
    ///
    /// @param new_path
    ///   New path/name for entry.
    /// @param flags
    ///   Flags specified how to copy directory.
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    /// @sa
    ///   CDirEntry::TCopyFlags, CDirEntry::Copy, CFile::Copy
    virtual bool Copy(const string& new_path, TCopyFlags flags = fCF_Default,
                      size_t buf_size = 0) const;

    /// Delete existing directory.
    ///
    /// @param flags
    ///   Directory processing flags. Some popular sets of flags
    ///   combined together and listed below:
    ///   - eOnlyEmpty
    ///        directory can be removed only if it is empty;
    ///   - eTopDirOnly
    ///        remove all files in the top directory only,
    ///        no any subdirectory or files in it;
    ///   - eNonRecursive
    ///        same as eTopDirOnly, but removes also empty
    ///        subdirectories in the top directory only;
    ///   - eRecursive
    ///        remove all files in directory and all its subdirectories;
    ///   - eRecursiveIgnoreMissing
    ///        same as eRecursive, but do not report an error
    ///        for missed entries.
    /// @return
    ///   TRUE if operation successful; FALSE otherwise.
    /// @sa
    ///   CDirEntry::Remove, EProcessingFlags
    virtual bool Remove(TRemoveFlags flags = eRecursive) const;


    /// Set permission mode(s) for a directory.
    ///
    /// Permissions are set as specified by the passed values for
    /// user_mode, group_mode and other_mode. This method rewrites
    /// each user/group/other/special permission by default.
    /// @note
    ///   Adding one of the EModeRelative flags to mode change default
    ///   behavior that replace modes with passed values and allow
    ///   for relative mode change for any of user/group/other/special.
    ///   It is possble to keep it as is, or add/remove some permission
    ///   modes specified in parameters.
    /// @note
    ///   Passing "fDefault" will cause the corresponding mode
    ///   to be taken and set from its default setting.
    /// @param flags
    ///   Directory processing flags. Some popular sets of flags
    ///   combined together and listed below:
    ///   - eEntryOnly
    ///        change modes for the directory entry itself;
    ///   - eTopDirOnly
    ///        change modes for all files in the top directory only,
    ///        no for any subdirectory or files in it;
    ///   - eNonRecursive
    ///        change modes for all entries in the top directory only,
    ///        including subdirectory entries, but do not for any files
    ///        in them;
    ///   - eRecursive
    ///         change modes for all files and subdirectories recursively;
    ///   - eRecursiveIgnoreMissing
    ///        same as eRecursive, but do not report an error
    ///        for missed entries.
    /// @return
    ///   TRUE if permissions successfully set for all entries;  FALSE, otherwise.
    /// @sa
    ///   SetMode, SetDefaultMode, SetDefaultModeGlobal,
    ///   EMode, EModeRelative, EProcessingFlags 
    virtual bool SetMode(TMode            user_mode,  // e.g. fDefault
                         TMode            group_mode  = fDefault,
                         TMode            other_mode  = fDefault,
                         TSpecialModeBits special     = 0,
                         TSetModeFlags    flags       = eEntryOnly) const;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CSymLink --
///
/// Define class to work with symbolic links.
///
/// Models the files in a file system. Basic functionality is derived from
/// CDirEntry and extended for files.

class NCBI_XNCBI_EXPORT CSymLink : public CDirEntry
{
    typedef CDirEntry CParent;    ///< CDirEntry is the parent class

public:
    /// Default constructor.
    CSymLink(void);

    /// Constructor using specified path string.
    CSymLink(const string& link);

    /// Copy constructor.
    CSymLink(const CDirEntry& link);

    /// Destructor.
    virtual ~CSymLink(void);

    /// Get a type of constructed object.
    virtual EType GetObjectType(void) const { return eLink; };

    /// Check existence of link.
    virtual bool Exists(void) const;

    /// Create symbolic link.
    ///
    /// @param path
    ///   Path to some entry that link will be pointed to.
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    ///   Return FALSE also if link already exists.
    bool Create(const string& path) const;

    /// Copy link.
    ///
    /// @param new_path
    ///   Target entry path/name.
    /// @param flags
    ///   Flags specified how to copy entry.
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    /// @sa
    ///   CDirEntry::TCopyFlags, CDirEntry::Copy, Create
    virtual bool Copy(const string& new_path, TCopyFlags flags = fCF_Default,
                      size_t buf_size = 0) const;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CFileUtil --
///
/// Utility functions.
///
/// Throws an exceptions on error.

class NCBI_XNCBI_EXPORT CFileUtil
{
public:
    ///   Unix:
    ///       The path name to any file/dir withing filesystem.
    ///   MS Windows:
    ///       The path name to any file/dir withing filesystem.
    ///       The root directory of the disk, or UNC name
    ///       (for example, \\MyServer\MyShare\, C:\).
    ///   The "." can be used to get disk space on current disk.

    //// Type of file system
    enum EFileSystemType {
        eUnknown = 0,   ///< File system type could not be determined

        eADFS,          ///< Acorn's Advanced Disc Filing System
        eAdvFS,         ///< Tru64 UNIX Advanced File System
        eAFFS,          ///< Amiga Fast File System
        eAFS,           ///< AFS File System
        eAUTOFS,        ///< Automount File Sustem
        eBEFS,          ///< The Be (BeOS) File System (BeFS)
        eBFS,           ///< Boot File System
        eCacheFS,       ///< Cache File System
        eCryptFS,       ///< eCryptfs (Enterprise Cryptographic Filesystem)
        eCDFS,          ///< ISO 9660 CD-ROM file system (CDFS/ISO9660)
        eCIFS,          ///< Common Internet File System
        eCODA,          ///< Coda File System
        eCOH,           ///< Coherent (System V)
        eCRAMFS,        ///< Compressed ROMFS
        eDebugFS,       ///< Debug File Sytem (Linux)
        eDEVFS,         ///< Device File Sytem
        eDFS,           ///< DCE Distributed File System (DCE/DFS)
        eEFS,           ///< The Encrypting File System (EFS) (MSWin)
        eEXOFS,         ///< EXtended Object File System (EXOFS)
        eExt,           ///< Extended file system
        eExt2,          ///< Second Extended file system
        eExt3,          ///< Journalled form of ext2
        eFAT,           ///< Traditional 8.3 MSDOS-style file system
        eFAT32,         ///< FAT32 file system
        eFDFS,          ///< File Descriptor File System
        eFFM,           ///< File-on-File Mounting file system
        eFFS,           ///< Fast File System (*BSD)
        eFUSE,          ///< Filesystem in Userspace (FUSE)
        eFUSE_CTL,      ///< Fusectl (helper filesystem for FUSE)
        eGFS2,          ///< Global File System
        eGPFS,          ///< IBM General Parallel File System
        eHFS,           ///< Hierarchical File System
        eHFSPLUS,       ///< Hierarchical File System
        eHPFS,          ///< OS/2 High-Performance File System
        eHSFS,          ///< High Sierra File System
        eJFS,           ///< Journalling File System
        eJFFS,          ///< Journalling Flash File System
        eJFFS2,         ///< Journalling Flash File System v2
        eLOFS,          ///< Loopback File System
        eMFS,           ///< Memory File System
        eMinix,         ///< Minix v1
        eMinix2,        ///< Minix v2
        eMinix3,        ///< Minix v3
        eMSFS,          ///< Mail Slot File System
        eNCPFS,         ///< NetWare Core Protocol File System
        eNFS,           ///< Network File System (NFS)
        eNTFS,          ///< New Technology File System
        eOCFS2,         ///< Oracle Cluster File System 2
        eOPENPROM,      ///< /proc/openprom filesystem
        ePANFS,         ///< Panasas FS
        ePROC,          ///< /proc file system
        ePVFS2,         ///< Parallel Virtual File System 
        eReiserFS,      ///< Reiser File System
        eRFS,           ///< Remote File Share file system (AT&T RFS)
        eQNX4,          ///< QNX4 file system
        eROMFS,         ///< ROM File System
        eSELINUX,       ///< Security-Enhanced Linux (SELinux)
        eSMBFS,         ///< Samba File System 
        eSPECFS,        ///< SPECial File System
        eSquashFS,      ///< Compressed read-only filesystem (Linux)
        eSYSFS,         ///< (Linux)
        eSYSV2,         ///< System V
        eSYSV4,         ///< System V
        eTMPFS,         ///< Virtual Memory File System (TMPFS/SHMFS)
        eUBIFS,         ///< The Unsorted Block Image File System
        eUDF,           ///< Universal Disk Format
        eUFS,           ///< UNIX File System
        eUFS2,          ///< UNIX File System
        eUSBDEVICE,     ///< USBDevice file system
        eV7,            ///< UNIX V7 File System
        eVxFS,          ///< VERITAS File System (VxFS)
        eVZFS,          ///< Virtuozzo File System (VZFS)
        eXENIX,         ///< Xenix (SysV) file system
        eXFS,           ///< XFS File System
        eXIAFS          ///<
    };

    /// Structure to store information about file system.
    struct SFileSystemInfo {
        EFileSystemType  fs_type;       ///< Type of filesystem
        Uint8            total_space;   ///< Total disk space in bytes
        Uint8            free_space;    ///< Free disk space in bytes
        unsigned long    block_size;    ///< Allocation unit (block) size
        unsigned long    filename_max;  ///< Maximum filename length
                                        ///< (part between slashes)
    };

    /// Get file system information.
    ///
    /// Get information for the user associated with the calling thread only.
    /// @param path
    ///   String that specifies filesystem for which information
    ///   is to be returned. 
    /// @param info
    ///   Pointer to structure which receives file system information.
    static void GetFileSystemInfo(const string& path, SFileSystemInfo* info);

    /// Get free disk space information.
    ///
    /// Get information for the user associated with the calling thread only.
    /// If per-user quotas are in use, then the returned values may be less
    /// than the actual number of free bytes available on disk.
    /// @param path
    ///   String that specifies the file system for which to return
    ///   the number of free bytes available on disk.
    /// @return
    ///   The amount of free space in bytes.
    /// @sa
    ///   GetFileSystemInfo, GetTotalDiskSpace
    static Uint8 GetFreeDiskSpace(const string& path);

    /// Get total disk space information.
    ///
    /// Get information for the user associated with the calling thread only.
    /// If per-user quotas are in use, then the returned value may be less
    /// than the actual total number of bytes on the disk.
    /// @param path
    ///   String that specifies the file system for which to return
    ///   the total disk space in bytes.
    /// @return
    ///   The amount of total disk space in bytes.
    /// @sa
    ///   GetFileSystemInfo, GetFreeDiskSpace
    static Uint8 GetTotalDiskSpace(const string& path);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CFileDeleteList --
///
/// Define a list of dir entries for deletion.
///
/// Each Object of this class maintains a list of names of dir entries
/// that will be deleted from the file system when the object
/// goes out of scope.
///
/// Note: Directories will be removed recursively, symbolic links -- 
/// without dir entries which they points to.

class NCBI_XNCBI_EXPORT CFileDeleteList : public CObject
{
public:
    /// Destructor removes all dir entries on list.
    ~CFileDeleteList();

    typedef list<string> TNames;

    /// Add a dir entry for later deletion.
    void Add(const string& entryname);
    /// Get the underlying list.
    const TNames& GetNames() const;
    /// Set the underlying list.
    void SetNames(TNames& names);

private:
    TNames  m_Names;   ///< List of dir entries for deletion
};


/////////////////////////////////////////////////////////////////////////////
///
/// CFileDeleteAtExit --
///
/// This class is used to mark dir entries for deletion at application exit.
/// @sa CFileDeleteList

class NCBI_XNCBI_EXPORT CFileDeleteAtExit
{
public:
    /// Add the name of a dir entry; it will be deleted on (normal) exit
    static void Add(const string& entryname);

    /// Get underlying static CFileDeleteList object
    static const CFileDeleteList& GetDeleteList();
    /// Set the underlying static CFileDeleteList object
    static void SetDeleteList(CFileDeleteList& list);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CTmpFile --
///
/// Define class to generate temporary file name (or use specified), which
/// can be automatically removed on the object destruction.
///
/// This class generate temporary file name in the temporary directory
/// specified by OS. But this behavior can be changed, just set desired
/// temporary directory using global parameter (see CParam class description)
/// in the registry or environment (section 'NCBI', name 'TmpDir') and it
/// will used by default in this class.
/// @note
///   The files created this way are not really temporary. If application
///   terminates abnormally, that such files can be left on the file system.
///   To avoid this you can use CFile::CreateTmpFile().
/// @sa CFile::CreateTmpFile, CFile::GetTmpName, CParam

class NCBI_XNCBI_EXPORT CTmpFile : public CObject
{
public:
    /// What to do with the file on object destruction.
    enum ERemoveMode {
        eRemove,    ///< Remove file
        eNoRemove   ///< Do not remove file
    };

    /// Default constructor.
    ///
    /// Automaticaly generate temporary file name.
    CTmpFile(ERemoveMode remove_file = eRemove);

    /// Constructor.
    ///
    /// Use given temporary file name.
    CTmpFile(const string& file_name, ERemoveMode remove_file = eRemove);

    /// Destructor.
    ~CTmpFile(void);

    /// What to do if stream already exists in the AsInputFile/AsOutputFile.
    enum EIfExists {
        /// You can make call of AsInputFile/AsOutputFile only once,
        /// on each following call throws CFileException exception.
        eIfExists_Throw,
        /// Delete previous stream and return reference to new object.
        /// Invalidate all previously returned references.
        eIfExists_Reset,
        /// Return reference to current stream, create new one if it
        /// does not exists yet.
        eIfExists_ReturnCurrent
    };

    /// Create I/O stream on the base of our file.
    CNcbiIstream& AsInputFile (EIfExists if_exists,
                               IOS_BASE::openmode mode = IOS_BASE::in);
    CNcbiOstream& AsOutputFile(EIfExists if_exists,
                               IOS_BASE::openmode mode = IOS_BASE::out);

    /// Return used file name (generated or given in the constructor).
    const string& GetFileName(void) const;

private:
    string      m_FileName;            ///< Name of temporary file.
    ERemoveMode m_RemoveOnDestruction; ///< Remove file on destruction

    // Automatic pointers to store I/O streams.
    auto_ptr<CNcbiIstream> m_InFile;
    auto_ptr<CNcbiOstream> m_OutFile;

private:
    // Prevent copying
    CTmpFile(const CTmpFile&);
    CTmpFile& operator=(const CTmpFile&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// Forward declaration of struct containing OS-specific mem.-file handle.

struct SMemoryFileHandle;
struct SMemoryFileAttrs;


/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFile_Base --
///
/// Define base class for support file memory mapping.

class NCBI_XNCBI_EXPORT CMemoryFile_Base
{
public:
    /// Which operations are permitted in memory map file.
    typedef enum {
        eMMP_Read,        ///< Data can be read
        eMMP_Write,       ///< Data can be written
        eMMP_ReadWrite    ///< Data can be read and written
    } EMemMapProtect;

    /// Whether to share changes or not.
    typedef enum {
        eMMS_Shared,      ///< Changes are shared
        eMMS_Private      ///< Changes are private (write do not change file)
    } EMemMapShare;

    /// Memory file open mode.
    enum EOpenMode {
        eCreate,          ///< Create new file or rewrite existent with zeros.
        eOpen,            ///< Open existent file, throw exception otherwise.
        eExtend,          ///< Extend file size with zeros if it exist,
                          ///< throw exception otherwise.
        eDefault = eOpen  ///< Default open mode
    };

    /// Constructor.
    ///
    CMemoryFile_Base(void);


    /// Check if memory-mapping is supported by the C++ Toolkit on this
    /// platform.
    static bool IsSupported(void);


    /// What type of data access pattern will be used for mapped region.
    ///
    /// Advises the VM system that the a certain region of user mapped memory 
    /// will be accessed following a type of pattern. The VM system uses this 
    /// information to optimize work with mapped memory.
    ///
    /// NOTE: Works on UNIX platform only.
    /// @sa
    ///   EMemoryAdvise, MemoryAdvise
    typedef enum {
        eMMA_Normal      = eMADV_Normal,
        eMMA_Random      = eMADV_Random,
        eMMA_Sequential  = eMADV_Sequential,
        eMMA_WillNeed    = eMADV_WillNeed,
        eMMA_DontNeed    = eMADV_DontNeed,
        eMMA_DoFork      = eMADV_DoFork,
        eMMA_DontFork    = eMADV_DontFork,
        eMMA_Mergeable   = eMADV_Mergeable,
        eMMA_Unmergeable = eMADV_Unmergeable
    } EMemMapAdvise;

    /// Advise on memory map usage for specified region.
    ///
    /// @param addr
    ///   Address of memory region whose usage is being advised.
    /// @param len
    ///   Length of memory region whose usage is being advised.
    /// @param advise
    ///   Advise on expected memory usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful.
    ///   - FALSE, if memory advise operation not successful, or is not supported
    ///     on current platform.
    /// @sa
    ///   EMemMapAdvise, MemMapAdvise
    static bool MemMapAdviseAddr(void* addr, size_t len, EMemMapAdvise advise);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFileSegment --
///
/// Define auxiliary class for mapping a memory file region of the file
/// into the address space of the calling process. 
/// 
/// Throws an exceptions on error.

class NCBI_XNCBI_EXPORT CMemoryFileSegment : public CMemoryFile_Base
{
public:
    /// Constructor.
    ///
    /// Maps a view of the file, represented by "handle", into the address
    /// space of the calling process. 
    /// @param handle
    ///   Handle to view of the mapped file. 
    /// @param attr
    ///   Specify operations permitted on memory mapped region.
    /// @param offset
    ///   The file offset where mapping is to begin.
    ///   Cannot accept values less than 0.
    /// @param length
    ///   Number of bytes to map. The parameter value should be more than 0.
    /// @sa
    ///   EMemMapProtect, EMemMapShare, GetPtr, GetSize, GetOffset
    CMemoryFileSegment(SMemoryFileHandle& handle,
                       SMemoryFileAttrs&  attrs,
                       off_t              offset,
                       size_t             length);

    /// Destructor.
    ///
    /// Unmaps a mapped area of the file.
    ~CMemoryFileSegment(void);

    /// Get pointer to beginning of data.
    ///
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    void* GetPtr(void) const;

    /// Get offset of the mapped area from beginning of file.
    ///
    /// @return
    ///   Offset in bytes of mapped area from beginning of the file.
    ///   Always return value passed in constructor even if data
    ///   was not successfully mapped.
    off_t GetOffset(void) const;

    /// Get length of the mapped area.
    ///
    /// @return
    ///   - Length in bytes of the mapped area, or
    ///   - 0 if not mapped.
    size_t GetSize(void) const;

    /// Get pointer to beginning of really mapped data.
    ///
    /// When the mapping object is creating and the offset is not a multiple
    /// of the allocation granularity, that offset and length can be adjusted
    /// to match it. The "length" value will be automatically increased on the
    /// difference between passed and real offsets.
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    /// @sa
    ///    GetRealOffset, GetRealSize, GetPtr 
    void* GetRealPtr(void) const;

    /// Get real offset of the mapped area from beginning of file.
    ///
    /// This real offset is adjusted to system's memory allocation granularity
    /// value and can be less than requested "offset" in the constructor.
    /// @return
    ///   Offset in bytes of mapped area from beginning of the file.
    ///   Always return adjusted value even if data was not successfully mapped.
    /// @sa
    ///    GetRealPtr, GetRealSize, GetOffset 
    off_t GetRealOffset(void) const;

    /// Get real length of the mapped area.
    ///
    /// Number of really mapped bytes of file. This length value can be
    /// increased if "offset" is not a multiple of the allocation granularity.
    /// @return
    ///   - Length in bytes of the mapped area, or
    ///   - 0 if not mapped.
    /// @sa
    ///    GetRealPtr, GetRealOffset, GetSize 
    size_t GetRealSize(void) const;

    /// Flush by writing all modified copies of memory pages to the
    /// underlying file.
    ///
    /// NOTE: By default data will be flushed in the destructor.
    /// @return
    ///   - TRUE, if all data was flushed successfully.
    ///   - FALSE, if not mapped or if an error occurs.
    bool Flush(void) const;

    /// Unmap file view from memory.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    bool Unmap(void);

    /// Advise on mapped memory map usage.
    ///
    /// @param advise
    ///   Advise on expected memory usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful.
    ///   - FALSE, if memory advise operation not successful, or is not supported
    ///     on current platform.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(EMemMapAdvise advise) const;

private:
    // Check that file is mapped, throw exception otherwise.
    void x_Verify(void) const;

private:
    // Values for user
    void*   m_DataPtr;     ///< Pointer to the beginning of the mapped area.
                           ///> The user seen this one.
    off_t   m_Offset;      ///< Requested starting offset of the
                           ///< mapped area from beginning of file.
    size_t  m_Length;      ///< Requested length of the mapped area.

    // Internal real values
    void*   m_DataPtrReal; ///< Real pointer to the beginning of the mapped
                           ///< area which should be fried later.
    off_t   m_OffsetReal;  ///< Corrected starting offset of the
                           ///< mapped area from beginning of file.
    size_t  m_LengthReal;  ///< Corrected length of the mapped area.

    // Friend classes
    friend class CMemoryFile;

private:
    // Prevent copying
    CMemoryFileSegment(const CMemoryFileSegment&);
    void operator=(const CMemoryFileSegment&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFileMap --
///
/// Define class for support a partial file memory mapping.
///
/// Note, that all mapped into memory file segments have equal protect and
/// share attributes, because they restricted with file open mode.
/// Note, that the mapping file must exists or can be create/extended.
/// If file size changes after mapping, the effect of references to portions
/// of the mapped region that correspond to added or removed portions of
/// the file is unspecified.
///
/// Throws an exceptions on error.

class NCBI_XNCBI_EXPORT CMemoryFileMap : public CMemoryFile_Base
{
public:
    /// Constructor.
    ///
    /// Initialize the memory mapping on file "file_name".
    /// @param filename
    ///   Name of file to map to memory.
    /// @param protect_attr
    ///   Specify operations permitted on memory mapped file.
    /// @param share_attr
    ///   Specify if change to memory mapped file can be shared or not.
    /// @param mode
    ///   File open mode.
    /// @param max_file_len
    ///   The size of created file if 'mode' parameter is eCreate or eExtend.
    ///   File will be never truncated if open mode is eExtend and real file
    ///   size is more that specified maximum length.
    /// @sa
    ///   EMemMapProtect, EMemMapShare, EOpenMode
    /// @note
    ///   MS Windows: If file is open in eMMP_Read mode with eMMS_Private
    ///   protection, that memory pages will be mapped in exclusive mode,
    ///   and any other process cannot access the same file for writing.
    CMemoryFileMap(const string&  file_name,
                   EMemMapProtect protect_attr = eMMP_Read,
                   EMemMapShare   share_attr   = eMMS_Shared,
                   EOpenMode      mode         = eDefault,
                   Uint8          max_file_len = 0);

    /// Destructor.
    ///
    /// Calls Unmap() and cleans up memory.
    ~CMemoryFileMap(void);

    /// Map file segment.
    ///
    /// @param offset
    ///   The file offset where mapping is to begin. If the offset is not
    ///   a multiple of the allocation granularity, that it can be decreased 
    ///   to match it. The "length" value will be automatically increased on
    ///   the difference between passed and real offsets. The real offset can
    ///   be obtained using GetRealOffset(). Cannot accept values less than 0.
    /// @param length
    ///   Number of bytes to map. This value can be increased if "offset"
    ///   is not a multiple of the allocation granularity.
    ///   The real length of mapped region can be obtained using
    ///   GetRealSize() method.
    ///   The value 0 means that all file size will be mapped.
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    /// @sa
    ///   Unmap, GetOffset, GetSize, GetRealOffset, GetRealSize
    /// @note
    ///   MS Windows: If file is open in eMMP_Read mode with eMMS_Private
    ///   protection, that memory pages will be mapped in exclusive mode,
    ///   and any other process cannot access the same file for writing.
    void* Map(off_t offset, size_t length);

    /// Unmap file segment.
    ///
    /// @param ptr
    ///   Pointer returned by Map().
    /// @return
    ///   TRUE on success; or FALSE on error.
    /// @sa
    ///   Map
    bool Unmap(void* ptr);

    /// Unmap all mapped segment.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    ///   In case of error some segments can be not unmapped.
    bool UnmapAll(void);

    /// Get offset of the mapped segment from beginning of the file.
    ///
    /// @param prt
    ///   Pointer to mapped data returned by Map().
    /// @return
    ///   Offset in bytes of mapped segment from beginning of the file.
    ///   Returned value is a value of "offset" parameter passed
    ///   to Map() method for specified "ptr".
    off_t GetOffset(void* ptr) const;

    /// Get length of the mapped segment.
    ///
    /// @param prt
    ///   Pointer to mapped data returned by Map().
    /// @return
    ///   Length in bytes of the mapped area.
    ///   Returned value is a value of "length" parameter passed
    ///   to Map() method for specified "ptr".
    size_t GetSize(void* ptr) const;

    /// Get length of the mapped file.
    ///
    /// @return
    ///   Size in bytes of the mapped file.
    Int8 GetFileSize(void) const;

    /// Flush memory mapped file segment.
    ///
    /// Flush specified mapped segment by writing all modified copies of
    /// memory pages to the underlying file.
    ///
    /// NOTE: By default data will be flushed in the destructor.
    /// @param prt
    ///   Pointer to mapped data returned by Map().
    /// @return
    ///   - TRUE, if all data was flushed successfully.
    ///   - FALSE, if an error occurs.
    bool Flush(void* ptr) const;

    /// Get pointer to memory mapped file segment by pointer to data.
    ///
    /// @param prt
    ///   Pointer to mapped data returned by Map().
    /// @return
    ///   Pointer to memory file mapped segment. 
    const CMemoryFileSegment* GetMemoryFileSegment(void* ptr) const;

    /// Advise on mapped memory map usage.
    ///
    /// @param advise
    ///   Advise on expected memory usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful.
    ///   - FALSE, if memory advise operation not successful, or is not supported
    ///     on current platform.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(void* ptr, EMemMapAdvise advise) const;

protected:
    /// Open file mapping for file with name m_FileName.
    void x_Open(void);
    /// Unmap mapped memory and close mapped file.
    void x_Close(void);
    /// Create new file or rewrite existent with zeros.
    void x_Create(Uint8 size);
    /// Extend file size from 'size' to 'new_size' with zero bytes.
    /// Note, that 'new_size' should be greater that current file size 'size'.
    void x_Extend(Uint8 size, Uint8 new_size);

    /// Get pointer to memory mapped file segment by pointer to data.
    CMemoryFileSegment* x_GetMemoryFileSegment(void* ptr) const;

protected:
    string              m_FileName;  ///< File name. 
    SMemoryFileHandle*  m_Handle;    ///< Memory file handle.
    SMemoryFileAttrs*   m_Attrs;     ///< Specify operations permitted on
                                     ///< memory mapped file and mapping mode.

    typedef map<void*,CMemoryFileSegment*> TSegments;
    TSegments           m_Segments;  ///< Map of pointers to mapped segments.

private:
    // Prevent copying
    CMemoryFileMap(const CMemoryFileMap&);
    void operator=(const CMemoryFileMap&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFile --
///
/// Define class for support file memory mapping.
///
/// This is a simple version of the CMemoryFileMap class, supported one
/// mapped segment only.
///
/// Note, that the mapping file must exists or can be create/extended.
/// If file size changes after mapping, the effect of references to portions
/// of the mapped region that correspond to added or removed portions of
/// the file is unspecified.
///
/// Throws an exceptions on error.

class NCBI_XNCBI_EXPORT CMemoryFile : public CMemoryFileMap
{
public:
    /// Constructor.
    ///
    /// Initialize the memory mapping on file "file_name".
    /// @param filename
    ///   Name of file to map to memory.
    /// @param protect_attr
    ///   Specify operations permitted on memory mapped file.
    /// @param share_attr
    ///   Specify if change to memory mapped file can be shared or not.
    /// @param offset
    ///   The file offset where mapping is to begin.
    /// @param length
    ///   Number of bytes to map.
    ///   The value 0 means that all file size will be mapped.
    /// @param mode
    ///   File open mode.
    /// @param max_file_len
    ///   The size of created file if 'mode' parameter is eCreate or eExtend.
    ///   File will be never truncated if open mode is eExtend and real file
    ///   size is more that specified maximum length.
    /// @sa
    ///   EMemMapProtect, EMemMapShare, EOpenMode
    /// @sa
    ///   EMemMapProtect, EMemMapShare, Map
    /// @note
    ///   MS Windows: If file is open in eMMP_Read mode with eMMS_Private
    ///   protection, that memory pages will be mapped in exclusive mode,
    ///   and any other process cannot access the same file for writing.
    CMemoryFile(const string&  file_name,
                EMemMapProtect protect_attr = eMMP_Read,
                EMemMapShare   share_attr   = eMMS_Shared,
                off_t          offset       = 0,
                size_t         lendth       = 0,
                EOpenMode      mode         = eDefault,
                Uint8          max_file_len = 0);

    /// Map file.
    ///
    /// @param offset
    ///   The file offset where mapping is to begin. If the offset is not
    ///   a multiple of the allocation granularity, that it can be decreased 
    ///   to match it. The "length" value will be automatically increased on
    ///   the difference between passed and real offsets. The real offset can
    ///   be obtained using GetRealOffset(). Cannot accept values less than 0.
    /// @param length
    ///   Number of bytes to map. This value can be increased if "offset"
    ///   is not a multiple of the allocation granularity.
    ///   The real length of mapped region can be obtained using
    ///   GetRealSize() method.
    ///   The value 0 means that file will be mapped from 'offset'
    ///   to the end of file.
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    /// @sa
    ///   Unmap, GetPtr, GetOffset, GetSize, Extend
    void* Map(off_t offset = 0, size_t length = 0);

    /// Unmap file if mapped.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    /// @sa
    ///   Map, Extend
    bool Unmap(void);

    /// Extend length of the mapped region.
    ///
    /// If the sum of the current offset (from Map() method) and new size of
    /// the mapped region is more than current file size, that file size will
    /// be increased, added space filed with zeros and mapped region will
    /// be remapped. 
    /// @param new_length
    ///   New length of the mapped region. 
    ///   The value 0 means that file will be mapped from 'offset'
    ///   to the end of file.
    /// @return
    ///   New pointer to start of data.
    /// @sa
    ///   GetPtr, GetOffset, GetSize
    void* Extend(size_t new_lendth = 0);

    /// Get pointer to beginning of data.
    ///
    /// @return
    ///   Pointer to start of data.
    /// @sa
    ///   Map, Extend
    void* GetPtr(void) const;

    /// Get offset of the mapped area from beginning of the file.
    ///
    /// @return
    ///   Offset in bytes of mapped area from beginning of the file.
    off_t GetOffset(void) const;

    /// Get length of the mapped region.
    ///
    /// @return
    ///   - Length in bytes of the mapped region, or
    ///   - 0 if not mapped or file is empty.
    size_t GetSize(void) const;

    /// Flush by writing all modified copies of memory pages to the
    /// underlying file.
    ///
    /// NOTE: By default data will be flushed in the destructor.
    /// @return
    ///   - TRUE, if all data was flushed successfully.
    ///   - FALSE, if an error occurs.
    bool Flush(void) const;

    /// Advise on memory map usage.
    ///
    /// @param advise
    ///   Advise on expected memory usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful.
    ///   - FALSE, if memory advise operation not successful, or is not supported
    ///     on current platform.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(EMemMapAdvise advise) const;

private:
    // Check that file is mapped, throw exception otherwise.
    void x_Verify(void) const;

private:
    void* m_Ptr;   ///< Pointer to mapped view of file.

private:
    // Prevent copying
    CMemoryFile(const CMemoryFile&);
    void operator=(const CMemoryFile&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  Find files algorithms
//

/// File finding flags
enum EFindFiles {
    fFF_File       = (1<<0),             ///< find files
    fFF_Dir        = (1<<1),             ///< find directories
    fFF_Recursive  = (1<<2),             ///< descend into sub-dirs
    fFF_Nocase     = (1<<3),             ///< case-insensitive name search
    fFF_Default    = fFF_File | fFF_Dir  ///< default behavior
};
/// Bitwise OR of "EFindFiles"
typedef int TFindFiles; 


/// Find files in the specified directory
template<class TFindFunc>
TFindFunc FindFilesInDir(const CDir&            dir,
                         const vector<string>&  masks,
                         const vector<string>&  masks_subdir,
                         TFindFunc&             find_func,
                         TFindFiles             flags = fFF_Default)
{
    TFindFiles find_type = flags & (fFF_Dir | fFF_File);
    if ( find_type == 0 ) {
        // nothing to find
        return find_func;
    }
    auto_ptr<CDir::TEntries> 
        contents(dir.GetEntriesPtr(kEmptyStr, CDir::fIgnoreRecursive | CDir::fIgnorePath));
    if (contents.get() == NULL) {
        // error
        return find_func;
    }

    NStr::ECase use_case = (flags & fFF_Nocase) ? NStr::eNocase : NStr::eCase;

    string path;
    if ( dir.GetPath().length() ) {
        path = CDirEntry::AddTrailingPathSeparator(dir.GetPath());
    }
    ITERATE(CDir::TEntries, it, *contents) {
        CDirEntry& dir_entry = **it;
        string name = dir_entry.GetPath();
        dir_entry.Reset(CDirEntry::MakePath(path, name));
        
        TFindFiles entry_type = fFF_Dir | fFF_File; // unknown
        if ( CDirEntry::MatchesMask(name, masks, use_case) ) {
            if ( find_type != (fFF_Dir | fFF_File) ) {
                // need to check actual entry type
                entry_type = dir_entry.IsDir()? fFF_Dir: fFF_File;
            }
            if ( (entry_type & find_type) != 0 ) {
                // entry type matches
                find_func(dir_entry);
            }
        }
        if ( (flags & fFF_Recursive) &&
             (entry_type & fFF_Dir) /*possible dir*/ &&
             CDirEntry::MatchesMask(name, masks_subdir, use_case) &&
             (entry_type == fFF_Dir || dir_entry.IsDir()) /*real dir*/ ) {
            CDir nested_dir(dir_entry.GetPath());
            find_func = FindFilesInDir(nested_dir, masks, masks_subdir,
                                       find_func, flags);
        }
    } // ITERATE
    return find_func;
}


/// Find files in the specified directory
template<class TFindFunc>
TFindFunc FindFilesInDir(const CDir&   dir,
                         const CMask&  masks,
                         const CMask&  masks_subdir,
                         TFindFunc&    find_func,
                         TFindFiles    flags = fFF_Default)
{
    TFindFiles find_type = flags & (fFF_Dir | fFF_File);
    if ( find_type == 0 ) {
        // nothing to find
        return find_func;
    }
    auto_ptr<CDir::TEntries> 
        contents(dir.GetEntriesPtr(kEmptyStr, CDir::fIgnoreRecursive |  CDir::fIgnorePath));
    if (contents.get() == NULL) {
        // error
        return find_func;
    }

    NStr::ECase use_case = (flags & fFF_Nocase) ? NStr::eNocase : NStr::eCase;

    string path;
    if ( dir.GetPath().length() ) {
        path = CDirEntry::AddTrailingPathSeparator(dir.GetPath());
    }
    ITERATE(CDir::TEntries, it, *contents) {
        CDirEntry& dir_entry = **it;
        string name = dir_entry.GetPath();
        dir_entry.Reset(CDirEntry::MakePath(path, name));

        TFindFiles entry_type = fFF_Dir | fFF_File; // unknown
        if ( masks.Match(name, use_case) ) {
            if ( find_type != (fFF_Dir | fFF_File) ) {
                // need to check actual entry type
                entry_type = dir_entry.IsDir()? fFF_Dir: fFF_File;
            }
            if ( (entry_type & find_type) != 0 ) {
                // entry type matches
                find_func(dir_entry);
            }
        }
        if ( (flags & fFF_Recursive) &&
             (entry_type & fFF_Dir) /*possible dir*/ &&
             masks_subdir.Match(name, use_case) &&
             (entry_type == fFF_Dir || dir_entry.IsDir()) /*real dir*/ ) {
            CDir nested_dir(dir_entry.GetPath());
            find_func = FindFilesInDir(nested_dir, masks, masks_subdir,
                                       find_func, flags);
        }
    } // ITERATE entries
    return find_func;
}



/////////////////////////////////////////////////////////////////////////////
///
/// Generic algorithm for file search
///
/// Algorithm scans the provided directories using iterators,
/// finds files to match the masks and stores all calls functor
/// object for all found entries.
/// Functor call should match: void Functor(const CDirEntry& dir_entry).
///
/// The difference between FindFiles<> and FileFiles2<> is that last one
/// use two different masks - one for dir entries (files and/or subdirs)
/// and second for subdirectories, that will be used for recursive
/// search. FindFiles<> use one set of masks for all subdirectories with
/// recursive search.
///

template<class TPathIterator, 
         class TFindFunc>
TFindFunc FindFiles(TPathIterator         path_begin,
                    TPathIterator         path_end,
                    const vector<string>& masks,
                    TFindFunc&            find_func,
                    TFindFiles            flags = fFF_Default)
{
    vector<string> masks_empty;
    for (; path_begin != path_end; ++path_begin) {
        const string& dir_name = *path_begin;
        CDir dir(dir_name);
        find_func = FindFilesInDir(dir, masks, masks_empty, find_func, flags);
    }
    return find_func;
}


template<class TPathIterator, 
         class TFindFunc>
TFindFunc FindFiles2(TPathIterator         path_begin,
                     TPathIterator         path_end,
                     const vector<string>& masks,
                     const vector<string>& masks_subdir,
                     TFindFunc&            find_func,
                     TFindFiles            flags = fFF_Default)
{
    for (; path_begin != path_end; ++path_begin) {
        const string& dir_name = *path_begin;
        CDir dir(dir_name);
        find_func = FindFilesInDir(dir, masks, masks_subdir,
                                   find_func, flags);
    }
    return find_func;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Generic algorithm for file search
///
/// Algorithm scans the provided directories using iterators,
/// finds files to match the masks and stores all calls functor
/// object for all found entries.
/// Functor call should match: void Functor(const CDirEntry& dir_entry).
///

template<class TPathIterator, 
         class TMaskIterator, 
         class TFindFunc>
TFindFunc FindFiles(TPathIterator path_begin,
                    TPathIterator path_end,
                    TMaskIterator mask_begin,
                    TMaskIterator mask_end,
                    TFindFunc&    find_func,
                    TFindFiles    flags = fFF_Default)
{
    vector<string> masks;
    for (; mask_begin != mask_end; ++mask_begin) {
        masks.push_back(*mask_begin);
    }
    return FindFiles(path_begin, path_end, masks, find_func, flags);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Generic algorithm for file search
///
/// Algorithm scans the provided directories using iterators,
/// finds files to match the masks and stores all calls functor
/// object for all found entries.
/// Functor call should match: void Functor(const CDirEntry& dir_entry).
///

template<class TPathIterator, 
         class TFindFunc>
TFindFunc FindFiles(TPathIterator path_begin,
                    TPathIterator path_end,
                    const CMask&  masks,
                    TFindFunc&    find_func,
                    TFindFiles    flags = fFF_Default)
{
    CMaskFileName masks_empty;
    for (; path_begin != path_end; ++path_begin) {
        const string& dir_name = *path_begin;
        CDir dir(dir_name);
        find_func = FindFilesInDir(dir, masks, masks_empty, find_func, flags);
    }
    return find_func;
}


template<class TPathIterator, 
         class TFindFunc>
TFindFunc FindFiles2(TPathIterator path_begin,
                     TPathIterator path_end,
                     const CMask&  masks,
                     const CMask&  masks_subdir,
                     TFindFunc&    find_func,
                     TFindFiles    flags = fFF_Default)
{
    for (; path_begin != path_end; ++path_begin) {
        const string& dir_name = *path_begin;
        CDir dir(dir_name);
        find_func = FindFilesInDir(dir, masks, masks_subdir,
                                   find_func, flags);
    }
    return find_func;
}


/// Functor for generic FindFiles, adds file name to the specified container
template<class TNames>
class CFindFileNamesFunc
{
public:
    CFindFileNamesFunc(TNames& names) : m_FileNames(&names) {}

    void operator()(const CDirEntry& dir_entry)
    {
        m_FileNames->push_back(dir_entry.GetPath());
    }
protected:
    TNames*  m_FileNames;
};


/////////////////////////////////////////////////////////////////////////////
///
/// Utility algorithm scans the provided directories using iterators
/// finds files to match the masks and stores all found files in 
/// the container object.
///

template<class TContainer, class TPathIterator>
void FindFiles(TContainer&           out, 
               TPathIterator         first_path, 
               TPathIterator         last_path, 
               const vector<string>& masks,
               TFindFiles            flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles(first_path, last_path, masks, func, flags);
}

template<class TContainer, class TPathIterator>
void FindFiles2(TContainer&           out, 
                TPathIterator         first_path, 
                TPathIterator         last_path, 
                const vector<string>& masks,
                const vector<string>& masks_subdir,
                TFindFiles            flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles2(first_path, last_path, masks, masks_subdir, func, flags);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Utility algorithm scans the provided directories using iterators
/// finds files to match the masks and stores all found files in 
/// the container object.
///

template<class TContainer, class TPathIterator>
void FindFiles(TContainer&    out, 
               TPathIterator  first_path, 
               TPathIterator  last_path, 
               const CMask&   masks,
               TFindFiles     flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles(first_path, last_path, masks, func, flags);
}

template<class TContainer, class TPathIterator>
void FindFiles2(TContainer&    out, 
                TPathIterator  first_path, 
                TPathIterator  last_path, 
                const CMask&   masks,
                const CMask&   masks_subdir,
                TFindFiles     flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles2(first_path, last_path, masks, masks_subdir, func, flags);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Utility algorithm scans the provided directories using iterators
/// finds files to match the masks and stores all found files in 
/// the container object.
///

template<class TContainer, class TPathIterator, class TMaskIterator>
void FindFiles(TContainer&    out, 
               TPathIterator  first_path,
               TPathIterator  last_path, 
               TMaskIterator  first_mask,
               TMaskIterator  last_mask,
               TFindFiles     flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles(first_path, last_path, first_mask, last_mask, func, flags);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Utility function working like glob(): takes a pattern and fills the
/// result list with files/directories matching the pattern.
///

void NCBI_XNCBI_EXPORT FindFiles(const string& pattern,
                                 list<string>& result,
                                 TFindFiles flags);



/////////////////////////////////////////////////////////////////////////////
///
/// Base class for CFileIO, CFileReader, CFileWriter, CFileReaderWriter.
///
/// Defines common types.

class NCBI_XNCBI_EXPORT CFileIO_Base
{
public:
    /// File open mode.
    enum EOpenMode {
        ///< Create a new file, or truncate an existing one.
        eCreate,
        ///< Create a new file, or fail if the file already exists.
        eCreateNew,
        ///< Open an existing file, or fail if the file does not exist.
        eOpen,
        ///< Open an existing file, or create a new one.
        eOpenAlways,
        /// Open an existing file and truncate its size to 0.
        /// Fail if the file does not exist.
        eTruncate
    };

    /// Which I/O operations permitted on the file.
    enum EAccessMode {
        eRead,        ///< File can be read.
        eWrite,       ///< File can be written.
        eReadWrite    ///< File can be read and written.
    };

    /// Sharing mode for opened file.
    /// @note
    ///   If OS does not support sharing mode for files, that it will be
    ///   ignored.  But you can use CFileLock to lock a file or its part.
    /// @sa CFileLock
    enum EShareMode {
        /// Enables subsequent open operations on the file that request read
        /// access. Otherwise, other processes cannot open the file for reading. 
        eShareRead,
        /// Enables subsequent open operations on the file that request write
        /// access. Otherwise, other processes cannot open the file for writing. 
        eShareWrite,
        /// Combines both eShareRead and eShareWrite modes.
        eShare,
        /// Open file for exclusive access. Disables any subsequent open
        /// operations on the file.
        eExclusive
    };

    /// Which starting point to use for the moves of the file pointer.
    enum EPositionMoveMethod {
        eBegin,       ///< Absolute position from beginning of the file.
        eCurrent,     ///< Relative to current position.
        eEnd          ///< The starting point is the current EOF position.
    };
};


/////////////////////////////////////////////////////////////////////////////
///
/// Class for support low level input/output for files.
///
/// Throw CFileException/CFileErrnoException on error.

class NCBI_XNCBI_EXPORT CFileIO : public CFileIO_Base
{
public:
    /// Default constructor
    CFileIO(void);

    /// Destruct object closing system handle if necessary
    ~CFileIO(void);

    /// Open file.
    void Open(const string& filename, EOpenMode open_mode,
              EAccessMode access_mode, EShareMode share_mode = eShare);

    /// Controls how temporary file is removed.
    enum EAutoRemove {
        eDoNotRemove,           ///< Do not ever remove temporary file.
        eRemoveInClose,         ///< Remove temporary file immediately
                                ///< after closing its handle in Close().
        eRemoveASAP,            ///< Remove the file at the earliest
                                ///< possible moment (in CreateTemporary()
                                ///< on UNIX).
    };
    /// Create temporary file in the specified directory.
    /// The prefix argument is used to generate a unique file name.
    void CreateTemporary(const string& dir, const string& prefix,
                         EAutoRemove auto_remove = eRemoveInClose);

    /// Close file.
    void Close(void);

    /// Read file.
    ///
    /// @return
    ///   The number of bytes read (zero indicates end of file, 
    ///   or that 'count' is zero).
    size_t Read(void* buf, size_t count) const;

    /// Write file.
    ///
    /// Always write all 'count' bytes of data to the file.
    /// @return
    ///   The number of bytes written (equal to 'count'). 
    size_t Write(const void* buf, size_t count) const;

    /// Flush file buffers.
    void Flush(void) const;

    /// Return file path and name as it was specified in the
    /// Open() method or created in CreateTemporary().
    const string& GetPathname(void) const { return m_Pathname; }

    /// Return system file handle associated with the file.
    TFileHandle GetFileHandle(void) const { return m_Handle; };

    /// Close previous handle if needed and use given handle for all I/O.
    void SetFileHandle(TFileHandle handle);

    /// Get file position.
    ///
    /// @return
    ///   Current file position.
    Uint8 GetFilePos(void) const;

    /// Set file position from beginning of the file.
    void SetFilePos(Uint8 position) const;

    /// Set file position using 'move_method'.
    ///
    /// 'offset' parameter is a number of bytes to move the file pointer.
    /// A positive value moves the pointer forward in the file and
    ///  a negative value moves the file pointer backward.
    void SetFilePos(Int8 offset, EPositionMoveMethod move_method) const;

    /// Get file size.
    ///
    /// @return
    ///   Size of the file.
    Uint8 GetFileSize(void) const;

    /// Set new size for the file.
    ///
    /// This method can be used to truncate or extend the file.
    /// @note
    ///   The file must be opened with write access rights.
    ///   Function repositions the offset of the file descriptor to the EOF.
    /// @param length
    ///   New file size.
    ///   If the file was previously longer than length, bytes past "length"
    ///   will no longer be accessible. If it was shorter, the contents of
    ///   the file between the old EOF position and the new position are
    ///   not defined. Usually it will be read in as zeros, but this depends
    ///   from OS.
    /// @param pos
    ///   Defines how to set current file position after changing file size.
    ///   eCurrent means that file position does not change, eBegin and eEnd
    ///   move it to the start or the end of the file accordingly.
    void SetFileSize(Uint8 length, EPositionMoveMethod pos = eCurrent) const;

    /// Define whether the open file handle needs to be closed
    /// in the destructor.
    void SetAutoClose(bool auto_close = true) { m_AutoClose = auto_close; }

    /// Define whether the temporary file created by CreateTemporary()
    /// must be automatically removed in Close(). This method will
    /// also work for regular files created with Open().
    void SetAutoRemove(EAutoRemove auto_remove = eRemoveInClose)
        { m_AutoRemove = auto_remove; }

protected:
    string       m_Pathname;    ///< File path and name.
    TFileHandle  m_Handle;      ///< System file handle.
    bool         m_AutoClose;   ///< Need to close file handle in destructor.
    EAutoRemove  m_AutoRemove;  ///< When (if ever) should the temporary
                                ///< file be removed.

private:
    // prevent copying
    CFileIO(const CFileIO&);
    void operator=(const CFileIO&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// File based IReader/IWriter/IReaderWriter with low level IO for speed
///
/// Throw CFileException/CFileErrnoException on error.

class NCBI_XNCBI_EXPORT CFileReaderWriter_Base : public CFileIO_Base
{
public:
    /// Default constructor
    CFileReaderWriter_Base(void) {};
    /// Return system file handle associated with the file.
    TFileHandle GetFileHandle(void) { return m_File.GetFileHandle(); };
    /// Get an underlaying fileio object
    CFileIO& GetFileIO(void) { return m_File; }

protected:
    CFileIO  m_File;
private:
    // prevent copying
    CFileReaderWriter_Base(const CFileReaderWriter_Base&);
    void operator=(const CFileReaderWriter_Base&);
};


class NCBI_XNCBI_EXPORT CFileReader : public IReader,
                                      public CFileReaderWriter_Base
{
public:
    /// Construct CFileReader for reading from the file with name 'filename'.
    /// Throw CFileErrnoException on error.
    CFileReader(const string& filename,
                EShareMode share_mode = eShareRead);

    /// Construct CFileReader for reading from system handle 'handle'.
    /// Specified handle should have read access right.
    CFileReader(TFileHandle handle);

    /// Return a new IReader object corresponding to the given
    /// filename, taking "-" (but not "./-") to read from the standard input.
    static IReader* New(const string& filename, 
                        EShareMode share_mode = eShareRead);

    /// Virtual methods from IReader
    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* count);

private:
    // prevent copying
    CFileReader(const CFileReader&);
    void operator=(const CFileReader&);
};


class NCBI_XNCBI_EXPORT CFileWriter : public IWriter,
                                      public CFileReaderWriter_Base
{
public:
    /// Construct CFileWriter for writing to the file with name 'filename'.
    /// Throw CFileErrnoException on error.
    CFileWriter(const string& filename,
                EOpenMode  open_mode  = eCreate,
                EShareMode share_mode = eShareRead);

    /// Construct CFileWriter for writing to system handle 'handle'.
    /// Specified handle should have read/write access rights.
    CFileWriter(TFileHandle handle);

    /// Return a new IWriter object corresponding to the given
    /// filename, taking "-" (but not "./-") to write to the standard output.
    static IWriter* New(const string& filename,
                        EOpenMode  open_mode  = eCreate,
                        EShareMode share_mode = eShareRead);

    /// Virtual methods from IWriter
    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t* bytes_written = 0);
    virtual ERW_Result Flush(void);

private:
    // prevent copying
    CFileWriter(const CFileWriter&);
    void operator=(const CFileWriter&);
};


class NCBI_XNCBI_EXPORT CFileReaderWriter : public IReaderWriter,
                                            public CFileReaderWriter_Base
{
public:
    /// Construct CFileReaderWriter for reading/writing to/from
    /// the file with name 'filename'. 
    /// Throw CFileErrnoException on error.
    CFileReaderWriter(const string& filename,
                      EOpenMode  open_mode  = eOpen,
                      EShareMode share_mode = eShareRead);

    /// Construct CFileReaderWriter for writing to system handle 'handle'.
    /// Specified handle should have read and write access rights.
    CFileReaderWriter(TFileHandle handle);

    /// Return a new IReaderWriter object corresponding to the given
    /// filename.
    static IReaderWriter* New(const string& filename,
                              EOpenMode  open_mode  = eOpen,
                              EShareMode share_mode = eShareRead);

    /// Virtual methods from IReaderWriter
    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* count);
    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t* bytes_written = 0);
    virtual ERW_Result Flush(void);

private:
    // prevent copying
    CFileReaderWriter(const CFileReaderWriter&);
    void operator=(const CFileReaderWriter&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// File locking
///
/// Lock a given file (by file descriptor or the file name) to read/write.
///
/// Notes:
///
/// 1) On majority of Unix platforms all locks are advisory, this means that 
///    cooperating processes may use locks to coordinate access to a file
///    between themselves, but programs are also free to ignore locks and
///    access the file in any way they choose to.
///    MS Windows supports only mandatory locks, and operating system fully
///    enforces them.
///
/// 2) After locking the file you should work with this file using ONLY
///    specified file descriptor, or if the constructor with file name was
///    used, obtain a file descriptor from CFileLock object using method
///    GetFileHandle(). Because on Unix all locks associated with a file
///    for a given process are removed when any file descriptor for that
///    file is closed by that process, even if a lock was never requested
///    for that file descriptor. On Windows you cannot open the file for
///    writing, if it already have an exclusive lock, even established
///    by the same process.
///    So, often is better to open file somewhere else and pass its file
///    descriptor to CFileLock class. In this case you have more control
///    over file. But note that lock type should match to the file open mode.
///
/// 3) If you close a file that have locks, the locks will be unlocked by
///    the operating system. However, the time it takes for the operating
///    system to unlock these locks depends upon available system resources.
///    Therefore, it is recommended that your process explicitly remove all
///    locks, before closing a file. If this is not done, access to file
///    may be denied if the operating system has not yet unlocked them.
///
///  4) Locks can be inherited or not by a child process, depending from OS.
///
///  5) Locked file cannot be empty. CFileLock can work with empty files,
///     depending from OS, but it is better to avoid this for compatibility.
///
/// All methods of this class except the destructor throw exceptions
/// CFileErrnoException on errors.

class NCBI_XNCBI_EXPORT CFileLock
{
public:
    /// Type of file lock.
    ///
    /// Shared lock allows all processes to read from the locked portion
    /// of the file, while denying to write into it.
    /// Exclusive lock denies other processes both read and write to
    /// the locked portion of the file, while allowing the locking process
    /// to read or write through a specified or obtained file handle.
    typedef enum {
        eShared,       ///< "read" lock.
        eExclusive     ///< "write" lock.
    } EType;

    /// Flags, used in constructors.
    ///
    /// Default flag in each group have priority above non-default,
    /// if they are used together.
    enum EFlags {
        /// Lock file using parameters specified in constructor.
        fLockNow        = (1 << 1), 
        fLockLater      = (1 << 2),
        /// Automatically remove all obtained locks in the destructor.
        /// Note, that you still can unlock any segment. All remaining locks
        /// will be removed in the destructor.
        fAutoUnlock     = (1 << 3),
        fNoAutoUnlock   = (1 << 4),
        /// Default flags
        fDefault        = fLockNow | fAutoUnlock
    };
    typedef unsigned int TFlags;   ///< Binary OR of "EFlags"


    /// Construct CFileLock for locking a file with a given name 'filename'.
    /// File will be automatically closed in destructor and all locks removed.
    /// Throw CFileException if file doesn't exist, or on error.
    /// @sa Lock, Unlock, GetFileHandle
    CFileLock(const string& filename,
              TFlags flags  = fDefault,
              EType  type   = eShared,
              off_t  offset = 0,
              size_t length = 0);
    CFileLock(const char* filename,
              TFlags flags  = fDefault,
              EType  type   = eShared,
              off_t  offset = 0,
              size_t length = 0);

    /// Construct CFileLock for locking file by system file handle 'handle'.
    /// @note
    ///   The file will not be closed at CFileLock destruction.
    /// @sa Lock, LockSegment
    CFileLock(TFileHandle handle,
              TFlags flags  = fDefault,
              EType  type   = eShared,
              off_t  offset = 0,
              size_t length = 0);

    /// Destruct the CFileLock, close file and remove all locks if necessary.
    ~CFileLock(void);

    /// Lock file
    ///
    /// Lock whole file, or the part of the file.
    /// Previous lock will be removed. It do not remove locks,
    /// established on the file somewhere else.
    /// Throw CFileException if the lock cannot be obtained (since someone
    /// else has it locked already), or on error.
    /// @param type
    ///   Type of the lock, one of eShared or eExclusive.
    /// @param offset
    ///   The file offset where lock starts. Cannot accept values less than 0.
    /// @param length
    ///   Number of bytes to lock.
    ///   The value 0 means that whole file will be locked.
    /// @sa Unlock
    void Lock(EType type, off_t offset = 0, size_t length = 0);

    /// Unlock file.
    ///
    /// Unlock range of the file previously locked using Lock() method.
    /// The file remains open. Note, that this method cannot remove locks,
    /// established on the file somewhere else. Only closing a file can
    /// unlock all locks.
    /// @sa Lock
    void Unlock(void);

    /// Return system file handle.
    ///
    /// If you want to read/write from/to the file, you should work with it
    /// using only the file descriptor, obtained using this method.
    /// It can be the same file descriptor as was given in the constructor.
    /// @return
    ///   File descriptor associated with the file. 
    TFileHandle GetFileHandle(void) { return m_Handle; };

protected:
    /// Auxiliary method for constructors.
    void x_Init(const char* filename, EType type, off_t offset, size_t length);

private:
    TFileHandle    m_Handle;      ///< System file handle.
    bool           m_CloseHandle; ///< Need to close file handle in destructor.
    TFlags         m_Flags;       ///< General flags.
    bool           m_IsLocked;    ///< Lock established.
    AutoPtr<SLock> m_Lock;        ///< Offset and length of the locked area.

private:
    // Prevent copying
    CFileLock(const CFileLock&);
    void operator=(const CFileLock&);
};


/* @} */



//////////////////////////////////////////////////////////////////////////////
//
// Inline
//


// CDirEntry

inline
CDirEntry::CDirEntry(void)
{
    return;
}

inline
const string& CDirEntry::GetPath(void) const
{
    return m_Path;
}

inline
string CDirEntry::GetName(void) const
{
    string title, ext;
    SplitPath(GetPath(), 0, &title, &ext);
    return title + ext;
}

inline
string CDirEntry::GetBase(void) const
{
    string base;
    SplitPath(GetPath(), 0, &base);
    return base;
}

inline
string CDirEntry::GetExt(void) const
{
    string ext;
    SplitPath(GetPath(), 0, 0, &ext);
    return ext;
}

inline
bool CDirEntry::IsFile(EFollowLinks follow) const
{
    return GetType(follow) == eFile;
}

inline
bool CDirEntry::IsDir(EFollowLinks follow) const
{
    return GetType(follow) == eDir;
}

inline
bool CDirEntry::IsLink(void) const
{
    return GetType(eIgnoreLinks) == eLink;
}

inline
bool CDirEntry::Exists(void) const
{
    return GetType() != eUnknown;
}

inline
bool CDirEntry::MatchesMask(const string& name, const string& mask,
                            NStr::ECase use_case)
{
    return NStr::MatchesMask(name, mask, use_case);
}

inline
bool CDirEntry::MatchesMask(const string& name, const CMask& mask,
                            NStr::ECase use_case) 
{
    return mask.Match(name, use_case);
}

inline 
bool CDirEntry::CopyToDir(const string& dir, TCopyFlags flags,
                          size_t buf_size) const
{
    string path = MakePath(dir, GetName());
    return Copy(path, flags, buf_size);
}

inline 
bool CDirEntry::MoveToDir(const string& dir, TRenameFlags flags)
{
    string path = MakePath(dir, GetName());
    return Rename(path, flags);
}

inline
const char* CDirEntry::GetBackupSuffix(void)
{
    return m_BackupSuffix;
}

inline
void CDirEntry::SetBackupSuffix(const char* suffix)
{
    m_BackupSuffix = const_cast<char*>(suffix);
}


// CFile

inline
CFile::CFile(void)
{
    SetDefaultMode(eFile, fDefault, fDefault, fDefault);
}


inline
CFile::CFile(const string& filename) : CParent(filename)
{ 
    SetDefaultMode(eFile, fDefault, fDefault, fDefault);
}

inline
CFile::CFile(const CDirEntry& file) : CParent(file)
{
    return;
}

inline
bool CFile::Exists(void) const
{
    return IsFile();
}


// CDir

inline
CDir::CDir(void)
{
    SetDefaultMode(eDir, fDefault, fDefault, fDefault);
}

inline
CDir::CDir(const string& dirname) : CParent(dirname)
{
    SetDefaultMode(eDir, fDefault, fDefault, fDefault);
}

inline
CDir::CDir(const CDirEntry& dir) : CDirEntry(dir)
{
    return;
}

inline
bool CDir::Exists(void) const
{
    return IsDir();
}

inline CDir::TEntries 
CDir::GetEntries(const string&    mask,
                 EGetEntriesMode  mode,
                 NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntries(mask, mode);
}

inline CDir::TEntries
CDir::GetEntries(const vector<string>&  masks,
                 EGetEntriesMode        mode,
                 NStr::ECase            use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntries(masks, mode);
}

inline CDir::TEntries
CDir::GetEntries(const CMask&     masks,
                 EGetEntriesMode  mode,
                 NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntries(masks, mode);
}

inline CDir::TEntries*
CDir::GetEntriesPtr(const string&    mask,
                    EGetEntriesMode  mode,
                    NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntriesPtr(mask, mode);
}

inline CDir::TEntries*
CDir::GetEntriesPtr(const vector<string>&  masks,
                    EGetEntriesMode        mode,
                    NStr::ECase            use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntriesPtr(masks, mode);
}

inline CDir::TEntries*
CDir::GetEntriesPtr(const CMask&     masks,
                    EGetEntriesMode  mode,
                    NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntriesPtr(masks, mode);
}


// CSymLink

inline
CSymLink::CSymLink(void)
{
    return;
}

inline
CSymLink::CSymLink(const string& link) : CParent(link)
{ 
    // We dont need SetDefaultMode() here
    return;
}

inline
CSymLink::CSymLink(const CDirEntry& link) : CDirEntry(link)
{
    return;
}

inline
bool CSymLink::Exists(void) const
{
    return IsLink();
}


// CFileDelete*

inline
void CFileDeleteList::Add(const string& entryname)
{
    m_Names.push_back(entryname);
}

inline
const CFileDeleteList::TNames& CFileDeleteList::GetNames(void) const
{
    return m_Names;
}

inline
void CFileDeleteList::SetNames(CFileDeleteList::TNames& names)
{
    m_Names = names;
}


// CMemoryFile_Base

inline
bool CMemoryFile_Base::MemMapAdviseAddr(void* addr, size_t len,
                                        EMemMapAdvise advise)
{
    return MemoryAdvise(addr, len, (EMemoryAdvise)advise);
}


// CMemoryFileSegment

inline
void* CMemoryFileSegment::GetPtr(void) const
{
    return m_DataPtr;
}

inline
size_t CMemoryFileSegment::GetSize(void) const
{
    return m_Length;
}


inline
off_t CMemoryFileSegment::GetOffset(void) const
{
    return m_Offset;
}

inline
void* CMemoryFileSegment::GetRealPtr(void) const
{
    return m_DataPtrReal;
}

inline
size_t CMemoryFileSegment::GetRealSize(void) const
{
    return m_LengthReal;
}


inline
off_t CMemoryFileSegment::GetRealOffset(void) const
{
    return m_OffsetReal;
}

inline
bool CMemoryFileSegment::MemMapAdvise(EMemMapAdvise advise) const
{
    if ( !m_DataPtr ) {
        return false;
    }
    return MemMapAdviseAddr(m_DataPtrReal, m_LengthReal, advise);
}


// CMemoryFileMap

inline const CMemoryFileSegment* 
CMemoryFileMap::GetMemoryFileSegment(void* ptr) const
{
    return x_GetMemoryFileSegment(ptr);
}

inline
off_t CMemoryFileMap::GetOffset(void* ptr) const
{
    return GetMemoryFileSegment(ptr)->GetOffset();
}

inline
size_t CMemoryFileMap::GetSize(void* ptr) const
{
    return GetMemoryFileSegment(ptr)->GetSize();
}

inline
bool CMemoryFileMap::Flush(void* ptr) const
{
    return GetMemoryFileSegment(ptr)->Flush();
}

inline
bool CMemoryFileMap::MemMapAdvise(void* ptr, EMemMapAdvise advise) const
{
    return GetMemoryFileSegment(ptr)->MemMapAdvise(advise);
}



// CMemoryFile

inline
void* CMemoryFile::GetPtr(void) const
{
    return m_Ptr;
}

inline
size_t CMemoryFile::GetSize(void) const
{
    // Special case: file is not mapped and its length is zero.
    if (!m_Ptr  &&  GetFileSize() == 0) {
        return 0;
    }
    x_Verify();
    return CMemoryFileMap::GetSize(m_Ptr);
}

inline
off_t CMemoryFile::GetOffset(void) const
{
    x_Verify();
    return CMemoryFileMap::GetOffset(m_Ptr);
}

inline
bool CMemoryFile::Flush(void) const
{
    x_Verify();
    return CMemoryFileMap::Flush(m_Ptr);
}

inline
bool CMemoryFile::MemMapAdvise(EMemMapAdvise advise) const
{
    x_Verify();
    return CMemoryFileMap::MemMapAdvise(m_Ptr, advise);
}


END_NCBI_SCOPE

#endif  /* CORELIB___NCBIFILE__HPP */
