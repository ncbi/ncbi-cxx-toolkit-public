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

/// @file seqdbblob.cpp
/// Defines BlastDb `Blob' class for SeqDB and WriteDB.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdbblob.hpp>
#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include "seqdbgeneral.hpp"

BEGIN_NCBI_SCOPE


CBlastDbBlob::CBlastDbBlob(int size)
    : m_Owner(true), m_ReadOffset(0), m_WriteOffset(0)
{
    if (size) {
        m_DataHere.reserve(size);
    }
}

CBlastDbBlob::CBlastDbBlob(CTempString data, bool copy)
    : m_Owner(copy), m_ReadOffset(0), m_WriteOffset(0)
{
    if (m_Owner) {
        m_DataHere.assign(data.data(), data.data() + data.size());
    } else {
        m_DataRef = data;
    }
}

void CBlastDbBlob::Clear()
{
    m_Owner = true;
    m_ReadOffset = 0;
    m_WriteOffset = 0;
    m_DataHere.resize(0);
    m_DataRef = CTempString("");
    m_Lifetime.Reset();
}

void CBlastDbBlob::ReferTo(CTempString data)
{
    m_Owner = false;
    m_DataRef = data;
    m_Lifetime.Reset();
}

void CBlastDbBlob::ReferTo(CTempString data, CRef<CObject> lifetime)
{
    m_Owner = false;
    m_DataRef = data;
    m_Lifetime = lifetime;
}

Int8 CBlastDbBlob::ReadVarInt()
{
    return x_ReadVarInt(& m_ReadOffset);
}

Int8 CBlastDbBlob::ReadVarInt(int offset) const
{
    return x_ReadVarInt(& offset);
}

Int8 CBlastDbBlob::x_ReadVarInt(int * offsetp) const
{
    CTempString all = Str();
    Int8 rv(0);
    
    for(size_t i = *offsetp; i < all.size(); i++) {
        int ch = all[i];
        
        if (ch & 0x80) {
            // middle
            rv = (rv << 7) | (ch & 0x7F);
        } else {
            // end
            rv = (rv << 6) | (ch & 0x3F);
            *offsetp = i+1;
            
            return (ch & 0x40) ? -rv : rv;
        }
    }
    
    NCBI_THROW(CSeqDBException,
               eFileErr,
               "CBlastDbBlob::ReadVarInt: eof while reading integer.");
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
int CBlastDbBlob::ReadInt1()
{
    return x_ReadIntFixed<int,1>(& m_ReadOffset);
}

int CBlastDbBlob::ReadInt1(int offset) const
{
    return x_ReadIntFixed<int, 1>(& offset);
}

int CBlastDbBlob::ReadInt2()
{
    return x_ReadIntFixed<int,2>(& m_ReadOffset);
}

int CBlastDbBlob::ReadInt2(int offset) const
{
    return x_ReadIntFixed<int, 2>(& offset);
}

Int4 CBlastDbBlob::ReadInt4()
{
    return x_ReadIntFixed<Int4,4>(& m_ReadOffset);
}

Int4 CBlastDbBlob::ReadInt4(int offset) const
{
    return x_ReadIntFixed<Int4, 4>(& offset);
}

Int8 CBlastDbBlob::ReadInt8()
{
    return x_ReadIntFixed<Int8, 8>(& m_ReadOffset);
}

Int8 CBlastDbBlob::ReadInt8(int offset) const
{
    return x_ReadIntFixed<Int8, 8>(& offset);
}

CTempString CBlastDbBlob::ReadString(EStringFormat fmt)
{
    return x_ReadString(fmt, & m_ReadOffset);
}

CTempString CBlastDbBlob::ReadString(EStringFormat fmt, int offset) const
{
    return x_ReadString(fmt, & offset);
}

CTempString CBlastDbBlob::x_ReadString(EStringFormat fmt, int * offsetp) const
{
    int sz = 0;
    
    if (fmt == eSize4) {
        sz = x_ReadIntFixed<int,4>(offsetp);
    } else if (fmt == eSizeVar) {
        sz = x_ReadVarInt(offsetp);
    }
    
    const char * datap = "";
    
    if (fmt == eNUL) {
        CTempString ts = Str();
        int zoffset = -1;
        
        for(size_t i = *offsetp; i < ts.size(); i++) {
            if (ts[i] == (char)0) {
                zoffset = i;
                break;
            }
        }
        
        if (zoffset == -1) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "CBlastDbBlob::ReadString: Unterminated string.");
        }
        
        datap = ts.data() + *offsetp;
        sz = zoffset - *offsetp;
        *offsetp = zoffset+1;
    } else {
        datap = x_ReadRaw(sz, offsetp);
    }
    
    return CTempString(datap, sz);
}
#endif

