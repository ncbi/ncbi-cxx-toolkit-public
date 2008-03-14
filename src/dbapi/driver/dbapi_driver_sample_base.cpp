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
* Author:  Sergey Sikorskiy
*
* File Description:
*
*/


#include <ncbi_pch.hpp>

#include "dbapi_driver_sample_base.hpp"
#include <corelib/ncbiargs.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CDbapiDriverSampleApp::
//


CDbapiDriverSampleApp::CDbapiDriverSampleApp(const string& server_name,
                                             int tds_version) :
    CNcbiApplication(),
    m_DefaultServerName(server_name),
    m_DefaultTDSVersion(tds_version)
{
    return;
}


CDbapiDriverSampleApp::~CDbapiDriverSampleApp()
{
    return;
}


CDbapiDriverSampleApp::EServerType
CDbapiDriverSampleApp::GetServerType(void) const
{
    if ( GetServerName() == "STRAUSS" ||
         GetServerName() == "MOZART" ||
         GetServerName() == "SCHUMANN" ||
         GetServerName() == "OBERON" ||
         GetServerName() == "TAPER" ||
         GetServerName() == "THALBERG" ||
         NStr::StartsWith(GetServerName(), "BARTOK") ) {
        return eSybase;
    } else if (NStr::StartsWith(GetServerName(), "MS_DEV") ||
               NStr::StartsWith(GetServerName(), "MSSQL") ||
               NStr::StartsWith(GetServerName(), "MSDEV")
			   ) {
        return eMsSql;
    }

    return eUnknown;
}


void
CDbapiDriverSampleApp::Init()
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "DBAPI Sample Application");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString,
                            m_DefaultServerName);

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString,
                            "anyone");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString,
                            "allowed");

    arg_desc->AddDefaultKey("v", "version",
                            "TDS protocol version",
                            CArgDescriptions::eInteger,
                            NStr::IntToString(m_DefaultTDSVersion));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int
CDbapiDriverSampleApp::Run()
{

    const CArgs& args = GetArgs();

    // Get command-line arguments ...
    m_ServerName = args["S"].AsString();
    m_UserName   = args["U"].AsString();
    m_Password   = args["P"].AsString();
    m_TDSVersion = args["v"].AsInteger();

    return RunSample();
}


END_NCBI_SCOPE

