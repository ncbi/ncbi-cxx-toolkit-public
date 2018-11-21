#ifndef CHECKSUM__HPP
#define CHECKSUM__HPP

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
 * Author:  Eugene Vasilchenko, Vladimir Ivanov
 *
 */

/// @file checksum.hpp
/// Checksum and hash calculation classes.

#include <corelib/ncbistd.hpp>
#include <corelib/reader_writer.hpp>
#include <util/md5.hpp>


/** @addtogroup Checksum
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CChecksumBase -- Base class with auxiliary methods for CHash and CChecksum.
/// 
class NCBI_XUTIL_EXPORT CChecksumBase
{
protected:
    /// All supported methods for CHash and CCheksum.
    enum EMethodDef {
        eNone,        ///< Undefined (for internal use).
        eCRC32,       ///< 32-bit Cyclic Redundancy Check.
                      ///< Most significant bit first, no inversions.
        eCRC32ZIP,    ///< Exact zip CRC32. Same polynomial as eCRC32.
                      ///< Least significant bit are processed first,
                      ///< extra inversions at the beginning and the end.
        eCRC32INSD,   ///< Inverted CRC32ZIP. Hash function used in
                      ///< the ID system to verify sequence uniqueness.
        eCRC32CKSUM,  ///< CRC32 implemented by cksum utility.
                      ///< Same as eCRC32, but with data length included
                      ///< in CRC, and extra inversion at the end.
        eCRC32C,      ///< CRC32C (Castagnoli). Better polynomial used in some
                      ///< places (iSCSI). This method has hardware support in new
                      ///< Intel processors. Least significant bits are processed first,
                      ///< extra inversions at the beginning and the end.
        eAdler32,     ///< A bit faster than CRC32ZIP, not recommended for small data sizes.
        eMD5,         ///< Message Digest version 5.

        /// @attention
        /// You should not use CityHash for persistent storage.  
        /// CityHash does not maintain backward compatibility with previous versions.
        eCityHash32,  ///< CityHash, 32-bit result.
        eCityHash64,  ///< CityHash, 64-bit result.

        /// @attention
        /// You should not use FarmHash for persistent storage.  
        /// Both FarmHash 32/64-bit methods may change from time to time,
        /// may differ on different platforms or compiler flags.
        eFarmHash32,  ///< FarmHash, 32-bit result.
        eFarmHash64,  ///< FarmHash, 64-bit result.

        /// @attention
        /// You should not use MurmurHash for persistent storage.  
        /// It may produce different results depends on platforms, depending on 
        /// little-endian or big-endian architectures, 32- or 64-bit, or compiler
        /// options related to alignment.
        eMurmurHash2_32,  ///< MurmurHash2 for x86, 32-bit result.
        eMurmurHash2_64,  ///< MurmurHash2 for x64, 64-bit result.
        eMurmurHash3_32   ///< MurmurHash3 for x86, 32-bit result.
    };

public:
    /// Default constructor.
    CChecksumBase(EMethodDef method);
    /// Destructor.
    ~CChecksumBase();
    /// Copy constructor.
    CChecksumBase(const CChecksumBase& other);
    /// Assignment operator.
    CChecksumBase& operator=(const CChecksumBase& other);

    /// Return size of checksum/hash in bytes, depending on used method.
    /// @sa GetMethod, GetBits 
    size_t GetSize(void) const;

    /// Return size of checksum/hash in bits (32, 64).
    /// @sa GetSize
    size_t GetBits(void) const;

    /// Return calculated result.
    /// @attention Only valid in 32-bit modes, like: CRC32*, Adler32, CityHash32, FarmHash32.
    Uint4 GetResult32(void) const;
    
    /// Return calculated result.
    /// @attention Only valid in 64-bit modes, like: CityHash64, FarmHash64.
    Uint8 GetResult64(void) const;

    /// Return string with checksum/hash in hexadecimal form.
    string GetResultHex(void) const;

public:
    /// Initialize static tables used in CRC32 calculation.
    /// There is no need to call this method since it's done internally when necessary.
    static void InitTables(void);

    /// Print C++ code for CRC32 tables for direct inclusion into library.
    /// Such inclusion eliminates the need to initialize CRC32 tables.
    static void PrintTables(CNcbiOstream& out);

    ///
    EMethodDef x_GetMethod(void) const {
        return m_Method;
    };

protected:
    EMethodDef m_Method;      ///< Current method
    size_t     m_CharCount;   ///< Number of processed chars

    /// Unique seed used by some hash methods. Usually this value 
    /// sets once per process if needed. 0 by default, if not specified.
    /// @sa CHash::SetSeed()
    static Uint8 m_Seed;

    /// Checksum/Hash computation result
    union {
        Uint4 v32;   ///< Used to store 32-bit results
        Uint8 v64;   ///< Used to store 64-bit results
        CMD5* md5;   ///< Used for MD5 calculation
    } m_Value;

    /// Update current control sum with data provided.
    void x_Update(const char* str, size_t len);
    /// Reset the object to prepare it to the next computation using selected method.
    void x_Reset(EMethodDef method);
    /// Cleanup (used in destructor and assignment operator).
    void x_Free(void);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CHash -- Hash calculator
///
/// This class is used to compute hash.
///
/// @note
///   This class may use static tables (for CRC32 based methods only).
///   This presents a potential race condition in MT programs. To avoid races
///   call static InitTables() method before first concurrent use of CHash
///   if use it with CRC32-based methods.
///
/// @attention
///   You should not use some CHash methods for persistent storage, 
///   like CityHash, FarmHash or Murmur. Many hash methods designed
///   to be used inside current process only, here and now.
///   Please use CChecksum class if you need to store checksums for later usage.
///
/// @attention
///   Some hash methods have 32/64 bit variants. Usually 32-bit version works
///   faster on 32-bit machines and vice versa.
///
/// @sa
///   CChecksumBase::EMethods, NHash, CChecksum
///
class NCBI_XUTIL_EXPORT CHash : public CChecksumBase
{
public:
    /// Method used to compute hash.
    /// @sa CChecksumBase::EMethods
    enum EMethod {
        eCRC32          = CChecksumBase::eCRC32C,
        eCRC32ZIP       = CChecksumBase::eCRC32ZIP,
        eCRC32INSD      = CChecksumBase::eCRC32INSD,
        eCRC32CKSUM     = CChecksumBase::eCRC32CKSUM,
        eCRC32C         = CChecksumBase::eCRC32C,
        eAdler32        = CChecksumBase::eAdler32,
        eCityHash32     = CChecksumBase::eCityHash32,
        eCityHash64     = CChecksumBase::eCityHash64,
        eFarmHash32     = CChecksumBase::eFarmHash32,
        eFarmHash64     = CChecksumBase::eFarmHash64,
        eMurmurHash2_32 = CChecksumBase::eMurmurHash2_32,
        eMurmurHash2_64 = CChecksumBase::eMurmurHash2_64,
        eMurmurHash3_32 = CChecksumBase::eMurmurHash3_32,
        eDefault        = eCityHash64
    };

    /// Default constructor.
    CHash(EMethod method = eDefault);
    /// Copy constructor.
    CHash(const CHash& other);
    /// Assignment operator.
    CHash& operator=(const CHash& other);

    /// Get current method used to compute hash.
    EMethod GetMethod(void) const;

    /// Unique seed used by some hash methods. Usually this value 
    /// sets once per process if needed. 0 by default, if not specified.
    static void SetSeed(Uint8 seed);

    /// Calculate hash.
    /// @sa GetResult32(), GetResult66()
    void Calculate(const CTempString str);
    void Calculate(const char* str, size_t len);

    /// Static methods for simplified one line calculations.
    static void Calculate(const CTempString str, EMethod method, Uint4& hash);
    static void Calculate(const CTempString str, EMethod method, Uint8& hash);
    static void Calculate(const char* str, size_t len, EMethod method, Uint4& hash);
    static void Calculate(const char* str, size_t len, EMethod method, Uint8& hash);

    /// Reset the object to prepare it to the next computation using another method.
    void Reset(EMethod method);
};



/////////////////////////////////////////////////////////////////////////////
///
/// NHash -- 
///
/// Wrappers for portable versions of 3rd party hash functions.
///
/// @note
///   For more generic code see CHash, almost the same performance,
///   but similar interface for all hash methods.
/// @sa
///   CHash, CChecksum

class NCBI_XNCBI_EXPORT NHash
{
public:
    /// CityHash
    static Uint4 CityHash32(const CTempString str);
    static Uint4 CityHash32(const char* str, size_t len);
    static Uint8 CityHash64(const CTempString str);
    static Uint8 CityHash64(const char* str, size_t len);

    /// FarmHash
    static Uint4 FarmHash32(const CTempString str);
    static Uint4 FarmHash32(const char* str, size_t len);
    static Uint8 FarmHash64(const CTempString str);
    static Uint8 FarmHash64(const char* str, size_t len);

    /// MurmurHash2
    /// @note The length of string is limited with int type.
    static Uint4 MurmurHash2(const CTempString str, Uint4 seed = 0);
    static Uint4 MurmurHash2(const char* str, size_t len, Uint4 seed = 0);
    static Uint8 MurmurHash64A(const CTempString str, Uint8 seed = 0);
    static Uint8 MurmurHash64A(const char* str, size_t len, Uint8 seed = 0);

    /// MurmurHash3 (32-bit version only)
    /// @note The length of string is limited with int type.
    /// MurmurHash2 for x64, 64-bit result.
    static Uint4 MurmurHash3_x86_32(const CTempString str, Uint4 seed = 0);
    static Uint4 MurmurHash3_x86_32(const char* str, size_t len, Uint4 seed = 0);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CChecksum -- Checksum calculator
///
/// This class is used to compute control sums.
///
/// @note
///   This class may use static tables (for CRC32 based methods only).
///   This presents a potential race condition in MT programs. To avoid races
///   call static InitTables() method before first concurrent use of CChecksum.
///
class NCBI_XUTIL_EXPORT CChecksum : public CChecksumBase
{
public:
    /// Method used to compute control sum.
    /// @sa CChecksumBase::EMethods
    enum EMethod {
        eCRC32       = CChecksumBase::eCRC32,
        eCRC32ZIP    = CChecksumBase::eCRC32ZIP,
        eCRC32INSD   = CChecksumBase::eCRC32INSD,
        eCRC32CKSUM  = CChecksumBase::eCRC32CKSUM,
        eCRC32C      = CChecksumBase::eCRC32C,
        eAdler32     = CChecksumBase::eAdler32,
        eMD5         = CChecksumBase::eMD5,
        eDefault     = eCRC32
    };
    enum {
        kMinimumChecksumLength = 20
    };

    /// Default constructor.
    CChecksum(EMethod method = eDefault);
    /// Copy constructor.
    CChecksum(const CChecksum& other);
    /// Assignment operator.
    CChecksum& operator=(const CChecksum& other);

    /// Get current method used to compute checksum.
    EMethod GetMethod(void) const;

    /// Return size of checksum in bytes.
    /// Use this name for backward compatibility.
    /// @sa GetSize()
    size_t GetChecksumSize(void) const {
        return GetSize();
    };

    /// Return calculated checksum.
    /// @attention
    ///   Valid for all methods except MD5, please use GetMD5Digest() instead.
    /// @sa GetMD5Digest
    Uint4 GetChecksum(void) const {
        return GetResult32();
    };

    /// Return calculated MD5 digest.
    /// @attention  Only valid in MD5 mode!
    void GetMD5Digest(unsigned char digest[16]) const;
    void GetMD5Digest(string& str) const;

    /// Return string with checksum in hexadecimal form.
    /// Use this name for backward compatibility.
    /// @sa GetHex()
    string GetHexSum(void) const {
        return GetResultHex();
    };

    /// Update current control sum with data provided.
    void AddChars(const char* str, size_t len);
    void AddLine(const char* line, size_t len);
    void AddLine(const string& line);
    void NextLine(void);

    /// Update checksum with the file data.
    /// On error an exception will be thrown, and the checksum not change.
    void AddFile(const string& file_path);

    /// Update checksum with the stream data.
    /// On error an exception will be thrown, and the checksum not change.
    /// @param is
    ///   Input stream to read data from.
    ///   Please use ios_base::binary flag for the input stream
    ///   if you want to count all symbols there, including end of lines.
    void AddStream(CNcbiIstream& is);

    /// Reset the object to prepare it to the next checksum computation with the same method.
    void Reset(void);
    /// Reset the object to prepare it to the next checksum computation with different method.
    void Reset(EMethod method);

    /// Check for checksum line.
    bool ValidChecksumLine(const char* line, size_t len) const;
    bool ValidChecksumLine(const string& line) const;

    /// Write checksum calculation results into output stream
    CNcbiOstream& WriteChecksum(CNcbiOstream& out) const;
    CNcbiOstream& WriteChecksumData(CNcbiOstream& out) const;
    CNcbiOstream& WriteHexSum(CNcbiOstream& out) const;

private:
    size_t  m_LineCount;   ///< Number of processed lines

    /// Check for checksum line.
    bool ValidChecksumLineLong(const char* line, size_t len) const;
};


/// Write checksum calculation results into output stream
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum);

/// Compute checksum for the given file.
/// @deprecated
///   Please use CChecksum::AddFile() method instead.
NCBI_DEPRECATED NCBI_XUTIL_EXPORT
CChecksum  ComputeFileChecksum(const string& path, CChecksum::EMethod method);

/// Computes checksum for the given file.
/// @deprecated
///   Please use CChecksum::AddFile() method instead.
NCBI_DEPRECATED NCBI_XUTIL_EXPORT
CChecksum& ComputeFileChecksum(const string& path, CChecksum& checksum);

/// Compute CRC32 checksum for the given file.
/// @deprecated
///   Please use CChecksum::AddFile() method instead.
NCBI_DEPRECATED NCBI_XUTIL_EXPORT
Uint4      ComputeFileCRC32(const string& path);



/////////////////////////////////////////////////////////////////////////////
///
/// CChecksumException --
///
/// Define exceptions generated for CChecksum methods.
///
/// CChecksumException inherits its basic functionality from CCoreException
/// and defines additional error codes for CChecksum methods.

class NCBI_XNCBI_EXPORT CChecksumException : public CCoreException
{
public:
    /// Error types that can be generated.
    enum EErrCode {
        eStreamIO,
        eFileIO
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const override;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CChecksumException, CCoreException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CChecksumStreamWriter --
///
/// Stream class to compute checksum for written data.
///
class NCBI_XUTIL_EXPORT CChecksumStreamWriter : public IWriter
{
public:
    /// Construct object to compute checksum for written data.
    CChecksumStreamWriter(CChecksum::EMethod method);
    virtual ~CChecksumStreamWriter(void);

    /// Virtual methods from IWriter
    virtual ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0);
    virtual ERW_Result Flush(void);

    /// Return checksum
    const CChecksum& GetChecksum(void) const;

private:
    CChecksum m_Checksum;  ///< Checksum calculator
};


/* @} */


