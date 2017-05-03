#ifndef UTIL_COMPRESS__ARCHIVE__HPP
#define UTIL_COMPRESS__ARCHIVE__HPP

/* $Id$
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
 * Authors:  Vladimir Ivanov
 *
 * File Description:
 *   Compression archive API.
 *
 */

///  @file archive.hpp
///  Archive API.

#include <util/compress/archive_.hpp>


/** @addtogroup Compression
 *
 * @{
 */

BEGIN_NCBI_SCOPE


// Forward declarations
class CArchiveZip;


//////////////////////////////////////////////////////////////////////////////
///
/// CArchive - base class for file- or memory-based archives.
///
/// Do not use it directly, use CArchiveFile or CArchiveMemory instead.
/// Throws exceptions on errors.

class NCBI_XUTIL_EXPORT CArchive
{
public:
    // Type definitions
    typedef CCompression::ELevel ELevel;

    /// Archive formats
    enum EFormat {
        eZip
    };

    /// General flags
    enum EFlags {
        // --- Extract --- (fUpdate also applies to Update)
        /// Allow to overwrite destinations with entries from the archive
        fOverwrite          = (1<<3),
        /// Only update entries that are older than those already existing
        fUpdate             = (1<<4) | fOverwrite,
        /// Backup destinations if they exist (all entries including dirs)
        fBackup             = (1<<5) | fOverwrite,
        /// If destination entry exists, it must have the same type as source
        fEqualTypes         = (1<<6),
        /// Create extracted files with the original ownership
        fPreserveOwner      = (1<<7),
        /// Create extracted files with the original permissions
        fPreserveMode       = (1<<8),
        /// Preserve date/times for extracted files
        /// Note, that some formats, as zip for example, store modification
        /// time only, so creation and last access time will be the same as
        /// modification time. And even it can be a little bit off due a 
        /// rounding errors.
        fPreserveTime       = (1<<9),
        /// Preserve all file attributes
        fPreserveAll        = fPreserveOwner | fPreserveMode | fPreserveTime,
        // --- Extract/Append/Update ---
        /// Follow symbolic links (instead of storing/extracting them)
        fFollowLinks        = (1<<2),
        // --- Extract/List/Append/Update ---
        /// Skip unsupported entries rather than making files out of them
        /// when extracting (the latter is the default POSIX requirement).
        /// On adding entry of unsupported type to the archive it will be
        /// skipping also.
        fSkipUnsupported    = (1<<15),
        // --- Miscellaneous ---
        /// Default flags
        fDefault            = fOverwrite | fPreserveAll
    };
    typedef unsigned int TFlags;  ///< Bitwise OR of EFlags

    /// Define a list of entries.
    typedef list<CArchiveEntryInfo> TEntries;


    //------------------------------------------------------------------------
    // Constructors
    //------------------------------------------------------------------------

protected:
    /// Construct an archive object of specified format.
    ///
    /// Declared as protected to avoid direct usage. 
    /// Use derived classes CArchiveFile or CArchiveMemory instead.
    /// @param format
    ///   Archive format
    /// @sa
    ///   CArchiveFile, CArchiveMemory
    CArchive(EFormat format);

public:
    /// Destructor
    ///
    /// Close the archive if currently open.
    /// @sa
    ///   Close
    virtual ~CArchive(void);

    //------------------------------------------------------------------------
    // Main functions
    //------------------------------------------------------------------------

    /// Create a new empty archive.
    ///
    /// @sa
    ///   Append
    virtual void Create(void);

    /// Close the archive making sure all pending output is flushed.
    /// 
    /// @sa
    ///   ~CArchive
    virtual void Close(void);

    /// Get information about archive entries.
    ///
    /// @return
    ///   An array containing information on those archive entries, whose
    ///   names match the preset mask if any, or all entries otherwise.
    /// @sa
    ///   SetMask
    virtual unique_ptr<TEntries> List(void);

    /// Verify archive integrity.
    ///
    /// Read through the archive without actually extracting anything from it.
    /// Test all archive entries, whose names match the preset mask.
    /// @return
    ///   An array containing information on those archive entries, whose
    ///   names match the preset mask if any, or all entries otherwise.
    /// @sa
    ///   SetMask
    virtual unique_ptr<TEntries> Test(void);

