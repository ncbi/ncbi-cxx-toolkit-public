#ifndef CORELIB__NCBIFILE__HPP
#define CORELIB__NCBIFILE__HPP

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
 * Author: Vladimir Ivanov, Denis Vakatov
 *
 *
 */

/// @file ncbifile.hpp
/// Define files and directories accessory functions.
///
/// Defines classes CDirEntry, CFile, CDir, CMemoryFile, CFileException
/// to allow various file and directory operations.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <vector>

#if defined(NCBI_OS_MAC)
struct FSSpec;
#endif


/** @addtogroup Files
 *
 * @{
 */


BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
///
/// CFileException --
///
/// Define exceptions generated for file operations.
///
/// CFileException inherits its basic functionality from CCoreException
/// and defines additional error codes for file operations.

class CFileException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eMemoryMap,
        eRelativePath
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eMemoryMap: return "eMemoryMap";
        default:         return CException::GetErrCodeString();
        }
    }

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CFileException, CCoreException);
};


/// Whether to follow symbolic links (aka shortcuts or aliases)
enum EFollowLinks {
    eIgnoreLinks,
    eFollowLinks
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDirEntry --
///
/// Base class to work with files and directories.
///
/// Models a directory entry structure for the file system. Assumes that
/// the path argument has the following form, where any or all components may
/// be missing:
///
/// <dir><title><ext>
///
/// - dir   - file path             ("/usr/local/bin/"  or  "c:\windows\")
/// - title - file name without ext ("autoexec")
/// - ext   - file extension        (".bat" - whatever goes after a last dot)
///
/// Supported filename formats:  MS DOS/Windows, UNIX and MAC.

class NCBI_XNCBI_EXPORT CDirEntry
{
public:
    /// Constructor.
    CDirEntry();

#  ifdef NCBI_OS_MAC
    /// Copy constructor - for Mac file system.
    CDirEntry(const CDirEntry& other);

    /// Constructor with FSSpec argument - for Mac file system.
    CDirEntry(const FSSpec& fss);

    /// Assignment operator - for Mac file system.
    CDirEntry& operator= (const CDirEntry& other);

    /// Equality operator.
    bool operator== (const CDirEntry& other) const;
#  endif

    /// Constructor using specified path string.
    CDirEntry(const string& path);

    /// Reset path string.
    void Reset(const string& path);

    /// Destructor.
    virtual ~CDirEntry(void);

# if defined(NCBI_OS_MAC)
    /// Get FSSpec setting - for Mac file system.
    const FSSpec& FSS() const;
#endif

    /// Get directory entry path.
    string GetPath(void) const;


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
    /// @param ext
    ///   The extension component. This will always start with a dot
    ///   (example: ".bat").
    static void SplitPath(const string& path,
                          string* dir = 0, string* base = 0, string* ext = 0);

    /// Get the directory component for this directory entry.
    string GetDir (void) const;

    /// Get the base entry name with extension.
    string GetName(void) const;

    /// Get the base entry name without extension.
    string GetBase(void) const;

    /// Get extension name.
    string GetExt (void) const;

    /// Make a path from the basic components.
    ///
    /// @param dir
    ///   The directory component to make the path string. This will always
    ///   have a terminating path separator (example: "/usr/local/").
    /// @param base
    ///   The basename of the file component that is used to make up the path.
    /// @param ext
    ///   The extension component. This will always start with a dot
    ///   (example: ".bat").
    /// @return
    ///   Path built from the components.
    static string MakePath(const string& dir  = kEmptyStr,
                           const string& base = kEmptyStr,
                           const string& ext  = kEmptyStr);

    /// Get path separator symbol specific for current platform.
    static char GetPathSeparator(void);

    /// Check character "c" as path separator symbol specific for
    /// current platform.
    static bool IsPathSeparator(const char c);

    /// Add trailing path separator, if needed.
    static string AddTrailingPathSeparator(const string& path);

    /// Delete trailing path separator, if needed.
    static string DeleteTrailingPathSeparator(const string& path);

    /// Convert relative "path" on any OS to current OS dependent relative
    /// path. 
    static string ConvertToOSPath(const string& path);

    /// Check if the "path" is absolute for the current OS.
    ///
    /// Note that the "path" must be for current OS. 
    static bool IsAbsolutePath(const string& path);

    /// Check if the "path" is absolute for any OS.
    ///
    /// Note that the "path" can be for any OS (MSWIN, UNIX, MAC).
    static bool IsAbsolutePathEx(const string& path);

    /// Create relative path from 2 absolute pathes.
    ///
    /// @param path_from 
    ///   Absolute path that defines the start of the relative path.
    /// @param path_to
    ///   Absolute path that defines the endpoint of the relative path.
    /// @return
    ///   Return the relative path (empty string if the paths are the same).
    ///   Throw CFileException on error (e.g. if any of the paths is not
    ///   absolute, or if it is impossible to create relative path, such
    ///   as in case of different disks on MS-Windows).
    static string CreateRelativePath(const string& path_from, 
                                     const string& path_to);

