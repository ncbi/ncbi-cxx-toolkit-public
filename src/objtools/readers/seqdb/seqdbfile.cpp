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

#include <ncbi_pch.hpp>
#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

/// Index file.
///
/// Index files (extension nin or pin) contain information on where to
/// find information in other files.  The OID is the (implied) key.


// A Word About Mutexes and Mutability in the File Classes
//
// The stream object in CSeqDBRawFile is mutable: this is because the
// stream access methods modify the file.  Specifically, they modify
// the file offset.  This means that two users of a stream object will
// step on each other if they try to read from different offsets
// concurrently.  Memory mapping does not have this problem of course.
//
// To fix this, the file object is mutable, but to access it, the user
// needs to hold the m_FileLock mutex.
//
// One goal I have for these classes is to eliminate all locking for
// the mmap case.  Locking is not needed to call a const method, so
// methods are marked const whenever possible.  After construction of
// CSeqDB, ONLY const methods are called.
//
// Some of the const methods need to modify fields; to do this, I mark
// the fields 'mutable' and hold a mutex whenever accessing them.
//
// Each method falls into one of these categories:
//
// 1. Non-const: called only during CSeqDB construction.
// 2. Const: no changes to any fields.
// 3. Const: modifies mutable fields while holding m_FileLock.

Uint8 BytesToUint8(char * bytes_sc)
{
    unsigned char * bytes = (unsigned char *) bytes_sc;
    Uint8 value;
    Int4 i;

    value = 0;
    for(i = 7; i >= 0; i--) {
        value += bytes[i];
        if(i) value <<= 8;
    }
    
    return value;
}

bool CSeqDBRawFile::Open(const string & name)
{
    Clear();
    
    if (m_UseMMap) {
        try {
            m_Mapped = new CMemoryFile(name);
            x_SetLength(false);
        }
        catch(...) {
        }
    }
    
    if (! m_Mapped) {
        try {
            // For now, no file creation
            CFastMutexGuard guard(m_FileLock);
            
            m_Stream.clear();
            m_Stream.open(name.data());
            
            if (m_Stream) {
                m_Opened = true;
                x_SetLength(true);
            }
        }
        catch(...) {
        }
    }
    
    return Valid();
}

void CSeqDBRawFile::Clear(void)
{
    if (m_Mapped) {
        delete m_Mapped;
        m_Mapped = 0;
    }
    
    // It might be good to clear out the parts of the mempool that
    // relate to this file, by range... but only if the design was
    // changed so that volumes could expire, or be cleaned up in some
    // way, before the destruction of CSeqDB.
}

const char * CSeqDBRawFile::GetRegion(Uint4 start, Uint4 end) const
{
    const char * retval = 0;
        
    if (m_Mapped) {
        if (x_ValidGet(start, end, (Uint4)m_Mapped->GetSize())) {
            retval = ((const char *)m_Mapped->GetPtr()) + start;
        }
    } else if (m_Opened && x_ValidGet(start, end, (Uint4) m_Length)) {
        //  Note that a more 'realistic' approach would involve a
        //  cache of blocks or sections that have been brought in;
        //  and would either free these on a refcount basis, or
        //  return pointers to the insides of existing blocks, or
        //  both.  In an advanced design, you might expand all
        //  requests to block boundaries or powers of two, and
        //  cache lists of existing blocks at different sizes.
        //
        //  Many of these would be an improvement, but if the mmap
        //  fails, it may be impossible to do a good job with the
        //  "open file" size of the equation, because there may
        //  simply not be enough memory to do everything we want,
        //  regardless.
        
        char * region = (char*) m_MemPool.Alloc(end-start);
        
        if (region) {
            if (! x_ReadFileRegion(region, start, end)) {
                m_MemPool.Free((void*)region);
                region = 0;
            } else {
                retval = region;
            }
        }
    }
    
    return retval;
}