//////////////////////////////////////////////////////////////////////////////
//
// Inline
//

inline
size_t CChecksumBase::GetSize(void) const
{
    switch ( m_Method ) {
    case eMD5:  
        return 16;
    case eFarmHash64:
    case eCityHash64:
    case eMurmurHash2_64:
        return 8;
    default:
        return 4;
    }
}

inline 
size_t CChecksumBase::GetBits(void) const
{
    return GetSize() * 8;
}

inline
CHash::EMethod CHash::GetMethod(void) const
{
    return (EMethod)m_Method;
}

inline
CChecksum::EMethod CChecksum::GetMethod(void) const
{
    return (EMethod)m_Method;
}

inline
Uint4 CChecksumBase::GetResult32(void) const
{
    switch ( m_Method ) {
    case eCRC32:
    case eCRC32INSD:
    case eAdler32:
    case eCityHash32:
    case eFarmHash32:
    case eMurmurHash2_32:
    case eMurmurHash3_32:
        return m_Value.v32;
    case eCRC32ZIP:
    case eCRC32C:
        return ~m_Value.v32;
    case eCRC32CKSUM:
    {
        // POSIX checksum includes length of data
        char extra_bytes[8];
        size_t extra_len = 0;
        for ( size_t len = m_CharCount; len; len >>= 8 ) {
            extra_bytes[extra_len++] = char(len & 0xff);
        }
        CChecksumBase extra(*this);
        extra.x_Update(extra_bytes, extra_len);
        return ~extra.m_Value.v32;
    }
    default:
        _ASSERT(0);
        return 0;
    }
}