    /// Concatenate the two parts of the path for the current OS.
    ///
    /// Note that the arguments must be for the current OS.
    /// @param first
    ///   The first part of the path which can be either absolute or relative.
    /// @param second
    ///   The second part of the path must always be relative.
    /// @return
    ///   The concatenated path.
    static string ConcatPath(const string& first, const string& second);

    /// Concatenate the two parts of the path for any OS.
    ///
    /// Note that the arguments must be for any OS (MSWIN, UNIX, MAC).
    /// @param first
    ///   The first part of the path which can be either absolute or relative.
    /// @param second
    ///   The second part of the path must always be relative.
    /// @return
    ///   The concatenated path.
    static string ConcatPathEx(const string& first, const string& second);

    /// Normalize path.
    ///
    /// Remove from the "path" all redundancy, convert it to the more
    /// simple form, if possible.
    /// Note that the "path" must be for current OS. 
    /// @param follow_links
    ///   Whether to follow symlinks (shortcuts, aliases)
    static string NormalizePath(const string& path,
                                EFollowLinks  follow_links = eIgnoreLinks);


    //
    // Checks & manipulations
    //

    /// Match "name" against the filename "mask".
    static bool MatchesMask(const char *name, const char *mask);

    /// Check existence of entry "path".
    virtual bool Exists(void) const;

    /// Rename entry to specified "new_path".
    bool Rename(const string& new_path);

    /// Directory remove mode.
    enum EDirRemoveMode {
        eOnlyEmpty,     ///< Remove only empty directory
        eNonRecursive,  ///< Remove all files in directory, but not remove
                        ///< subdirectories and files in it
        eRecursive      ///< Remove all files and subdirectories
    };

    /// Remove directory entry.
    ///
    /// Remove directory using the specified "mode".
    /// @sa
    ///   EDirRemoveMode
    virtual bool Remove(EDirRemoveMode mode = eRecursive) const;
    
    /// Check if directory entry a file.
    /// @sa
    ///   IsDir(), GetType()
    bool IsFile(EFollowLinks follow = eFollowLinks) const;

    /// Check if directory entry a directory.
    /// @sa
    ///   IsFile(), GetType()
    bool IsDir(EFollowLinks follow = eFollowLinks) const;

    /// Which directory entry type.
    enum EType {
        eFile = 0,     ///< Regular file
        eDir,          ///< Directory
        ePipe,         ///< Pipe
        eLink,         ///< Symbolic link     (UNIX only)
        eSocket,       ///< Socket            (UNIX only)
        eDoor,         ///< Door              (UNIX only)
        eBlockSpecial, ///< Block special     (UNIX only)
        eCharSpecial,  ///< Character special
        //
        eUnknown       ///< Unknown type
    };

    /// Get type of directory entry.
    ///
    /// @return
    ///   Return one of the values in EType. If the directory entry does
    ///   not exist return "eUnknown".
    EType GetType(EFollowLinks follow = eIgnoreLinks) const;

    /// Get time stamp of directory entry.
    ///
    /// The "creation" time under MS windows is actual creation time of the
    /// entry. Under UNIX "creation" time is the time of last entry status
    /// change. 
    /// @return
    ///   TRUE if time was acquired or FALSE otherwise.
    /// @sa
    ///   SetTime()
    bool GetTime(CTime *modification, CTime *creation = 0, 
                 CTime *last_access = 0) const;

    /// Set time stamp on directory entry.
    ///
    /// The process must be the owner of the file or have write permissions
    /// in order to change the time. If value of parameters modification or
    /// last access time is zero that current time will be used.
    /// @param modification
    ///   New file modification time.
    /// @param last_access
    ///   New last file access time. It cannot be less than the file
    ///   creation time. In last case it will be set equal to creation time.
    /// @return
    ///   TRUE if time was changed or FALSE otherwise.
    /// @sa
    ///   GetTime()
    bool SetTime(CTime *modification = 0 , CTime *last_access = 0) const;

    //
    // Access permissions.
    //

    /// Directory entry's access permissions.
    enum EMode {
        fExecute = 1,       ///< Execute permission
        fWrite   = 2,       ///< Write permission
        fRead    = 4,       ///< Read permission
        // initial defaults for dirs
        fDefaultDirUser  = fRead | fExecute | fWrite,
                            ///< Default user permission for dir.
        fDefaultDirGroup = fRead | fExecute,
                            ///< Default group permission for dir.
        fDefaultDirOther = fRead | fExecute,
                            ///< Default other permission for dir.
        // initial defaults for non-dir entries (files, etc.)
        fDefaultUser     = fRead | fWrite,
                            ///< Default user permission for file
        fDefaultGroup    = fRead,
                            ///< Default group permission for file
        fDefaultOther    = fRead,
                            ///< Default other permission for file
        fDefault = 8        ///< Special flag:  ignore all other flags,
                            ///< use current default mode
    };
    typedef unsigned int TMode;  ///< Binary OR of "EMode"

