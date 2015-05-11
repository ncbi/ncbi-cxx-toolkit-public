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
 * Author: Sema
 *
 * File Description:
 *   Main() of Cpp Discrepany Report, stand-alone asndisc application
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <misc/discrepancy/discrepancy.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <serial/objistr.hpp>
#include <util/format_guess.hpp>
#include <util/compress/stream_util.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(NDiscrepancy);
USING_SCOPE(objects);


class CDiscRepApp : public CNcbiApplication
{
public:
    CDiscRepApp(void);
    virtual void Init(void);
    virtual int  Run (void);

protected:
    string x_ConstructOutputName(const string& input);
    vector<CSeq_entry_Handle> x_ReadFile(const string& filename);
    void x_ParseDirectory(const string& name, bool recursive);
    void x_ProcessFile(const string& filename);
    void x_ProcessAll(const string& outname);
    void x_Output(const string& filename, const vector<CRef<CDiscrepancyCase> >& tests);

    CScope m_Scope;
    string m_SuspectRules;
    string m_Lineage;   // override lineage
    vector<string> m_Files;
    vector<string> m_Tests;
};


CDiscRepApp::CDiscRepApp(void) :
    m_Scope(*CObjectManager::GetInstance())
{
}


class CDiscRepArgDescriptions : public CArgDescriptions
{
public:
    virtual string& PrintUsage(string& str, bool detailed = false) const;
};


string& CDiscRepArgDescriptions::PrintUsage(string& str, bool detailed) const
{
    CArgDescriptions::PrintUsage(str, detailed);
    if (detailed)
    {
        str+="TESTS\n";
        const vector<string> Name = GetDiscrepancyNames();
        ITERATE(vector<string>, nm, Name) {
            str += "   ";
            str += *nm;
            const vector<string> Alias = GetDiscrepancyAliases(*nm);
            ITERATE(vector<string>, al, Alias) {
                str += " / ";
                str += *al;
            }
            str += "\n";
        }
    }
    return str;
}


void CDiscRepApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CDiscRepArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Discrepancy Report");

    arg_desc->AddOptionalKey("i", "InFile", "Single Input File", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("p", "Directory", "Path to ASN.1 Files", CArgDescriptions::eInputFile);
    arg_desc->AddFlag("u", "Recurse");
    arg_desc->AddOptionalKey("o", "OutFile", "Single Output File", CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("s", "OutputFileSuffix", "Output File Suffix", CArgDescriptions::eString, ".dr");
    arg_desc->AddDefaultKey("x", "Suffix", "File Selection Substring", CArgDescriptions::eString, ".sqn");
    arg_desc->AddOptionalKey("r", "OutPath", "Output Directory", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("e", "EnableTests", "List of enabled tests, seperated by ','", CArgDescriptions::eString); 
    arg_desc->AddOptionalKey("d", "DisableTests",  "List of disabled tests, seperated by ','", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("a", "Asn1Type", "Asn.1 Type: a: Any, e: Seq-entry, b: Bioseq, s: Bioseq-set, m: Seq-submit, t: Batch Bioseq-set, u: Batch Seq-submit, c: Catenated seq-entry", CArgDescriptions::eString);
    // use CArgAllow_Strings
    arg_desc->AddFlag("S", "SummaryReport");

    arg_desc->AddOptionalKey("w", "SuspectProductFile", "Suspect product rule file name", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("L", "LineageToUse", "Default lineage", CArgDescriptions::eString);

/*
    arg_desc->AddOptionalKey("M", "MessageLevel", 
        "Output message level: 'a': add FATAL tags and output all messages, 'f': add FATAL tags and output FATAL messages only",
                                CArgDescriptions::eString);
    arg_desc->AddOptionalKey("P", "ReportType",
                   "Report type: g - Genome, b - Big Sequence, m - MegaReport, t - Include FATAL Tag, s - FATAL Tag for Superuser, x - XML format",
                   CArgDescriptions::eString);
    arg_desc->AddDefaultKey("R", "Remote", 
                          "Allow GenBank data loader: 'T' = true, 'F' = false",
                           CArgDescriptions::eBoolean, "T");
    arg_desc->AddOptionalKey("X", "ExpandCategories", 
         "Expand Report Categories (comma-delimited list of test names or ALL)",
                                CArgDescriptions::eString); 
*/
    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
};


string CDiscRepApp::x_ConstructOutputName(const string& input)
{
    const CArgs& args = GetArgs();
    CDirEntry fname(input);
    string path = args["r"] ? args["r"].AsString() : fname.GetDir();
    string ext = args["s"] ? args["s"].AsString() : ".dr";
    return CDirEntry::MakePath(path, fname.GetBase(), ext);
}


auto_ptr<CObjectIStream> OpenUncompressedStream(const string& fname) // JIRA: CXX open the ticket 
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
        InputStream.reset(new CDecompressIStream(*InputStream.release(), method, CCompressStream::fDefault, eTakeOwnership));
        format = CFormatGuess::Format(*InputStream);
    }

    auto_ptr<CObjectIStream> objectStream;
    switch (format)
    {
        case CFormatGuess::eBinaryASN:
        case CFormatGuess::eTextASN:
            objectStream.reset(CObjectIStream::Open(format==CFormatGuess::eBinaryASN ? eSerial_AsnBinary : eSerial_AsnText, *InputStream.release(), eTakeOwnership));
            break;
        default:
            break;
    }
    return objectStream;
}


vector<CSeq_entry_Handle> CDiscRepApp::x_ReadFile(const string& fname)
{
    vector<CSeq_entry_Handle> seh;
    auto_ptr<CObjectIStream> in = OpenUncompressedStream(fname);
    string header = in->ReadFileHeader();
    if (header.empty() && GetArgs()["a"]) {
        string type = GetArgs()["a"].AsString();
        if (type == "e") header = "Seq-entry";
        else if (type == "m") header = "Seq-submit";
        else if (type == "s") header = "Bioseq-set";
        else if (type == "b") header = "Bioseq";
    }
    if (header == "Seq-submit" ) {
        CRef<CSeq_submit> ss(new CSeq_submit);
        in->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);
        if (ss->IsSetData() && ss->GetData().IsEntrys()) {
            ITERATE(CSeq_submit::TData::TEntrys, it, ss->GetData().GetEntrys()) {
                seh.push_back(m_Scope.AddTopLevelSeqEntry(**it));
            }
        }
    } else if (header == "Seq-entry") {
        CRef<CSeq_entry> se(new CSeq_entry);
        in->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);
        seh.push_back(m_Scope.AddTopLevelSeqEntry(*se));
    } else if (header == "Bioseq-set" ) {
        CRef<CBioseq_set> set(new CBioseq_set);
        in->Read(ObjectInfo(*set), CObjectIStream::eNoFileHeader);
        CRef<CSeq_entry> se(new CSeq_entry());
        se->SetSet().Assign(*set);
        seh.push_back(m_Scope.AddTopLevelSeqEntry(*se));
    } else if (header == "Bioseq" ) {
        CRef<CBioseq> seq(new CBioseq);
        in->Read(ObjectInfo(*seq), CObjectIStream::eNoFileHeader);
        CRef<CSeq_entry> se(new CSeq_entry());
        se->SetSeq().Assign(*seq);
        seh.push_back(m_Scope.AddTopLevelSeqEntry(*se));
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }

    return seh;
}


void CDiscRepApp::x_ParseDirectory(const string& name, bool recursive)
{
    CDir Dir(name);
    string ext = GetArgs()["x"].AsString();
    if (!Dir.Exists() || !Dir.IsDir()) return;
    CDir::TEntries Entries = Dir.GetEntries();
    ITERATE(CDir::TEntries, entry, Entries) {
        if ((*entry)->GetName() == "." || (*entry)->GetName() == "..") continue;
        if (recursive && (*entry)->IsDir()) x_ParseDirectory((*entry)->GetPath(), true);
        if ((*entry)->IsFile() && (*entry)->GetExt() == ext) m_Files.push_back((*entry)->GetPath());
    }
}


void CDiscRepApp::x_ProcessFile(const string& fname)
{
    CRef<CDiscrepancySet> Tests = CDiscrepancySet::New(m_Scope);
    ITERATE(vector<string>, tname, m_Tests) {
        Tests->AddTest(*tname);
    }

    Tests->SetSuspectRules(m_SuspectRules);
    Tests->SetLineage(m_Lineage);

    vector<CSeq_entry_Handle> seh = x_ReadFile(fname);
    ITERATE(vector<CSeq_entry_Handle>, sh, seh) {
        Tests->Parse(*sh);
        m_Scope.RemoveTopLevelSeqEntry(*sh);
    }
    Tests->Summarize();
    x_Output(x_ConstructOutputName(fname), Tests->GetTests());
}


