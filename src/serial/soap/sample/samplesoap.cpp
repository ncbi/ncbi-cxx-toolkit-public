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
 *   Sample soap server
 *
 */

#include <ncbi_pch.hpp>
#include <serial/soap/soap_server.hpp>
#include "dataobj__.hpp"

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  CSoapSampleServer
//

class CSoapSampleServer : public CSoapServerApplication
{
public:
    CSoapSampleServer(const string& wsdl_filename, const string& namespace_name);
    virtual void Init(void);
    bool GetDescription(CSoapMessage& response, const CSoapMessage& request);
    bool GetDescription2(CSoapMessage& response, const CSoapMessage& request);
    bool GetVersion(CSoapMessage& response, const CSoapMessage& request);
    bool DoMath(CSoapMessage& response, const CSoapMessage& request);
};

CSoapSampleServer::CSoapSampleServer(const string& wsdl_filename, const string& namespace_name)
    : CSoapServerApplication(wsdl_filename,namespace_name)
{
}

void CSoapSampleServer::Init()
{
    RegisterObjectType(CVersion::GetTypeInfo);
    RegisterObjectType(CMath::GetTypeInfo);
    AddMessageListener((TWebMethod)(&CSoapSampleServer::GetDescription), "Description");
    AddMessageListener((TWebMethod)(&CSoapSampleServer::GetDescription2), "Description");
    AddMessageListener((TWebMethod)(&CSoapSampleServer::GetVersion), "Version");
    AddMessageListener((TWebMethod)(&CSoapSampleServer::DoMath), "Math");
}

bool CSoapSampleServer::GetDescription(CSoapMessage& response, const CSoapMessage& request)
{
    return true;
}

bool CSoapSampleServer::GetDescription2(CSoapMessage& response, const CSoapMessage& request)
{
    CRef<CDescriptionText> resp(new CDescriptionText);
    resp->SetText("Sample Soap Server");
    response.AddObject(*resp,CSoapMessage::eMsgBody);
    return true;
}

bool CSoapSampleServer::GetVersion(CSoapMessage& response, const CSoapMessage& request)
{
    CConstRef<CVersion> req = SOAP_GetKnownObject<CVersion>(request);
    CRef<CVersionResponse> resp(new CVersionResponse);
    resp->SetVersionStruct().SetClientID(bool(req) ? req->GetClientID() : "unknown clientid");
    resp->SetVersionStruct().SetMajor("1");
    resp->SetVersionStruct().SetMinor("0");
    response.AddObject(*resp,CSoapMessage::eMsgBody);
    return true;
}

bool CSoapSampleServer::DoMath(CSoapMessage& response, const CSoapMessage& request)
{
    CConstRef<CMath> req = SOAP_GetKnownObject<CMath>(request);
    CRef<CMathResponse> resp(new CMathResponse);
    const CMath::TOperand& ops = req->GetOperand();
    CMath::TOperand::const_iterator it;
    for (it = ops.begin(); it != ops.end(); ++it) {
        int x = NStr::StringToInt((*it)->GetX());
        int y = NStr::StringToInt((*it)->GetY());
        int res;
        COperand::C_Attlist::TOperation op_type = (*it)->GetAttlist().GetOperation();
        if (op_type == COperand::C_Attlist::eAttlist_operation_add) {
            res = x+y;
        } else {
            res = x-y;
        }
        resp->SetMathResult().push_back(NStr::IntToString(res));
    }
    response.AddObject(*resp,CSoapMessage::eMsgBody);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[])
{
    int result = CSoapSampleServer("samplesoap.wsdl", "http://ncbi.nlm.nih.gov/").AppMain(argc, argv, 0, eDS_Default);
    return result;
}

/* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/07/06 13:48:49  gouriano
* Initial revision: sample SOAP server and client
*
*
* ===========================================================================
*/