    /// Get the directory entry's permission settings.
    ///
    /// On WINDOWS, there is only the "user_mode" permission setting, and
    /// "group_mode" and "other_mode" settings will be ignored.
    /// @return
    ///   TRUE if successful return of permission settings; FALSE, otherwise.
    /// @sa
    ///   SetMode()
    bool GetMode(TMode* user_mode,
                 TMode* group_mode = 0,
                 TMode* other_mode = 0) const;

    /// Set permission mode for the directory entry.
    ///
    /// Permissions are set as specified by the passed values for user_mode,
    /// group_mode and other_mode. The default value for group_mode and
    /// other mode is "fDefault". Setting to "fDefault" will set the mode to
    /// its default permission settings.
    /// @return
    ///   TRUE if permission successfully set;  FALSE, otherwise.
    /// @sa
    ///   SetDefaultMode(), SetDefaultModeGlobal(), GetMode()
    bool SetMode(TMode user_mode,  // e.g. fDefault
                 TMode group_mode = fDefault,
                 TMode other_mode = fDefault) const;

    /// Set default mode globally for all CDirEntry objects.
    ///
    /// The default mode is set globally for all CDirEntry objects except for
    /// those having their own mode set with SetDefaultMode().
    ///
    /// When "fDefault" is passed as value of the mode parameters, the default
    /// mode will be set to the default values defined in EType:
    ///
    /// If user_mode is "fDefault", then default for user mode is set to
    /// - "fDefaultDirUser" if this is a directory or to
    /// - "fDefaultUser" if this is a file.
    /// 
    /// If group_mode is "fDefault", then default for group mode is set to
    /// - "fDefaultDirGroup" if this is a directory or to
    /// - "fDefaultGroup" if this is a file.
    ///
    /// If other_mode is "fDefault", then default for other mode is set to
    /// - "fDefaultDirOther" if this is a directory or to
    /// - "fDefaultOther" if this is a file.
    static void SetDefaultModeGlobal(EType entry_type,
                                     TMode user_mode,  // e.g. fDefault
                                     TMode group_mode = fDefault,
                                     TMode other_mode = fDefault);

    /// Set mode for this one object only.
    ///
    /// When "fDefault" is passed as value of the mode parameters, the mode
    /// will be set to the current global mode as specified
    /// by SetDefaultModeGlobal().
    virtual void SetDefaultMode(EType entry_type,
                                TMode user_mode,  // e.g. fDefault
                                TMode group_mode = fDefault,
                                TMode other_mode = fDefault);

protected:
    /// Get the default global mode.
    ///
    /// Used by derived classes like CDir and CFile.
    static void GetDefaultModeGlobal(EType  entry_type,
                                     TMode* user_mode,
                                     TMode* group_mode,
                                     TMode* other_mode);

    /// Get the default mode.
    ///
    /// Used by derived classes like CDir and CFile.
    void GetDefaultMode(TMode* user_mode,
                        TMode* group_mode,
                        TMode* other_mode) const;

private:
#  ifdef NCBI_OS_MAC
    FSSpec* m_FSS;      ///< Mac OS specific file description
#  else
    string m_Path;      ///< Full directory entry path
#  endif

    /// Which default mode: user, group, or other.
    ///
    /// Used as index into array that contains default mode values;
    /// so there is no "fDefault" as an enumeration value for EWho, here!
    enum EWho {
        eUser = 0,      ///< User mode
        eGroup,         ///< Group mode
        eOther          ///< Other mode
    };

    /// Holds default mode global values.
    static TMode m_DefaultModeGlobal[eUnknown][3/*EWho*/];

    /// Holds default mode values.
    TMode        m_DefaultMode[3/*EWho*/];
};



/////////////////////////////////////////////////////////////////////////////
///
/// CFile --
///
/// Define class to work with files.
///
/// Models the files in a file system. Basic functionality is derived from
/// CDirEntry and extended for files.

class NCBI_XNCBI_EXPORT CFile : public CDirEntry
{
    typedef CDirEntry CParent;    ///< CDirEntry is parent class

public:
    /// Constructor.
    CFile(const string& file);

    /// Destructor.
    virtual ~CFile(void);

    /// Check existence of file.
    virtual bool Exists(void) const;

    /// Get size of file.
    ///
    /// @return
    /// - size of file, if no error.
    /// - -1, if there was an error obtaining file size.
    Int8 GetLength(void) const;


    //
    // Temporary files
    //

    /// Temporary file creation mode
    enum ETmpFileCreationMode {
        eTmpFileCreate,     ///< Create empty file for each GetTmpName* call.
        eTmpFileGetName     ///< Get name of the file only.
    };

    /// Get temporary file name.
    ///
    /// @param mode
    ///   Temporary file creation mode.
    /// @return
    ///   Name of temporary file, or "kEmptyStr" if there was an error
    ///   getting temporary file name.
    /// @sa
    ///    GetTmpNameEx(), ETmpFileCreationMode
    static string GetTmpName(ETmpFileCreationMode mode = eTmpFileGetName);

