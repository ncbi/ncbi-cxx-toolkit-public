
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
 *   Sample SOAP client demo
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/util_exception.hpp>
#include "sampleclient.hpp"

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CDemoApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CDemoApp::Init(void)
{
    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Program description
    string prog_description = "Test SampleSoapServer\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);
    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

    // enable the following two lines to see communication log:
//    SetDiagTrace(eDT_Enable);
//    CONNECT_Init();
}

int CDemoApp::Run(void)
{
    CSampleSoapClient ws;
    try {

        CConstRef<CDescriptionText> v1 = ws.GetDescription();
        if (bool(v1)) {
            cout << v1->GetText() << endl;
        }

        CConstRef<CVersionResponse> v2 = ws.GetVersion("C++ soap client");
        if (bool(v2)) {
            cout << v2->GetVersionStruct().GetMajor() << "." <<
                    v2->GetVersionStruct().GetMinor() << ":  " <<
                    v2->GetVersionStruct().GetClientID() << endl;
        }

        CMath ops;
        CRef<COperand> op1(new COperand);
        op1->SetX("1");
        op1->SetY("2");
        op1->SetAttlist().SetOperation( COperand::C_Attlist::eAttlist_operation_add);
        ops.SetOperand().push_back(op1);
        CRef<COperand> op2(new COperand);
        op2->SetX("22");
        op2->SetY("11");
        op2->SetAttlist().SetOperation( COperand::C_Attlist::eAttlist_operation_subtract);
        ops.SetOperand().push_back(op2);

        CConstRef<CMathResponse> v3 = ws.DoMath(ops);
        if (bool(v3)) {
            CMathResponse::TMathResult::const_iterator i;
            for (i = v3->GetMathResult().begin();
                 i != v3->GetMathResult().end(); ++i) {
                cout << *i << endl;
            }
                 
        }
    } catch (CEofException&) {
        cout << "service unavailable" << endl;
        throw;
    } catch (CException&) {
        cout << "request failed" << endl;
        throw;
    }
    cout << "done" << endl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CDemoApp().AppMain(argc, argv);
}

/* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/07/06 13:48:49  gouriano
* Initial revision: sample SOAP server and client
*
*
* ===========================================================================
*/