void CDiscRepApp::x_ProcessAll(const string& outname)
{
    CRef<CDiscrepancySet> Tests = CDiscrepancySet::New(m_Scope);
    ITERATE(vector<string>, tname, m_Tests) {
        Tests->AddTest(*tname);
    }

    Tests->SetSuspectRules(m_SuspectRules);
    Tests->SetLineage(m_Lineage);

    ITERATE(vector<string>, fname, m_Files) {
        vector<CSeq_entry_Handle> seh = x_ReadFile(*fname);
        ITERATE(vector<CSeq_entry_Handle>, sh, seh) {
            Tests->SetFile(*fname);
            Tests->Parse(*sh);
            m_Scope.RemoveTopLevelSeqEntry(*sh);
        }
    }
    Tests->Summarize();
    x_Output(outname, Tests->GetTests());
}


void CDiscRepApp::x_Output(const string& filename, const vector<CRef<CDiscrepancyCase> >& tests)
{
    bool summary = GetArgs()["S"].AsBoolean();

    CNcbiOfstream out(filename.c_str(), ofstream::out);
    out << "Discrepancy Report Results\n\n";

    out << "Summary\n";
    ITERATE(vector<CRef<CDiscrepancyCase> >, tst, tests) {
        TReportItemList rep = (*tst)->GetReport();
        ITERATE(TReportItemList, it, rep) {
            out << (*it)->GetTitle() << ": " << (*it)->GetMsg() << "\n";
        }
    }
    if (summary) return;

    out << "\nDetailed Report\n\n";
    ITERATE(vector<CRef<CDiscrepancyCase> >, tst, tests) {
        TReportItemList rep = (*tst)->GetReport();
        ITERATE(TReportItemList, it, rep) {
            out << (*it)->GetTitle() << ": " << (*it)->GetMsg() << "\n";
            cout << (*it)->GetTitle() << ": " << (*it)->GetMsg() << "\n";        // TODO: remove from the final version
            TReportObjectList det = (*it)->GetDetails();
            ITERATE(TReportObjectList, obj, det) {
                out << (*obj)->GetText() << "\n";
                cout << (*obj)->GetText() << "\n";  // TODO: remove from the final version
            }
            out << "\n";
            cout << "\n";                           // TODO: remove from the final version
        }
    }
}


int CDiscRepApp::Run(void)
{
    const CArgs& args = GetArgs();

    // constructing the test list
    set<string> Tests;
    if (args["e"]) {
        list<string> List;
        NStr::Split(args["e"].AsString(), ", ", List);
        ITERATE(list<string>, s, List) {
            string name = GetDiscrepancyCaseName(*s);
            if (name.empty()) {
                ERR_POST("Test name not found: " + *s);
                return 1;
            } else {
                Tests.insert(name);
            }
        }
    } else {
        vector<string> AllTests = GetDiscrepancyNames();
        copy(AllTests.begin(), AllTests.end(), inserter(Tests, Tests.begin()));
    }
    if (args["d"]) {
        list<string> List;
        NStr::Split(args["d"].AsString(), ", ", List);
        ITERATE(list<string>, s, List) {
            string name = GetDiscrepancyCaseName(*s);
            if (name.empty()) {
                ERR_POST("Test name not found: " + *s);
                return 1;
            } else {
                Tests.erase(name);
            }
        }
    }
    copy(Tests.begin(), Tests.end(), back_inserter(m_Tests));
    if (m_Tests.empty()) {
        ERR_POST("Empty test list");
        return 1;
    }

    // input files
    if (args["i"]) m_Files.push_back(args["i"].AsString());
    if (args["p"]) x_ParseDirectory(args["p"].AsString(), args["u"].AsBoolean());
    if (m_Files.empty()) {
        ERR_POST("No input files");
        return 1;
    }

    // set defaults
    if(args["w"]) m_SuspectRules = args["w"].AsString();
    if(args["L"]) m_Lineage = args["L"].AsString();

    // run tests
    if (args["o"]) {
        x_ProcessAll(args["o"].AsString());
    } else {
        ITERATE(vector<string>, f, m_Files) {
            x_ProcessFile(*f);
        }
    }
   return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CDiscRepApp().AppMain(argc, argv);
}
