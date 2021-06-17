/*  
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>

#include <memory>


#include <corelib/ncbiapp.hpp>

#include <serial/objistr.hpp>

#include <objects/general/Date_std.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include "journal_report.hpp"
#include "journal_hook.hpp"
#include "unpub_hook.hpp"
#include "unpub_report.hpp"
#include "seq_entry_hook.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace pub_report
{

class CPubReportApp : public CNcbiApplication
{
public:
    CPubReportApp()
    {
        SetVersionByBuild(0);
    }

private:
    virtual void Init(void);
    virtual int  Run(void);

    bool IsJournalReport() const;
    bool IsUnpublishedReport() const;

    ESerialDataFormat GetSerialDataFormat() const;

    int GetMaxDateCheck() const;

    CNcbiIstream* GetInputStream();
    CNcbiOstream* GetOutputStream();
};


void CPubReportApp::Init(void)
{
    // Prepare command line descriptions
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Publications Report");

    arg_desc->AddOptionalKey("i", "InFile", "Single Input File", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("o", "OutFile", "Single Output File", CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey("Q", "ReportType", "Report Type", CArgDescriptions::eString, "j");
    arg_desc->AddDefaultKey("b", "BinaryFormat", "Binary mode", CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("max-date-check", "MaxDateCheck", "Maximal amount of years from the publication date", CArgDescriptions::eInteger, "1");

    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
}

CNcbiIstream* CPubReportApp::GetInputStream()
{
    CNcbiIstream* in = &cin;

    const CArgs& args = GetArgs();
    if (args["i"].HasValue()) {
        in = &args["i"].AsInputFile();
    }

    return in;
}

CNcbiOstream* CPubReportApp::GetOutputStream()
{
    CNcbiOstream* out = &cout;

    const CArgs& args = GetArgs();
    if (args["o"].HasValue()) {
        out = &args["o"].AsOutputFile();
    }

    return out;
}

int CPubReportApp::GetMaxDateCheck() const
{
    int ret = 0;

    const CArgs& args = GetArgs();
    if (args["max-date-check"].HasValue()) {
        ret = args["max-date-check"].AsInteger();
    }

    return ret;
}

bool CPubReportApp::IsJournalReport() const
{
    return GetArgs()["Q"].AsString() == "j";
}

bool CPubReportApp::IsUnpublishedReport() const
{
    return GetArgs()["Q"].AsString() == "u";
}

ESerialDataFormat CPubReportApp::GetSerialDataFormat() const
{
    return GetArgs()["b"].AsBoolean() ? eSerial_AsnBinary : eSerial_AsnText;
}

int CPubReportApp::Run(void)
{
    CNcbiIstream* in = GetInputStream();
    CNcbiOstream* out = GetOutputStream();

    ESerialDataFormat format = GetSerialDataFormat();
    unique_ptr<CObjectIStream> in_obj(CObjectIStream::Open(format, *in));

    shared_ptr<CBaseReport> report;
    CRef<CSkipObjectHook> pub_hook;

    if (IsJournalReport()) {

        shared_ptr<CJournalReport> journal_report(new CJournalReport(*out));
        pub_hook.Reset(new CSkipPubJournalHook(*journal_report));
        report = journal_report;
    }
    else if (IsUnpublishedReport()) {

        shared_ptr<CUnpublishedReport> unpub_report(new CUnpublishedReport(*out, GetMaxDateCheck()));
        pub_hook.Reset(new CSkipPubUnpublishedHook(*unpub_report));
        report = unpub_report;
    }
    else {
        NCBI_THROW(CArgHelpException, eHelp, kEmptyStr);
    }

    CObjectTypeInfo pub_type_info = CType<CPub_equiv>();
    pub_type_info.SetLocalSkipHook(*in_obj, pub_hook);

    CObjectTypeInfo seq_entry_type_info = CType<CSeq_entry>();
    CRef<CSkipSeqEntryHook> seq_entry_hook(new CSkipSeqEntryHook(*report));
    seq_entry_type_info.SetLocalSkipHook(*in_obj, seq_entry_hook);

    // supported types
    set<TTypeInfo> known_types;
    known_types.insert(CBioseq_set::GetTypeInfo());
    known_types.insert(CSeq_submit::GetTypeInfo());
    known_types.insert(CSeq_entry::GetTypeInfo());

    set<TTypeInfo> types_found = in_obj->GuessDataType(known_types);
    string strObjType;
    if (types_found.size() == 1) {
        const TTypeInfo& objtype = *(types_found.begin());
        strObjType = objtype->GetName();
        if (strObjType == "Bioseq-set") {
            in_obj->Skip(CType<CBioseq_set>());
        }
        else if (strObjType == "Seq-submit") {
            in_obj->Skip(CType<CSeq_submit>());
        }
        else if (strObjType == "Seq-entry") {
            in_obj->Skip(CType<CSeq_entry>());
        }
        else {
            // not supported type
            strObjType.clear();
        }
    }
    
    if (strObjType.empty()) {
        NCBI_THROW(CException, eUnknown,
            "Failed to determine object type from the file: " + GetArgs()["i"].AsString());
    }

    report->CompleteReport();

    return 0;
}

} // namespace pub_report


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return pub_report::CPubReportApp().AppMain(argc, argv);
}
