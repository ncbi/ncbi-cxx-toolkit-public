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
* Revision 1.2  2001/01/05 20:09:05  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.1  2000/11/22 16:26:29  vasilche
* Added generation/checking of checksum to user files.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <util/checksum.hpp>

BEGIN_NCBI_SCOPE

// sx_Start MUST begin with "/* O" (see ValidChecksumLine() in checksum.inl)
static const char sx_Start[] = "/* Original file checksum: ";

static const char sx_End[] = " */";
static const char sx_LineCount[] = "lines: ";
static const char sx_CharCount[] = "chars: ";

CChecksum::CChecksum(EMethod method)
    : m_LineCount(0), m_CharCount(0), m_Method(method), m_Checksum(0)
{
    InitTables();
}

CNcbiOstream& CChecksum::WriteChecksum(CNcbiOstream& out) const
{
    if ( !Valid() )
        return out;

    out << sx_Start <<
        sx_LineCount << m_LineCount << ", " <<
        sx_CharCount << m_CharCount << ", ";
    WriteChecksumData(out);
    return out << sx_End << '\n';
}

bool CChecksum::ValidChecksumLineLong(const char* line, size_t length) const
{
    if ( !Valid() )
        return false;
    CNcbiOstrstream buffer;
    WriteChecksum(buffer);
    size_t bufferLength = buffer.pcount() - 1; // do not include '\n'
    if ( bufferLength != length )
        return false;
    const char* bufferPtr = buffer.str();
    buffer.freeze(false);
    return memcmp(line, bufferPtr, length) == 0;
}

CNcbiOstream& CChecksum::WriteChecksumData(CNcbiOstream& out) const
{
    switch ( GetMethod() ) {
    case eCRC32:
        return out << "CRC32: " << hex << setprecision(8) << m_Checksum;
    default:
        return out << "none";
    }
}

void CChecksum::AddChars(const char* str, size_t count)
{
    m_Checksum = UpdateCRC32(m_Checksum, str, count);
    m_CharCount += count;
}

void CChecksum::NextLine(void)
{
    char eol = '\n';
    m_Checksum = UpdateCRC32(m_Checksum, &eol, 1);
    ++m_LineCount;
}

//  This code assumes that an unsigned is at least 32 bits wide and
//  that the predefined type char occupies one 8-bit byte of storage.

//  The polynom used is
//  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x^1+x^0
#define CRC32_POLYNOM 0x04c11db7

Uint4 CChecksum::sm_CRC32Table[256];

void CChecksum::InitTables(void)
{
    if ( sm_CRC32Table[0] == 0 ) {
        for ( size_t i = 0;  i < 256;  ++i ) {
            Uint4 byteCRC = Uint4(i) << 24;
            for ( int j = 0;  j < 8;  ++j ) {
                if ( byteCRC & 0x80000000L )
                    byteCRC = (byteCRC << 1) ^ CRC32_POLYNOM;
                else
                    byteCRC = (byteCRC << 1);
            }
            sm_CRC32Table[i] = byteCRC;
        }
    }
}

Uint4 CChecksum::UpdateCRC32(Uint4 checksum, const char *str, size_t count)
{
    for ( size_t j = 0;  j < count;  ++j ) {
        size_t tableIndex = (int(checksum >> 24) ^ *str++) & 0xff;
        checksum = ((checksum << 8) ^ sm_CRC32Table[tableIndex]) & 0xffffffff;
    }
    return checksum;
}

END_NCBI_SCOPE
