#ifndef ASNIO__HPP
#define ASNIO__HPP

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
*   ASN.1 <-> memory buffer converter
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  1999/10/21 16:20:38  golikov
* Mode param added
*
* Revision 1.3  1999/04/14 19:11:48  vakatov
* Added "LIBCALLBACK" to AsnRead/Write proto (MSVC++ feature)
*
* Revision 1.2  1999/04/14 17:25:45  vasilche
* Fixed warning about mixing pointers to "C" and "C++" functions.
*
* Revision 1.1  1999/02/17 22:03:08  vasilche
* Assed AsnMemoryRead & AsnMemoryWrite.
* Pager now may return NULL for some components if it contains only one
* page.
*
* 
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

class AsnMemoryRead
{
public:
    AsnMemoryRead(Uint2 mode, const string& str);
    AsnMemoryRead(Uint2 mode, const char* buffer, size_t size);
    virtual ~AsnMemoryRead(void);

    AsnIoPtr GetIn(void) const
        { return m_In; }
    operator AsnIoPtr(void) const
        { return m_In; }

    // ASN.1 communication interface
    size_t Read(char* buffer, size_t size);

private:
    void Init(void);

    string m_Source;
    const char* m_Data;
    size_t m_Size;
    size_t m_Ptr;
    int m_mode;

    // cached ASN.1 communication interface pointer
    AsnIoPtr m_In;
};

class AsnMemoryWrite
{
public:
    AsnMemoryWrite(void);
    virtual ~AsnMemoryWrite(void);

    AsnIoPtr GetOut(void) const
        { return m_Out; }
    operator AsnIoPtr(void) const
        { return m_Out; }
    
    void flush(void) const;

    const char* Data(void) const
        { flush(); return m_Data; }
    const size_t Size(void) const
        { flush(); return m_Ptr; }

    operator string(void) const
        { flush(); return string(m_Data, m_Ptr); }

    // ASN.1 communication interface
    size_t Write(const char* buffer, size_t size);

private:
    char* m_Data;
    size_t m_Size;
    size_t m_Ptr;

    // cached ASN.1 communication interface pointer
    AsnIoPtr m_Out;
};

END_NCBI_SCOPE

#endif /* ASNIO_HPP */
