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
 *   Sample SOAP server
 *
 */

#include <ncbi_pch.hpp>
#include <serial/soap/soap_server.hpp>
#include <sample/app/soap/soap_dataobj__.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CSampleSoapServerApplication
//

class CSampleSoapServerApplication : public CSoapServerApplication
{
public:
    CSampleSoapServerApplication(const string& wsdl_filename, const string& namespace_name);
    virtual void Init(void);
    bool GetDescription(CSoapMessage& response, const CSoapMessage& request);
    bool GetDescription2(CSoapMessage& response, const CSoapMessage& request);
    bool GetVersion(CSoapMessage& response, const CSoapMessage& request);
    bool DoMath(CSoapMessage& response, const CSoapMessage& request);
};

CSampleSoapServerApplication::CSampleSoapServerApplication(
    const string& wsdl_filename, const string& namespace_name)
    : CSoapServerApplication(wsdl_filename,namespace_name)
{
}

void CSampleSoapServerApplication::Init()
{
    CSoapServerApplication::Init();
// Register incoming object types
// so the SOAP message parser can recognize these objects
// in incoming data and parse them correctly.
    RegisterObjectType(ncbi::objects::CVersion::GetTypeInfo);
    RegisterObjectType(CMath::GetTypeInfo);
// Register SOAP message processors.
// It is possible to set more than one listeners for a particular message.
// Such listeners will be called in the order of registration.
    AddMessageListener((TWebMethod)(&CSampleSoapServerApplication::GetDescription), "Description");
    AddMessageListener((TWebMethod)(&CSampleSoapServerApplication::GetDescription2), "Description");
    AddMessageListener((TWebMethod)(&CSampleSoapServerApplication::GetVersion), "Version");
    AddMessageListener((TWebMethod)(&CSampleSoapServerApplication::DoMath), "Math");
}

bool CSampleSoapServerApplication::GetDescription(
    CSoapMessage& response, const CSoapMessage& request)
{
    // Continue processing; call other listeners if any
    // (Return 'false' to stop the processing)
    return true;
}

bool CSampleSoapServerApplication::GetDescription2(
    CSoapMessage& response, const CSoapMessage& request)
{
    CRef<CDescriptionText> resp(new CDescriptionText);
    resp->SetText("NCBI C++ Toolkit, Sample Soap Server Application");
    response.AddObject(*resp,CSoapMessage::eMsgBody);
    return true;
}

bool CSampleSoapServerApplication::GetVersion(
    CSoapMessage& response, const CSoapMessage& request)
{
    CConstRef<ncbi::objects::CVersion> req =
        SOAP_GetKnownObject<ncbi::objects::CVersion>(request);
    CRef<CVersionResponse> resp(new CVersionResponse);
    // Just bounce ClientID
    resp->SetVersionStruct().SetClientID(req ? req->GetClientID() : "unknown clientid");
    resp->SetVersionStruct().SetMajor(1);
    resp->SetVersionStruct().SetMinor(0);
    response.AddObject(*resp,CSoapMessage::eMsgBody);
    return true;
}

bool CSampleSoapServerApplication::DoMath(
    CSoapMessage& response, const CSoapMessage& request)
{
    CConstRef<CMath> req = SOAP_GetKnownObject<CMath>(request);
    CRef<CMathResponse> resp(new CMathResponse);
    const CMath::TOperand& ops = req->GetOperand();
    CMath::TOperand::const_iterator it;
    for (it = ops.begin(); it != ops.end(); ++it) {
        int x = (*it)->GetX();
        int y = (*it)->GetY();
        int res;
        COperand::C_Attlist::TOperation op_type = (*it)->GetAttlist().GetOperation();
        if (op_type == COperand::C_Attlist::eAttlist_operation_add) {
            res = x+y;
        } else {
            res = x-y;
        }
        resp->SetMathResult().push_back(res);
    }
    response.AddObject(*resp,CSoapMessage::eMsgBody);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[])
{
    int result = CSampleSoapServerApplication(

// WSDL file name is needed so the server can return it to the client upon request.
// In this case WSDL file should be deployed with the server.
//
// Still, it is possible to pass an empty string here -
// in case you do not want to disclose the WSDL specification
        "soapserver.wsdl",
        "http://www.ncbi.nlm.nih.gov/").AppMain(argc, argv);
    return result;
}
