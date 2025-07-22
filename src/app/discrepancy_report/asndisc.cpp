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
#include <common/ncbi_source_ver.h>
#include <corelib/ncbiapp.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>
#include <misc/discrepancy/discrepancy.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <util/format_guess.hpp>
#include <util/compress/stream_util.hpp>
#include <util/line_reader.hpp>
#include <corelib/rwstream.hpp>
#include <util/multi_writer.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(NDiscrepancy);
USING_SCOPE(objects);


class CDiscRepApp : public CNcbiApplication
{
public:
    CDiscRepApp();
    void Init() override;
    int Run() override;

protected:
    string x_ConstructOutputName(const string& input);
    string x_DefaultHeader();
    void x_ProcessFile(const string& filename, CDiscrepancySet& tests);
    void x_ParseDirectory(const string& name, bool recursive);
    unsigned x_ProcessOne(const string& filename);
    unsigned x_ProcessAll(const string& outname);
    void x_Output(const string& fname, CDiscrepancyProduct& tests, unsigned short flags);
    void x_Autofix(CDiscrepancySet& tests);

    CRef<CScope> m_Scope;
    string m_SuspectRules;
    string m_Lineage;   // override lineage
    vector<string> m_Files;
    TTestNamesSet m_Tests;

    char m_Group{0};
    bool m_SuspectProductNames{false};
    bool m_Ext{false};
    bool m_Fat{false};
    bool m_AutoFix{false};
    bool m_Xml{false};
    bool m_Print{false};
};


CDiscRepApp::CDiscRepApp()
{
    SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
}


class CDiscRepArgDescriptions : public CArgDescriptions
{
public:
    string& PrintUsage(string& str, bool detailed = false) const override;
};


string& CDiscRepArgDescriptions::PrintUsage(string& str, bool detailed) const
{
    CArgDescriptions::PrintUsage(str, detailed);
    if (detailed) {
        str += "TESTS\n";
        auto names = GetDiscrepancyTests(EGroup::eAll);
        for (auto nm : names) {
            str += "   ";
            str += GetDiscrepancyCaseName(nm);
            auto aliases = GetDiscrepancyAliases(nm);
            for (auto al : aliases) {
                str += " / ";
                str += al;
            }
            str += "\n";
        }
    }
    return str;
}


static void PrintMsg(EDiagSev sev, const string& msg)
{
    if (sev == eDiag_Error)
        cerr << "Error: ";
    cerr << msg << endl;
}


