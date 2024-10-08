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
 *   Sample SOAP HTTP client
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/util_exception.hpp>

#include <serial/soap/soap_client.hpp>
#include <sample/app/soap/soap_dataobj__.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// to see communication log, put the following into environment:
//  CONN_DEBUG_PRINTOUT=data
//  DIAG_POST_LEVEL=0


/////////////////////////////////////////////////////////////////////////////
//
//  CSampleSoapClient
//

class CSampleSoapClient: public CSoapHttpClient
{
public:
    CSampleSoapClient(void);
    ~CSampleSoapClient(void);

    CConstRef<CDescriptionText> GetDescription( CConstRef<CSoapFault>* fault=0);
    CConstRef<CVersionResponse> GetVersion(const string& client_id, CConstRef<CSoapFault>* fault=0);
    CConstRef<CMathResponse>    DoMath(CMath& ops, CConstRef<CSoapFault>* fault=0);
};


/////////////////////////////////////////////////////////////////////////////

CSampleSoapClient::CSampleSoapClient(void)
    : CSoapHttpClient(
    // This Server URL is valid only within NCBI
#if 0
    // works either way
        "http://intrawebdev8:6224/staff/gouriano/samplesoap/samplesoap.cgi",
#else
        "https://intrawebdev8/staff/gouriano/samplesoap/samplesoap.cgi",
#endif
        "http://ncbi.nlm.nih.gov/")
{
    // Register incoming object types
    // so the SOAP message parser can recognize these objects
    // in incoming data and parse them correctly
    RegisterObjectType(CDescriptionText::GetTypeInfo);
    RegisterObjectType(CVersionResponse::GetTypeInfo);
    RegisterObjectType(CMathResponse::GetTypeInfo);
}


CSampleSoapClient::~CSampleSoapClient(void)
{
}


CConstRef<CDescriptionText>
CSampleSoapClient::GetDescription(CConstRef<CSoapFault>* fault)
// Request with no parameters
// Response is a single string
{
    CSoapMessage request( GetDefaultNamespaceName() ), response;

    // Both variants are okay
#if 0
    CRef<CAnyContentObject> any(new CAnyContentObject);
    any->SetName("Description");
//    any->SetNamespaceName(GetDefaultNamespaceName());
#else
    CRef<CDescription> any(new CDescription);
    any->Set();
#endif
    request.AddObject( *any, CSoapMessage::eMsgBody);

    Invoke(response,request,fault);
    return SOAP_GetKnownObject<CDescriptionText>(response);
}


// Request is a single string: ClientID
// Response is a sequence of 3 strings: Major, Minor, ClientID
// here ClientID is same as the one sent in request
CConstRef<CVersionResponse>
CSampleSoapClient::GetVersion(const string& client_id, CConstRef<CSoapFault>* fault)
{
    CSoapMessage request( GetDefaultNamespaceName() ), response;

    CRef<CSampleVersion> req(new CSampleVersion);
    req->SetClientID(client_id);
//    req->SetNamespaceName(GetDefaultNamespaceName());
    request.AddObject( *req, CSoapMessage::eMsgBody);

    Invoke(response,request,fault);
    return SOAP_GetKnownObject<CVersionResponse>(response);
}


// Request is a list of Operand where each Operand is a sequence of 2 strings + attribute
// Response is a list of result strings
CConstRef<CMathResponse>
CSampleSoapClient::DoMath(CMath& ops, CConstRef<CSoapFault>* fault)
{
    CSoapMessage request, response;
    // The following call is not needed, because the ops object already has
    // a (correct) namespace name defined
//    ops.SetNamespaceName(GetDefaultNamespaceName());
    request.AddObject( ops, CSoapMessage::eMsgBody);

    Invoke(response,request,fault);
    return SOAP_GetKnownObject<CMathResponse>(response);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CSampleSoapClientApplication
//

class CSampleSoapClientApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


/////////////////////////////////////////////////////////////////////////////

void CSampleSoapClientApplication::Init(void)
{
    CNcbiApplication::Init();
    // Create
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Program description
    string prog_description = "Test NCBI C++ Toolkit Sample Soap Server\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);
    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}


int CSampleSoapClientApplication::Run(void)
{
    CSampleSoapClient ws;
    try {

        CConstRef<CSoapFault> fault;
        CConstRef<CDescriptionText> v1 = ws.GetDescription(&fault);
        if (v1) {
            cout << v1->GetText() << endl;
        } else if (fault) {
            ERR_POST(Error << fault->GetFaultcode() << ": " << fault->GetFaultstring());
        }

        CConstRef<CVersionResponse> v2 = ws.GetVersion("C++ SOAP client",&fault);
        if (v2) {
            cout << v2->GetVersionStruct().GetMajor() << "." <<
                    v2->GetVersionStruct().GetMinor() << ":  " <<
                    v2->GetVersionStruct().GetClientID() << endl;
        } else if (fault) {
            ERR_POST(Error << fault->GetFaultcode() << ": " << fault->GetFaultstring());
        }

        CMath ops;
        CRef<COperand> op1(new COperand);
        op1->SetX(1);
        op1->SetY(2);
        op1->SetAttlist().SetOperation( COperand::C_Attlist::eAttlist_operation_add);
        ops.SetOperand().push_back(op1);
        CRef<COperand> op2(new COperand);
        op2->SetX(22);
        op2->SetY(11);
        op2->SetAttlist().SetOperation( COperand::C_Attlist::eAttlist_operation_subtract);
        ops.SetOperand().push_back(op2);

        CConstRef<CMathResponse> v3 = ws.DoMath(ops,&fault);
        if (v3) {
            for (auto i : v3->GetMathResult()) {
                cout << i << endl;
            }
        } else if (fault) {
            ERR_POST(Error << fault->GetFaultcode() << ": " << fault->GetFaultstring());
        }
    } catch (CEofException&) {
        ERR_POST(Error << "service unavailable");
        throw;
    } catch (CException&) {
        ERR_POST(Error << "request failed");
        throw;
    }
//    cout << "done" << endl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CSampleSoapClientApplication().AppMain(argc, argv);
}
