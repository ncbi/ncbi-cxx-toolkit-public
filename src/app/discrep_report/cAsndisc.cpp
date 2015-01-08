/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   Main() of Cpp Discrepany Report, stand-alone asndisc application
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/submit/Seq_submit.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <dbapi/driver/drivers.hpp>


#include <misc/discrepancy_report/hDiscRep_config.hpp>

// for new library
#include <misc/discrepancy_report/analysis_workflow.hpp>
#include <util/format_guess.hpp>
#include <util/compress/stream_util.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(DiscRepNmSpc);

class CDiscRepApp : public CNcbiApplication
{
public:
    CDiscRepApp(void);

    virtual void Init(void);
    virtual int  Run (void);

protected:
    bool x_ValidateArgs(const CArgs& args);
    void x_ProcessConfigArgs(const CArgs& args);
    void x_ProcessOutputArgs(const CArgs& args);
    void x_ProcessFiles(const CArgs& args);
    void x_ProcessOneFile(const string& filename, const CArgs& args);
    void x_ProduceReport(const string& filename);

    string x_OutputFilenameFromInputFilename(const string& input, const CArgs& args);

    
    CAnalysisWorkflow m_Workflow;
    CReportOutputConfig m_OutputConfig;
    CReportOutputConfig m_SummaryConfig;
    CRef<CObjectManager> m_ObjMgr;
    CScope m_Scope;
    bool m_MakeSummaryReport;
};


CDiscRepApp::CDiscRepApp(void)
    : m_ObjMgr(CObjectManager::GetInstance()),
      m_Scope(*m_ObjMgr),
      m_MakeSummaryReport(true)
{

}


void CDiscRepApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                                "C++ Discrepancy Report");
    DisableArgDescriptions();

    // Pass argument descriptions to the application
    //
    arg_desc->AddDefaultKey("a", "Asn1Type", 
                 "Asn.1 Type: a: Any, e: Seq-entry, b: Bioseq, s: Bioseq-set, m: Seq-submit, t: Batch Bioseq-set, u: Batch Seq-submit, c: Catenated seq-entry",
                 CArgDescriptions::eString, "a");
    arg_desc->AddDefaultKey("b", "BatchBinary", 
                              "Batch File is Binary: 'T' = true, 'F' = false", 
                              CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("B", "BigSequenceReport", 
                             "Big Sequence Report: 'T' = true, 'F' = false",
                              CArgDescriptions::eBoolean, "F");
    arg_desc->AddOptionalKey("d", "DisableTests", 
                               "List of disabled tests, seperated by ','",
                               CArgDescriptions::eString);
    arg_desc->AddOptionalKey("e", "EnableTests", 
                              "List of enabled tests, seperated by ','",
                              CArgDescriptions::eString); 
    arg_desc->AddOptionalKey("i", "InputFile", "Single input file (mandatory)", 
                                CArgDescriptions::eString);
    arg_desc->AddOptionalKey("M", "MessageLevel", 
        "Output message level: 'a': add FATAL tags and output all messages, 'f': add FATAL tags and output FATAL messages only",
                                CArgDescriptions::eString);
    arg_desc->AddOptionalKey("o", "OutputFile", "Single output file",
                                CArgDescriptions::eString);
    arg_desc->AddOptionalKey("p", "InPath", "Path to ASN.1 Files", 
                                 CArgDescriptions::eString);
    arg_desc->AddOptionalKey("P", "ReportType",
                   "Report type: g - Genome, b - Big Sequence, m - MegaReport, t - Include FATAL Tag, s - FATAL Tag for Superuser, x - XML format",
                   CArgDescriptions::eString);
    arg_desc->AddOptionalKey("r", "OutPath", "Output Directory", 
                              CArgDescriptions::eString);
    arg_desc->AddDefaultKey("R", "Remote", 
                          "Allow GenBank data loader: 'T' = true, 'F' = false",
                           CArgDescriptions::eBoolean, "T");
    arg_desc->AddDefaultKey("s", "OutputFileSuffix", "Output File Suffix", 
                              CArgDescriptions::eString, ".dr");
    arg_desc->AddDefaultKey("S", "SummaryReport", 
                            "Summary Report: 'T'=true, 'F' =false", 
                            CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("u", "Recurse", "Recurse", 
                               CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("x", "Suffix", "File Selection Substring", 
                              CArgDescriptions::eString, ".sqn");
    arg_desc->AddOptionalKey("X", "ExpandCategories", 
         "Expand Report Categories (comma-delimited list of test names or ALL)",
                                CArgDescriptions::eString); 
    arg_desc->AddOptionalKey("L", "Lineage to use",
        "Use this text in place of lineage when testing for eukaryote etc.",
                             CArgDescriptions::eString);
    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
};


bool CDiscRepApp :: x_ValidateArgs(const CArgs& args)
{
    bool rval = true;

    if (!args["p"] && !args["i"]) {
        ERR_POST("Input path or input file must be specified");
        rval = false;
    }

    if (args["e"] && args["d"]) {
        ERR_POST("Cannot specify both -e and -d.  Choose -e to enable only a few tests and disable the rest, choose -d to disable only a few tests and enable the rest.");
        rval = false;
    }

    if (args["B"] && args["B"].AsBoolean() && args["P"] && NStr::Find(args["P"].AsString(), "b") == string::npos) {
        ERR_POST("Cannot combine -B with another report type");
        rval = false;
    }

    return rval;
}


void CDiscRepApp :: x_ProcessConfigArgs(const CArgs& args)
{
    if (args["B"] && args["B"].AsBoolean()) {
        m_Workflow.Configure(DiscRepNmSpc::CReportConfig::eConfigTypeBigSequences);
    } else if (args["P"]) {
        string report_type = args["P"].AsString();
        if (NStr::Find(report_type, "g") != string::npos) {
            m_Workflow.Configure(DiscRepNmSpc::CReportConfig::eConfigTypeGenomes);
        } else if (NStr::Find(report_type, "b") != string::npos) {
            m_Workflow.Configure(DiscRepNmSpc::CReportConfig::eConfigTypeBigSequences);
        } else if (NStr::Find(report_type, "m") != string::npos) {
            m_Workflow.Configure(DiscRepNmSpc::CReportConfig::eConfigTypeMegaReport);
        }
    }

    if (args["e"]) {
    }

    if (args["L"]) {
        m_Workflow.SetConfig().SetDefaultLineage(args["L"].AsString());
    }

    m_Workflow.BuildTestList();
}


void CDiscRepApp :: x_ProcessOutputArgs(const CArgs& args)
{
    // add (or do not add) tags
    m_OutputConfig.SetAddTag(false);
    m_SummaryConfig.SetAddTag(false);
    m_OutputConfig.SetFormat(CReportOutputConfig::eFormatText);
    m_SummaryConfig.SetFormat(CReportOutputConfig::eFormatText);
    
    if (args["P"]) {
        string report_type = args["P"].AsString();
        if (NStr::Find(report_type, "t") != string::npos) {
            m_OutputConfig.SetAddTag(true);
            m_SummaryConfig.SetAddTag(true);
        }
        if (NStr::Find(report_type, "x") != string::npos) {
            m_OutputConfig.SetFormat(CReportOutputConfig::eFormatXML);
            m_SummaryConfig.SetFormat(CReportOutputConfig::eFormatXML);
        }
    }

    // Set up expanded test configuration
    m_OutputConfig.ClearExpanded();
    m_SummaryConfig.ClearExpanded();
    if (args["X"]) {
        string expanded = args["X"].AsString();
        vector<string> tests;
        NStr::Tokenize(expanded, ",", tests);
        ITERATE(vector<string>, it, tests) {
            m_OutputConfig.SetExpand(*it, true);
        }
    }
    // Always expand for kDISC_SOURCE_QUALS_ASNDISC
    m_OutputConfig.SetExpand(kDISC_SOURCE_QUALS_ASNDISC);

    m_OutputConfig.SetLabelSub(true);
    m_SummaryConfig.SetLabelSub(false);

    // when to show object list
    m_SummaryConfig.SetSuppressObjectList(true);
    m_OutputConfig.ClearListObjects();
    m_OutputConfig.SetListObjects(kDISC_SOURCE_QUALS_ASNDISC, false);

    m_MakeSummaryReport = true;
}


void CDiscRepApp :: x_ProduceReport(const string& filename)
{
    CNcbiOfstream ostr(filename);

    m_Workflow.Collate();
    DiscRepNmSpc::TReportItemList list = m_Workflow.GetResults();

    if (m_OutputConfig.GetFormat() == CReportOutputConfig::eFormatText) {
        ostr << "Discrepancy Report Results\n\n";

        if (m_MakeSummaryReport) {
            ostr << "Summary\n";
            ITERATE(DiscRepNmSpc::TReportItemList, it, list) {
                string out = (*it)->Format(m_SummaryConfig);
                ostr << out;
            }
            ostr << "\nDetailed Report\n\n";
        }
    
        ITERATE(DiscRepNmSpc::TReportItemList, it, list) {
            string out = (*it)->Format(m_OutputConfig);
            ostr << out;
        }
    } else {
        xml::document xmldoc("asndisc");
        xml::node& xml_root = xmldoc.get_root_node();

        ITERATE(DiscRepNmSpc::TReportItemList, it, list) {
            (*it)->FormatXML(m_OutputConfig, xml_root);
        }

        xmldoc.set_is_standalone(true);
        xmldoc.set_encoding("UTF-8");

        ostr << xmldoc;

    }
    ostr.flush();
    ostr.close();
    m_Workflow.ResetData();
}


auto_ptr<CObjectIStream> OpenUncompressedStream(const string& fname)
{
    auto_ptr<CNcbiIstream> InputStream(new CNcbiIfstream (fname.c_str(), ios::binary));
    CCompressStream::EMethod method;
    
    CFormatGuess::EFormat format = CFormatGuess::Format(*InputStream);
    switch (format)
    {
    case CFormatGuess::eGZip:  method = CCompressStream::eGZipFile;  break;
    case CFormatGuess::eBZip2: method = CCompressStream::eBZip2;     break;
    case CFormatGuess::eLzo:   method = CCompressStream::eLZO;       break;
    default:                   method = CCompressStream::eNone;      break;
    }
    if (method != CCompressStream::eNone)
    {
        CDecompressIStream* decompress(new CDecompressIStream(*InputStream, method, CCompressStream::fDefault, eTakeOwnership));
        InputStream.release();
        InputStream.reset(decompress);
        format = CFormatGuess::Format(*InputStream);
    }

    auto_ptr<CObjectIStream> objectStream;
    switch (format)
    {
        case CFormatGuess::eBinaryASN:
        case CFormatGuess::eTextASN:
            objectStream.reset(CObjectIStream::Open(format==CFormatGuess::eBinaryASN ? eSerial_AsnBinary : eSerial_AsnText, *InputStream, eTakeOwnership));
            InputStream.release();
            break;
        default:
            break;
    }
    return objectStream;
}



void CDiscRepApp :: x_ProcessOneFile(const string& filename, const CArgs& args)
{
    if (!CFile(filename).Exists()) {
        NCBI_USER_THROW("File not found: " + filename);
    }

    auto_ptr<CObjectIStream> instr = OpenUncompressedStream(filename);
    if (!instr.get()) {
        NCBI_USER_THROW("Unable to read from file: " + filename);
    }

    string file_type = "a";
    if (args["a"]) {
        file_type = args["a"].AsString();
    }

    // Process file based on its content
    // Unless otherwise specified we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    // ASN.1 Type (a Automatic, z Any, e Seq-entry, b Bioseq, s Bioseq-set, m Seq-submit, t Batch Bioseq-set, u Batch Seq-submit",
    string header = instr->ReadFileHeader();
    if (header.empty() && !file_type.empty())
    {
        switch (file_type[0])
        {
        case 'e':
            header = "Seq-entry";
            break;
        case 'm':
            header = "Seq-submit";
            break;
        case 's':
            header = "Bioseq-set";
            break;
        case 'b':
            header = "Bioseq";
            break;
        }
    }

    if (header == "Seq-submit" ) {  // Seq-submit
        // Get seq-submit to validate
        CRef<CSeq_submit> ss(new CSeq_submit);
        instr->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);
        if (ss->IsSetData() && ss->GetData().IsEntrys()) {
            ITERATE(CSeq_submit::TData::TEntrys, it, ss->GetData().GetEntrys()) {
                CSeq_entry_Handle seh = m_Scope.AddTopLevelSeqEntry(**it);
                m_Workflow.CollectData(seh, true, filename);
                m_Scope.RemoveTopLevelSeqEntry(seh);
            }
        }
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        CRef<CSeq_entry> se(new CSeq_entry);
        instr->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);
        CSeq_entry_Handle seh = m_Scope.AddTopLevelSeqEntry(*se);
        m_Workflow.CollectData(seh, true, filename);
        m_Scope.RemoveTopLevelSeqEntry(seh);
    } else if (header == "Bioseq-set" ) {           // Bioseq-set
        CRef<CBioseq_set> set(new CBioseq_set);
        instr->Read(ObjectInfo(*set), CObjectIStream::eNoFileHeader);
        CRef<CSeq_entry> se(new CSeq_entry());
        se->SetSet().Assign(*set);
        CSeq_entry_Handle seh = m_Scope.AddTopLevelSeqEntry(*se);
        m_Workflow.CollectData(seh, true, filename);
        m_Scope.RemoveTopLevelSeqEntry(seh);
    } else if (header == "Bioseq" ) {               // Bioseq
        CRef<CBioseq> seq(new CBioseq);
        instr->Read(ObjectInfo(*seq), CObjectIStream::eNoFileHeader);
        CRef<CSeq_entry> se(new CSeq_entry());
        se->SetSeq().Assign(*seq);
        CSeq_entry_Handle seh = m_Scope.AddTopLevelSeqEntry(*se);
        m_Workflow.CollectData(seh, true, filename);
        m_Scope.RemoveTopLevelSeqEntry(seh);
    } else {
        instr->Close();
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }

    instr->Close();
}


