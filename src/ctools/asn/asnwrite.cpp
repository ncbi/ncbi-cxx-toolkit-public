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
* Revision 1.10  2004/05/17 20:59:07  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.9  2001/02/13 20:44:24  vakatov
* Use `reinterpret_cast<IoFuncType>(WriteAsn)' instead of a more safe
* (but not-compilable by MIPSpro7.3 compiler on IRIX) `extern "C"'
* pre-declaration.
*
* Revision 1.8  2001/02/10 05:00:17  lavr
* ctools added in #includes
*
* Revision 1.7  2000/11/29 17:25:16  vasilche
* Added possibility to change ASNIO mode (mainly for XML output).
* Fixed warnings on 64 bit compilers.
*
* Revision 1.6  1999/11/24 20:18:19  golikov
* flush moved from CreateSubNodes to PrintChildren -> loose of text fixed
*
* Revision 1.5  1999/05/15 23:00:59  vakatov
* Moved "asnio" and "asnwrite" modules to the (new) library
* "xasn"(project "asn")
*
* Revision 1.4  1999/04/15 21:59:58  vakatov
* [MSVC++]  Added "LIBCALLBACK" to the WriteAsn() proto
*
* Revision 1.3  1999/04/14 17:26:52  vasilche
* Fixed warning about mixing pointers to "C" and "C++" functions.
*
* Revision 1.2  1999/02/17 22:03:16  vasilche
* Assed AsnMemoryRead & AsnMemoryWrite.
* Pager now may return NULL for some components if it contains only one
* page.
*
* Revision 1.1  1999/01/28 15:11:09  vasilche
* Added new class CAsnWriteNode for displaying ASN.1 structure in HTML page.
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <ctools/asn/asnwrite.hpp>

BEGIN_NCBI_SCOPE


CAsnWriteNode::CAsnWriteNode(void)
    : m_Out(0), m_Mode(ASNIO_TEXT)
{
}

CAsnWriteNode::CAsnWriteNode(int mode)
    : m_Out(0), m_Mode(mode)
{
}

CAsnWriteNode::~CAsnWriteNode(void)
{
    AsnIoClose(m_Out);
}


static Int2 LIBCALLBACK WriteAsn(Pointer data, CharPtr buffer, Uint2 size)
{
    if ( !data || !buffer )
        return -1;

    static_cast<CAsnWriteNode*>(data)->AppendPlainText(string(buffer, size));
    return size;
}


AsnIoPtr CAsnWriteNode::GetOut(void)
{
    AsnIoPtr out = m_Out;
    if ( !out ) {
        out = m_Out = AsnIoNew(m_Mode | ASNIO_OUT, 0, this,
                               0, reinterpret_cast<IoFuncType>(WriteAsn));
    }
    return out;
}

CNcbiOstream& CAsnWriteNode::PrintChildren(CNcbiOstream& out, TMode mode)
{
    AsnIoFlush(m_Out);
    CHTMLNode::PrintChildren(out, mode);
    return out;
}

END_NCBI_SCOPE
