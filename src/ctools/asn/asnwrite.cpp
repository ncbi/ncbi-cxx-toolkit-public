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