const char * CBlastDbBlob::x_ReadRaw(int size, int * offsetp) const
{
    _ASSERT(offsetp);
    _ASSERT(size >= 0);
    
    CTempString s = Str();
    
    int begin = *offsetp;
    int end = begin + size;
    
    if (begin > end || end > (int)s.size()) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "CBlastDbBlob::x_ReadRaw: hit end of data");
    }
    
    *offsetp = end;
    return s.data() + begin;
}


// Variable length format for an integer.  The most significant byte
// is first.  7 bits are encoded per byte, except for the last byte,
// where the sign is encoded using the 0x40 bit, with 6 bits of other
// data.  Termination is handled by the 0x80 bit, which is on for all
// bytes except the last (the least significant byte).  The data looks
// like Uint1 for values 0 to 63.
// 
// 23 -> 23
// 84 -> 82 04
// 55 -> 81 15
// -55 -> 81 55
// 01 01 -> 82 01

int CBlastDbBlob::WriteVarInt(Int8 x)
{
    return x_WriteVarInt(x, NULL);
}

int CBlastDbBlob::WriteVarInt(Int8 x, int offset)
{
    return x_WriteVarInt(x, & offset);
}

int CBlastDbBlob::x_WriteVarInt(Int8 x, int * offsetp)
{
    // The variable length integer is written into the end of the 16
    // byte array shown below.
    
    _ASSERT(((x >> 62) == -1) || ((x >> 62) == 0));
    
    char buf[16];
    int end_ptr = sizeof(buf);
    int ptr = end_ptr;
    
    Uint8 ux = (Uint8)((x >= 0) ? x : -x);
    
    buf[--ptr] = (ux & 0x3F);
    ux >>= 6;
    
    if (x < 0) {
        buf[ptr] |= 40;
    }
    
    while(ux) {
        ux >>= 7;
        buf[--ptr] = (ux & 0x7F) | 0x80;
    }
    
    int bytes = end_ptr - ptr;
    
    x_WriteRaw(buf + ptr, bytes, offsetp);
    
    return offsetp ? (bytes + *offsetp) : m_WriteOffset;
}