inline
Uint8 CChecksumBase::GetResult64(void) const
{
    switch ( m_Method ) {
    case eCityHash64:
    case eFarmHash64:
    case eMurmurHash2_64:
        return m_Value.v64;
    default:
        _ASSERT(0);
        return 0;
    }
}

inline
void CChecksumBase::x_Free(void)
{
    if ( m_Method == eMD5 ) {
        delete m_Value.md5;
        m_Value.md5 = NULL;
    }
}

inline
void CHash::Calculate(const CTempString str)
{
    x_Update(str.data(), str.size());
    m_CharCount = str.size();
}

inline
void CHash::Calculate(const char* str, size_t count)
{
    x_Update(str, count);
    m_CharCount = count;
}

inline
void CHash::Reset(EMethod method)
{
    x_Reset((EMethodDef)method);
}

inline
void CChecksum::Reset(EMethod method)
{
    x_Reset((EMethodDef)method);
}

inline
void CChecksum::Reset(void)
{
    x_Reset(m_Method);
}

inline
void CChecksum::AddChars(const char* str, size_t count)
{
    x_Update(str, count);
    m_CharCount += count;
}

inline
void CChecksum::AddLine(const char* line, size_t len)
{
    AddChars(line, len);
    NextLine();
}

inline
void CChecksum::AddLine(const string& line)
{
    AddLine(line.data(), line.size());
}

inline
bool CChecksum::ValidChecksumLine(const char* line, size_t len) const
{
    return len > kMinimumChecksumLength &&
        line[0] == '/' && line[1] == '*' &&  // first four letters of checksum
        line[2] == ' ' && line[3] == 'O' &&  // see sx_Start in checksum.cpp
        ValidChecksumLineLong(line, len);    // complete check
}

inline
bool CChecksum::ValidChecksumLine(const string& line) const
{
    return ValidChecksumLine(line.data(), line.size());
}

inline
void CChecksum::GetMD5Digest(unsigned char digest[16]) const
{
    _ASSERT(GetMethod() == eMD5);
    m_Value.md5->Finalize(digest);
}

inline
void CChecksum::GetMD5Digest(string& str) const
{
    _ASSERT(GetMethod() == eMD5);
    unsigned char buf[16];
    m_Value.md5->Finalize(buf);
    str.clear();
    str.insert(str.end(), (const char*)buf, (const char*)buf + 16);
}

inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum)
{
    return checksum.WriteChecksum(out);
}

inline
const CChecksum& CChecksumStreamWriter::GetChecksum(void) const
{
    return m_Checksum;
}


END_NCBI_SCOPE

#endif  /* CHECKSUM__HPP */