    /// Extract the entire archive.
    ///
    /// Extract all archive entries, whose names match the preset mask.
    /// Entries will be extracted into either current directory or
    /// a directory otherwise specified by SetBaseDir().
    /// @return
    ///   A list of entries that have been actually extracted.
    /// @sa
    ///   SetMask, SetBaseDir
    virtual unique_ptr<TEntries> Extract(void);

    /// Extract single file entry to a memory buffer.
    /// 
    /// @param info
    ///   [in]  Entry to extract.
    /// @param buf
    ///   [in]  Memory buffer for extracted data.
    /// @param buf_size
    ///   [in]  Size of memory buffer.
    /// @param out_size
    ///   [out] Size of extracted data in the buffer.
    /// @note
    ///   The buffer size should be big enough to fit whole extracted file.
    /// @sa 
    ///   ExtractFileToHeap, EctractFileToCallback, CArchiveEntryInfo::GetSize, List
    virtual void ExtractFileToMemory(const CArchiveEntryInfo& info, 
                                     void* buf, size_t buf_size, 
                                     size_t* /*out*/ out_size);

    /// Extract single file entry to a dynamically allocated memory buffer.
    /// 
    /// @param info
    ///   [in]  Entry to extract.
    /// @param buf_ptr
    ///   [out] Pointer to an allocated memory buffer.
    /// @param buf_size_ptr
    ///   [out] Size of allocated memory buffer, it is equal to the size of extracted data.
    /// @note
    ///   Do not forget to deallocate memory buffer after usage.
    ///   Use free() or AutoPtr<char, CDeleter<char>>.
    /// @sa 
    ///   ExtractFileToMemory, EctractFileToCallback, CArchiveEntryInfo::GetSize, List
    virtual void ExtractFileToHeap(const CArchiveEntryInfo& info,
                                   void** buf_ptr, size_t* buf_size_ptr);

    /// Extract single file entry using user-defined callback.
    /// 
    /// @param info
    ///   [in] Entry to extract.
    /// @param callback
    ///   [in] User callback for processing extracted data on the fly.
    /// @sa 
    ///   ExtractFileToMemory, EctractFileToHeap, CArchiveEntryInfo::GetSize, List
    virtual void ExtractFileToCallback(const CArchiveEntryInfo& info,
                                       IArchive::Callback_Write callback);

    /// Append an entry to the archive.
    ///
    /// Appended entry can be either a file, directory, symbolic link or etc.
    /// Each archive format have its own list of supported entry types.
    /// The name is taken with respect to the base directory, if any set.
    /// Adding a directory results in all its files and subdirectories to
    /// get added (examine the return value to find out what has been added).
    /// The names of all appended entries will be converted to Unix format
    /// (only forward slashes in the paths, and drive letter, if any on
    /// MS-Windows, stripped).
    /// @param path
    ///   Path to appended entry.
    /// @param level
    ///   Compression level (if selected format support it, or default).
    /// @param comment
    ///   Optional comment for the appended entry (if selected format support it).
    ///   For directories the comment will be added to upper directory only.
    /// @return
    ///   A list of entries appended.
    /// @note
    ///   On the current moment you can use this method to add files to newly
    ///   created archive only, modifying existed archive is not allowed. 
    /// @sa
    ///   Create, AppendFileFromMemory, SetBaseDir, HaveSupport, Update
    virtual unique_ptr<TEntries> Append(const string& path,
                                        ELevel level = CCompression::eLevel_Default,
                                        const string& comment = kEmptyStr);

