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

#include <memory>


#include <corelib/ncbiapp.hpp>

#include <serial/objistr.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

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
    virtual void Init(void);
    virtual int  Run(void);

    bool IsJuornalReport() const;
    bool IsUnpublishedReport() const;

    ESerialDataFormat GetSerialDataFormat() const;

    CNcbiIstream* GetInputStream();
    CNcbiOstream* GetOutputStream();

    CNcbiIfstream m_in_file;
    CNcbiOfstream m_out_file;
};


void CPubReportApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Publications Report");

    arg_desc->AddOptionalKey("i", "InFile", "Single Input File", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("o", "OutFile", "Single Output File", CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey("Q", "ReportType", "Report Type", CArgDescriptions::eString, "j");
    arg_desc->AddDefaultKey("b", "BinaryFormat", "Binary mode", CArgDescriptions::eBoolean, "F");

    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
}

CNcbiIstream* CPubReportApp::GetInputStream()
{
    CNcbiIstream* in = &cin;

    const CArgs& args = GetArgs();
    if (args["i"].HasValue()) {

        string input_file = args["i"].AsString();
        m_in_file.open(input_file);
        in = &m_in_file;
    }

    return in;
}

CNcbiOstream* CPubReportApp::GetOutputStream()
{
    CNcbiOstream* out = &cout;

    const CArgs& args = GetArgs();
    if (args["o"].HasValue()) {

        string output_file = args["o"].AsString();
        m_out_file.open(output_file);
        out = &m_out_file;
    }

    return out;
}

bool CPubReportApp::IsJuornalReport() const
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
    auto_ptr<CObjectIStream> in_obj(CObjectIStream::Open(format, *in));

    auto_ptr<CBaseReport> report;
    CRef<CSkipObjectHook> pub_hook;

    if (IsJuornalReport()) {

        auto_ptr<CJournalReport> journal_report(new CJournalReport(*out));
        pub_hook.Reset(new CSkipPubJournalHook(*journal_report));
        report = journal_report;
    }
    else if (IsUnpublishedReport()) {

        auto_ptr<CUnpublishedReport> unpub_report(new CUnpublishedReport(*out));
        pub_hook.Reset(new CSkipPubUnpublishedHook(*unpub_report));
        report = unpub_report;
    }
    else {
        NCBI_THROW(CArgHelpException, eHelp, kEmptyStr);
    }

    CObjectTypeInfo desc_type_info = CType<CSeqdesc>();
    desc_type_info.SetLocalSkipHook(*in_obj, pub_hook);

    CObjectTypeInfo seq_entry_type_info = CType<CSeq_entry>();
    CRef<CSkipSeqEntryHook> seq_entry_hook(new CSkipSeqEntryHook(*report));
    seq_entry_type_info.SetLocalSkipHook(*in_obj, seq_entry_hook);

    in_obj->Skip(CType<CBioseq_set>());

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