    /// Get temporary file name.
    ///
    /// @param dir
    ///   Directory in which temporary file is to be created;
    ///   default of kEmptyStr means that system temporary directory will
    ///   be used or current, if a system temporary directory cannot
    ///   be determined.
    /// @param prefix
    ///   Temporary file name will be prefixed by value of this parameter;
    ///   default of kEmptyStr means that, effectively, no prefix value will
    ///   be used.
    /// @param mode
    ///   Temporary file creation mode. 
    ///   If set to "eTmpFileCreate", empty file with unique name will be
    ///   created. Please, do not forget to remove it youself as soon as it
    ///   is no longer needed. On some platforms "eTmpFileCreate" mode is 
    ///   equal to "eTmpFileGetName".
    ///   If set to "eTmpFileGetName", returns only the name of the temporary
    ///   file, without creating it. This method is faster but it have
    ///   potential race condition, when other process can leave as behind and
    ///   create file with the same name first.
    /// @return
    ///   Name of temporary file, or "kEmptyStr" if there was an error
    ///   getting temporary file name.
    /// @sa
    ///    GetTmpName(), ETmpFileCreationMode
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
    ///   CreateTmpFileEx()
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
    ///   CreateTmpFile()
    static fstream* CreateTmpFileEx(const string& dir         = ".",
                                    const string& prefix      = kEmptyStr,
                                    ETextBinary   text_binary = eBinary,
                                    EAllowRead    allow_read  = eAllowRead);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDir --
///
/// Define class to work with directories.
///
/// NOTE: Next functions are unsafe in multithreaded applications:
///       - static bool Exists() (for Mac only);
///       - bool Exists() (for Mac only).

class NCBI_XNCBI_EXPORT CDir : public CDirEntry
{
    typedef CDirEntry CParent;  ///< CDirEntry is the parent class

public:
    /// Constructor.
    CDir();
#  if defined(NCBI_OS_MAC)
    /// Constructor - for Mac OS.
    CDir(const FSSpec& fss);
#  endif
    /// Constructor using specified directory name.
    CDir(const string& dirname);

    /// Destructor.
    virtual ~CDir(void);

    /// Check if directory "dirname" exists.
    virtual bool Exists(void) const;

    /// Get user "home" directory.
    static string GetHome(void);

    /// Get temporary directory.
    static string GetTmpDir(void);

    /// Get the current working directory.
    static string GetCwd();

    /// Define a vector of pointers to directory entries.
    typedef vector< AutoPtr<CDirEntry> > TEntries;

    /// Modes for GetEntries
    /// @sa GetEntries
    enum EGetEntriesMode {
        eAllEntries,        ///< All included
        eIgnoreRecursive    ///< Supress self recursive elements (like "..")
    };


    /// Get directory entries based on the specified "mask".
    ///
    /// @param mask
    ///   Use to select only files that match this mask. Do not use file mask
    ///   if set to "kEmptyStr".
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const string&   mask = kEmptyStr,
                        EGetEntriesMode mode = eAllEntries) const;

    /// Get directory entries based on the specified set of"masks".
    ///
    /// @param mask
    ///   Use to select only files that match this set of masks.
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const vector<string>& masks,
                        EGetEntriesMode       mode = eAllEntries) const;

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

    /// Delete existing directory.
    ///
    /// @param mode
    ///   - If set to "eOnlyEmpty" the directory can be removed only if it
    ///   is empty.
    ///   - If set to "eNonRecursive" remove only files in directory, but do
    ///   not remove subdirectories and files in them.
    ///   - If set to "eRecursive" remove all files in directory, and
    ///   subdirectories.
    /// @return
    ///   TRUE if operation successful; FALSE otherwise.
    /// @sa
    ///   EDirRemoveMode
    virtual bool Remove(EDirRemoveMode mode = eRecursive) const;
};


/// File finding flags
enum EFindFiles {
    fFF_File       = (1<<0),             ///< find files
    fFF_Dir        = (1<<1),             ///< find directories
    fFF_Recursive  = (1<<2),             ///< decsend into sub-dirs
    fFF_Default    = fFF_File | fFF_Dir  ///< default behaviour
};

/// bitwise OR of "EFindFiles"
typedef int TFindFiles; 


/// Find files in the specified directory
template<class TFindFunc>
TFindFunc FindFilesInDir(const CDir&            dir,
                         const vector<string>&  masks,
                         TFindFunc              find_func,
                         TFindFiles             flags = fFF_Default)
{
    CDir::TEntries contents = dir.GetEntries(masks, CDir::eIgnoreRecursive);

    ITERATE(CDir::TEntries, it, contents) {
        const CDirEntry& dir_entry = **it;

        if (dir_entry.IsDir()) {
            if (flags & fFF_Dir) {
                find_func(dir_entry);
            }

            if (flags & fFF_Recursive) {
                CDir nested_dir(dir_entry.GetPath());
                find_func = 
                  FindFilesInDir(nested_dir, masks, find_func, flags);
            }
        }
        else
        if (dir_entry.IsFile() && (flags & fFF_File)) {
            find_func(dir_entry);
        }
    } // ITERATE
    return find_func;
}