void CSeqDBRawFile::x_SetLength(bool have_lock)
{
    if (m_Mapped) {
        m_Length = m_Mapped->GetSize();
    } else if (m_Opened) {
        CFastMutexGuard guard;
        
        if (! have_lock) {
            guard.Guard(m_FileLock);
        }
        
        CT_POS_TYPE p = m_Stream.tellg();
        
        m_Stream.seekg(0, ios::end);
        CT_POS_TYPE retval = m_Stream.tellg();
        
        m_Stream.seekg(p);
        
        m_Length = retval - CT_POS_TYPE(0);
    }
}

void CSeqDBRawFile::ReadSwapped(Uint4 * z)
{
    if (m_Mapped) {
        Uint4 offset = m_Offset;
        m_Offset += 4;
        *z = SeqDB_GetStdOrd( (const Uint4 *)(((char *)m_Mapped->GetPtr()) + offset) );
    } else if (m_Opened) {
        CFastMutexGuard guard(m_FileLock);
        
        char buf[4];
        Uint4 offset = m_Offset;
        m_Stream.seekg(offset, ios::beg);
        m_Stream.read(buf, 4);
        // Should throw if .bad()?
            
        m_Offset += 4;
        *z = SeqDB_GetStdOrd( (const Uint4 *)(buf) );
    } else {
        NCBI_THROW(CSeqDBException, eFileErr, "Could not open [raw] file.");
    }
}

void CSeqDBRawFile::ReadSwapped(Uint8 * z)
{
    if (m_Mapped) {
        Uint4 offset = m_Offset;
        m_Offset += 8;
	*z = SeqDB_GetBroken((Int8 *) (((char *)m_Mapped->GetPtr()) + offset));
    } else if (m_Opened) {
        CFastMutexGuard guard(m_FileLock);
        
        char buf[8];
        Uint4 offset = m_Offset;
        m_Stream.seekg(offset, ios::beg);
        m_Stream.read(buf, 8);
        // Should throw if .bad()?
            
        m_Offset += 8;
	*z = SeqDB_GetBroken((Int8 *) buf);
    } else {
	NCBI_THROW(CSeqDBException, eFileErr, "Could not open [raw] file.");
    }
}

void CSeqDBRawFile::ReadSwapped(string * z)
{
    // This reads a string from byte data, assuming that the
    // string is represented as the four bytes length followed by
    // the contents.
    
    if (m_Mapped) {
        Uint4 offset = m_Offset;
        m_Offset += sizeof(offset);
	
	Uint4 string_size = SeqDB_GetStdOrd((Uint4 *)(((char *)m_Mapped->GetPtr()) + offset));
        const char * str = ((const char *)m_Mapped->GetPtr()) + m_Offset;
	
        m_Offset += string_size;
	
        z->assign(str, str + string_size);
    } else if (m_Opened) {
        CFastMutexGuard guard(m_FileLock);
        
        char sl_buf[4];
        m_Stream.seekg(m_Offset, ios::beg);
        m_Stream.read(sl_buf, 4);
        Uint4 string_size = SeqDB_GetStdOrd((Uint4 *) sl_buf);
	
        char * strbuf = new char[string_size+1];
        strbuf[string_size] = 0;
        m_Stream.read(strbuf, string_size);
            
        // Should throw something if read fails? i.e. if .gcount()!=string_size
            
        z->assign(strbuf, strbuf + string_size);
        m_Offset += string_size + 4;
            
        delete [] strbuf;
    } else { 
        NCBI_THROW(CSeqDBException, eFileErr, "Could not open [raw] file.");
    }
}

// Does not modify (or use) internal file offset

bool CSeqDBRawFile::ReadBytes(char * z, Uint4 start, Uint4 end) const
{
    // Read bytes from memory, no handling or adjustments.
    if (m_Mapped) {
        if (! x_ValidGet(start, end, (Uint4) m_Mapped->GetSize())) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Invalid file offset: possible file corruption.");
        }
        
        memcpy(z, ((char *) m_Mapped->GetPtr()) + start, end - start);
        
        return true;
    } else if (m_Opened) {
        if (! x_ValidGet(start, end, (Uint4) m_Length)) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Invalid file offset: possible file corruption.");
        }
        
        CFastMutexGuard guard(m_FileLock);
        
        m_Stream.seekg(start, ios::beg);
        m_Stream.read((char *) z, end - start);
        
        return true;
    }
    
    return false;
}

