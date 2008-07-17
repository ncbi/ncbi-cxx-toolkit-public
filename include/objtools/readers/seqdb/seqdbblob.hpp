#ifndef OBJTOOLS__READERS__SEQDB__SEQDBBLOB_HPP
#define OBJTOOLS__READERS__SEQDB__SEQDBBLOB_HPP

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

/// @file seqdbblob.hpp
/// Defines BlastDb `Blob' class for SeqDB and WriteDB.
/// 
/// Defines classes:
///     CBlastDbBlob
/// 
/// Implemented for: UNIX, MS-Windows

#include <ncbiconf.h>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE


/// `Blob' Class for SeqDB (and WriteDB).
///
/// This manages serialization and deserialization of binary data of
/// unspecified size and format, known in RDBMS jargon as `blob' data.
/// The primary goals here are to standardize the encoding of data
/// types and to optimize performance.  Read and write operations are
/// handled by the same class.  For both read and write operations,
/// random and stream access are supported.

class NCBI_XOBJREAD_EXPORT CBlastDbBlob : public CObject {
public:
    /// Create a new object, reserving 'size' bytes for writing.
    /// @param size This many bytes will be reserved for writing.
    CBlastDbBlob(int size = 0);
    
    /// Create a readable object containing the specified data.
    /// 
    /// If `copy' is specified as false, only a reference to the data
    /// will be kept, and write operations are illegal.  If `copy' is
    /// specified as true, the data is copied into an internal buffer,
    /// and both read and write operations are legal.
    /// 
    /// @param data The data to refer to.
    /// @param copy Specify true to copy the data to a buffer.
    CBlastDbBlob(CTempString data, bool copy);
    
    /// Get blob contents as a CTempString.
    CTempString Str() const;
    
    /// Get size of blob contents.
    int Size() const;
    
    /// Get the number of unread bytes after the read pointer.
    int BytesLeft() const;
    
    /// Clear all owned data and reference an empty string.
    void Clear();
    
    /// Refer to an existing memory area.
    /// 
    /// This method causes this blob to refer to an existing area of
    /// memory without copying it.  The caller should guarantee that
    /// the referenced data is valid until past the last read of the
    /// data.  If such a guarantee is not possible, then Clear() and
    /// WriteRaw() can be substituted (at the cost of an additional
    /// copy operation).  Alternately, the two-argument ReferTo()
    /// operation can be used to provides `lifetime' management.
    ///
    /// @param data Specifies the referenced memory region.
    void ReferTo(CTempString data);
    
    /// Refer to an existing memory area.
    /// 
    /// This method causes this blob to refer to an existing area of
    /// memory without copying it.  This version allows the caller to
    /// specify a CObject that maintains the lifetime of the memory
    /// region.  This object will keep a reference to the CObject as
    /// long as it references the specified memory region, after which
    /// the CObject should be released.  The specified CObject should
    /// be allocated on the heap.
    /// 
    /// @param data Specifies the referenced memory region.
    /// @param lifetime The lifetime management object.
    void ReferTo(CTempString data, CRef<CObject> lifetime);

    /// Read a variable length integer from the blob.
    /// @param x The integer to read.
    /// @return The number of bytes read.
    Int8 ReadVarInt();
    
    /// Read a variable length integer from the blob.
    /// @param x The integer to read.
    /// @param offset The offset to read the integer at.
    /// @return The number of bytes read.
    Int8 ReadVarInt(int offset) const;
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Read a 1 byte integer at the pointer (and move the pointer).
    int ReadInt1();
    
    /// Read a 1 byte integer from the given offset.
    /// @param off The offset from which to read the integer.
    /// @return The 1 byte value read from the data.
    int ReadInt1(int offset) const;
    
    /// Read a 2 byte integer at the pointer (and move the pointer).
    int ReadInt2();
    
    /// Read a 2 byte integer from the given offset.
    /// @param off The offset from which to read the integer.
    /// @return The 2 byte value read from the data.
    int ReadInt2(int offset) const;
    
    /// Read a 4 byte integer at the pointer (and move the pointer).
    Int4 ReadInt4();
    
    /// Read a 4 byte integer from the given offset.
    /// @param off The offset from which to read the integer.
    /// @return The four byte value read from the data.
    Int4 ReadInt4(int offset) const;
    
    /// Read an 8 byte integer at the pointer (and move the pointer).
    Int8 ReadInt8();
    
    /// Read an 8 byte integer from the given offset.
    /// @param off The offset from which to read the integer.
    /// @return The eight byte value read from the data.
    Int8 ReadInt8(int offset) const;
#endif
    
    /// Move the read pointer to a specific location.
    /// @param offset The new read offset.
    void SeekRead(int offset);
    
    
    /// Write a variable length integer to the blob.
    /// @param x The integer to write.
    /// @return The number of bytes written.
    int WriteVarInt(Int8 x);
    
    /// Write a variable length integer to the blob.
    /// @param x The integer to write.
    /// @param offset The offset to write the integer at.
    /// @return The number of bytes written.
    int WriteVarInt(Int8 x, int offset);
    
