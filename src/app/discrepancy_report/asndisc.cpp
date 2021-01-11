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
#include <misc/data_loaders_util/data_loaders_util.hpp>
#include <misc/discrepancy/discrepancy.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <util/format_guess.hpp>
#include <util/compress/stream_util.hpp>
#include <util/line_reader.hpp>

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
    string x_DefaultHeader();
    void x_ProcessFile(const string& filename, CDiscrepancySet& tests);
    void x_ParseDirectory(const string& name, bool recursive);
    unsigned x_ProcessOne(const string& filename);
    unsigned x_ProcessAll(const string& outname);
    void x_Output(const string& filename, CDiscrepancySet& tests, unsigned short flags);
    void x_OutputXml(const string& filename, CDiscrepancySet& tests, unsigned short flags);
    void x_Autofix(CDiscrepancySet& tests);

    CRef<CScope> m_Scope;
    string m_SuspectRules;
    string m_Lineage;   // override lineage
    vector<string> m_Files;
    vector<string> m_Tests;

    char m_Group{0};
    bool m_SuspectProductNames{false};
    bool m_Ext{false};
    bool m_Fat{false};
    bool m_AutoFix{false};
    bool m_Xml{false};
    bool m_Print{false};
};


CDiscRepApp::CDiscRepApp(void)
{
    SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
}


class CDiscRepArgDescriptions : public CArgDescriptions
{
public:
    virtual string& PrintUsage(string& str, bool detailed = false) const;
};


string& CDiscRepArgDescriptions::PrintUsage(string& str, bool detailed) const
{
    CArgDescriptions::PrintUsage(str, detailed);
    if (detailed) {
        str+="TESTS\n";
        const vector<string> Name = GetDiscrepancyNames();
        for (auto& nm: Name) {
            str += "   ";
            str += nm;
            const vector<string> Alias = GetDiscrepancyAliases(nm);
            for (auto& al: Alias) {
                str += " / ";
                str += al;
            }
            str += "\n";
        }
    }
    return str;
}


