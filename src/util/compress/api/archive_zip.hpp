#ifndef UTIL_COMPRESS__ARCHIVE_ZIP__HPP
#define UTIL_COMPRESS__ARCHIVE_ZIP__HPP

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
 *   Compression archive API - ZIP file support.
 *
 * Implementation based on this fork of 'miniz' library:
 *   https://github.com/uroni/miniz
 * It have support for ZIP64 format, can handle archive files > 4GB in size, 
 * and the number of files in the archive is not limited to 65535.
 *
 */

///  @file archive_.hpp
///  Archive API.

#include <util/compress/archive_.hpp>


/** @addtogroup Compression
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Forward declaration of ZIP archive handle.
struct SZipHandle;


/////////////////////////////////////////////////////////////////////////////
///
/// CArchiveZip -- implementation of IArchive interface for ZIP-archives
///
/// Throws exceptions on errors.

class NCBI_XUTIL_EXPORT CArchiveZip : public IArchive
{
public:
    /// Constructor
    CArchiveZip(void) : m_Handle(NULL) {}
    /// Destructor
    virtual ~CArchiveZip(void);

    /// Create new archive file.
    ///
    /// @param filename
    ///   File name of the archive to create.
    /// @note
    ///   File can be overwritten if exists.
    /// @sa
    ///   CreateMemory, AddEntryFromFile, AddEntryFromMemory
    virtual void CreateFile(const string& filename);

    /// Create new archive located in memory.
    ///
    /// @param initial_allocation_size
    ///   Estimated size of the archive, if known.
    ///   Bigger size allow to avoid extra memory reallocations.
    /// @sa
    ///   FinalizeMemory, AddEntryFromFile, AddEntryFromMemory
    virtual void CreateMemory(size_t initial_allocation_size = 0);

    /// Open archive file for reading.
    ///
    /// @param filename
    ///   File name of the existing archive to open.
    /// @sa
    ///   CreateFile, OpenMemory, ExtractEntryToFileSystem, ExtractEntryToMemory
    virtual void OpenFile(const string& filename);
    
    /// Open archive located in memory for reading.
    /// @param buf
    ///   Pointer to an archive located in memory. Used only to open already
    ///   existed archive for reading. 
    /// @param size
    ///   Size of the archive.
    /// @sa
    ///   CreateMemory, OpenFile, ExtractEntryToFileSystem, ExtractEntryToMemory
    virtual void OpenMemory(const void* buf, size_t size);

    /// Close the archive.
    /// 
    /// For file-based archives it flushes all pending output.
    /// All added entries will appears in the archive only after this call.
    /// This method will be automatically called from destructor
    /// if you forgot to call it directly. But note, that if the archive
    /// is created in memory, it will be lost. You should call 
    /// FinalizeMemory() to get it.
    /// @sa
    ///   FinalizeMemory, OpenFile, OpenMemory
    virtual void Close(void);

    /// Finalize the archive created in memory.
    ///
    /// Return pointer to buffer with created archive and its size.
    /// The only valid operation after this call is Close(). 
    /// @param buf
    ///   Pointer to an archive located in memory.
    /// @param size
    ///   Size of the newly created archive.
    /// @note
    ///   Do not forget to deallocate memory buffer after usage.
    /// @sa
    ///   CreateMemory, Close
    virtual void FinalizeMemory(void** buf, size_t* size);

    /// Returns the total number of entries in the archive.
    virtual size_t GetNumEntries(void);

    /// Get detailed information about an archive entry by index.
    ///
    /// @param index
    ///   Zero-based index of entry in the archive.
    /// @param info
    ///   Pointer to entry information structure that will be filled with
    ///   information about entry with specified index.
    /// @note
    ///   Note that the archive can contain multiple versions of the same entry
    ///   (in case if an update was done on it), all of which but the last one
    ///   are to be ignored.
    /// @sa
    ///   CArchiveEntryInfo
    virtual void GetEntryInfo(size_t index, CArchiveEntryInfo* info);

    /// Check that current archive format have support for specific feature.
    /// @sa CArchive
    virtual bool HaveSupport_Type(CDirEntry::EType type);
    virtual bool HaveSupport_AbsolutePath(void) { return false; };

    /// Extracts an archive entry to file system.
    /// 
    /// @param info
    ///   Entry to extract.
    /// @param dst_path
    ///   Destination path for extracted entry.
    /// @note
    ///   This method do not set last accessed and modified times.
    ///   See CArchive.
    /// @sa 
    ///   ExtractEntryToMemory, ExtractEntryToCallback, CArchiveEntryInfo::GetSize
    virtual void ExtractEntryToFileSystem(const CArchiveEntryInfo& info,
                                          const string& dst_path);

    /// Extracts an archive file to a memory buffer.
    /// 
    /// Do nothing for entries of other types.
    /// @param info
    ///   Entry to extract.
    /// @param buf
    ///   Memory buffer for extracted data.
    /// @param size
    ///   Size of memory buffer.
    ///   Note, that the buffer size should be big enough to fit whole extracted file.
    /// @sa 
    ///   ExtractEntryToFileSystem, ExtractEntryToCallback, CArchiveEntryInfo::GetSize
    virtual void ExtractEntryToMemory(const CArchiveEntryInfo& info,
                                      void* buf, size_t size);

    /// Extracts an archive file using user-defined callback to process extracted data.
    ///
    /// Do nothing for entries of other types.
    /// @param info
    ///   Entry to extract.
    /// @param callback
    ///   User callback for processing extracted data on the fly.
    /// @sa 
    ///   ExtractEntryToFileSystem, ExtractEntryToMemory, CArchiveEntryInfo::GetSize
    virtual void ExtractEntryToCallback(const CArchiveEntryInfo& info,
                                        Callback_Write callback);

    /// Verify entry integrity.
    ///
    /// @param info
    ///   Entry to verify.
    virtual void TestEntry(const CArchiveEntryInfo& info);

    /// Don't need to be implemented for ZIP format.
    virtual void SkipEntry(const CArchiveEntryInfo& /*info*/) {};

    /// Add single entry to newly created archive from file system.
    ///
    /// @param info
    ///   Entry information to add. It should have name and type of adding entry at least.
    ///   If added entry is a directory, just add an information about it to archive.
    ///   CArchive process all directories recursively.
    /// @param src_path
    ///   Path to file system object to add.
    /// @param level
    ///   Compression level used to compress file .
    /// @sa
    ///   CreateFile, CreateMemory, AddEntryFromMemory
    virtual void AddEntryFromFileSystem(const CArchiveEntryInfo& info,
                                        const string& src_path, ELevel level);

    /// Add entry to newly created archive from memory buffer.
    /// 
    /// @param info
    ///   Entry information to add. It should have name and type of adding entry at least.
    /// @param buf
    ///   Memory buffer with data to add.
    /// @param size
    ///   Size of data in memory buffer.
    /// @param level
    ///   Compression level for added data.
    /// @sa
    ///   CreateFile, CreateMemory, AddEntryFromFileSystem
    virtual void AddEntryFromMemory(const CArchiveEntryInfo& info,
                                    void* buf, size_t size, ELevel level);

protected:
    SZipHandle* m_Handle;  ///< Archive handle
};


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL_COMPRESS__ARCHIVE_ZIP__HPP */
