#ifndef ASNWRITE__HPP
#define ASNWRITE__HPP

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
* Revision 1.6  1999/11/24 20:18:09  golikov
* flush moved from CreateSubNodes to PrintChildren -> loose of text fixed
*
* Revision 1.5  1999/05/15 23:00:56  vakatov
* Moved "asnio" and "asnwrite" modules to the (new) library
* "xasn"(project "asn")
*
* Revision 1.4  1999/04/14 17:26:49  vasilche
* Fixed warning about mixing pointers to "C" and "C++" functions.
*
* Revision 1.3  1999/02/26 21:03:29  vasilche
* CAsnWriteNode made simple node. Use CHTML_pre explicitely.
* Fixed bug in CHTML_table::Row.
* Added CHTML_table::HeaderCell & DataCell methods.
*
* Revision 1.2  1999/02/18 18:42:14  vasilche
* Added autoflushing.
*
* Revision 1.1  1999/01/28 15:11:06  vasilche
* Added new class CAsnWriteNode for displaying ASN.1 structure in HTML page.
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <html/html.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

class CAsnWriteNode : public CHTMLNode
{
public:
    CAsnWriteNode(void);
    ~CAsnWriteNode(void);

    AsnIoPtr GetOut(void);
    operator AsnIoPtr(void)
        { return GetOut(); }

    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);

private:
    // ASN.1 communication interface
    //    static Int2 WriteAsn(Pointer data, CharPtr buffer, Uint2 size);

    // cached ASN.1 communication interface pointer
    AsnIoPtr m_Out;
};

#include <asn/asnwrite.inl>

END_NCBI_SCOPE

#endif  /* ASNWRITE__HPP */