string CDiscRepApp :: x_OutputFilenameFromInputFilename(const string& input, const CArgs& args)
{
    string output = input;
    size_t pos = NStr::Find(output, ".", 0, string::npos, NStr::eLast);
    if (pos != string::npos) {
        output = output.substr(0, pos);
    }
    string suffix = ".dr";
    if (args["s"]) {
        suffix = args["s"].AsString();
    }
    output += suffix;
    return output;
}


void CDiscRepApp :: x_ProcessFiles(const CArgs& args)
{
    string outputfile = "";
    bool   make_single_report = false;

    string output_file = "";
    if (args["o"]) {
        output_file = args["o"].AsString();
        make_single_report = true;
    }

    x_ProcessOutputArgs(args);        
        
    // read input file/path and perform tests

    // for now, just handle single input file
    if (args["i"]) {
        string filename = args["i"].AsString();
        x_ProcessOneFile(filename, args);  
        if (NStr::IsBlank(output_file)) {
            output_file = x_OutputFilenameFromInputFilename(filename, args);
        }
        make_single_report = true;
    } else {
        CDir dir(args["p"].AsString());
        CDir::TGetEntriesFlags flag = (args["u"] && args["u"].AsBoolean()) ? 0 : CDir::fIgnoreRecursive ;
        list <AutoPtr <CDirEntry> > entries = dir.GetEntries(args["x"] ? "*" + args["x"].AsString() : kEmptyStr, flag);
        ITERATE (list <AutoPtr <CDirEntry> >, it, entries) {
            string filename = (*it)->GetDir() + (*it)->GetName();
            x_ProcessOneFile(filename, args);
            if (!make_single_report) {
                output_file = x_OutputFilenameFromInputFilename(filename, args);
                x_ProduceReport(output_file);
            }
        }
    }

    if (make_single_report) {
        x_ProduceReport(output_file);
    }
}


