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
 *   Holds SOAP message contents
 */

// standard includes
#include <ncbi_pch.hpp>
#include <serial/serialimpl.hpp>

// generated includes
#include "soap_envelope.hpp"
#include "soap_body.hpp"
#include "soap_header.hpp"

BEGIN_NCBI_SCOPE
// generated classes

void CSoapEnvelope::ResetHeader(void)
{
    m_Header.Reset();
}

void CSoapEnvelope::SetHeader(CSoapHeader& value)
{
    m_Header.Reset(&value);
}

CSoapHeader& CSoapEnvelope::SetHeader(void)
{
    if ( !m_Header )
        m_Header.Reset(new CSoapHeader());
    return (*m_Header);
}

void CSoapEnvelope::ResetBody(void)
{
    (*m_Body).Reset();
}

void CSoapEnvelope::SetBody(CSoapBody& value)
{
    m_Body.Reset(&value);
}

void CSoapEnvelope::Reset(void)
{
    ResetHeader();
    ResetBody();
}

BEGIN_NAMED_CLASS_INFO("Envelope", CSoapEnvelope)
{
    SET_CLASS_MODULE("soap");
    ADD_NAMED_REF_MEMBER("Header", m_Header, CSoapHeader)->SetOptional()->SetNoPrefix();
    ADD_NAMED_REF_MEMBER("Body", m_Body, CSoapBody)->SetNoPrefix();
}
END_CLASS_INFO

// constructor
CSoapEnvelope::CSoapEnvelope(void)
    : m_Body(new CSoapBody())
{
}

// destructor
CSoapEnvelope::~CSoapEnvelope(void)
{
}

END_NCBI_SCOPE

/* --------------------------------------------------------------------------
* $Log$
* Revision 1.3  2004/05/17 21:03:24  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.2  2003/09/25 19:45:33  gouriano
* Added soap Fault object
*
*
* ===========================================================================
*/
