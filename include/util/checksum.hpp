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
*   CRC32 calculation class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/01/05 20:08:52  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.1  2000/11/22 16:26:21  vasilche
* Added generation/checking of checksum to user files.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CChecksum
{
public:
    enum EMethod {
        eNone,             // no checksum in file
        eCRC32,            // CRC32
        eDefault = eCRC32
    };
    enum {
        kMinimumChecksumLength = 20
    };

    CChecksum(EMethod method = eDefault);

    bool Valid(void) const;
    EMethod GetMethod(void) const;

    void AddLine(const char* line, size_t length);
    void AddLine(const string& line);

    void AddChars(const char* str, size_t length);
    void NextLine(void);

    bool ValidChecksumLine(const char* line, size_t length) const;
    bool ValidChecksumLine(const string& line) const;

    CNcbiOstream& WriteChecksum(CNcbiOstream& out) const;

private:
    CNcbiOstream& WriteChecksumData(CNcbiOstream& out) const;

private:
    size_t m_LineCount;
    size_t m_CharCount;
    EMethod m_Method;
    Uint4 m_Checksum;

    static Uint4 sm_CRC32Table[256];
    static void InitTables(void);
    static Uint4 UpdateCRC32(Uint4 checksum, const char* str, size_t length);

    bool ValidChecksumLineLong(const char* line, size_t length) const;
};

inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum);

#include <util/checksum.inl>

END_NCBI_SCOPE

#endif  /* CHECKSUM__HPP */
