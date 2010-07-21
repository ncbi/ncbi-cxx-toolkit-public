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


/** @addtogroup Checksum
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CMD5;

/// Checksum calculator
///
/// This class is used to compute control sums (CRC32, MD5).
/// Please note that this class uses static tables. This presents a potential
/// race condition in MT programs. To avoid races call static InitTables method
/// before first concurrent use of CChecksum.
///
class NCBI_XUTIL_EXPORT CChecksum
{
public:
    enum EMethod {
        eNone,             // no checksum in file
        eCRC32,            // 32-bit Cyclic Redundancy Check
        eMD5,              // Message Digest version 5
        eCRC32ZIP,         // Exact zip CRC32, slightly differs from eCRC32
        eAdler32,          // A bit faster than CRC32ZIP, not recommended
                           // for small data sizes.
        eDefault = eCRC32
    };
    enum {
        kMinimumChecksumLength = 20
    };

    CChecksum(EMethod method = eDefault);
    CChecksum(const CChecksum& cks);
    ~CChecksum();
    CChecksum& operator=(const CChecksum& cks);

    /// Initialize static tables used in checksum calculation.
    /// There is no need to call this method since it's done internally.
    static void InitTables(void);

    /// Print C++ code for CRC32 tables for direct inclusion into library.
    /// It eliminates the need to initialize CRC32 tables.
    static void PrintTables(CNcbiOstream& out);

    bool Valid(void) const;
    EMethod GetMethod(void) const;

    void AddLine(const char* line, size_t length);
    void AddLine(const string& line);

    void AddChars(const char* str, size_t length);
    void NextLine(void);

    bool ValidChecksumLine(const char* line, size_t length) const;
    bool ValidChecksumLine(const string& line) const;

    CNcbiOstream& WriteChecksum(CNcbiOstream& out) const;

    /// Only valid in CRC32 and Adler32 modes!
    Uint4 GetChecksum() const;

    /// Only valid in MD5 mode!
    void GetMD5Digest(unsigned char digest[16]) const;

    CNcbiOstream& WriteChecksumData(CNcbiOstream& out) const;

    // Reset the object to prepare it to the next checksum computation
    void Reset();

private:
    size_t m_LineCount;
    size_t m_CharCount;
    EMethod m_Method;
    union {
        Uint4 m_CRC32;
        CMD5* m_MD5;
    } m_Checksum;

    bool ValidChecksumLineLong(const char* line, size_t length) const;

    void x_Update(const char* str, size_t length);
    void x_Free();
};


/// Stream class to compute checksum.
class CChecksumStreamWriter : public IWriter
{
public:
    CChecksumStreamWriter(CChecksum::EMethod method)
        : m_Checksum(method)
        {}

    virtual ERW_Result Write(const void* buf,
                             size_t count,
                             size_t* bytes_written = 0)
    {
        if (bytes_written) {
            *bytes_written = count;
        }
        m_Checksum.AddChars((const char*)buf, count);
        return eRW_Success;
    }

    virtual ERW_Result Flush()
    {
        return eRW_Success;
    }

    /// Only valid in CRC32 and Adler32 modes!
    Uint4 GetChecksum() const
    {
        return m_Checksum.GetChecksum();
    }

    /// Only valid in MD5 mode!
    void GetMD5Sum(string& s) const
    {
        unsigned char buf[16];
        m_Checksum.GetMD5Digest(buf);
        s.clear();
        s.insert(s.end(), (const char*)buf, (const char*)buf + 16);
    }

private:
    CChecksum m_Checksum;
};


CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum);

/// This function computes the checksum for the given file.
NCBI_XUTIL_EXPORT
CChecksum ComputeFileChecksum(const string& path,
                              CChecksum::EMethod method);

/// This function computes the checksum for the given file.
NCBI_XUTIL_EXPORT
CChecksum& ComputeFileChecksum(const string& path,
                               CChecksum& checksum);

/// This function computes the CRC32 checksum for the given file.
inline Uint4 ComputeFileCRC32(const string& path)
{
    return ComputeFileChecksum(path, CChecksum::eCRC32).GetChecksum();
}

/* @} */

#include <util/checksum.inl>

END_NCBI_SCOPE

#endif  /* CHECKSUM__HPP */
