
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
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   Sample SOAP client
 *
 */

#include <ncbi_pch.hpp>
#include "sampleclient.hpp"


USING_NCBI_SCOPE;

CSampleSoapClient::CSampleSoapClient(void)
    : CSoapHttpClient("http://graceland.ncbi.nlm.nih.gov:6224/staff/gouriano/cgi/samplesoap/samplesoap.cgi",
                      "http://ncbi.nlm.nih.gov/")
{
// Register incoming object types
    RegisterObjectType(CDescriptionText::GetTypeInfo);
    RegisterObjectType(CVersionResponse::GetTypeInfo);
    RegisterObjectType(CMathResponse::GetTypeInfo);
}

CSampleSoapClient::~CSampleSoapClient(void)
{
}

CConstRef<CDescriptionText> CSampleSoapClient::GetDescription(void)
{
    CSoapMessage request, response;

    CRef<CAnyContentObject> any(new CAnyContentObject);
    any->SetName("Description");
    any->SetNamespaceName(GetDefaultNamespaceName());
    request.AddObject( *any, CSoapMessage::eMsgBody);

    Invoke(response,request);
    return SOAP_GetKnownObject<CDescriptionText>(response);
}

CConstRef<CVersionResponse> CSampleSoapClient::GetVersion(const string& client_id)
{
    CSoapMessage request, response;

    CRef<CVersion> req(new CVersion);
    req->SetClientID(client_id);
    req->SetNamespaceName(GetDefaultNamespaceName());
    request.AddObject( *req, CSoapMessage::eMsgBody);

    Invoke(response,request);
    return SOAP_GetKnownObject<CVersionResponse>(response);
}

CConstRef<CMathResponse>  CSampleSoapClient::DoMath(CMath& ops)
{
    CSoapMessage request, response;
    ops.SetNamespaceName(GetDefaultNamespaceName());
    request.AddObject( ops, CSoapMessage::eMsgBody);

    Invoke(response,request);
    return SOAP_GetKnownObject<CMathResponse>(response);
}

/* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/07/06 13:48:49  gouriano
* Initial revision: sample SOAP server and client
*
*
* ===========================================================================
*/
