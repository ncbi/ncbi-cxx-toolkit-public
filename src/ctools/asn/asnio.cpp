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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  1999/04/16 17:45:33  vakatov
* [MSVC++] Replace the <windef.h>'s min/max macros by the hand-made templates.
*
* Revision 1.4  1999/04/15 21:58:24  vakatov
* Use NcbiMin instead of MIN
*
* Revision 1.3  1999/04/14 19:11:51  vakatov
* Added "LIBCALLBACK" to AsnRead/Write proto (MSVC++ feature)
*
* Revision 1.2  1999/04/14 17:25:41  vasilche
* Fixed warning about mixing pointers to "C" and "C++" functions.
*
* Revision 1.1  1999/02/17 22:03:12  vasilche
* Assed AsnMemoryRead & AsnMemoryWrite.
* Pager now may return NULL for some components if it contains only one
* page.
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/asnio.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

extern "C" {
    static Int2 LIBCALLBACK ReadAsn(Pointer data, CharPtr buffer, Uint2 size);
    static Int2 LIBCALLBACK WriteAsn(Pointer data, CharPtr buffer, Uint2 size);
}

AsnMemoryRead::AsnMemoryRead(const string& str)
    : m_Source(str), m_Data(str.c_str()), m_Size(str.size())
{
    Init();
}

AsnMemoryRead::AsnMemoryRead(const char* data, size_t size)
    : m_Data(data), m_Size(size)
{
    Init();
}

AsnMemoryRead::~AsnMemoryRead(void)
{
    AsnIoClose(m_In);
}

void AsnMemoryRead::Init(void)
{
    m_Ptr = 0;
    m_In = AsnIoNew(ASNIO_TEXT | ASNIO_IN, 0, this, ReadAsn, 0);
}

size_t AsnMemoryRead::Read(char* buffer, size_t size)
{
    int count = min(size, m_Size - m_Ptr);
    memcpy(buffer, m_Data + m_Ptr, count);
    m_Ptr += count;
    return count;
}

Int2 LIBCALLBACK ReadAsn(Pointer data, CharPtr buffer, Uint2 size)
{
    if ( !data || !buffer )
        return -1;

    return static_cast<AsnMemoryRead*>(data)->Read(buffer, size);
}

AsnMemoryWrite::AsnMemoryWrite(void)
    : m_Data(new char[512]), m_Size(512), m_Ptr(0)
{
    m_Out = AsnIoNew(ASNIO_TEXT | ASNIO_OUT, 0, this, 0, WriteAsn);
}

AsnMemoryWrite::~AsnMemoryWrite(void)
{
    AsnIoClose(m_Out);
    delete[] m_Data;
}

size_t AsnMemoryWrite::Write(const char* buffer, size_t size)
{
    if ( m_Size - m_Ptr < size ) { // not enough space
        // new buffer
        char* data = new char[m_Size *= 2];
        if ( m_Ptr ) // copy old data
            memcpy(data, m_Data, m_Ptr);
        // delete old buffer
        delete[] m_Data;
        // set new buffer
        m_Data = data;
    }
    // append data
    memcpy(m_Data + m_Ptr, buffer, size);
    // increase size
    m_Ptr += size;
    return size;
}

Int2 LIBCALLBACK WriteAsn(Pointer data, CharPtr buffer, Uint2 size)
{
    if ( !data || !buffer )
        return -1;

    return static_cast<AsnMemoryWrite*>(data)->Write(buffer, size);
}

void AsnMemoryWrite::flush(void) const
{
    AsnIoFlush(m_Out);
}

END_NCBI_SCOPE