    /// Compute bytes used for a variable length integer.
    /// @param x The integer value.
    /// @return The number of bytes that would be written.
    static int VarIntSize(Int8 x);
    
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Write a 1 byte integer to the blob.
    /// @param x The integer to write.
    void WriteInt1(int x);
    
    /// Write a 1 byte integer to the blob.
    /// @param x The integer to write.
    /// @param offset The offset to write the integer at.
    void WriteInt1(int x, int offset);
    
    
    /// Write a 1 byte integer to the blob.
    /// @param x The integer to write.
    void WriteInt2(int x);
    
    /// Write a 1 byte integer to the blob.
    /// @param x The integer to write.
    /// @param offset The offset to write the integer at.
    void WriteInt2(int x, int offset);
    
    
    /// Write a 4 byte integer to the blob.
    /// @param x The integer to write.
    void WriteInt4(Int4 x);
    
    /// Write a 4 byte integer into the blob at a given offset.
    /// @param x The integer to write.
    /// @param offset The offset to write the integer at.
    void WriteInt4(Int4 x, int offset);
    
    
    /// Write an 8 byte integer to the blob.
    /// @param x The integer to write.
    void WriteInt8(Int8 x);
    
    /// Write an 8 byte integer into the blob at a given offset.
    /// @param x The integer to write.
    /// @param offset The offset to write the integer at.
    void WriteInt8(Int8 x, int offset);
#endif
    
    /// Seek write pointer to a specific location.
    /// @param offset The new write offset.
    void SeekWrite(int offset);
    
    
    /// String termination style.
    enum EStringFormat {
        eNone,    ///< Write the string as-is.
        eNUL,     ///< Write a NUL terminated string.
        eSize4,   ///< Write string length as Int4, then string data.
        eSizeVar  ///< Write string length as VarInt, then string data.
    };
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Read string data from the blob (moving the read pointer).
    /// @param str The string to read.
    /// @param fmt String termination criteria.
    /// @return The number of bytes read.
    CTempString ReadString(EStringFormat fmt);
    
    /// Read string data from the blob at a given offset.
    /// @param str The string to read.
    /// @param fmt String termination criteria.
    /// @param offset The offset to read from.
    /// @return The number of bytes read.
    CTempString ReadString(EStringFormat fmt, int offset) const;
    
    /// Write string data to the blob.
    /// @param str The string to write.
    /// @param fmt String termination criteria.
    /// @return The number of bytes written.
    int WriteString(CTempString str, EStringFormat fmt);
    
    /// Write string data to the blob at a specific offset.
    /// @param str The string to write.
    /// @param fmt String termination criteria.
    /// @param offset The offset to write at.
    /// @return The number of bytes written.
    int WriteString(CTempString str, EStringFormat fmt, int offset);
#endif
    
    /// Padding style.
    enum EPadding {
        eSimple, ///< Just write NUL bytes until aligned.
        eString  ///< Pad using NUL terminated string of '#' bytes.
    };
    
    /// Align the offset by writing pad bytes.
    ///
    /// One of two padding methods is used.  If eSimple is specified,
    /// zero or more NUL bytes are written.  This uses less overhead
    /// but readers of the blob need to know the alignment to read
    /// fields found after the padding.  If eString is specified, a
    /// normal string write is used with eNUL termination and a string
    /// that will result in the requested alignment.  This is self
    /// describing but requires at least one byte.
    ///
    /// @param align Pad to a multiple of this size.
    /// @param fmt String termination criteria.
    void WritePadBytes(int align, EPadding fmt);
    
    /// Align the offset by skipping bytes.
    ///
    /// This works just like WritePadBytes, but verifies that the pad
    /// bytes exist and have the correct values, and skips over them,
    /// rather than writing them.  If fmt is eString, the alignment
    /// value is ignored.
    ///
    /// @param align Pad to a multiple of this size.
    /// @param fmt String termination criteria.
    void SkipPadBytes(int align, EPadding fmt);
    
    /// Write raw data to the blob (moving the write pointer).
    /// @param begin Pointer to the start of the data.
    /// @param size Number of bytes to copy.
    void WriteRaw(const char * begin, int size);
    
    /// Write raw data to the blob at the given offset.
    /// @param begin Pointer to the start of the data.
    /// @param size Number of bytes to copy.
    /// @param offset Location to write data at.
    void WriteRaw(const char * begin, int size, int offset);
    
    /// Get the current write pointer offset.
    /// @return The offset at which the next write would occur.
    int GetWriteOffset() const;
    
    /// Get the current read pointer offset.
    /// @return The offset at which the next read would occur.
    int GetReadOffset() const;
    
private:
    /// Copy referenced data to owned data.
    /// 
    /// This handles the Copy part of Copy On Write.  To reduce the
    /// allocation count, the `total' parameter can be used to request
    /// the total number of bytes needed.  If `total' is less than the
    /// current size, the current size will be used instead.
    /// 
    /// @param total Total space needed.
    void x_Copy(int total);
    
    /// Write raw bytes as a CTempString.
    /// @param data String data to write.
    void x_Reserve(int size);
    