int CDiscRepApp :: Run(void)
{
    // Crocess command line args:  get GI to load
    const CArgs& args = GetArgs();

#ifdef HAVE_PUBSEQ_OS
    // we may require PubSeqOS readers at some point, so go ahead and make
    // sure they are properly registered
    GenBankReaders_Register_Pubseq();
    GenBankReaders_Register_Pubseq2();
    DBAPI_RegisterDriver_FTDS();
#endif

#if 1
    int rval = 1;
    // TODO: read in disc_report.ini
    // Process input arguments
    if (!x_ValidateArgs(args)) {
        rval = 0;
    } else {
        x_ProcessConfigArgs(args);
        
        x_ProcessFiles(args);

    }
    
#else
    try {
       CRef <DiscRepNmSpc::CRepConfig> 
           config( DiscRepNmSpc::CRepConfig :: factory(fAsndisc) );
       CRef <IRWRegistry> reg(0);
       if (CFile("disc_report.ini").Exists()) {
           CMetaRegistry::SEntry 
                 entry = CMetaRegistry :: Load("disc_report.ini");
           reg.Reset(entry.registry);
       }
       config->InitParams(reg);
       config->ReadArgs(args);
       config->CollectTests();
       config->Run();

       return 0;
   }
   catch (CException& eu) {
       string err_msg(eu.GetMsg());
       if (err_msg == "Input path or input file must be specified") {
           err_msg = "You need to supply at least an input file (-i) or a path in which to find input files (-p). Please see 'asndisc -help' for additional details.";
       }
       ERR_POST( err_msg);
       return 1;
    }
#endif
   return rval;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
//  SetDiagTrace(eDT_Enable);
   SetDiagPostLevel(eDiag_Error);

   try {
     return CDiscRepApp().AppMain(argc, argv);
   }
   catch(CException& eu) {
      ERR_POST( eu.GetMsg());
      return 1;
   }
}