    /// Append a single file entry to the created archive using data from memory buffer.
    /// 
    /// These function assign the current local time to added entry.
    /// @param name_in_archive
    ///   Name of the file entry in the archive. You can use any symbols,
    ///   that allowed for file names. Also, you can use relative path here, 
    ///   if you want to create some structure in the archive and put the data
    ///   to a file in the subdirectory.
    /// @param buf
    ///   Buffer with data to add.
    /// @param buf_size
    ///   Size of data in the buffer.
    /// @param level
    ///   Compression level (if selected format support it, or default).
    /// @param comment
    ///   Optional comment for the appended entry (if selected format support it).
    /// @return
    ///   An information about added entry.
    /// @note
    ///   On the current moment you can use this method to add files to newly
    ///   created archive only, modification existing archive is not allowed.
    /// @sa
    ///   Create, Append
    virtual unique_ptr<CArchive::TEntries>
        AppendFileFromMemory(const string& name_in_archive,
                             void* buf, size_t buf_size,
                             ELevel level = CCompression::eLevel_Default,
                             const string& comment = kEmptyStr);

    //------------------------------------------------------------------------
    // Utility functions
    //------------------------------------------------------------------------

    /// Get flags.
    virtual TFlags GetFlags(void) const { return m_Flags; }
    /// Set flags.
    virtual void   SetFlags(TFlags flags) { m_Flags = flags; }
    
    /// Get base directory to use for files while extracting from/adding to
    /// the archive, and in the latter case used only for relative paths.
    /// @sa
    ///   SetBaseDir
    virtual const string& GetBaseDir(void) const { return m_BaseDir; }

    /// Set base directory to use for files while extracting from/adding to
    /// the archive, and in the latter case used only for relative paths.
    /// @sa
    ///   GetBaseDir
    virtual void SetBaseDir(const string& dirname);

    /// Mask type enumerator.
    /// @enum eFullPathMask
    ///   CMask can select both inclusions and exclusions (in this order) of
    ///   fully-qualified archive entries. Whole entry name will be matched.
    ///   It always use Unix format for path matching, so please use forward
    ///   slash "/" as directory delimiter in the masks.
    /// @enum ePatternMask
    ///   CMask can select both inclusions and exclusions (in this order) of
    ///   patterns of archive entries. If eFullMask use the full path
    ///   to match mask, that ePatternMask allow to match each path component,
    ///   including names for each subdirectory and/or file name. This type
    ///   of mask is used only if eFullMask matches or not specified. 
    enum EMaskType {
        eFullPathMask,
        ePatternMask
    };

    /// Set name mask for processing.
    ///
    /// The set of masks is used to process existing entries in the archive,
    /// and apply to list, extract and append operations.
    /// If masks are not defined then all archive entries will be processed.
    /// Each "mask" is a set of inclusion and exclusion patterns, each of them
    /// can be a wildcard file mask or regular expression.
    /// @param mask
    ///   Set of masks (NULL unset the current set without setting a new one).
    /// @param own
    ///   Whether to take ownership on the mask (delete upon CArchive destruction).
    /// @param type
    ///   Type of the mask. You can set two types of masks at the same time.
    ///   The mask with type eFullPathMask applies to whole path name.
    ///   The mask with type ePatternMask applies to each path component, to all
    ///   subdirectories or file name, and if one of them matches, the entry
    ///   will be processed. If masks for both types are set, the entry will
    ///   be processed if it matches for each of them.
    /// @sa
    ///    UnsetMask, CMaskFileName, CMaskRegexp
    /// @note
    ///   Unset mask means wildcard processing (all entries match).
    void SetMask(CMask*      mask,
                 EOwnership  own   = eNoOwnership,
                 EMaskType   type  = eFullPathMask,
                 NStr::ECase acase = NStr::eNocase);

    /// Unset name mask for processing.
    ///
    /// @sa
    ///    SetMask
    /// @note
    ///   Unset mask means wildcard processing (all entries match).
    void UnsetMask(EMaskType type);
    void UnsetMask(void);

    /// Support check enumerator.
    /// 
    /// Use HaveSupport() to check that current archive format have support for
    /// specific feature.
    /// @enum eType
    ///    Check that archive can store entries with specific directory entry type.
    /// @enum eAbsolutePath
    ///    Archive can store full absolute path entries. Otherwise they will
    ///    be converted to relative path from root directory.
    /// @sa HaveSupport  
    enum ESupport {
        eType,
        eAbsolutePath
    };

