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

#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

/// Index file.
///
/// Index files (extension nin or pin) contain information on where to
/// find information in other files.  The OID is the (implied) key.


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
        }
        catch(...) {
        }
    }
        
    if (! m_Mapped) {
        try {
            // For now, no file creation
            m_Stream.clear();
            m_Stream.open(name.data());
                
            if (m_Stream) {
                m_Opened = true;
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
    
    // It might be good to keep a handle to a mempool and clear out
    // the parts of the mempool that relate to this file, by
    // range... if and only if the design was changed so that volumes
    // could expire, or be cleaned up in some way, before the final
    // destruction of CSeqDB.  There are several reasons this might be
    // done, which should be obvious to the reader (like the proof to
    // Fermat's last theorem.)
}

Int4 CSeqDBRawFile::x_GetOpenedLength(void)
{
    // Should this cache the length?
        
    CT_POS_TYPE p = m_Stream.tellg();
        
    m_Stream.seekg(0, ios::end);
    CT_POS_TYPE retval = m_Stream.tellg();
        
    m_Stream.seekg(p);
        
    return retval - CT_POS_TYPE(0);
}

const char * CSeqDBRawFile::GetRegion(Uint4 start, Uint4 end)
{
    const char * retval = 0;
        
    if (m_Mapped) {
        if (x_ValidGet(start, end, m_Mapped->GetSize())) {
            retval = ((const char *)m_Mapped->GetPtr()) + start;
        }
    } else if (m_Opened && x_ValidGet(start, end, x_GetOpenedLength())) {
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

Uint8 CSeqDBRawFile::GetFileLength(void)
{
    if (m_Mapped) {
        return (Uint8) m_Mapped->GetSize();
    } else if (m_Opened) {
        return (Uint8) x_GetOpenedLength();
    }
    
    return 0;
}

void CSeqDBRawFile::ReadSwapped(Uint4 * z)
{
    if (m_Mapped) {
        Uint4 offset = m_Offset;
        m_Offset += 4;
        *z = SeqDB_GetStdOrd( (const Uint4 *)(((char *)m_Mapped->GetPtr()) + offset) );
    } else if (m_Opened) {
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

bool CSeqDBRawFile::ReadBytes(char * z, Uint4 start, Uint4 end)
{
    // Read bytes from memory, no handling or adjustments.
    if (m_Mapped) {
        if (! x_ValidGet(start, end, m_Mapped->GetSize())) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Invalid file offset: possible file corruption.");
        }
        
        memcpy(z, ((char *) m_Mapped->GetPtr()) + start, end - start);
        
        return true;
    } else if (m_Opened) {
        if (! x_ValidGet(start, end, x_GetOpenedLength())) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Invalid file offset: possible file corruption.");
        }
        
        m_Stream.seekg(start, ios::beg);
        m_Stream.read((char *) z, end - start);
        
        return true;
    }
    
    return false;
}

bool CSeqDBRawFile::x_ReadFileRegion(char * region, Uint4 start, Uint4 end)
{
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
        m_Offset += end - start;
    }
        
    return retval;
}

bool CSeqDBExtFile::x_NeedFile(void)
{
    if (m_File.Valid())
        return true;
        
    // If "Unknown", try each type.
    char save_pnu = m_ProtNucl;
        
    if (save_pnu == kSeqTypeUnkn) {
        x_SetFileType(kSeqTypeProt);
    }
        
    if ((m_ProtNucl == kSeqTypeProt) && m_File.Open(m_FileName)) {
        return true;
    }
        
    if (save_pnu == kSeqTypeUnkn) {
        x_SetFileType(kSeqTypeNucl);
    }
        
    if ((m_ProtNucl == kSeqTypeNucl) && m_File.Open(m_FileName)) {
        return true;
    }
        
    // Not found; reset to original type.
    x_SetFileType(save_pnu);
        
    return false;
}

bool CSeqDBIdxFile::x_NeedIndexFile(void)
{
    if (m_Valid) {
        return true;
    }
        
    if (! x_NeedFile()) {
        return false;
    }
        
    Uint4 f_format_version; // L3064
    Uint4 f_db_seqtype;     // L3077
        
    x_ReadSwapped(& f_format_version);
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
        
    // Check validity
        
    if (f_format_version != 4)
        return false;
        
    char db_seqtype = ((f_db_seqtype == 1)
                       ? kSeqTypeProt
                       : kSeqTypeNucl);
        
    if (db_seqtype != x_GetSeqType()) {
        return false;
    }
        
    return (m_Valid = true);
}

END_NCBI_SCOPE