/// Generic algorithm for file search
///
/// Algorithm scans the provided directories using iterators,
/// finds files to match the masks and stores all calls functor
/// object for all found entries
/// Functor call should match: void Functor(const CDirEntry& dir_entry)
///

template<class TPathIterator, 
         class TMaskIterator, 
         class TFindFunc>
TFindFunc FindFiles(TPathIterator path_begin,
                    TPathIterator path_end,
                    TMaskIterator mask_begin,
                    TMaskIterator mask_end,
                    TFindFunc     find_func,
                    TFindFiles    flags = fFF_Default)
{
    vector<string> masks;
    for (; mask_begin != mask_end; ++mask_begin) {
        masks.push_back(*mask_begin);
    }

    for (; path_begin != path_end; ++path_begin) {
        const string& dir_name = *path_begin;

        CDir dir(dir_name);
        find_func = FindFilesInDir(dir, masks, find_func, flags);
    } // for
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

template<class TContainer, class It1>
void FindFiles(TContainer&           out, 
               It1                   first_path, 
               It1                   last_path, 
               const vector<string>& masks,
               TFindFiles            flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles(first_path, last_path, 
              masks.begin(), masks.end(), func, flags);
}



/////////////////////////////////////////////////////////////////////////////
///
/// Utility algorithm scans the provided directories using iterators
/// finds files to match the masks and stores all found files in 
/// the container object.
///

template<class TContainer, class It1, class It2>
void FindFiles(TContainer&  out, 
               It1          first_path,
               It1          last_path, 
               It2          first_mask,
               It2          last_mask,
               TFindFiles   flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles(first_path, last_path, 
              first_mask, last_mask, func, flags);
}



/////////////////////////////////////////////////////////////////////////////
///
/// fwd-decl of struct containing OS-specific mem.-file handle.

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
        eMMS_Private      ///< Changes are private
    } EMemMapShare;

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
    /// NOTE: Now works on UNIX platform only.
    typedef enum {
        eMMA_Normal,      ///< No further special treatment
        eMMA_Random,      ///< Expect random page references
        eMMA_Sequential,  ///< Expect sequential page references
        eMMA_WillNeed,    ///< Will need these pages
        eMMA_DontNeed     ///< Don't need these pages
    } EMemMapAdvise;


    /// Advise on memory map usage for specified region.
    ///
    /// @param addr
    ///   Address of memory region whose usage is being advised.
    /// @param len
    ///   Length of memory region whose usage is being advised.
    /// @param advise
    ///   One of the values in EMemMapAdvise that advises on expected
    ///   usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful. Always return
    ///   TRUE if memory advise not implemented such as on Windows system.
    ///   - FALSE, if memory advise operation not successful.
    /// @sa
    ///   EMemMapAdvise, MemMapAdvise
    static bool MemMapAdviseAddr(void* addr, size_t len,EMemMapAdvise advise);
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
    ///    EMemMapProtect, EMemMapShare, GetPtr(), GetSize(), GetOffset().
    CMemoryFileSegment(SMemoryFileHandle& handle,
                       SMemoryFileAttrs&  attrs,
                       off_t              offset,
                       size_t             lendth);

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
    ///   was not succesfully mapped.
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
    /// to match it. The "length" value will be automaticaly increased on the
    /// difference between passed and real offsets.
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    /// @sa
    ///    GetRealOffset(), GetRealSize(), GetPtr(). 
    void* GetRealPtr(void) const;

    /// Get real offset of the mapped area from beginning of file.
    ///
    /// This real offset is adjusted to system's memory allocation granularity
    /// value and can be less than requested "offset" in the constructor.
    /// @return
    ///   Offset in bytes of mapped area from beginning of the file.
    ///   Always return adjusted value even if data was not succesfully mapped.
    /// @sa
    ///    GetRealPtr(), GetRealSize(), GetOffset(). 
    off_t GetRealOffset(void) const;

    /// Get real length of the mapped area.
    ///
    /// Number of really mapped bytes of file. This length value can be
    /// increased if "offset" is not a multiple of the allocation granularity.
    /// @return
    ///   - Length in bytes of the mapped area, or
    ///   - 0 if not mapped.
    /// @sa
    ///    GetRealPtr(), GetRealOffset(), GetSize(). 
    size_t GetRealSize(void) const;

    /// Flush by writing all modified copies of memory pages to the
    /// underlying file.
    ///
    /// NOTE: By default data will be flushed in the destructor.
    /// @return
    ///   - TRUE, if all data was flushed succesfully.
    ///   - FALSE, if not mapped or if an error occurs.
    bool Flush(void) const;

    /// Unmap file view from memory.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    bool Unmap(void);

    /// Advise on memory map usage.
    ///
    /// @param advise
    ///   One of the values in EMemMapAdvise that advises on expected
    ///   usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful. Always return
    ///     TRUE if memory advise not implemented such as on Windows system.
    ///   - FALSE, if memory advise operation not successful.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(EMemMapAdvise advise) const;

