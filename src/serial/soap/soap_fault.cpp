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
 *   SOAP Fault object
 *
 * ===========================================================================
 */

// standard includes
#include <ncbi_pch.hpp>
#include <serial/serialimpl.hpp>

// generated includes
#include <serial/soap/soap_fault.hpp>

BEGIN_NCBI_SCOPE
// generated classes

void CSoapFault::ResetSoapCode(void)
{
    (*m_SoapCode).Reset();
}

void CSoapFault::SetSoapCode(CSoapCode& value)
{
    m_SoapCode.Reset(&value);
}

void CSoapFault::ResetSoapReason(void)
{
    (*m_SoapReason).Reset();
}

void CSoapFault::SetSoapReason(CSoapReason& value)
{
    m_SoapReason.Reset(&value);
}

void CSoapFault::ResetSoapNode(void)
{
    m_SoapNode.erase();
    m_set_State[0] &= ~0x30;
}

void CSoapFault::ResetSoapRole(void)
{
    m_SoapRole.erase();
    m_set_State[0] &= ~0xc0;
}

void CSoapFault::ResetSoapDetail(void)
{
    m_SoapDetail.Reset();
}

void CSoapFault::SetSoapDetail(CSoapDetail& value)
{
    m_SoapDetail.Reset(&value);
}

CSoapDetail& CSoapFault::SetSoapDetail(void)
{
    if ( !m_SoapDetail )
        m_SoapDetail.Reset(new CSoapDetail());
    return (*m_SoapDetail);
}

void CSoapFault::Reset(void)
{
    ResetSoapCode();
    ResetSoapReason();
    ResetSoapNode();
    ResetSoapRole();
    ResetSoapDetail();
}

BEGIN_NAMED_CLASS_INFO("Fault", CSoapFault)
{
    SET_CLASS_MODULE("soap");
    ADD_NAMED_REF_MEMBER("Code", m_SoapCode, CSoapCode)->SetNoPrefix();
    ADD_NAMED_REF_MEMBER("Reason", m_SoapReason, CSoapReason)->SetNoPrefix();
    ADD_NAMED_STD_MEMBER("Node", m_SoapNode)->SetOptional()->SetSetFlag(MEMBER_PTR(m_set_State[0]))->SetNoPrefix();
    ADD_NAMED_STD_MEMBER("Role", m_SoapRole)->SetOptional()->SetSetFlag(MEMBER_PTR(m_set_State[0]))->SetNoPrefix();
    ADD_NAMED_REF_MEMBER("Detail", m_SoapDetail, CSoapDetail)->SetOptional()->SetNoPrefix();
    info->RandomOrder();
}
END_CLASS_INFO

// constructor
CSoapFault::CSoapFault(void)
    : m_SoapCode(new CSoapCode()), m_SoapReason(new CSoapReason())
{
    memset(m_set_State,0,sizeof(m_set_State));
}

// destructor
CSoapFault::~CSoapFault(void)
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