    /// Check that current archive format have support for specific features.
    ///
    /// @param feature
    ///   Name of the feature to check.
    /// @param param
    ///   Additional parameter (for eType only).
    /// @sa ESupport
    bool HaveSupport(ESupport feature, int param = 0);

protected:
    /// Archive open mode
    enum EOpenMode {
        eNone = 0,
        eRO   = 1,
        eWO   = 2,
        eRW   = eRO | eWO
    };

    /// Action, performed on the archive
    enum EAction {
        eUndefined =  eNone,
        eCreate    = (1 <<  8) | eWO,
        eAppend    = (1 <<  9) | eWO,
        eList      = (1 << 10) | eRO,
        eUpdate    = eList | eAppend,
        eExtract   = (1 << 11) | eRO,
        eTest      = eList | eExtract
    };

    /// Mask storage
    struct SMask {
        CMask*       mask;
        NStr::ECase  acase;
        EOwnership   owned;
        SMask(void)
            : mask(0), acase(NStr::eNocase), owned(eNoOwnership)
        {}
    };

protected:
    //------------------------------------------------------------------------
    // User-redefinable callback
    //------------------------------------------------------------------------

    /// Return false to skip the current entry when processing.
    ///
    /// Note that the callback can encounter multiple entries of the same file
    /// in case the archive has been updated (so only the last occurrence is
    /// the actual copy of the file when extracted).
    virtual bool Checkpoint(const CArchiveEntryInfo& /*current*/, EAction /*action*/)
        { return true; }

protected:
    //------------------------------------------------------------------------
    // Redefinable methods for inherited classes
    //------------------------------------------------------------------------

    /// Open archive.
    virtual void Open(EAction action) = 0;
    /// Process current entry (List/Test/Extract/Append)
    virtual void SkipEntry   (void);
    virtual void TestEntry   (void);
    virtual void ExtractEntry(const CDirEntry& dst);
    virtual void AppendEntry (const string& path, ELevel level);

protected:
    //------------------------------------------------------------------------
    // Internal processing methods
    //------------------------------------------------------------------------

    // Open archive.
    // Wrapper around Open() that perform all necessary checks and processing.
    void x_Open(EAction action);

    // Read the archive and do the requested "action" on current entry.
    unique_ptr<TEntries> x_ReadAndProcess(EAction action);

    // Append an entry from the file system to the archive.
    unique_ptr<TEntries> x_Append(const string&   path,
                                  ELevel          level,
                                  const string&   comment,
                                  const TEntries* toc = NULL);
    // Append a single entry from the file system to the archive.
    // Wrapper around AppendEntry(). 
    // Return FALSE if entry should be skipped (via user Checkpoint()).
    bool x_AppendEntry(const string& path, ELevel level = CCompression::eLevel_Default);

    // Extract current entry.
    // Wrapper around ExtractEntry() that perform all necessary checks and
    // flags processing.
    void x_ExtractEntry(const TEntries* prev_entries);

    // Restore attributes of an entry in the file system.
    // If "dst" is not specified, then the destination path will be
    // constructed from "info", and the base directory (if any).  Otherwise,
    // "dst" will be used "as is", assuming it corresponds to "info".
    void x_RestoreAttrs(const CArchiveEntryInfo& info,
                        const CDirEntry*         dst = NULL) const;

protected:
    unique_ptr<IArchive>  m_Archive;       ///< Pointer to interface to EFormat-specific archive support
    EFormat               m_Format;        ///< Archive format
    IArchive::ELocation   m_Location;      ///< Archive location (file/memory)
    TFlags                m_Flags;         ///< Bitwise OR of flags
    string                m_BaseDir;       ///< Base directory for relative paths
    CArchiveEntryInfo     m_Current;       ///< Information about current entry being processed
    SMask                 m_MaskFullPath;  ///< Set of masks for operations (full path)
    SMask                 m_MaskPattern;   ///< Set of masks for operations (path components)
    EOpenMode             m_OpenMode;      ///< What was it opened for
    bool                  m_Modified;      ///< True after at least one write

protected:
    // Prohibit assignment and copy
    CArchive& operator=(const CArchive&);
    CArchive(const CArchive&);
};



