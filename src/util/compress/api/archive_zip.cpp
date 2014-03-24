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
 */
 
#include <ncbi_pch.hpp>
#include <util/error_codes.hpp>
#include <util/compress/zlib.hpp>
#include "archive_zip.hpp"

#define NCBI_USE_ERRCODE_X  Util_Compress


BEGIN_NCBI_SCOPE


// Directly include miniz library
#include "miniz/miniz.c"


/////////////////////////////////////////////////////////////////////////
//
// Constants / macros / typedefs
//

/// ZIP archive handle type definition.
struct SZipHandle {
    SZipHandle() {
        Reset();
    }
    void Reset(void) {
        memset(&zip, 0, sizeof(zip));
    }
    mz_zip_archive zip;
};


// Macros to work with zip-archive handle
#define ZIP_HANDLE  &(m_Handle->zip)
#define ZIP_CHECK   _ASSERT(m_Handle != NULL)
#define ZIP_NEW \
    { \
        _ASSERT(m_Handle == NULL);   \
        m_Handle = new SZipHandle(); \
        _ASSERT(m_Handle != NULL);   \
    }

#define ZIP_DELETE \
    { \
        _ASSERT(m_Handle != NULL);  \
        delete m_Handle; \
        m_Handle = NULL; \
    }

// Throw exception
#define ZIP_THROW(errcode, message) \
    NCBI_THROW(CArchiveException, errcode, message)



/////////////////////////////////////////////////////////////////////////
//
// CZipArchive
//

CArchiveZip::~CArchiveZip(void)
{
    try {
        if ( m_Handle ) {
            Close();
            delete m_Handle;
        }
    }
    COMPRESS_HANDLE_EXCEPTIONS(94, "CArchiveZip::~CArchiveZip");
}


void CArchiveZip::CreateFile(const string& filename)
{
    ZIP_NEW;
    m_Mode = eWrite;
    m_Location = eFile;
    mz_bool status = mz_zip_writer_init_file(ZIP_HANDLE, filename.c_str(), 0);
    if (!status) {
        m_Handle = NULL;
        ZIP_THROW(eCreate, "Cannot create archive file '" + filename + "'");
    }
    return;
}


void CArchiveZip::CreateMemory(size_t initial_allocation_size)
{
    ZIP_NEW;
    m_Mode = eWrite;
    m_Location = eMemory;
    mz_bool status = mz_zip_writer_init_heap(ZIP_HANDLE, 0, initial_allocation_size);
    if (!status) {
        m_Handle = NULL;
        ZIP_THROW(eCreate, "Cannot create archive in memory");
    }
    return;
}


void CArchiveZip::OpenFile(const string& filename)
{
    ZIP_NEW;
    m_Mode = eRead;
    m_Location = eFile;
    mz_bool status = mz_zip_reader_init_file(ZIP_HANDLE, filename.c_str(), 0);
    if (!status) {
        ZIP_DELETE;
        ZIP_THROW(eOpen, "Cannot open archive file '" + filename + "'");
    }
    return;
}


void CArchiveZip::OpenMemory(const void* buf, size_t size)
{
    ZIP_NEW;
    m_Mode = eRead;
    m_Location = eMemory;
    mz_bool status = mz_zip_reader_init_mem(ZIP_HANDLE, buf, size, 0);
    if (!status) {
        ZIP_DELETE;
        ZIP_THROW(eOpen, "Cannot open archive in memory");
    }
    return;
}


void CArchiveZip::FinalizeMemory(void** buf, size_t* size)
{
    _ASSERT(m_Location == eMemory);
    _ASSERT(m_Mode == eWrite);
    _ASSERT(buf);
    _ASSERT(size);
    ZIP_CHECK;
    
    *buf = NULL;
    *size = 0;
    mz_bool status = mz_zip_writer_finalize_heap_archive(ZIP_HANDLE, buf, size);
    if (!status) {
        // Deallocate memory buffer to avoid memory leak
        if (*buf) {
            free(*buf);
            *buf = NULL;
            *size = 0;
        }
        ZIP_THROW(eMemory, "Cannot finalize archive in memory");
    }
    return;
}


void CArchiveZip::Close(void)
{
    _ASSERT(m_Mode == eRead || m_Mode == eWrite);
    ZIP_CHECK;
    
    mz_bool status = true;
    switch(m_Mode) {
    case eRead:
        status = mz_zip_reader_end(ZIP_HANDLE);
        break;
    case eWrite:
        // Automatically finalize file archive only.
        // The archive located in memory will be lost
        // on this step, unless FinalizeMemory() was
        // not called before.
        if (m_Location == eFile) {
            status = mz_zip_writer_finalize_archive(ZIP_HANDLE);
        }
        if ( !mz_zip_writer_end(ZIP_HANDLE) ) {
            status = false;
        }
        break;
    default:
        break;
    }
    if (!status) {
        ZIP_THROW(eClose, "Error closing archive");
    }
    ZIP_DELETE;
    return;
}