void CDiscRepApp::Init(void)
{
    // Prepare command line descriptions
    unique_ptr<CArgDescriptions> arg_desc(new CDiscRepArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Discrepancy Report");

    arg_desc->AddOptionalKey("i", "InFile", "Single Input File", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("indir", "InputDirectory", "Path to ASN.1 Files", CArgDescriptions::eInputFile);
    arg_desc->AddFlag("u", "Recurse");
    arg_desc->AddOptionalKey("o", "OutFile", "Single Output File", CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("s", "OutputFileSuffix", "Output File Suffix", CArgDescriptions::eString, ".dr");
    arg_desc->AddDefaultKey("x", "Suffix", "File Selection Substring", CArgDescriptions::eString, ".sqn");
    arg_desc->AddOptionalKey("outdir", "OutputDirectory", "Output Directory", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("e", "EnableTests", "List of enabled tests, seperated by ','", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("d", "DisableTests",  "List of disabled tests, seperated by ','", CArgDescriptions::eString);

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
    arg_desc->AddOptionalKey("R", "SevCount", "Severity for Error in Return Code\n\tinfo(0)\n\twarning(1)\n\terror(2)\n\tcritical(3)\n\tfatal(4)\n\ttrace(5)", CArgDescriptions::eInteger);

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, CDataLoadersUtil::fDefault | CDataLoadersUtil::fGenbankOffByDefault);

/*
    arg_desc->AddOptionalKey("M", "MessageLevel", 
        "Output message level: 'a': add FATAL tags and output all messages, 'f': add FATAL tags and output FATAL messages only",
                                CArgDescriptions::eString);
*/
    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
};


string CDiscRepApp::x_ConstructOutputName(const string& input) // LCOV_EXCL_START
{
    const CArgs& args = GetArgs();
    CDirEntry fname(input);
    string path = args["outdir"] ? args["outdir"].AsString() : fname.GetDir();
    string ext = args["s"] ? args["s"].AsString() : ".dr";
    if (m_Xml) {
        ext += ".xml";
    }
    return CDirEntry::MakePath(path, fname.GetBase(), ext);
} // LCOV_EXCL_STOP


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
    if (in)
        tests.ParseStream(*in, fname, !compressed, x_DefaultHeader());
}


void CDiscRepApp::x_ParseDirectory(const string& name, bool recursive)
{
    CDir Dir(name);
    if (!Dir.Exists() || !Dir.IsDir()) return;

    string ext = GetArgs()["x"].AsString();
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }
    string autofixext = ".autofix" + ext;

    CDir::TEntries Entries = Dir.GetEntries();
    for (auto& entry: Entries) {
        auto name = entry->GetName();
        if (name == "." || name == "..") continue;
        if (recursive && entry->IsDir()) x_ParseDirectory(entry->GetPath(), true);
        if (NStr::EndsWith(name, ext) && !NStr::EndsWith(name, autofixext))
        {
            if (entry->IsFile()) 
                m_Files.push_back(entry->GetPath());
        }
    }
}


unsigned CDiscRepApp::x_ProcessOne(const string& fname) // LCOV_EXCL_START
{
    unsigned severity = 0;
    CRef<CDiscrepancySet> Tests = CDiscrepancySet::New(*m_Scope);
    Tests->SetSuspectRules(m_SuspectRules, false);
    if (m_SuspectProductNames) {
        Tests->AddTest("_SUSPECT_PRODUCT_NAMES");
        Tests->ParseStrings(fname);
        severity = Tests->Summarize();
    }
    else {
        for (auto& tname : m_Tests) {
            Tests->AddTest(tname);
        }
        Tests->SetLineage(m_Lineage);
        x_ProcessFile(fname, *Tests);
        severity = Tests->Summarize();
        if (m_AutoFix) {
            x_Autofix(*Tests);
        }
    }
    unsigned short flags = (GetArgs()["S"].AsBoolean() ? CDiscrepancySet::eOutput_Summary : 0) | (m_Fat ? CDiscrepancySet::eOutput_Fatal : 0) | (m_Ext ? CDiscrepancySet::eOutput_Ext : 0);
    m_Xml ? x_OutputXml(x_ConstructOutputName(fname), *Tests, flags) : x_Output(x_ConstructOutputName(fname), *Tests, flags);
    if (m_Print) {
        m_Xml ? Tests->OutputXML(cout, flags) : Tests->OutputText(cout, m_Fat, false);
    }
    return severity;
} // LCOV_EXCL_STOP


unsigned CDiscRepApp::x_ProcessAll(const string& outname)
{
    int count = 0;
    unsigned severity = 0;
    CRef<CDiscrepancySet> Tests = CDiscrepancySet::New(*m_Scope);
    Tests->SetSuspectRules(m_SuspectRules, false);
    if (m_SuspectProductNames) {
        Tests->AddTest("_SUSPECT_PRODUCT_NAMES");
        for (auto& fname : m_Files) {
            ++count;
            if (m_Files.size() > 1) {
                LOG_POST("Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            Tests->ParseStrings(fname);
        }
        severity = Tests->Summarize();
    } else if (m_AutoFix) {
        for (auto& tname : m_Tests) {
            Tests->AddTest(tname);
        }
        Tests->SetLineage(m_Lineage);
        for (auto& fname : m_Files) {
            ++count;
            if (m_Files.size() > 1) {
                LOG_POST("Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            x_ProcessFile(fname, *Tests);
            severity = Tests->Summarize();
            x_Autofix(*Tests);
        }
    } else {
        for (auto& fname : m_Files) {
            for (auto& tname : m_Tests) {
                Tests->AddTest(tname);
            }
            Tests->SetLineage(m_Lineage);
            ++count;
            if (m_Files.size() > 1) {
                LOG_POST("Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            x_ProcessFile(fname, *Tests);
        }
        severity = Tests->Summarize();
    }
    unsigned short flags = (GetArgs()["S"].AsBoolean() ? CDiscrepancySet::eOutput_Summary : 0) | (m_Fat ? CDiscrepancySet::eOutput_Fatal : 0) | (m_Ext ? CDiscrepancySet::eOutput_Ext : 0) | CDiscrepancySet::eOutput_Files;
    m_Xml ? x_OutputXml(outname, *Tests, flags) : x_Output(outname, *Tests, flags);
    if (m_Print) {
        m_Xml ? Tests->OutputXML(cout, flags) : Tests->OutputText(cout, m_Fat, true);
    }
    return severity;
}


void CDiscRepApp::x_Output(const string& filename, CDiscrepancySet& tests, unsigned short flags)
{
    CNcbiOfstream out(filename, ofstream::out);
    tests.OutputText(out, flags, m_Group);
}


void CDiscRepApp::x_OutputXml(const string& filename, CDiscrepancySet& tests, unsigned short flags)
{
    CNcbiOfstream out(filename, ofstream::out);
    tests.OutputXML(out, flags);
}


void CDiscRepApp::x_Autofix(CDiscrepancySet& tests)
{
    TReportObjectList tofix;
    map<string, size_t> ignore;
    for (auto& tst: tests.GetTests()) {
        const TReportItemList& list = tst.second->GetReport();
        for (auto it: list) {
            for (auto obj : it->GetDetails()) {
                if (obj->CanAutofix()) {
                    tofix.push_back(CRef<CReportObj>(&*obj));
                }
            }
        }
    }
    tests.Autofix(tofix, ignore);
}


int CDiscRepApp::Run(void)
{
    const CArgs& args = GetArgs();

    if (args["P"]) {
        const string& s = args["P"].AsString();
        for (size_t i = 0; i < s.length(); i++) {
            if (s[i] == 't') {
                m_Fat = true;
            }
            else if (s[i] == 's') {
                m_Ext = true;
                m_Fat = true;
            }
            else if (s[i] == 'q' || s[i] == 'b' || s[i] == 'u' || s[i] == 'f') {
                if (m_Group && m_Group != s[i]) {
                    ERR_POST(string("-P options are not compatible: ") + m_Group + " and " + s[i]);
                    return 1;
                }
                m_Group = s[i];
                m_Fat = true;
            }
            else {
                ERR_POST(string("Unrecognized character in -P argument: ") + s[i]);
                return 1;
            }
        }
    }

    // constructing the test list
    set<string> Tests;
    if (args["e"]) {
        if (args["N"]) {
            ERR_POST("Options -N and -e are mutually exclusive");
            return 1;
        }
        list<string> List;
        NStr::Split(args["e"].AsString(), ", ", List, NStr::fSplit_Tokenize);
        for (auto& s: List) {
            string name = GetDiscrepancyCaseName(s);
            if (name.empty()) {
                ERR_POST("Test name not found: " + s);
                return 1;
            } else {
                Tests.insert(name);
            }
        }
        if (m_Group) {
            LOG_POST(string("Option ignored: -P ") + m_Group);
            m_Group = 0;
        }
    }
    else if (!args["N"]) {
        vector<string> AllTests;
        switch (m_Group) {
            case 'q':
                AllTests = GetDiscrepancyNames(eSmart);
                break;
            case 'u':
                AllTests = GetDiscrepancyNames(eSubmitter);
                break;
            case 'b':
                AllTests = GetDiscrepancyNames(eBig);
                break;
            case 'f':
                AllTests = GetDiscrepancyNames(eFatal);
                break;
            default:
                AllTests = GetDiscrepancyNames();
        }
        copy(AllTests.begin(), AllTests.end(), inserter(Tests, Tests.begin()));
    }
    if (args["d"]) {
        if (args["N"]) {
            ERR_POST("Options -N and -d are mutually exclusive");
            return 1;
        }
        list<string> List;
        NStr::Split(args["d"].AsString(), ", ", List, NStr::fSplit_Tokenize);
        for (auto& s: List) {
            string name = GetDiscrepancyCaseName(s);
            if (name.empty()) {
                ERR_POST("Test name not found: " + s);
                return 1;
            } else {
                Tests.erase(name);
            }
        }
    }
    if (args["N"]) {
        m_SuspectProductNames = true;
    }
    else {
        copy(Tests.begin(), Tests.end(), back_inserter(m_Tests));
        if (m_Tests.empty()) {
            ERR_POST("Empty test list");
            return 1;
        }
    }

    if (args["X"]) {
        list<string> List;
        NStr::Split(args["X"].AsString(), ", ", List, NStr::fSplit_Tokenize);
        for (auto& s: List) {
            if (s != "ALL") {
                ERR_POST("Unknown option: " + s);
                return 1;
            }
            else {
                m_Ext = true;
            }
        }
    }

    if (args["LIST"]) {
        for (auto& t : m_Tests) {
            cout << t << "\n";
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
        ERR_POST("No input files");
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
    if (args["o"]) {
        if (abs_input_path == CDirEntry::CreateAbsolutePath(args["o"].AsString())) {
            ERR_POST("Input and output files should be different"); // LCOV_EXCL_START
            return 1;
        } // LCOV_EXCL_STOP
        severity = x_ProcessAll(args["o"].AsString());
    }
    else { // LCOV_EXCL_START
        int count = 0;
        for (auto& f : m_Files) {
            ++count;
            if (m_Files.size() > 1) {
                LOG_POST("Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            unsigned sev = x_ProcessOne(f);
            severity = sev > severity ? sev : severity;
        }
    } // LCOV_EXCL_STOP
    if (args["R"]) {
        auto r = args["R"].AsInteger();
        if (r < 1 || (r < 2 && severity > 0) || severity > 1) {
            return 1;
        }
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    #ifdef _DEBUG
    // this code converts single argument into multiple, just to simplify testing
    std::list<std::string> split_args;
    std::vector<const char*> new_argv;    

    if (argc==2 && argv && argv[1] && strchr(argv[1], ' '))
    {
        NStr::Split(argv[1], " ", split_args);
        argc = 1 + split_args.size();
        new_argv.reserve(argc);
        new_argv.push_back(argv[0]);
        for (auto& s: split_args)
            new_argv.push_back(s.c_str());

        argv = new_argv.data();
    }
    #endif

    return CDiscRepApp().AppMain(argc, argv);
}
