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
* Author: Eugene Vasilchenko
*
* File Description:
*   checksum (CRC32 or MD5) calculation class
*/

#include <util/md5.hpp>


/** @addtogroup Checksum
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XUTIL_EXPORT CChecksum
{
public:
    enum EMethod {
        eNone,             // no checksum in file
        eCRC32,            // 32-bit Cyclic Redundancy Check
        eMD5,              // Message Digest version 5
        eDefault = eCRC32
    };
    enum {
        kMinimumChecksumLength = 20
    };

    CChecksum(EMethod method = eDefault);
    CChecksum(const CChecksum& cks);
    ~CChecksum();
    CChecksum& operator=(const CChecksum& cks);

    bool Valid(void) const;
    EMethod GetMethod(void) const;

    void AddLine(const char* line, size_t length);
    void AddLine(const string& line);

    void AddChars(const char* str, size_t length);
    void NextLine(void);

    bool ValidChecksumLine(const char* line, size_t length) const;
    bool ValidChecksumLine(const string& line) const;

    CNcbiOstream& WriteChecksum(CNcbiOstream& out) const;

    /// Only valid in CRC32 mode!
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

    static Uint4 sm_CRC32Table[256];
    static void InitTables(void);
    static Uint4 UpdateCRC32(Uint4 checksum, const char* str, size_t length);

    bool ValidChecksumLineLong(const char* line, size_t length) const;

    void x_Free();
};

inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum);


/// This function computes the checksum for the given file.

CChecksum NCBI_XUTIL_EXPORT ComputeFileChecksum(const string& path,
                                                CChecksum::EMethod method);


NCBI_XUTIL_EXPORT CChecksum& ComputeFileChecksum(const string& path,
                                                 CChecksum& checksum);

inline Uint4 ComputeFileCRC32(const string& path)
{
    return ComputeFileChecksum(path, CChecksum::eCRC32).GetChecksum();
}


/* @} */


#include <util/checksum.inl>

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2003/08/11 16:45:52  kuznets
* Added Reset() function to clear the accumulated checksum and reuse the object
* for consecutive checksum calculations
*
* Revision 1.7  2003/07/29 21:29:26  ucko
* Add MD5 support (cribbed from the C Toolkit)
*
* Revision 1.6  2003/05/08 19:10:21  kuznets
* + ComputeFileCRC32 function
*
* Revision 1.5  2003/04/17 17:50:13  siyan
* Added doxygen support
*
* Revision 1.4  2003/04/15 16:12:09  kuznets
* GetChecksum() method implemented
*
* Revision 1.3  2002/12/19 14:51:00  dicuccio
* Added export specifier for Win32 DLL builds.
*
* Revision 1.2  2001/01/05 20:08:52  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.1  2000/11/22 16:26:21  vasilche
* Added generation/checking of checksum to user files.
*
* ===========================================================================
*/

#endif  /* CHECKSUM__HPP */