size_t CArchiveZip::GetNumEntries(void)
{
    _ASSERT(m_Mode == eRead);
    ZIP_CHECK;
    mz_uint n = mz_zip_reader_get_num_files(ZIP_HANDLE);
    return n;
}


void CArchiveZip::GetEntryInfo(size_t index, CArchiveEntryInfo* info)
{
    _ASSERT(m_Mode == eRead);
    _ASSERT(info);
    ZIP_CHECK;

    // Check index to fit 'unsigned int' which used internally in miniz
    if (index > (size_t)kMax_UInt) {
        NCBI_THROW(CCoreException, eInvalidArg, "Bad index value");
    }
    // Get file informations
    mz_zip_archive_file_stat fs;
    mz_bool status = mz_zip_reader_file_stat(ZIP_HANDLE, (mz_uint)index, &fs);
    if (!status) {
        ZIP_THROW(eList, "Cannot get entry information by index " +
            NStr::SizetToString(index));
    }
    // Copy known data into CArchiveEntryInfo
    info->m_Index            = index;
    info->m_CompressedSize   = fs.m_comp_size;
    info->m_Stat.st_size     = fs.m_uncomp_size;
    info->m_Stat.st_atime    = fs.m_time;
    info->m_Stat.st_ctime    = fs.m_time;
    info->m_Stat.st_mtime    = fs.m_time;
    info->m_Name.assign(fs.m_filename);
    info->m_Comment.assign(fs.m_comment, fs.m_comment_size);

    // Rough check on a directory (using MS-DOS type compatible attribute)?
    status = mz_zip_reader_is_file_a_directory(ZIP_HANDLE, (mz_uint)index);
    info->m_Type = status ? CDirEntry::eDir : CDirEntry::eFile;

    // miniz don't work with entry attributes, because it
    // is very OS- and creation software dependent. 
    // Try to analyze some common cases for Unix-type attributes:

    char ver = fs.m_version_made_by >> 8;
    mode_t mode = (fs.m_external_attr >> 16) & 0xFFFF;

    switch (ver) {
    // Unix
    case 1:  // Amiga 
    case 2:  // VAX VMS
    case 3:  // Unix
    case 4:  // VM/CMS
    case 5:  // Atari ST
    case 7:  // Macintosh
    case 8:  // Z-System
    case 9:  // CP/M
        {{
            info->m_Stat.st_mode = mode;
            info->m_Type = CDirEntry::GetType(info->m_Stat);
            if (info->m_Type == CDirEntry::eUnknown) {
                // Reset attributes value, we cannot be sure that
                // it hold correct value
                info->m_Stat.st_mode = 0;
            }
        }}
        break;
    // Dos
    case 0:  // MS-DOS or OS/2 FAT 
    case 6:  // OS/2 HPFS
    // Unknown
    default:
        break;
    }
    return;
}


bool CArchiveZip::HaveSupport_Type(CDirEntry::EType type)
{
    switch (type) {
    // supported
    case CDirEntry::eFile:
    case CDirEntry::eDir:
        return true;
    // unsupported
    case CDirEntry::eLink:
    case CDirEntry::eBlockSpecial:
    case CDirEntry::eCharSpecial:
    case CDirEntry::ePipe:
    case CDirEntry::eDoor:
    case CDirEntry::eSocket:
    case CDirEntry::eUnknown:
    default:
        break;
    }
    return false;
}


void CArchiveZip::ExtractEntryToFileSystem(const CArchiveEntryInfo& info,
                                           const string& dst_path)
{
    _ASSERT(m_Mode == eRead);
    ZIP_CHECK;

    // If this is a directory entry, we should create it.
    if (info.GetType() == CDirEntry::eDir) {
        if (!CDir(dst_path).CreatePath()) {
            ZIP_THROW(eExtract, "Cannot create directory '" + dst_path + "'");
        }
        return;
    }
    // The code below extract files only.
    mz_bool status;
    MZ_FILE *pFile = MZ_FOPEN(dst_path.c_str(), "wb");
    if (!pFile) {
        ZIP_THROW(eExtract, "Cannot create target file '" + dst_path + "'");
    }
    status = mz_zip_reader_extract_to_callback(ZIP_HANDLE, (mz_uint)info.m_Index,
                                               mz_zip_file_write_callback, pFile, 0);
    if (MZ_FCLOSE(pFile) == EOF) {
        ZIP_THROW(eExtract, "Error close file '" + dst_path + "'");
    }
    if (!status) {
        ZIP_THROW(eExtract, "Error extracting entry with index '" +
            NStr::SizetToString(info.m_Index) + " to file '" + dst_path + "'");
    }
    return;
}