private:
    // Check that file is mapped, throw excepton otherwise.
    void x_Verify(void) const;

private:
    // Values for user
    void*   m_DataPtr;     ///< Pointer to the begining of the mapped area.
                           ///> The user seen this one.
    off_t   m_Offset;      ///< Requested starting offset of the
                           ///< mapped area from begining of file.
    size_t  m_Length;      ///< Requested length of the mapped area.

    // Internal real values
    void*   m_DataPtrReal; ///< Real pointer to the begining of the mapped
                           ///< area which should be fried later.
    off_t   m_OffsetReal;  ///< Corrected starting offset of the
                           ///< mapped area from begining of file.
    size_t  m_LengthReal;  ///< Corrected length of the mapped area.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFileMap --
///
/// Define class for support a partial file memory mapping.
///
/// Note, that all mapped into memory file segments have equal protect and
/// share attributes, because they restricted with file open mode.
/// Note, that the mapping file must exists and have non-zero length.
/// This class cannot increase size of mapped file. If the size of
/// the mapped file changes, the effect of references to portions of
/// the mapped region that correspond to added or removed portions of the
/// file is unspecified.
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
    /// @sa
    ///   EMemMapProtect, EMemMapShare
    CMemoryFileMap(const string&  file_name,
                   EMemMapProtect protect_attr = eMMP_Read,
                   EMemMapShare   share_attr   = eMMS_Private);

    /// Destructor.
    ///
    /// Calls Unmap() and cleans up memory.
    ~CMemoryFileMap(void);

    /// Map file segment if it is mapped.
    ///
    /// @param offset
    ///   The file offset where mapping is to begin. If the offset is not
    ///   a multiple of the allocation granularity, that it can be decreased 
    ///   to match it. The "length" value will be automaticaly increased on
    ///   the difference between passed and real offsets. The real offset can
    ///   be obtained using GetOffset(). The parameter must be more than 0.
    /// @param length
    ///   Number of bytes to map. This value can be increased if "offset"
    ///   is not a multiple of the allocation granularity.
    ///   The real length of mapped region can be obtained using GetSize().
    ///   The value 0 means that all file size will be mapped.
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    /// @sa
    ///   Unmap()
    void* Map(off_t offset, size_t length);

    /// Unmap file segment.
    ///
    /// @param ptr
    ///   Pointer returned by Map().
    /// @return
    ///   TRUE on success; or FALSE on error.
    /// @sa
    ///   Map()
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
    ///   to MapSegment() method for specified "ptr".
    off_t GetOffset(void* ptr) const;

    /// Get length of the mapped segment.
    ///
    /// @param prt
    ///   Pointer to mapped data returned by MapSegment().
    /// @return
    ///   Length in bytes of the mapped area.
    ///   Returned value is a value of "length" parameter passed
    ///   to MapSegment() method for specified "ptr".
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
    /// @return
    ///   - TRUE, if all data was flushed succesfully.
    ///   - FALSE, if an error occurs.
    bool Flush(void* ptr) const;

    /// Advise on memory map usage.
    ///
    /// @param advise
    ///   One of the values in EMemMapAdvise that advises on expected
    ///   usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful. Always return
    ///     TRUE if memory advise not implemented such as on Windows system.
    ///   - FALSE, if memory advise operation not successful.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(void* ptr, EMemMapAdvise advise) const;

private:
    // Open file mapping for file with name m_FileName.
    void x_Open(void);

    // Unmap mapped memory and close all handles.
    void x_Close(void);

    // Get segment by pointer to data.
    CMemoryFileSegment* x_Get(void* ptr) const;

