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
* Revision 1.1  2000/05/24 20:08:49  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objstack.hpp>
#include <serial/strbuffer.hpp>

BEGIN_NCBI_SCOPE

CObjectStack::~CObjectStack(void)
{
    _ASSERT(m_Top == 0);
}

void CObjectStack::UnendedFrame(void)
{
}

void CObjectStackFrame::PopUnended(void)
{
    _ASSERT(m_Stack.m_Top == this);
    try {
        m_Stack.UnendedFrame();
    }
    catch (...) {
        Pop();
        throw;
    }
    Pop();
}

bool CObjectStackFrame::AppendStackTo(string& s) const
{
    bool haveStack = m_Previous && m_Previous->AppendStackTo(s);
    if ( !HaveName() )
        return haveStack;
    if ( haveStack )
        s += '.';
    AppendTo(s);
    return true;
}

void CObjectStackFrame::AppendTo(string& s) const
{
    switch ( m_NameType ) {
    case eNameChar:
        s += m_NameChar;
        break;
    case eNameCharPtr:
        s += m_NameCharPtr;
        break;
    case eNameString:
        s += *m_NameString;
        break;
    case eNameId:
        if ( m_NameId->GetName().empty() ) {
            s += '[';
            s += NStr::IntToString(m_NameId->GetTag());
            s += ']';
        }
        else {
            s += m_NameId->GetName();
        }
        break;
    case eNameTypeInfo:
        s += m_NameTypeInfo->GetName();
        break;
    default:
        break;
    }
}

void CObjectStackFrame::AppendTo(COStreamBuffer& out) const
{
    switch ( m_NameType ) {
    case eNameChar:
        out.PutChar(m_NameChar);
        break;
    case eNameCharPtr:
        out.PutString(m_NameCharPtr);
        break;
    case eNameString:
        out.PutString(*m_NameString);
        break;
    case eNameId:
        if ( m_NameId->GetName().empty() ) {
            out.PutChar('[');
            out.PutInt(m_NameId->GetTag());
            out.PutChar(']');
        }
        else {
            out.PutString(m_NameId->GetName());
        }
        break;
    case eNameTypeInfo:
        out.PutString(m_NameTypeInfo->GetName());
        break;
    default:
        break;
    }
}

END_NCBI_SCOPE
