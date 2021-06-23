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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbatlas.cpp
/// Implementation for the CSeqDBAtlas class and several related
/// classes, which provide control of a set of memory mappings.
#include <ncbi_pch.hpp>

#include <objtools/blast/seqdb_reader/impl/seqdbatlas.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>
#include <memory>
#include <algorithm>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>

#include <corelib/ncbi_system.hpp>

#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

BEGIN_NCBI_SCOPE

// Further optimizations:

// 1. Regions could be stored in a map<>, sorted by file, then offset.
// This would allow a binary search instead of sequential and would
// vastly improve the "bad case" of 100_000s of buffers of file data.

// 2. "Scrounging" could be done in the file case.  It is bad to read
// 0-4096 then 4096 to 8192, then 4000-4220.  The third could use the
// first two to avoid reading.  It should either combine the first two
// regions into a new region, or else just copy to a new region and
// leave the old ones alone (possibly marking the old regions as high
// penalty).  Depending on refcnt, penalty, and region sizes.

// Throw function

void SeqDB_ThrowException(CSeqDBException::EErrCode code, const string & msg)
{
    switch(code) {
    case CSeqDBException::eArgErr:
        NCBI_THROW(CSeqDBException, eArgErr, msg);

    case CSeqDBException::eFileErr:
        NCBI_THROW(CSeqDBException, eFileErr, msg);

    default:
        NCBI_THROW(CSeqDBException, eMemErr, msg);
    }
}

/// Check the size of a number relative to the scope of a numeric type.

template<class TIn, class TOut>
TOut SeqDB_CheckLength(TIn value)
{
    TOut result = TOut(value);

    if (sizeof(TOut) < sizeof(TIn)) {
        if (TIn(result) != value) {
            SeqDB_ThrowException(CSeqDBException::eFileErr,
                                 "Offset type does not span file length.");
        }
    }

    return result;
}

CSeqDBAtlas::CSeqDBAtlas(bool use_atlas_lock)
     :m_UseLock           (use_atlas_lock),
      m_MaxFileSize       (0),      
      m_SearchPath        (GenerateSearchPath())
{
    m_OpenedFilesCount = 0;
    m_MaxOpenedFilesCount = 0;
}

CSeqDBAtlas::~CSeqDBAtlas()
{
}

CMemoryFile* CSeqDBAtlas::GetMemoryFile(const string& fileName)
{
    std::lock_guard<std::mutex> guard(m_FileMemMapMutex);
    auto it = m_FileMemMap.find(fileName);
    if (it != m_FileMemMap.end()) {
    	it->second.get()->m_Count++;
    	//LOG_POST(Info << "File: " << fileName << " count " << it->second.get()->m_Count);
        return it->second.get();
    }
    CAtlasMappedFile* file(new CAtlasMappedFile(fileName));
    m_FileMemMap[fileName].reset(file);
   	_TRACE("Open File: " << fileName);
    ChangeOpenedFilseCount(CSeqDBAtlas::eFileCounterIncrement);
    return file;
}

CMemoryFile* CSeqDBAtlas::ReturnMemoryFile(const string& fileName)
{
    std::lock_guard<std::mutex> guard(m_FileMemMapMutex);
    auto it = m_FileMemMap.find(fileName);
    if (it == m_FileMemMap.end()) {
        NCBI_THROW(CSeqDBException, eMemErr, "File not in mapped file list: " + fileName);
    }
    it->second.get()->m_Count--;
   	//LOG_POST(Info << "Return File: " << fileName << "count " << it->second.get()->m_Count);
   	if ((GetOpenedFilseCount() > CSeqDBAtlas::e_MaxFileDescritors) &&
   		(it->second.get()->m_isIsam) && (it->second.get()->m_Count == 0)) {
   		m_FileMemMap.erase(it);
   		LOG_POST(Info << "Unmap max file descriptor reached: " << fileName);
   		ChangeOpenedFilseCount(CSeqDBAtlas::eFileCounterDecrement);
   	}
    return NULL;
}

bool CSeqDBAtlas::DoesFileExist(const string & fname)
{
    TIndx length(0);
    return GetFileSize(fname, length);
}

bool CSeqDBAtlas::GetFileSize(const string   & fname,
                              TIndx          & length)
{
    return GetFileSizeL(fname, length);
}

