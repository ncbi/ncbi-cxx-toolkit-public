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
 * Author:  Eugene Vasilchenko
 *
 */

/// @file checksum.hpp
/// Checksum (CRC32 or MD5) calculation class.

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
/// CChecksum -- Checksum calculator
///
/// This class is used to compute control sums (CRC32, Adler32, MD5).
/// Please note that this class uses static tables. This presents a potential
/// race condition in MT programs. To avoid races call static InitTables method
/// before first concurrent use of CChecksum.
///
class NCBI_XUTIL_EXPORT CChecksum
{
public:
    /// Method used to compute control sum.
    enum EMethod {
        eNone,             ///< No checksum in file
        eCRC32,            ///< 32-bit Cyclic Redundancy Check
        eMD5,              ///< Message Digest version 5
        eCRC32ZIP,         ///< Exact zip CRC32, slightly differs from eCRC32
        eAdler32,          ///< A bit faster than CRC32ZIP, not recommended
                           ///< for small data sizes.
        eDefault = eCRC32
    };
    enum {
        kMinimumChecksumLength = 20
    };

    /// Default constructor.
    CChecksum(EMethod method = eDefault);
    /// Copy constructor.
    CChecksum(const CChecksum& cks);
    /// Destructor.
    ~CChecksum();
    /// Assignment operator.
    CChecksum& operator=(const CChecksum& cks);

    /// Initialize static tables used in checksum calculation.
    /// There is no need to call this method since it's done internally.
    static void InitTables(void);

    /// Print C++ code for CRC32 tables for direct inclusion into library.
    /// It also eliminates the need to initialize CRC32 tables.
    static void PrintTables(CNcbiOstream& out);

    /// Get current method used to compute control sum.
    EMethod GetMethod(void) const;

    /// Check that method is specified (not equal eNone).
    bool Valid(void) const;

    /// Return calculated checksum.
    /// Only valid in CRC32/CRC32ZIP/Adler32 modes!
    Uint4 GetChecksum(void) const;

    /// Return calculated MD5 digest.
    /// Only valid in MD5 mode!
    void GetMD5Digest(unsigned char digest[16]) const;
    void GetMD5Digest(string& str) const;

    /// Reset the object to prepare it to the next checksum computation.
    void Reset(EMethod method = eNone/**<keep current method*/);

    // Methods used for file/stream operations

    /// Update current control sum with data provided.
    void AddLine(const char* line, size_t length);
    void AddLine(const string& line);
    void AddChars(const char* str, size_t length);
    void NextLine(void);

    /// Check for checksum line.
    bool ValidChecksumLine(const char* line, size_t length) const;
    bool ValidChecksumLine(const string& line) const;

    /// Write checksum calculation results into output stream
    CNcbiOstream& WriteChecksum(CNcbiOstream& out) const;
    CNcbiOstream& WriteChecksumData(CNcbiOstream& out) const;

private:
    size_t   m_LineCount;  ///< Number of lines
    size_t   m_CharCount;  ///< Number of chars
    EMethod  m_Method;     ///< Current method

    /// Checksum computation results
    union {
        Uint4 m_CRC32; ///< Used to store CRC32/CRC32ZIP/Adler32 checksums
        CMD5* m_MD5;   ///< Used for MD5 calculation
    } m_Checksum;

    /// Check for checksum line.
    bool ValidChecksumLineLong(const char* line, size_t length) const;
    /// Update current control sum with data provided.
    void x_Update(const char* str, size_t length);
    /// Cleanup (used in destructor and assignment operator).
    void x_Free(void);
};


/// Write checksum calculation results into output stream
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum);

/// Compute checksum for the given file.
NCBI_XUTIL_EXPORT
CChecksum  ComputeFileChecksum(const string& path, CChecksum::EMethod method);

/// Computes checksum for the given file.
NCBI_XUTIL_EXPORT
CChecksum& ComputeFileChecksum(const string& path, CChecksum& checksum);

/// Compute CRC32 checksum for the given file.
Uint4      ComputeFileCRC32(const string& path);


/////////////////////////////////////////////////////////////////////////////
///
/// CChecksumStreamWriter --
///
/// Stream class to compute checksum for written data.
///
class CChecksumStreamWriter : public IWriter
{
public:
    /// Construct object to compute checksum for written data.
    CChecksumStreamWriter(CChecksum::EMethod method);

    /// Virtual methods from IWriter
    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t* bytes_written = 0);
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
CChecksum::EMethod CChecksum::GetMethod(void) const
{
    return m_Method;
}

inline
bool CChecksum::Valid(void) const
{
    return GetMethod() != eNone;
}

inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum)
{
    return checksum.WriteChecksum(out);
}

inline
void CChecksum::AddChars(const char* str, size_t count)
{
    x_Update(str, count);
    m_CharCount += count;
}

inline
void CChecksum::AddLine(const char* line, size_t length)
{
    AddChars(line, length);
    NextLine();
}

inline
void CChecksum::AddLine(const string& line)
{
    AddLine(line.data(), line.size());
}

inline
bool CChecksum::ValidChecksumLine(const char* line, size_t length) const
{
    return length > kMinimumChecksumLength &&
        line[0] == '/' && line[1] == '*' && // first four letters of checksum
        line[2] == ' ' && line[3] == 'O' && // see sx_Start in checksum.cpp
        ValidChecksumLineLong(line, length); // complete check
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
    m_Checksum.m_MD5->Finalize(digest);
}

inline
void CChecksum::GetMD5Digest(string& str) const
{
    unsigned char buf[16];
    m_Checksum.m_MD5->Finalize(buf);
    str.clear();
    str.insert(str.end(), (const char*)buf, (const char*)buf + 16);
}

inline
Uint4 ComputeFileCRC32(const string& path)
{
    return ComputeFileChecksum(path, CChecksum::eCRC32).GetChecksum();
}

inline
CChecksumStreamWriter::CChecksumStreamWriter(CChecksum::EMethod method)
    : m_Checksum(method)
{
}

inline
ERW_Result CChecksumStreamWriter::Write(const void* buf, size_t count,
                                        size_t* bytes_written)
{
    m_Checksum.AddChars((const char*)buf, count);
    if (bytes_written) {
        *bytes_written = count;
    }
    return eRW_Success;
}

inline
ERW_Result CChecksumStreamWriter::Flush(void)
{
    return eRW_Success;
}

inline
const CChecksum& CChecksumStreamWriter::GetChecksum(void) const
{
    return m_Checksum;
}


END_NCBI_SCOPE

#endif  /* CHECKSUM__HPP */
