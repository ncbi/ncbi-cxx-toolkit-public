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
 *   SOAP Subcode object
 *
 * ===========================================================================
 */

// standard includes
#include <ncbi_pch.hpp>
#include <serial/serialimpl.hpp>

// generated includes
#include <serial/soap/soap_subcode.hpp>

BEGIN_NCBI_SCOPE
// generated classes

void CSoapSubcode::ResetSoapValue(void)
{
    m_SoapValue.erase();
    m_set_State[0] &= ~0x3;
}

void CSoapSubcode::ResetSoapSubcode(void)
{
    m_SoapSubcode.Reset();
}

void CSoapSubcode::SetSoapSubcode(CSoapSubcode& value)
{
    m_SoapSubcode.Reset(&value);
}

CSoapSubcode& CSoapSubcode::SetSoapSubcode(void)
{
    if ( !m_SoapSubcode )
        m_SoapSubcode.Reset(new CSoapSubcode());
    return (*m_SoapSubcode);
}

void CSoapSubcode::Reset(void)
{
    ResetSoapValue();
    ResetSoapSubcode();
}

BEGIN_NAMED_CLASS_INFO("Subcode", CSoapSubcode)
{
    SET_CLASS_MODULE("soap");
    ADD_NAMED_STD_MEMBER("Value", m_SoapValue)->SetSetFlag(MEMBER_PTR(m_set_State[0]))->SetNoPrefix();
    ADD_NAMED_REF_MEMBER("Subcode", m_SoapSubcode, CSoapSubcode)->SetOptional()->SetNoPrefix();
    info->RandomOrder();
}
END_CLASS_INFO

// constructor
CSoapSubcode::CSoapSubcode(void)
{
    memset(m_set_State,0,sizeof(m_set_State));
}

// destructor
CSoapSubcode::~CSoapSubcode(void)
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