void CArchiveZip::ExtractEntryToMemory(const CArchiveEntryInfo& info, void* buf, size_t size)
{
    _ASSERT(m_Mode == eRead);
    _ASSERT(buf);
    _ASSERT(size);
    ZIP_CHECK;

    // If this is a directory entry, skip it
    if (info.GetType() == CDirEntry::eDir) {
        return;
    }
    // The code below extract files only.
    mz_bool status;
    status = mz_zip_reader_extract_to_mem(ZIP_HANDLE, (mz_uint)info.m_Index, buf, size, 0);
    if (!status) {
        ZIP_THROW(eExtract, "Error extracting entry with index " +
            NStr::SizetToString(info.m_Index) + " to memory");
    }
    return;
}


// Structure to pass all necessary data to write callback
struct SWriteCallbackData {
    IArchive::Callback_Write callback;
    const CArchiveEntryInfo* info;
};

// Callback for extracting data, call user-defined callback to do a real job.
extern "C" 
{
    static size_t s_ZipExtractCallback(void* params, mz_uint64 /*ofs*/, const void* buf, size_t n)
    {
        _ASSERT(params);
        SWriteCallbackData& data = *(SWriteCallbackData*)(params);
        // Call user callback
        size_t processed = data.callback(*data.info, buf, n);
        return processed;
    }
}

void CArchiveZip::ExtractEntryToCallback(const CArchiveEntryInfo& info, Callback_Write callback)
{
    _ASSERT(m_Mode == eRead);
    ZIP_CHECK;

    // If this is a directory entry, skip it
    if (info.GetType() == CDirEntry::eDir) {
        return;
    }
    // The code below extract files only.
    SWriteCallbackData data;
    data.callback = callback;
    data.info     = &info;
    mz_bool status;
    status = mz_zip_reader_extract_to_callback(ZIP_HANDLE, (mz_uint)info.m_Index,
                                               s_ZipExtractCallback, &data, 0);
    if (!status) {
        ZIP_THROW(eExtract, "Error extracting entry with index " +
            NStr::SizetToString(info.m_Index) + " to callback");
    }
    return;
}


// Dummy callback to test an entry extraction
extern "C" 
{
    static size_t s_ZipTestCallback(void* /*pOpaque*/, mz_uint64 /*ofs*/, 
                                   const void* /*pBuf*/, size_t n)
    {
        // Just return number of extracted bytes
        return n;
    }
}

void CArchiveZip::TestEntry(const CArchiveEntryInfo& info)
{
    _ASSERT(m_Mode == eRead);
    ZIP_CHECK;

    // If this is a directory entry, skip it
    if (info.GetType() == CDirEntry::eDir) {
        return;
    }
    // The code below test files only.
    mz_bool status;
    status = mz_zip_reader_extract_to_callback(ZIP_HANDLE, (mz_uint)info.m_Index,
                                               s_ZipTestCallback, 0, 0);
    if (!status) {
        ZIP_THROW(eExtract, "Test entry with index " +
            NStr::SizetToString(info.m_Index) + " failed");
    }
    return;
}


void CArchiveZip::AddEntryFromFileSystem(const CArchiveEntryInfo& info, 
                                         const string& src_path, ELevel level)
{
    const string& comment = info.m_Comment;
    mz_uint16 comment_size = (mz_uint16)comment.size();
    mz_bool status;
    if (info.m_Type == CDirEntry::eDir) {
        status = mz_zip_writer_add_mem_ex(ZIP_HANDLE, info.GetName().c_str(),
                                          NULL, 0, /* empty buffer */
                                          comment.c_str(), comment_size, (mz_uint)level, 0, 0);
    } else {
        // Files only
        _ASSERT(info.m_Type == CDirEntry::eFile);
        status = mz_zip_writer_add_file  (ZIP_HANDLE,
                                          info.GetName().c_str(), src_path.c_str(),
                                          comment.c_str(), comment_size, (mz_uint)level);
    }
    if (!status) {
        ZIP_THROW(eAppend, "Error appending entry '" + src_path + "' to archive");
    }
    return;
}


void CArchiveZip::AddEntryFromMemory(const CArchiveEntryInfo& info,
                                     void* buf, size_t size, ELevel level)
{
    const string& comment = info.m_Comment;
    mz_uint16 comment_size = (mz_uint16)comment.size();
    mz_bool status;
    status = mz_zip_writer_add_mem_ex(ZIP_HANDLE, info.GetName().c_str(),
                                      buf, size, comment.c_str(), comment_size, (mz_uint)level, 0, 0);
    if (!status) {
        ZIP_THROW(eAppend, "Error appending entry with name '" +
            info.GetName() + "' from memory to archive");
    }
    return;
}


END_NCBI_SCOPE