bool CSeqDBAtlas::GetFileSizeL(const string & fname, TIndx &length)
{
    {
        std::lock_guard<std::mutex> guard(m_FileSizeMutex);
        auto it = m_FileSize.find(fname);
        if (it != m_FileSize.end()) {
            length = it->second.second;
            return it->second.first;
        }
    }

    pair<bool, TIndx> val;
    CFile whole(fname);
    Int8 file_length = whole.GetLength();

    if (file_length >= 0) {
        val.first = true;
        val.second = SeqDB_CheckLength<Int8, TIndx>(file_length);
    }
    else {
        val.first = false;
        val.second = 0;
    }

    {
        std::lock_guard<std::mutex> guard(m_FileSizeMutex);
        m_FileSize[fname] = val;

        if (file_length >= 0 && (Uint8)file_length > m_MaxFileSize)
            m_MaxFileSize = file_length;
    }

    length = val.second;
    return val.first;
}

/// Simple idiom for RIIA with malloc + free.
struct CSeqDBAutoFree {
    /// Constructor.
    CSeqDBAutoFree()
        : m_Array(0)
    {
    }

    /// Specify a malloced area of memory.
    void Set(const char * x)
    {
        m_Array = x;
    }

    /// Destructor will free that memory.
    ~CSeqDBAutoFree()
    {
        if (m_Array) {
            free((void*) m_Array);
        }
    }

private:
    /// Pointer to malloced memory.
    const char * m_Array;
};



/// Releases allocated memory
void CSeqDBAtlas::RetRegion(const char * datap)
{
	delete [] datap;
}


char * CSeqDBAtlas::Alloc(size_t length, bool clear)
{
    if (! length) {
        length = 1;
    }

    // Allocate/clear

    char * newcp = 0;

    try {
        newcp = new char[length];

        // new() should have thrown, but some old implementations are
        // said to be non-compliant in this regard:

        if (! newcp) {
            throw std::bad_alloc();
        }

        if (clear) {
            memset(newcp, 0, length);
        }
    }
    catch(std::bad_alloc) {
        NCBI_THROW(CSeqDBException, eMemErr,
                   "CSeqDBAtlas::Alloc: allocation failed.");
    }

    return newcp;
}

void CSeqDBAtlas::RegisterExternal(CSeqDBMemReg   & memreg,
                                   size_t           bytes,
                                   CSeqDBLockHold & locked)
{
    if (bytes > 0) {
        Lock(locked);
        _ASSERT(memreg.m_Bytes == 0);        
	    memreg.m_Bytes = bytes;
    }
}

void CSeqDBAtlas::UnregisterExternal(CSeqDBMemReg & memreg)
{
    size_t bytes = memreg.m_Bytes;

    if (bytes > 0) {        
        memreg.m_Bytes = 0;
    }
}





CSeqDBAtlasHolder::CSeqDBAtlasHolder(CSeqDBLockHold * lockedp,
                                     bool use_atlas_lock)
    
{
    {{
    CFastMutexGuard guard(m_Lock);

    if (m_Count == 0) {
        m_Atlas = new CSeqDBAtlas(use_atlas_lock);
    }
    m_Count ++;
    }}
}


// FIXME: This constrctor is deprecated
CSeqDBAtlasHolder::CSeqDBAtlasHolder(bool use_atlas_lock,
                                     CSeqDBLockHold* locdep)
{
    {{
    CFastMutexGuard guard(m_Lock);

    if (m_Count == 0) {
        m_Atlas = new CSeqDBAtlas(use_atlas_lock);
    }
    m_Count ++;
    }}
}


DEFINE_CLASS_STATIC_FAST_MUTEX(CSeqDBAtlasHolder::m_Lock);

CSeqDBAtlasHolder::~CSeqDBAtlasHolder()
{
    
    CFastMutexGuard guard(m_Lock);
    m_Count --;

    if (m_Count == 0) {
        delete m_Atlas;
    }
}

CSeqDBAtlas & CSeqDBAtlasHolder::Get()
{
    _ASSERT(m_Atlas);
    return *m_Atlas;
}

CSeqDBLockHold::~CSeqDBLockHold()
{
    CHECK_MARKER();

    m_Atlas.Unlock(*this);
    BREAK_MARKER();
}

int CSeqDBAtlasHolder::m_Count = 0;
CSeqDBAtlas * CSeqDBAtlasHolder::m_Atlas = NULL;


CSeqDB_AtlasRegionHolder::
CSeqDB_AtlasRegionHolder(CSeqDBAtlas & atlas, const char * ptr)
    : m_Atlas(atlas), m_Ptr(ptr)
{
}

CSeqDB_AtlasRegionHolder::~CSeqDB_AtlasRegionHolder()
{
    if (m_Ptr) {
        CSeqDBLockHold locked(m_Atlas);
        m_Atlas.Lock(locked);

        //m_Atlas.RetRegion(m_Ptr);
        m_Ptr = NULL;
    }
}

END_NCBI_SCOPE
