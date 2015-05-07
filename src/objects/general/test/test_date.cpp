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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   Test for CDate_std::GetDate()
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objects/general/Date_std.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CTestDateApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CTestDateApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "test of CDate_std::GetDate()");
    
    arg_desc->AddKey("f", "format", "date format", CArgDescriptions::eString);
    arg_desc->AddKey("Y", "year",   "year number", CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("M", "month", "month number",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("D", "day", "day number",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("S", "season", "season name",
                             CArgDescriptions::eString);
    // hour, minute, second?
    SetupArgDescriptions(arg_desc.release());
}


int CTestDateApp::Run(void)
{
    const CArgs& args = GetArgs();
    CDate      date;
    CDate_std& std  = date.SetStd();
    std.SetYear(args["Y"].AsInteger());
    if (args["M"]) {
        std.SetMonth(args["M"].AsInteger());
    }
    if (args["D"]) {
        std.SetDay(args["D"].AsInteger());
    }
    if (args["S"]) {
        std.SetSeason(args["S"].AsString());
    }
    // ...

    string s;
    date.GetDate(&s);
    NcbiCout << '\"' << s << '\"' << NcbiEndl;
    s.erase();
    date.GetDate(&s, args["f"].AsString());
    NcbiCout << '\"' << s << '\"' << NcbiEndl;
    s.erase();
    date.GetDate(&s, "%Y-%M-%D");
    NcbiCout << '\"' << s << '\"' << NcbiEndl;

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestDateApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
