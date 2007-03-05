#if defined(CHECKSUM__HPP)  &&  !defined(CHECKSUM__INL)
#define CHECKSUM__INL

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
*   CRC32 calculation inline methods
*/

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
        line[0] == '/' && line[1] == '*' && // first four letter of checksum
        line[2] == ' ' && line[3] == 'O' && // see sx_Start in checksum.cpp
        ValidChecksumLineLong(line, length); // complete check
}

inline
bool CChecksum::ValidChecksumLine(const string& line) const
{
    return ValidChecksumLine(line.data(), line.size());
}

#endif /* def CHECKSUM__HPP  &&  ndef CHECKSUM__INL */