bool CSeqDBRawFile::x_ReadFileRegion(char * region, Uint4 start, Uint4 end) const
{
    CFastMutexGuard guard(m_FileLock);
    
    bool retval = false;
    _ASSERT(m_Opened);
    
    m_Stream.seekg(start, ios::beg);
    
    Int4 size_left = end - start;
    
    while ((size_left > 0) && m_Stream) {
        m_Stream.read(region, size_left);
        Int4 gcnt = m_Stream.gcount();
        
        if (gcnt <= 0) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Failed file read: possible file corruption.");
        }
        
        if (gcnt > size_left) {
            break;
        } else if (gcnt <= size_left) {
            size_left -= gcnt;
            region    += gcnt;
        }
    }
    
    if (size_left == 0) {
        retval = true;
        //m_Offset += end - start;
    }
        
    return retval;
}

CSeqDBExtFile::CSeqDBExtFile(CSeqDBMemPool & mempool,
                             const string  & dbfilename,
                             char            prot_nucl,
                             bool            use_mmap)
    : m_FileName(dbfilename),
      m_File    (mempool, use_mmap)
{
    if ((prot_nucl != kSeqTypeProt) && (prot_nucl != kSeqTypeNucl)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: Invalid sequence type requested.");
    }
    
    x_SetFileType(prot_nucl);
    
    if (! m_File.Open(m_FileName)) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: File could not be found.");
    }
}

CSeqDBIdxFile::CSeqDBIdxFile(CSeqDBMemPool & mempool,
                             const string  & dbname,
                             char            prot_nucl,
                             bool            use_mmap)
    : CSeqDBExtFile(mempool, dbname + ".-in", prot_nucl, use_mmap),
      m_NumSeqs       (0),
      m_TotLen        (0),
      m_MaxLen        (0),
      m_HdrHandle     (0),
      m_SeqHandle     (0),
      m_AmbCharHandle (0)
{
    // Input validation
    
    _ASSERT(! dbname.empty());
    
    if ((prot_nucl != kSeqTypeProt) && (prot_nucl != kSeqTypeNucl)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: Invalid sequence type requested.");
    }
    
    Uint4 f_format_version = 0; // L3064
    Uint4 f_db_seqtype = 0;     // L3077
    
    x_ReadSwapped(& f_format_version);
    
    if (f_format_version != 4) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: Not a valid version 4 database.");
    }
    
    x_ReadSwapped(& f_db_seqtype);
    x_ReadSwapped(& m_Title);
    x_ReadSwapped(& m_Date);
    x_ReadSwapped(& m_NumSeqs);
    x_ReadSwapped(& m_TotLen);
    x_ReadSwapped(& m_MaxLen);
        
    Uint4 file_offset = x_GetFileOffset();
        
    Uint4 region_bytes = 4 * (m_NumSeqs + 1);
        
    Uint4 off1, off2, off3, offend;
        
    off1   = file_offset;
    off2   = off1 + region_bytes;
    off3   = off2 + region_bytes;
    offend = off3 + region_bytes;
        
    m_HdrHandle     = x_GetRegion(off1, off2);
    m_SeqHandle     = x_GetRegion(off2, off3);
    m_AmbCharHandle = x_GetRegion(off3, offend);
    
    x_SetFileOffset(offend);
    
    char db_seqtype = ((f_db_seqtype == 1)
                       ? kSeqTypeProt
                       : kSeqTypeNucl);
    
    if (db_seqtype != x_GetSeqType()) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: requested sequence type does not match DB.");
    }
}

END_NCBI_SCOPE


