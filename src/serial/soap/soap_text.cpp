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
 *   SOAP Text object
 *
 * ===========================================================================
 */

// standard includes
#include <ncbi_pch.hpp>
#include <serial/serialimpl.hpp>

// generated includes
#include <serial/soap/soap_text.hpp>

BEGIN_NCBI_SCOPE
// generated classes

void CSoapText::C_Attlist::ResetXmllang(void)
{
    m_Xmllang.erase();
    m_set_State[0] &= ~0x3;
}

void CSoapText::C_Attlist::Reset(void)
{
    ResetXmllang();
}

BEGIN_NAMED_CLASS_INFO("", CSoapText::C_Attlist)
{
    ADD_NAMED_STD_MEMBER("xml:lang", m_Xmllang)->SetSetFlag(MEMBER_PTR(m_set_State[0]))->SetNoPrefix();
    info->SetRandomOrder(true);
}
END_CLASS_INFO

// constructor
CSoapText::C_Attlist::C_Attlist(void)
{
    memset(m_set_State,0,sizeof(m_set_State));
}

// destructor
CSoapText::C_Attlist::~C_Attlist(void)
{
}


void CSoapText::ResetAttlist(void)
{
    (*m_Attlist).Reset();
}

void CSoapText::SetAttlist(CSoapText::C_Attlist& value)
{
    m_Attlist.Reset(&value);
}

void CSoapText::ResetSoapText(void)
{
    m_SoapText.erase();
    m_set_State[0] &= ~0xc;
}

void CSoapText::Reset(void)
{
    ResetAttlist();
    ResetSoapText();
}

BEGIN_NAMED_CLASS_INFO("Text", CSoapText)
{
    SET_CLASS_MODULE("soap");
    ADD_NAMED_REF_MEMBER("Attlist", m_Attlist, C_Attlist)->SetNoPrefix()->SetAttlist();
    ADD_NAMED_STD_MEMBER("Text", m_SoapText)->SetSetFlag(MEMBER_PTR(m_set_State[0]))->SetNoPrefix()->SetNotag();
    info->RandomOrder();
}
END_CLASS_INFO

// constructor
CSoapText::CSoapText(void)
    : m_Attlist(new C_Attlist())
{
    memset(m_set_State,0,sizeof(m_set_State));
}

CSoapText::CSoapText(const std::string& value)
{
    SetSoapText(value);
}

// destructor
CSoapText::~CSoapText(void)
{
}

END_NCBI_SCOPE

/* --------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/05/17 21:03:24  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.1  2003/09/25 19:45:33  gouriano
* Added soap Fault object
*
*
* ===========================================================================
*/