protected:
    string              m_FileName;  ///< File name. 
    SMemoryFileHandle*  m_Handle;    ///< Memory file handle.
    SMemoryFileAttrs*   m_Attrs;     ///< Specify operations permitted on
                                     ///< memory mapped file and mapping mode.

    typedef map<void*,CMemoryFileSegment*> TSegments;
    TSegments           m_Segments;  ///< Map of pointers to mapped segments.
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
/// Note, that the mapping file must exists and have non-zero length.
/// This class cannot increase size of mapped file. If the size of
/// the mapped file changes, the effect of references to portions of
/// the mapped region that correspond to added or removed portions of the
/// file is unspecified.
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
    ///   The file offset where mapping is to begin. If the offset is not
    ///   a multiple of the allocation granularity, that it can be decreased 
    ///   to match it. The "length" value will be automaticaly increased on
    ///   the difference between passed and real offsets. The real offset can
    ///   be obtained using GetOffset(). The parameter must be more than 0.
    /// @param length
    ///   Number of bytes to map. This value can be increased if "offset"
    ///   is not a multiple of the allocation granularity.
    ///   The real length of mapped region can be obtained using GetSize().
    ///   The value 0 means that all file size will be mapped.
    /// @sa
    ///   EMemMapProtect, EMemMapShare
    CMemoryFile(const string&  file_name,
                EMemMapProtect protect_attr = eMMP_Read,
                EMemMapShare   share_attr   = eMMS_Private,
                off_t          offset       = 0,
                size_t         lendth       = 0);

    /// Unmap file if mapped.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    bool Unmap(void);

    /// Get pointer to beginning of data.
    ///
    /// @return
    ///   Pointer to start of data.
    void* GetPtr(void) const;

    /// Get offset of the mapped area from beginning of the file.
    ///
    /// The offset can be adjusted to system's memory allocation granularity
    /// value and can differ from "offset" parameter in the class constructor
    /// or in the Map() method.
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
    ///   - TRUE, if all data was flushed succesfully.
    ///   - FALSE, if an error occurs.
    bool Flush(void) const;

    /// Advise on memory map usage.
    ///
    /// @param advise
    ///   One of the values in EMemMapAdvise that advises on expected
    ///   usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful. Always return
    ///   TRUE if memory advise not implemented such as on Windows system.
    ///   - FALSE, if memory advise operation not successful.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(EMemMapAdvise advise) const;

private:
    // Check that file is mapped, throw excepton otherwise.
    void x_Verify(void) const;

private:
    void* m_Ptr;  ///< Pointer to mapped view of file.
};


/* @} */



//////////////////////////////////////////////////////////////////////////////
//
// Inline
//


// CDirEntry

#ifndef NCBI_OS_MAC
inline
void CDirEntry::Reset(const string& path)
{
    m_Path = DeleteTrailingPathSeparator(path);
}

inline
string CDirEntry::GetPath(void) const
{
    return m_Path;
}
#endif