//////////////////////////////////////////////////////////////////////////////
///
/// CArchiveFile -- file-based archive.
///
/// Throws exceptions on errors.

class NCBI_XUTIL_EXPORT CArchiveFile : public CArchive
{
public:
    /// Constructor for file-based archive.
    ///
    /// @param format
    ///   Archive format.
    /// @param filename
    ///   Path to archive file name.
    ///   Note, that directory in that archive file will be create should exists.
    /// @sa
    ///   Create, Extract, List, Test, Append
    CArchiveFile(EFormat format, const string& filename);

protected:
    /// Open the archive for specified action.
    virtual void Open(EAction action);

protected:
    string m_FileName;   ///< Archive file name

private:
    // Prohibit assignment and copy
    CArchiveFile& operator=(const CArchiveFile&);
    CArchiveFile(const CArchiveFile&);
};



//////////////////////////////////////////////////////////////////////////////
///
/// CArchiveMemory -- memory-based archive.
///
/// Throws exceptions on errors.

class NCBI_XUTIL_EXPORT CArchiveMemory : public CArchive
{
public:
    /// Constructor for memory-based archive.
    ///
    /// @param format
    ///   Archive format.
    /// @param buf
    ///   Pointer to an archive located in memory. Used only to open already
    ///   existed archive for reading. Never used if you would like to create
    ///   new archive, see Create().
    /// @param buf_size
    ///   Size of the archive.
    /// @sa
    ///   Create, Extract, List, Test, Append
    CArchiveMemory(EFormat format, const void* buf = NULL, size_t buf_size = 0);

    /// Create a new empty archive in memory.
    ///
    /// @param initial_allocation_size
    ///   Estimated size of the archive, if known.
    ///   Bigger size allow to avoid extra memory reallocations.
    /// @sa
    ///   Append, Finalize
    virtual void Create(size_t initial_allocation_size);
    virtual void Create(void);

    /// Save current opened/created archive to file.
    ///
    /// @param filename
    ///   Path to the archive file name. The directory in that archive
    ///   file will be create should exists. If destination file
    ///   already exists, it will be overwritten.
    /// @note
    ///   Newly created archive should be finalized first.
    /// @sa
    ///   Create, Finalize, Load
    void Save(const string& filename);

    /// Load existing archive from file system to memory.
    ///
    /// @param filename
    ///   Path to the existing archive.
    /// @note
    ///   If you have opened or created archive, it will be automatically closed.
    /// @sa
    ///   Open, Save
    void Load(const string& filename);

    /// Finalize the archive created in memory.
    ///
    /// Return pointer to a buffer with created archive and its size.
    /// After this call you cannot write to archive anymore, but you can
    /// read from it. Returning pointer to buffer and its size also
    /// will be saved internally and used for opening archive for reading
    /// (see constructor).
    /// @param buf_ptr
    ///   Pointer to an archive located in memory.
    /// @param buf_size_ptr
    ///   Size of the newly created archive.
    /// @note
    ///   Do not forget to deallocate memory buffer after usage.
    ///   Use free() or AutoPtr<char, CDeleter<char>>.
    /// @sa
    ///   Create, Close
    virtual void Finalize(void** buf_ptr, size_t* buf_size_ptr);

protected:
    /// Open the archive for specified action.
    virtual void Open(EAction action);

protected:
    // Open
    const void* m_Buf;         ///< Buffer where the opening archive is located
    size_t      m_BufSize;     ///< Size of m_Buf
    /// Holder for the pointer to memory buffer that will be automatically
    /// deallocated if we own it (used for Load() only).
    /// m_Buf will have the same pointer value.
    AutoArray<char> m_OwnBuf;
    // Create
    ///< Initial allocation size for created archive
    size_t m_InitialAllocationSize;

private:
    // Prohibit assignment and copy
    CArchiveMemory& operator=(const CArchiveMemory&);
    CArchiveMemory(const CArchiveMemory&);
};


END_NCBI_SCOPE


/* @} */


#endif  /* UTIL_COMPRESS__ARCHIVE__HPP */