void CDiscRepApp::Init()
{
    // Prepare command line descriptions
    unique_ptr<CArgDescriptions> arg_desc(new CDiscRepArgDescriptions);
    arg_desc->SetUsageContext("", "Discrepancy Report");

    arg_desc->AddOptionalKey("i", "InFile", "Single Input File", CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("x", "Suffix", "File Selection Substring", CArgDescriptions::eString, ".sqn");
    arg_desc->AddOptionalKey("indir", "InputDirectory", "Path to ASN.1 Files", CArgDescriptions::eInputFile);
    arg_desc->AddFlag("u", "Recurse");

    arg_desc->AddOptionalKey("o", "OutFile", "Single Output File", CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey("s", "OutputFileSuffix", "Output File Suffix", CArgDescriptions::eString, ".dr");
    arg_desc->AddOptionalKey("outdir", "OutputDirectory", "Output Directory", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("e", "EnableTests", "List of enabled tests, separated by ','", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("d", "DisableTests", "List of disabled tests, separated by ','", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("a", "Asn1Type", "Asn.1 Type: a: Any, e: Seq-entry, b: Bioseq, s: Bioseq-set, m: Seq-submit, t: Batch Bioseq-set, u: Batch Seq-submit, c: Catenated seq-entry", CArgDescriptions::eString);
    // use CArgAllow_Strings
    arg_desc->AddFlag("S", "SummaryReport");

    arg_desc->AddOptionalKey("w", "SuspectProductFile", "Suspect product rule file name", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("L", "LineageToUse", "Default lineage", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("X", "ExpandCategories", "Expand Report Categories (comma-delimited list of test names or ALL)", CArgDescriptions::eString);
    arg_desc->AddFlag("N", "Check Product Names");
    arg_desc->AddFlag("F", "Autofix");
    arg_desc->AddFlag("XML", "Generate XML output");
    arg_desc->AddFlag("STDOUT", "Copy the output to STDOUT");
    arg_desc->AddFlag("LIST", "List the tests without execution");

    //arg_desc->AddOptionalKey("P", "ReportType", "Report type: g - Genome, b - Big Sequence, m - MegaReport, t - Include FATAL Tag, s - FATAL Tag for Superuser", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("P", "ReportType", "Report type: q - SMART genomes, u - Genome Submitter, b - Big Sequence, f - FATAL, t - Include FATAL Tag, s - FATAL Tag for Superuser", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("R", "SevCount", "0: always fail, 1: fail if severity is WARNING or above, 2 and higher: fail  if severity is ERROR or above", CArgDescriptions::eInteger);
    //"Severity for Error in Return Code\n\tinfo(0)\n\twarning(1)\n\terror(2)\n\tcritical(3)\n\tfatal(4)\n\ttrace(5)", CArgDescriptions::eInteger);

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, CDataLoadersUtil::fDefault | CDataLoadersUtil::fGenbankOffByDefault);

/*
    arg_desc->AddOptionalKey("M", "MessageLevel",
        "Output message level: 'a': add FATAL tags and output all messages, 'f': add FATAL tags and output FATAL messages only",
                                CArgDescriptions::eString);
*/
    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
}

string CDiscRepApp::x_ConstructOutputName(const string& input)
{
    const CArgs& args = GetArgs();
    CDirEntry fname(input);
    string path = args["outdir"] ? args["outdir"].AsString() : fname.GetDir();
    string ext = args["s"] ? args["s"].AsString() : ".dr";
    if (m_Xml) {
        ext += ".xml";
    }
    return CDirEntry::MakePath(path, fname.GetBase(), ext);
}


static unique_ptr<CObjectIStream> OpenUncompressedStream(const string& fname, bool& compressed)
{
    unique_ptr<CNcbiIstream> InputStream(new CNcbiIfstream(fname, ios::binary));
    CCompressStream::EMethod method;

    CFormatGuess::EFormat format = CFormatGuess::Format(*InputStream);
    switch (format) {
        case CFormatGuess::eGZip:  method = CCompressStream::eGZipFile;  break;
        case CFormatGuess::eBZip2: method = CCompressStream::eBZip2;     break;
        case CFormatGuess::eLzo:   method = CCompressStream::eLZO;       break;
        default:                   method = CCompressStream::eNone;      break;
    }
    compressed = method != CCompressStream::eNone;
    if (compressed) {
        InputStream.reset(new CDecompressIStream(*InputStream.release(), method, CCompressStream::fDefault, eTakeOwnership));
        format = CFormatGuess::Format(*InputStream);
    }

    unique_ptr<CObjectIStream> objectStream;
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


string CDiscRepApp::x_DefaultHeader()
{
    if (GetArgs()["a"]) {
        string type = GetArgs()["a"].AsString();
        if (type == "e") return CSeq_entry::GetTypeInfo()->GetName();
        else if (type == "m") return CSeq_submit::GetTypeInfo()->GetName();
        else if (type == "s") return CBioseq_set::GetTypeInfo()->GetName();
        else if (type == "b") return CBioseq::GetTypeInfo()->GetName();
    }
    return kEmptyStr;
}


void CDiscRepApp::x_ProcessFile(const string& fname, CDiscrepancySet& tests)
{
    bool compressed = false;
    unique_ptr<CObjectIStream> in = OpenUncompressedStream(fname, compressed);
    if (!in) {
        NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
    }
    tests.ParseStream(*in, fname, !compressed, x_DefaultHeader());
}


void CDiscRepApp::x_ParseDirectory(const string& dirname, bool recursive)
{
    CDir Dir(dirname);
    if (!Dir.Exists() || !Dir.IsDir()) return;

    string ext = GetArgs()["x"].AsString();
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }
    string autofixext = ".autofix" + ext;

    CDir::TEntries Entries = Dir.GetEntries();
    for (const CDir::TEntry& entry : Entries) {
        auto filename = entry->GetName();
        if (filename == "." || filename == "..") continue;
        if (recursive && entry->IsDir()) x_ParseDirectory(entry->GetPath(), true);
        if (NStr::EndsWith(filename, ext) && !NStr::EndsWith(filename, autofixext))
        {
            if (entry->IsFile())
                m_Files.push_back(entry->GetPath());
        }
    }
}


unsigned CDiscRepApp::x_ProcessOne(const string& fname)
{
    unsigned severity;
    CRef<CDiscrepancyProduct> product;
    CRef<CDiscrepancySet> Tests = CDiscrepancySet::New(*m_Scope);
    Tests->SetSuspectRules(m_SuspectRules, false);
    if (m_SuspectProductNames) {
        Tests->AddTest(eTestNames::_SUSPECT_PRODUCT_NAMES);
        Tests->ParseStrings(fname);
        severity = Tests->Summarize();
    }
    else {
        for (auto tname : m_Tests) {
            Tests->AddTest(tname);
        }
        Tests->SetLineage(m_Lineage);
        x_ProcessFile(fname, *Tests);
        severity = Tests->Summarize();
        if (m_AutoFix) {
            x_Autofix(*Tests);
        }
    }
    auto outfilename = x_ConstructOutputName(fname);
    x_Output(outfilename, *Tests->GetProduct(), 0);
    return severity;
}


void CDiscRepApp::x_Output(const string& fname, CDiscrepancyProduct& tests, unsigned short iflags)
{
    //m_Fat m_Group
    unsigned short flags = (GetArgs()["S"].AsBoolean() ? CDiscrepancySet::eOutput_Summary : 0) | (m_Fat ? CDiscrepancySet::eOutput_Fatal : 0) | (m_Ext ? CDiscrepancySet::eOutput_Ext : 0);
    flags |= iflags;

    CNcbiOfstream out1(fname, ofstream::out);
    list<ostream*> olist{&out1};
    if (m_Print)
        olist.push_back(&std::cout);

    CWStream tee(new CMultiWriter(olist));
    m_Xml ? tests.OutputXML(tee, flags) : tests.OutputText(tee, flags, m_Group);
}

unsigned CDiscRepApp::x_ProcessAll(const string& outname)
{
    int count = 0;
    unsigned severity = 0;
    CRef<CDiscrepancySet> Tests = CDiscrepancySet::New(*m_Scope);
    Tests->SetSuspectRules(m_SuspectRules, false);
    if (m_SuspectProductNames) {
        Tests->AddTest(eTestNames::_SUSPECT_PRODUCT_NAMES);
        for (const string& fname : m_Files) {
            ++count;
            if (m_Files.size() > 1) {
                PrintMsg(eDiag_Info, "Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            Tests->ParseStrings(fname);
        }
        severity = Tests->Summarize();
    } else if (m_AutoFix) {
        for (auto tname : m_Tests) {
            Tests->AddTest(tname);
        }
        Tests->SetLineage(m_Lineage);
        for (const string& fname : m_Files) {
            ++count;
            if (m_Files.size() > 1) {
                PrintMsg(eDiag_Info, "Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            x_ProcessFile(fname, *Tests);
            severity = Tests->Summarize();
            x_Autofix(*Tests);
        }
    } else {
        for (const string& fname : m_Files) {
            for (auto tname : m_Tests) {
                Tests->AddTest(tname);
            }
            Tests->SetLineage(m_Lineage);
            ++count;
            if (m_Files.size() > 1) {
                PrintMsg(eDiag_Info, "Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            x_ProcessFile(fname, *Tests);
        }
        severity = Tests->Summarize();
    }
    unsigned short flags = CDiscrepancySet::eOutput_Files;
    x_Output(outname, *Tests->GetProduct(), flags);

    return severity;
}


void CDiscRepApp::x_Autofix(CDiscrepancySet& tests)
{
    tests.Autofix();
}


int CDiscRepApp::Run()
{
    const CArgs& args = GetArgs();

    if (args["P"]) {
        const string& s = args["P"].AsString();
        for (char c : s) {
            if (c == 't') {
                m_Fat = true;
            }
            else if (c == 's') {
                m_Ext = true;
                m_Fat = true;
            }
            else if (c == 'q' || c == 'b' || c == 'u' || c == 'f') {
                if (m_Group && m_Group != c) {
                    PrintMsg(eDiag_Error, string("-P options are not compatible: ") + m_Group + " and " + c);
                    return 1;
                }
                m_Group = c;
                m_Fat = true;
            }
            else {
                PrintMsg(eDiag_Error, string("Unrecognized character in -P argument: ") + c);
                return 1;
            }
        }
    }

    // constructing the test list
    TTestNamesSet Tests;
    if (args["e"]) {
        if (args["N"]) {
            PrintMsg(eDiag_Error, "Options -N and -e are mutually exclusive");
            return 1;
        }
        list<string> List;
        NStr::Split(args["e"].AsString(), ", ", List, NStr::fSplit_Tokenize);
        for (const string& s : List) {
            auto name = GetDiscrepancyCaseName(s);
            if (name == eTestNames::notset) {
                PrintMsg(eDiag_Error, "Test name not found: " + s);
                return 1;
            } else {
                Tests.set(name);
            }
        }
        if (m_Group) {
            PrintMsg(eDiag_Info, string("Option ignored: -P ") + m_Group);
            m_Group = 0;
        }
    }
    else if (!args["N"]) {
        TTestNamesSet AllTests;
        switch (m_Group) {
            case 'q':
                AllTests = GetDiscrepancyTests(eSmart);
                break;
            case 'u':
                AllTests = GetDiscrepancyTests(eSubmitter);
                break;
            case 'b':
                AllTests = GetDiscrepancyTests(eBig);
                break;
            case 'f':
                AllTests = GetDiscrepancyTests(eFatal);
                break;
            default:
                AllTests = GetDiscrepancyTests(eAll);
        }
        Tests += AllTests;
    }
    if (args["d"]) {
        if (args["N"]) {
            PrintMsg(eDiag_Error, "Options -N and -d are mutually exclusive");
            return 1;
        }
        list<string> List;
        NStr::Split(args["d"].AsString(), ", ", List, NStr::fSplit_Tokenize);
        for (const string& s : List) {
            auto name = GetDiscrepancyCaseName(s);
            if (name == eTestNames::notset) {
                PrintMsg(eDiag_Error, "Test name not found: " + s);
                return 1;
            } else {
                Tests.reset(name);
            }
        }
    }
    if (args["N"]) {
        m_SuspectProductNames = true;
    }
    else {
        m_Tests += Tests;
        if (m_Tests.empty()) {
            PrintMsg(eDiag_Error, "Empty test list");
            return 1;
        }
    }

    if (args["X"]) {
        list<string> List;
        NStr::Split(args["X"].AsString(), ", ", List, NStr::fSplit_Tokenize);
        for (const string& s : List) {
            if (s != "ALL") {
                PrintMsg(eDiag_Error, "Unknown option: " + s);
                return 1;
            }
            else {
                m_Ext = true;
            }
        }
    }

    if (args["LIST"]) {
        for (auto t : m_Tests) {
            cout << GetDiscrepancyCaseName(t) << "\n";
        }
        return 0;
    }
    // input files
    string abs_input_path;
    if (args["i"]) {
        m_Files.push_back(args["i"].AsString());
        abs_input_path = CDirEntry::CreateAbsolutePath(args["i"].AsString());
    }

    if (args["indir"]) x_ParseDirectory(args["indir"].AsString(), args["u"].AsBoolean());
    if (m_Files.empty()) {
        PrintMsg(eDiag_Error, "No input files");
        return 1;
    }

    // set defaults
    if (args["w"]) m_SuspectRules = args["w"].AsString();
    if (args["L"]) m_Lineage = args["L"].AsString();
    if (args["XML"]) m_Xml = args["XML"].AsBoolean();
    if (args["STDOUT"]) m_Print = args["STDOUT"].AsBoolean();

    if (args["F"]) {
        m_AutoFix = args["F"].AsBoolean();
    }

    transform(m_Files.begin(), m_Files.end(), m_Files.begin(), [](string& s) { for (auto n = s.find('\\'); n != string::npos; n = s.find('\\')) s[n] = '/'; return s; });
    // run in deterministic order for TC tests
    sort(m_Files.begin(), m_Files.end());

    CRef<CObjectManager> ObjMgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *ObjMgr, CDataLoadersUtil::fDefault | CDataLoadersUtil::fGenbankOffByDefault);
    m_Scope.Reset(new CScope (*ObjMgr));
    m_Scope->AddDefaults();

    // run tests
    unsigned severity = 0;

    try {
        if (args["o"]) {
            if (abs_input_path == CDirEntry::CreateAbsolutePath(args["o"].AsString())) {
                PrintMsg(eDiag_Error, "Input and output files should be different");
                return 1;
            }
            severity = x_ProcessAll(args["o"].AsString());
        }
        else {
            int count = 0;
            for (const string& f : m_Files) {
                ++count;
                if (m_Files.size() > 1) {
                    PrintMsg(eDiag_Info, "Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
                }
                unsigned sev = x_ProcessOne(f);
                severity = sev > severity ? sev : severity;
            }
        }
    }
    catch (const CException& e) {
        PrintMsg(eDiag_Error, e.GetMsg());
        return 1;
    }

    if (args["R"]) {
        int r = args["R"].AsInteger();
        if (r < 1 || (r < 2 && severity > 0) || severity > 1) {
            return 1;
        }
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int
#ifdef NCBI_SUBUTILS_MULTICALL_asndisc
asndisc_app_main
#else
main
#endif
(int argc, const char* argv[])
{
    // this code converts single argument into multiple, just to simplify testing
    list<string>        split_args;
    vector<const char*> new_argv;

    if (argc==2 && argv && argv[1] && strchr(argv[1], ' '))
    {
        NStr::Split(argv[1], " ", split_args, NStr::fSplit_CanEscape | NStr::fSplit_CanQuote | NStr::fSplit_Truncate | NStr::fSplit_MergeDelimiters);

        argc = 1 + split_args.size();
        new_argv.reserve(argc);
        new_argv.push_back(argv[0]);
        for (auto& s : split_args) {
            new_argv.push_back(s.c_str());
            #ifdef _DEBUG
            std::cerr << s.c_str() << " ";
            #endif
        }
        std::cerr << "\n";

        argv = new_argv.data();
    }

    return CDiscRepApp().AppMain(argc, argv);
}
