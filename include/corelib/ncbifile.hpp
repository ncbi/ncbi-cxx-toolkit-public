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


#ifdef NCBI_OS_MAC
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

class NCBI_XNCBI_EXPORT CFileException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eMemoryMap
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eMemoryMap:  return "eMemoryMap";
        default:    return CException::GetErrCodeString();
        }
    }

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CFileException, CCoreException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDirEntry --
///
/// Base class to work with files and directories.
///
/// Models the directory entry structure for the file system. Assumes that
/// the path argument has the following form, where any or all components may
/// be missing:
///
/// <dir><title><ext>
///
/// - dir   - file path             ("/usr/local/bin/"  or  "c:\windows\")
/// - title - file name without ext ("autoexec")
/// - ext   - file extension        (".bat" -- whatever goes after the last dot)
///
/// Supported filename formats:  MS DOS/Windows, UNIX and MAC.

class NCBI_XNCBI_EXPORT CDirEntry
{
public:
    /// Constructor.
    CDirEntry();

#ifdef NCBI_OS_MAC
    /// Copy constructor - for Mac file system.
    CDirEntry(const CDirEntry& other);

    /// Assignment operator - for Mac file system.
    CDirEntry& operator= (const CDirEntry& other);

    /// Constructor with FSSpec argument - for Mac file system.
    CDirEntry(const FSSpec& fss);
#endif

    /// Constructor using specified path string.
    CDirEntry(const string& path);

    /// Reset path string.
    void Reset(const string& path);

    /// Destructor.
    virtual ~CDirEntry(void);

    /// Equality operator.
	bool operator== (const CDirEntry& other) const;

#ifdef NCBI_OS_MAC
    /// Get FSSpec setting - for Mac file system.
	const FSSpec& FSS() const;
#endif

    /// Get directory entry path.
    string GetPath(void) const;


    //
    // Path processing.
    //

    /// Split the path string into its basic components.
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

    /// Get the Directory component for this directory entry.
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

    /// Get path separator symbol specific for the platform.
    static char GetPathSeparator(void);

    /// Check character "c" as path separator symbol specific for the platform.
    static bool IsPathSeparator(const char c);

    /// Add a trailing path separator, if needed.
    static string AddTrailingPathSeparator(const string& path);

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
    bool IsFile(void) const;

    /// Check if directory entry a directory.
    /// @sa
    ///   IsFile(), GetType()
    bool IsDir(void) const;

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
    ///   exist return "eUnknown".
    EType GetType(void) const;

    /// Get time stamp of directory entry.
    ///
    /// The "creation" time under MS windows is actual creation time of the
    /// entry. Under UNIX "creation" time is the time of last entry status
    /// change.
    /// @return
    ///   TRUE if time was acquired or FALSE otherwise.
    bool GetTime(CTime *modification, CTime *creation = 0, 
                 CTime *last_access = 0) const;

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
    bool GetMode(TMode* user_mode,
                 TMode* group_mode = 0,
                 TMode* other_mode = 0) const;

    /// Set permission mode for the directory entry.
    ///
    /// Permissions are set as specified by the passed values for user_mode,
    /// group_mode and other_mode. The default value for group_mode and
    /// other mode is "fDefault". Setting to "fDefault" will set the mode to
    /// its default permission settings.
    /// @sa
    ///   SetDefaultMode(), SetDefaultModeGlobal()
    /// @return
    ///   TRUE if permission successfully set;  FALSE, otherwise.
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
    /// When "fDefault" is passed as value of the mode parameters, the mode will
    /// be set to the current global mode as specified by SetDefaultModeGlobal().
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
#ifdef NCBI_OS_MAC
    FSSpec* m_FSS;      ///< Mac OS specific file description
#else
    string m_Path;      ///< Full directory entry path
#endif

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
///
/// NOTE: Next functions are unsafe in multithreaded applications:
///       - GetTmpName() (use GetTmpNameExt() -- it is safe);
///       - CreateTmpFile() with empty first parameter
///         (use CreateTmpFileExt() -- it is safe);

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

    /// Get temporary file name.
    ///
    /// @return
    ///   Name of temporary file, or "kEmptyStr" if there was an error
    ///   getting temporary file name.
    static string GetTmpName(void);

    /// Get temporary file name.
    ///
    /// @param dir
    ///   Directory in which temporary file is to be created.
    /// @param prefix
    ///   Temporary file name will be prefixed by value of this parameter;
    ///   default of kEmptyStr means that effectively no prefix value will
    ///   be used.
    /// @return
    ///   Name of temporary file, or "kEmptyStr" if there was an error
    ///   getting temporary file name.
    static string GetTmpNameExt(const string& dir,
                                const string& prefix = kEmptyStr);

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
    static fstream* CreateTmpFile(const string& filename = kEmptyStr,
                                  ETextBinary text_binary = eBinary,
                                  EAllowRead  allow_read  = eAllowRead);

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
    static fstream* CreateTmpFileEx(const string& dir = ".",
                                    const string& prefix = kEmptyStr,
                                    ETextBinary text_binary = eBinary,
                                    EAllowRead  allow_read  = eAllowRead);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDir --
///
/// Define class to work with directories.
///
/// NOTE: Next functions are unsafe in multithreaded applications:
///       - static bool Exists() (for Mac only);
///       - bool Exists() (for Mac only);
///       - iterators (also is unsafe in single threaded applications if
///         one iterator used inside another);

class NCBI_XNCBI_EXPORT CDir : public CDirEntry
{
    typedef CDirEntry CParent;  ///< CDirEntry is the parent class

public:
    /// Constructor.
    CDir();
#ifdef NCBI_OS_MAC
    /// Constructor - for Mac OS.
    CDir(const FSSpec& fss);
#endif
    /// Constructor using specified directory name.
    CDir(const string& dirname);