inline
string CDirEntry::GetDir(void) const
{
    string dir;
    SplitPath(GetPath(), &dir);
    return dir;
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
bool CDirEntry::Exists(void) const
{
    return GetType() != eUnknown;
}


// CFile

inline
bool CFile::Exists(void) const
{
    return IsFile();
}


// CDir

inline
bool CDir::Exists(void) const
{
    return IsDir();
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


// CMemoryFileMap


inline
off_t CMemoryFileMap::GetOffset(void* ptr) const
{
    return x_Get(ptr)->GetOffset();
}

inline
size_t CMemoryFileMap::GetSize(void* ptr) const
{
    return x_Get(ptr)->GetSize();
}

inline
Int8 CMemoryFileMap::GetFileSize(void) const
{
    return CFile(m_FileName).GetLength();
}

inline
bool CMemoryFileMap::Flush(void* ptr) const
{
    return x_Get(ptr)->Flush();
}

inline
bool CMemoryFileMap::MemMapAdvise(void* ptr, EMemMapAdvise advise) const
{
    return x_Get(ptr)->MemMapAdvise(advise);
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
    if ( !m_Ptr  &&  GetFileSize() == 0) {
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.47  2004/08/19 13:01:51  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.46  2004/08/03 12:04:57  ivanov
 * + CMemoryFile_Base   - base class for all memory mapping classes.
 * + CMemoryFileSegment - auxiliary class for mapping a memory file region.
 * + CMemoryFileMap     - class for support a partial file memory mapping.
 * * CMemoryFile        - now is the same as CMemoryFileMap but have only
 *                        one big mapped segment with offset 0 and length
 *                        equal to length of file.
 *
 * Revision 1.45  2004/07/29 18:20:40  ivanov
 * Added missed implementation for CMemoryFile::MemMapAdvise()
 *
 * Revision 1.44  2004/07/28 16:22:52  ivanov
 * Renamed CMemoryFileMap -> CMemoryFileSegment
 *
 * Revision 1.43  2004/07/28 15:47:17  ivanov
 * + CMemoryFile_Base, CMemoryFileMap.
 * Added "offset" and "length" parameters to CMemoryFile constructor to map
 * a part of file.
 *
 * Revision 1.42  2004/05/18 16:51:25  ivanov
 * Added CDir::GetTmpDir()
 *
 * Revision 1.41  2004/04/29 16:18:58  ivanov
 * operator== defined only for MAC OS
 *
 * Revision 1.40  2004/04/29 16:14:03  kuznets
 * Removed unnecessary typename
 *
 * Revision 1.39  2004/04/29 15:14:17  kuznets
 * + Generic FindFile algorithm capable of recursive searches
 * CDir::GetEntries received additional parameter to ignore self
 * recursive directory entries (".", "..")
 *
 * Revision 1.38  2004/04/28 19:04:16  ucko
 * Give GetType(), IsFile(), and IsDir() an optional EFollowLinks
 * parameter (currently only honored on Unix).
 *
 * Revision 1.37  2004/03/17 15:39:37  ivanov
 * CFile:: Fixed possible race condition concerned with temporary file name
 * generation. Added ETmpFileCreationMode enum. Fixed GetTmpName[Ex] and
 * CreateTmpFile[Ex] class methods.
 *
 * Revision 1.36  2004/03/11 22:16:52  vakatov
 * Cosmetics
 *
 * Revision 1.35  2004/01/05 21:41:55  gorelenk
 * += Exception throwing in CDirEntry::CreateRelativePath()
 *
 * Revision 1.34  2004/01/05 20:06:44  gorelenk
 * + CDirEntry::CreateRelativePath()
 *
 * Revision 1.33  2003/11/28 16:23:03  ivanov
 * + CDirEntry::SetTime()
 *
 * Revision 1.32  2003/11/05 16:27:18  kuznets
 * +FindFile template algorithm
 *
 * Revision 1.31  2003/11/05 15:35:44  kuznets
 * Added CDir::GetEntries() based on set of masks
 *
 * Revision 1.30  2003/10/23 12:11:37  ucko
 * Drop <memory> (now unneeded, and should have gone to ncbifile.cpp anyway)
 *
 * Revision 1.29  2003/10/23 03:18:53  ucko
 * +<memory> for auto_ptr
 *
 * Revision 1.28  2003/10/08 15:44:53  ivanov
 * Added CDirEntry::DeleteTrailingPathSeparator()
 *
 * Revision 1.27  2003/10/01 14:32:09  ucko
 * +EFollowLinks
 *
 * Revision 1.26  2003/09/30 15:08:28  ucko
 * Reworked CDirEntry::NormalizePath, which now handles .. correctly in
 * all cases and optionally resolves symlinks (on Unix).
 *
 * Revision 1.25  2003/09/16 15:18:13  ivanov
 * + CDirEntry::NormalizePath()
 *
 * Revision 1.24  2003/08/29 16:56:27  ivanov
 * Removed commit about unsafe GetTmpName() and CreateTmpFile() functions in the MT env
 *
 * Revision 1.23  2003/08/08 13:35:29  siyan
 * Changed GetTmpNameExt to GetTmpNameEx, as this is the more appropriate name.
 *
 * Revision 1.22  2003/08/06 13:45:35  siyan
 * Document changes.
 *
 * Revision 1.21  2003/05/29 17:21:31  gouriano
 * added CreatePath() which creates directories recursively
 *
 * Revision 1.20  2003/03/31 16:54:25  siyan
 * Added doxygen support
 *
 * Revision 1.19  2003/02/05 22:07:32  ivanov
 * Added protect and sharing parameters to the CMemoryFile constructor.
 * Added CMemoryFile::Flush() method.
 *
 * Revision 1.18  2003/01/16 13:03:47  dicuccio
 * Added CDir::GetCwd()
 *
 * Revision 1.17  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.16  2002/07/15 18:17:51  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.15  2002/07/11 19:21:58  ivanov
 * Added CMemoryFile::MemMapAdvise[Addr]()
 *
 * Revision 1.14  2002/07/11 14:17:54  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.13  2002/06/07 16:11:09  ivanov
 * Chenget GetTime() -- using CTime instead time_t, modification time by default
 *
 * Revision 1.12  2002/06/07 15:20:41  ivanov
 * Added CDirEntry::GetTime()
 *
 * Revision 1.11  2002/04/11 20:39:18  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.10  2002/04/01 18:49:07  ivanov
 * Added class CMemoryFile
 *
 * Revision 1.9  2002/01/24 22:17:40  ivanov
 * Changed CDirEntry::Remove() and CDir::Remove()
 *
 * Revision 1.8  2002/01/10 16:46:09  ivanov
 * Added CDir::GetHome() and some CDirEntry:: path processing functions
 *
 * Revision 1.7  2001/12/26 20:58:23  juran
 * Use an FSSpec* member instead of an FSSpec, so a forward declaration can 
 * be used.
 * Add copy constructor and assignment operator for CDirEntry on Mac OS,
 * thus avoiding memberwise copy which would blow up upon deleting the 
 * pointer twice.
 *
 * Revision 1.6  2001/12/13 20:14:34  juran
 * Add forward declaration of struct FSSpec for Mac OS.
 *
 * Revision 1.5  2001/11/19 18:07:38  juran
 * Change Contents() to GetEntries().
 * Implement MatchesMask().
 *
 * Revision 1.4  2001/11/15 16:30:46  ivanov
 * Moved from util to corelib
 *
 * Revision 1.3  2001/11/01 21:02:18  ucko
 * Fix to work on non-MacOS platforms again.
 *
 * Revision 1.2  2001/11/01 20:06:49  juran
 * Replace directory streams with Contents() method.
 * Implement and test Mac OS platform.
 *
 * Revision 1.1  2001/09/19 13:04:18  ivanov
 * Initial revision
 * ===========================================================================
 */

#endif  /* CORELIB__NCBIFILE__HPP */