int CBlastDbBlob::VarIntSize(Int8 x)
{
    // Compute storage length of a variable-length integer.
    
    int bytes = 1;
    
    Uint8 ux = ((Uint8)((x >= 0) ? x : -x)) >> 6;
    
    while(ux) {
        ux >>= 7;
        bytes++;
    }
    
    return bytes;
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
void CBlastDbBlob::WriteInt1(int x)
{
    x_WriteIntFixed<int,1>(x, NULL);
}

void CBlastDbBlob::WriteInt1(int x, int offset)
{
    x_WriteIntFixed<int,1>(x, & offset);
}

void CBlastDbBlob::WriteInt2(int x)
{
    x_WriteIntFixed<int,2>(x, NULL);
}

void CBlastDbBlob::WriteInt2(int x, int offset)
{
    x_WriteIntFixed<int,2>(x, & offset);
}

void CBlastDbBlob::WriteInt4(Int4 x)
{
    x_WriteIntFixed<int,4>(x, NULL);
}

void CBlastDbBlob::WriteInt4(Int4 x, int offset)
{
    x_WriteIntFixed<int,4>(x, & offset);
}

void CBlastDbBlob::WriteInt8(Int8 x)
{
    x_WriteIntFixed<Int8, 8>(x, NULL);
}

void CBlastDbBlob::WriteInt8(Int8 x, int offset)
{
    x_WriteIntFixed<Int8, 8>(x, & offset);
}

int CBlastDbBlob::WriteString(CTempString str, EStringFormat fmt)
{
    return x_WriteString(str, fmt, NULL);
}

int CBlastDbBlob::WriteString(CTempString str, EStringFormat fmt, int offset)
{
    return x_WriteString(str, fmt, & offset);
}

int CBlastDbBlob::x_WriteString(CTempString str, EStringFormat fmt, int * offsetp)
{
    int start_off = offsetp ? *offsetp : m_WriteOffset;
    
    if (fmt == eSize4) {
        x_WriteIntFixed<int,4>(str.size(), offsetp);
    } else if (fmt == eSizeVar) {
        x_WriteVarInt(str.size(), offsetp);
    }
    
    x_WriteRaw(str.data(), str.size(), offsetp);
    
    if (fmt == eNUL) {
        char buf = 0;
        x_WriteRaw(& buf, 1, offsetp);
    }
    
    int end_off = offsetp ? *offsetp : m_WriteOffset;
    
    return end_off - start_off;
}
#endif

void CBlastDbBlob::WriteRaw(const char * begin, int size)
{
    x_WriteRaw(begin, size, NULL);
}

void CBlastDbBlob::WriteRaw(const char * begin, int size, int offset)
{
    x_WriteRaw(begin, size, & offset);
}

void CBlastDbBlob::x_WriteRaw(const char * data, int size, int * offsetp)
{
    int orig_size = size;
    
    if (offsetp == NULL) {
        offsetp = & m_WriteOffset;
    }
    
    int off = *offsetp;
    
    _ASSERT(data != NULL);
    _ASSERT(off  >= 0);
    _ASSERT(size >= 0);
    
    // x_Reserve guarantees m_Owner == true.
    x_Reserve(off + size);
    _ASSERT(m_Owner);
    
    int overlap = int(m_DataHere.size()) - off;
    
    // If inserting past end of buffer, increase the buffer size.
    
    if (overlap < 0) {
        m_DataHere.insert(m_DataHere.end(), -overlap, (char) 0);
        overlap = 0;
    }
    
    // If data is partly or wholly written into existing array space,
    // memcpy the data into that space.
    
    if (overlap > 0) {
        int len = std::min(overlap, size);
        
        memcpy(& m_DataHere[off], data, len);
        
        size -= len;
        data += len;
        off  += len;
    }
    
    if (size) {
        m_DataHere.insert(m_DataHere.end(), data, data + size);
    }
    
    *offsetp += orig_size;
}

void CBlastDbBlob::x_Copy(int total)
{
    _ASSERT(! m_Owner);
    _ASSERT(! m_DataHere.size());
    
    if (total < (int)m_DataRef.size()) {
        total = m_DataRef.size();
    }
    
    m_Owner = true;
    const char * ptr = m_DataRef.data();
    
    m_DataHere.reserve(total);
    m_DataHere.assign(ptr, ptr + m_DataRef.size());
    m_DataRef = CTempString("");
    
    m_Lifetime.Reset();
}

void CBlastDbBlob::x_Reserve(int need)
{
    if (! m_Owner) {
        x_Copy(need);
    } else {
        int cur_cap = m_DataHere.capacity();
        
        if (cur_cap < need) {
            // Skip the first few reallocations.
            
            int new_cap = 64;
            
            while(new_cap < need) {
                new_cap *= 2;
            }
            
            m_DataHere.reserve(new_cap);
        }
    }
}

int CBlastDbBlob::Size() const
{
    if (m_Owner) {
        return m_DataHere.size();
    }
    return m_DataRef.size();
}

CTempString CBlastDbBlob::Str() const
{
    if (m_Owner) {
        if (m_DataHere.size()) {
            const char * p = & m_DataHere[0];
            return CTempString(p, m_DataHere.size());
        }
    } else {
        if (m_DataRef.size()) {
            return m_DataRef;
        }
    }
    
    return CTempString("");
}

void CBlastDbBlob::SeekWrite(int offset)
{
    m_WriteOffset = offset;
}

void CBlastDbBlob::SeekRead(int offset)
{
    m_ReadOffset = offset;
}

int CBlastDbBlob::GetWriteOffset() const
{
    return m_WriteOffset;
}

int CBlastDbBlob::GetReadOffset() const
{
    return m_ReadOffset;
}

void CBlastDbBlob::WritePadBytes(int align, EPadding fmt)
{
    vector<char> pad;
    CTempString tmp;
    
    int pads = align ? (m_WriteOffset % align) : 0;
    
    if (fmt == eSimple) {
        pads = pads ? (align - pads) : 0;
    } else {
        pads = align - pads;
    }
    
    if (fmt == eSimple) {
        for(int i = 0; i < pads; i++) {
            x_WriteRaw("#", 1, NULL);
        }
    } else {
        for(int i = 1; i < pads; i++) {
            x_WriteRaw("#", 1, NULL);
        }
        char ch = (char)0;
        x_WriteRaw(& ch, 1, NULL);
    }
    
    _ASSERT(! (m_WriteOffset % align));
}

void CBlastDbBlob::SkipPadBytes(int align, EPadding fmt)
{
    if (fmt == eString) {
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
        ReadString(eNUL);
#endif
    } else {
        _ASSERT(fmt == eSimple);
        
        int pads = align ? (m_ReadOffset % align) : 0;
        pads = pads ? (align-pads) : 0;
        
        CTempString tmp(x_ReadRaw(pads, & m_ReadOffset), pads);
        
        for(int i = 0; i < (int)tmp.size(); i++) {
            SEQDB_FILE_ASSERT(tmp[i] == '#');
        }
    }
}

END_NCBI_SCOPE