    /// Destructor.
    virtual ~CDir(void);

    /// Check if directory "dirname" exists.
    virtual bool Exists(void) const;

    /// Get user "home" directory.
    static string GetHome(void);

    /// Get the current working directory.
    static string GetCwd();

    /// Define a vector of pointers to directory entries.
    typedef vector< AutoPtr<CDirEntry> > TEntries;

    /// Get directory entries based on the specified "mask".
    ///
    /// @param mask
    ///   Use to select only files that match this mask. Do not use file mask
    ///   if set to "kEmptyStr".
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const string& mask = kEmptyStr) const;

    /// Create the directory using "dirname" passed in the constructor.
    /// 
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    bool Create(void) const;

    /// Create the directory path recursively possibly more than one at a time.
    ///
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
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
    ///   TRUE if operation successful; FALSE, otherwise.
    /// @sa
    ///   EDirRemoveMode
    virtual bool Remove(EDirRemoveMode mode = eRecursive) const;

};



// fwd-decl of struct containing OS-specific mem.-file handle.
struct SMemoryFileHandle;

/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFile --
///
/// Define class for support file memory mapping.

class NCBI_XNCBI_EXPORT CMemoryFile
{
public:
    /// Which operations are permitted in memory map file.
    typedef enum {
        eMMP_Read,	      ///< Data can be read
        eMMP_Write,	      ///< Data can be written
        eMMP_ReadWrite    ///< Data can be read and written
    } EMemMapProtect;

    /// Whether to share changes or not.
    typedef enum {
        eMMS_Shared,	  ///< Changes are shared.
        eMMS_Private	  ///< Changes are private.
    } EMemMapShare;

    /// Constructor.
    ///
    /// Initialize the memory mapping on file "file_name".  Throws an
    /// exception on error.
    /// @param filename
    ///   Name of file to map to memory.
    /// @param protect_attr
    ///   Specify operations permitted on memory mapped file.
    /// @param share_attr
    ///   Specify if change to memory mapped file can be shared or not.
    /// @sa
    ///   EMemMapProtect, EMemMapShare
    CMemoryFile(const string& file_name,
                EMemMapProtect protect_attr = eMMP_Read,
                EMemMapShare   share_attr   = eMMS_Private);

    /// Destructor.
    ///
    /// Calls Unmap() and cleans up memory mapped file.
    ~CMemoryFile(void);

    /// Check if memory-mapping is supported by the C++ Toolkit on this
    /// platform.
    static bool IsSupported(void);

    /// Get pointer to beginning of data.
    ///
    /// @return
    ///    - Pointer to start of data, or
    ///    - NULL if mapped to a file of zero length, or if unmapped already.
    void* GetPtr(void) const;

    /// Get size of the mapped area.
    ///
    /// @return
    ///   - Size in bytes of mapped area, or
    ///   - -1 if unmapped already.
    Int8 GetSize(void) const;

    /// Flush by writing all modified copies of memory pages to the
    /// underlying file.
    ///
    /// NOTE: By default data will be flushed in Unmap() or destructor.
    bool Flush(void) const;

    /// Unmap file if mapped.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    bool Unmap(void);

    /// What type of data access pattern will be used for mapped region.
    ///
    /// Advises the VM system that the a certain region of user mapped memory 
    /// will be accessed following a type of pattern. The VM system uses this 
    /// information to optimize work with mapped memory.
    ///
    /// NOTE: Now works on UNIX platform only.
    typedef enum {
        eMMA_Normal,	  ///< No further special treatment
        eMMA_Random,	  ///< Expect random page references
        eMMA_Sequential,  ///< Expect sequential page references
        eMMA_WillNeed,	  ///< Will need these pages
        eMMA_DontNeed	  ///< Don't need these pages
    } EMemMapAdvise;

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
    bool MemMapAdvise(EMemMapAdvise advise);

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
    static bool MemMapAdviseAddr(void* addr, size_t len, EMemMapAdvise advise);

private:
    /// Helper method to map file to memory.
    ///
    /// @param filename
    ///   Name of file to map to memory.
    /// @param protect_attr
    ///   Specify operations permitted on memory mapped file.
    /// @param share_attr
    ///   Specify if change to memory mapped file can be shared or not.
    /// @sa
    ///   EMemMapProtect, EMemMapShare
    void x_Map(const string& file_name,
               EMemMapProtect protect_attr,
               EMemMapShare   share_attr);

private:
    SMemoryFileHandle*  m_Handle;   ///< Memory file handle
    Int8                m_Size;     ///< Size (in bytes) of the mapped area
    void*               m_DataPtr;  ///< Pointer to the begining of mapped
                                    ///< data
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
    m_Path = path;
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
bool CDirEntry::IsFile(void) const
{
    return GetType() == eFile;
}

inline
bool CDirEntry::IsDir(void) const
{
    return GetType() == eDir;
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
    return GetType() == eFile;
}


// CDir

inline
bool CDir::Exists(void) const
{
    return GetType() == eDir;
}


// CMemoryFile

inline
void* CMemoryFile::GetPtr(void) const
{
    return m_DataPtr;
}

inline
Int8 CMemoryFile::GetSize(void) const
{
    return m_Size;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
