/* $Id$
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
 * Author: Andrei Gourianov
 *
 * File Description:
 *   Holds SOAP message Header contents
 *
 * ===========================================================================
 */

// standard includes
#include <ncbi_pch.hpp>
#include <serial/serialimpl.hpp>

// generated includes
#include "soap_header.hpp"

BEGIN_NCBI_SCOPE
// generated classes

void CSoapHeader::C_E::ResetAnyContent(void)
{
    (*m_AnyContent).Reset();
}

void CSoapHeader::C_E::SetAnyContent(CAnyContentObject& value)
{
    m_AnyContent.Reset(&value);
}

void CSoapHeader::C_E::Reset(void)
{
    ResetAnyContent();
}

BEGIN_NAMED_CLASS_INFO("", CSoapHeader::C_E)
{
    ADD_NAMED_REF_MEMBER("AnyContent", m_AnyContent, CAnyContentObject)->SetNoPrefix()->SetNotag()->SetAnyContent();
}
END_CLASS_INFO

// constructor
CSoapHeader::C_E::C_E(void)
    : m_AnyContent(0/*new CAnyContentObject()*/)
{
}

// destructor
CSoapHeader::C_E::~C_E(void)
{
}


void CSoapHeader::Reset(void)
{
    m_data.clear();
    m_set_State[0] &= ~0x3;
}

BEGIN_NAMED_IMPLICIT_CLASS_INFO("Header", CSoapHeader)
{
    SET_CLASS_MODULE("soap");
    ADD_NAMED_MEMBER("", m_data, STL_list, (STL_CRef, (CLASS, (C_E))))->SetSetFlag(MEMBER_PTR(m_set_State[0]))->SetNoPrefix();
}
END_CLASS_INFO

// constructor
CSoapHeader::CSoapHeader(void)
{
    memset(m_set_State,0,sizeof(m_set_State));
}

// destructor
CSoapHeader::~CSoapHeader(void)
{
}

END_NCBI_SCOPE

/* --------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/06/22 15:00:07  gouriano
* Corrected typeinfo definition
*
* Revision 1.3  2004/05/17 21:03:24  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.2  2003/09/25 19:45:33  gouriano
* Added soap Fault object
*
*
* ===========================================================================
*/