    /// Write raw bytes as ptr + size at a given offset.
    /// @param data String data to write.
    /// @param size Number of bytes to write.
    /// @param offsetp Offset to write at (NULL means use write pointer).
    void x_WriteRaw(const char * ptr, int size, int * offsetp);
    
    /// Read raw bytes from a given offset.
    ///
    /// This method checks that enough bytes exist, updates the read
    /// pointer, and returns a pointer to the given data.  Unlike with
    /// x_WriteRaw, do not use NULL for the read pointer, instead the
    /// internal read pointer should be provided if the user did not
    /// provide one.  This method will throw an exception if there is
    /// not enough data.
    ///
    /// @param size Number of bytes needed by caller.
    /// @param offsetp Offset from which to read (should not be NULL).
    /// @return Pointer to beginning of requested data.
    const char * x_ReadRaw(int size, int * offsetp) const;
    
    /// Write a variable length integer into the buffer.
    /// @param x The integer to write.
    /// @param offsetp The offset to write at (or NULL).
    /// @return The number of bytes written.
    int x_WriteVarInt(Int8 x, int * offsetp);
    
    /// Read a variable length integer from the buffer.
    /// @param offsetp The offset to read at (should not be NULL).
    /// @return The integer value.
    Int8 x_ReadVarInt(int * offsetp) const;
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Write string data to the blob.
    /// @param str The string to write.
    /// @param fmt String termination criteria.
    /// @param offset The offset to write at (NULL to use write pointer.)
    /// @return The number of bytes written.
    int x_WriteString(CTempString str, EStringFormat fmt, int * offsetp);
    
    /// Read string data from the blob.
    /// @param fmt String termination criteria.
    /// @param offset The offset to read at (should not be NULL).
    /// @return The string that was read.
    CTempString x_ReadString(EStringFormat fmt, int * offsetp) const;
    
    /// Read a fixed length integer.
    /// @param offsetp The offset to read at.
    /// @return The value that was read.
    template<typename TValue, int TBytes>
    TValue x_ReadIntFixed(int * offsetp) const
    {
        // Check that the value fits in the specified range.
        unsigned char * buf = (unsigned char*) x_ReadRaw(TBytes, offsetp);
        unsigned char * vbuf = buf - 8 + TBytes;
        
        TValue x = vbuf[7];
        
        switch(TBytes) {
        case 8: x |= Uint8(vbuf[0]) << 56;
        case 7: x |= Uint8(vbuf[1]) << 48;
        case 6: x |= Uint8(vbuf[2]) << 40;
        case 5: x |= Uint8(vbuf[3]) << 32;
        case 4: x |= Uint4(vbuf[4]) << 24;
        case 3: x |= Uint4(vbuf[5]) << 16;
        case 2: x |= Uint4(vbuf[6]) << 8;
        case 1:
            break;
        default:
            _ASSERT(0);
        }
        
        if (TBytes < sizeof(TValue)) {
            // This only applies to 'shortened' formats, such as an
            // Int8 packed in 5 bytes or an Int4 packed in 3 bytes.
            // It only affects signed values, its purposes is to fix
            // the numeric sign.  The current design does not use
            // shortened formats anywhere.
            
            int bits = (sizeof(TValue)-TBytes) * 8;
            x = (x << bits) >> bits;
        }
        
        return x;
    }
    
    /// Write a fixed length integer.
    /// @param x The value to write.
    /// @param offsetp The offset at which to write.
    template<typename TValue, int TBytes>
    void x_WriteIntFixed(TValue x, int * offsetp)
    {
        // Check that the value fits in the specified range.
        _ASSERT(((Int8(x) >> (TBytes*8-1)) >> 1) ==
                ((Int8(x) >> (TBytes*8-1)) >> 2));
        
        unsigned char buf[8];
        
        switch(TBytes) {
        case 8: buf[0] = Uint8(x) >> 56;
        case 7: buf[1] = Uint8(x) >> 48;
        case 6: buf[2] = Uint8(x) >> 40;
        case 5: buf[3] = Uint8(x) >> 32;
        case 4: buf[4] = Uint4(x) >> 24;
        case 3: buf[5] = Uint4(x) >> 16;
        case 2: buf[6] = Uint4(x) >> 8;
        case 1: buf[7] = Uint4(x);
            break;
        default:
            _ASSERT(0);
        }
        
        x_WriteRaw((char*)(buf + 8 - TBytes), TBytes, offsetp);
    }
#endif
    
    
    // Data
    
    /// True if this object owns the target data.
    bool m_Owner;
    
    /// The `read pointer' for stream-like access.
    int m_ReadOffset;
    
    /// The `write pointer' for stream-like access.
    int m_WriteOffset;
    
    /// Data owned by this object.
    vector<char> m_DataHere;
    
    /// Non-owned data (only used for `read' streams).
    CTempString m_DataRef;
    
    /// Lifetime maintenance object for referenced data.
    CRef<CObject> m_Lifetime;
};


END_NCBI_SCOPE

#endif // OBJTOOLS__READERS__SEQDB__SEQDBBLOB_HPP

