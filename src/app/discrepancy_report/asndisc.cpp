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
    string x_ConstructAutofixName(const string& input);
    string x_ConstructMacroName(const string& input);
    void x_ProcessFile(const string& filename, CDiscrepancySet& tests);
    void x_ParseDirectory(const string& name, bool recursive);
    void x_ProcessOne(const string& filename);
    void x_ProcessAll(const string& outname);
    void x_Output(const string& filename, CDiscrepancySet& tests);
    void x_OutputXml(const string& filename, CDiscrepancySet& tests);
    void x_Autofix(const TDiscrepancyCaseMap& tests);
    void x_OutputObject(const string& filename, const CSerialObject& obj);

    CRef<CScope> m_Scope;
    string m_SuspectRules;
    string m_Lineage;   // override lineage
    vector<string> m_Files;
    vector<string> m_Tests;
    map<string, CRef<CSerialObject> > m_Objects;

    bool m_SuspectProductNames;
    bool m_Ext;
    bool m_Fat;
    bool m_Big;
    bool m_AutoFix;
    bool m_Xml;
    bool m_Print;
};


CDiscRepApp::CDiscRepApp(void) : m_SuspectProductNames(false), m_Ext(false), m_Fat(false), m_Big(false), m_AutoFix(false), m_Xml(false), m_Print(false)
{
    SetVersionByBuild(1);
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
        ITERATE (vector<string>, nm, Name) {
            if ((*nm)[0] == '_') {
                continue;
            }
            str += "   ";
            str += *nm;
            const vector<string> Alias = GetDiscrepancyAliases(*nm);
            ITERATE (vector<string>, al, Alias) {
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

    //arg_desc->AddOptionalKey("P", "ReportType", "Report type: g - Genome, b - Big Sequence, m - MegaReport, t - Include FATAL Tag, s - FATAL Tag for Superuser", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("P", "ReportType", "Report type: q - SMART, u - Submitter, b - Big Sequence, t - Include FATAL Tag, s - FATAL Tag for Superuser", CArgDescriptions::eString);

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, CDataLoadersUtil::fDefault | CDataLoadersUtil::fGenbankOffByDefault);

/*
    arg_desc->AddOptionalKey("M", "MessageLevel", 
        "Output message level: 'a': add FATAL tags and output all messages, 'f': add FATAL tags and output FATAL messages only",
                                CArgDescriptions::eString);
*/
    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
};


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


string CDiscRepApp::x_ConstructMacroName(const string& input)
{
    const CArgs& args = GetArgs();
    CDirEntry fname(input);
    string path = args["outdir"] ? args["outdir"].AsString() : fname.GetDir();
    return CDirEntry::MakePath(path, fname.GetBase(), "macro.txt");
}


string CDiscRepApp::x_ConstructAutofixName(const string& input)
{
    const CArgs& args = GetArgs();
    CDirEntry fname(input);
    string path = args["outdir"] ? args["outdir"].AsString() : fname.GetDir();
    return CDirEntry::MakePath(path, fname.GetBase(), "autofix.asn");
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


void CDiscRepApp::x_ProcessFile(const string& fname, CDiscrepancySet& tests)
{
    auto_ptr<CObjectIStream> in = OpenUncompressedStream(fname);
    while (true) {
        string header;
        CRef<CSerialObject> obj;
        try {
            header = in->ReadFileHeader();
            if (header.empty() && GetArgs()["a"]) {
                string type = GetArgs()["a"].AsString();
                if (type == "e") header = CSeq_entry::GetTypeInfo()->GetName();
                else if (type == "m") header = CSeq_submit::GetTypeInfo()->GetName();
                else if (type == "s") header = CBioseq_set::GetTypeInfo()->GetName();
                else if (type == "b") header = CBioseq::GetTypeInfo()->GetName();
            }
            if (header == CSeq_submit::GetTypeInfo()->GetName()) {
                CRef<CSeq_submit> ss(new CSeq_submit);
                in->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);
                if (ss->IsSetData() && ss->GetData().IsEntrys()) {
                    NON_CONST_ITERATE (CSeq_submit::TData::TEntrys, it, ss->SetData().SetEntrys()) {
                        m_Scope->AddTopLevelSeqEntry(**it);
                    }
                }
                obj.Reset(ss);
            }
            else if (header == CSeq_entry::GetTypeInfo()->GetName()) {
                CRef<CSeq_entry> se(new CSeq_entry);
                in->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);
                m_Scope->AddTopLevelSeqEntry(*se);
                obj.Reset(se);
            }
            else if (header == CBioseq_set::GetTypeInfo()->GetName()) {
                CRef<CBioseq_set> set(new CBioseq_set);
                in->Read(ObjectInfo(*set), CObjectIStream::eNoFileHeader);
                CRef<CSeq_entry> se(new CSeq_entry());
                se->SetSet().Assign(*set);
                m_Scope->AddTopLevelSeqEntry(*se);
                obj.Reset(se);
            }
            else if (header == CBioseq::GetTypeInfo()->GetName()) {
                CRef<CBioseq> seq(new CBioseq);
                in->Read(ObjectInfo(*seq), CObjectIStream::eNoFileHeader);
                CRef<CSeq_entry> se(new CSeq_entry());
                se->SetSeq().Assign(*seq);
                m_Scope->AddTopLevelSeqEntry(*se);
                obj.Reset(se);
            }
            else {
                NCBI_THROW(CException, eUnknown, "Unsupported type " + header);
            }
            tests.Parse(*obj);
            if (m_AutoFix) {
                m_Objects[fname] = obj;
            }
            else {
                m_Scope->ResetDataAndHistory();
            }
            if (!GetArgs()["a"] || GetArgs()["a"].AsString() != "c") {
                break;
            }
        }
        catch (const CEofException&) {
            break;
        }
    }
}


void CDiscRepApp::x_ParseDirectory(const string& name, bool recursive)
{
    CDir Dir(name);
    string ext = GetArgs()["x"].AsString();
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }
    if (!Dir.Exists() || !Dir.IsDir()) return;
    CDir::TEntries Entries = Dir.GetEntries();
    ITERATE (CDir::TEntries, entry, Entries) {
        if ((*entry)->GetName() == "." || (*entry)->GetName() == "..") continue;
        if (recursive && (*entry)->IsDir()) x_ParseDirectory((*entry)->GetPath(), true);
        if ((*entry)->IsFile() && (*entry)->GetExt() == ext) m_Files.push_back((*entry)->GetPath());
    }
}


void CDiscRepApp::x_ProcessOne(const string& fname)
{
    CRef<CDiscrepancySet> Tests = CDiscrepancySet::New(*m_Scope);
    Tests->SetSuspectRules(m_SuspectRules, false);
    if (m_SuspectProductNames) {
        Tests->AddTest("_SUSPECT_PRODUCT_NAMES");
        CNcbiIfstream istr(fname.c_str());
        CStreamLineReader line_reader(istr);
        do {
            Tests->TestString(*++line_reader);
        }
        while (!line_reader.AtEOF());
        Tests->Summarize();
    }
    else {
        ITERATE (vector<string>, tname, m_Tests) {
            Tests->AddTest(*tname);
        }
        Tests->SetKeepRef(m_AutoFix);
        Tests->SetLineage(m_Lineage);
        x_ProcessFile(fname, *Tests);
        Tests->Summarize();
        if (m_AutoFix) {
            x_Autofix(Tests->GetTests());
            x_Autofix(Tests->GetTests());
            x_OutputObject(x_ConstructAutofixName(fname), *m_Objects.begin()->second);
            m_Objects.clear();
        }
    }
    m_Xml ? x_OutputXml(x_ConstructOutputName(fname), *Tests) : x_Output(x_ConstructOutputName(fname), *Tests);
    if (m_Print) {
        m_Xml ? Tests->OutputXML(cout) : Tests->OutputText(cout, false, m_Fat);
    }
}


void CDiscRepApp::x_ProcessAll(const string& outname)
{
    int count = 0;
    CRef<CDiscrepancySet> Tests = CDiscrepancySet::New(*m_Scope);
    Tests->SetSuspectRules(m_SuspectRules, false);
    if (m_SuspectProductNames) {
        Tests->AddTest("_SUSPECT_PRODUCT_NAMES");
        ITERATE (vector<string>, fname, m_Files) {
            ++count;
            if (m_Files.size() > 1) {
                LOG_POST("Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            Tests->SetFile(*fname);
            CNcbiIfstream istr(fname->c_str());
            CStreamLineReader line_reader(istr);
            do {
                Tests->TestString(*++line_reader);
            }
            while (!line_reader.AtEOF());
        }
        Tests->Summarize();
    }
    else {
        ITERATE (vector<string>, tname, m_Tests) {
            Tests->AddTest(*tname);
        }
        Tests->SetKeepRef(m_AutoFix);
        Tests->SetLineage(m_Lineage);
        ITERATE (vector<string>, fname, m_Files) {
            ++count;
            if (m_Files.size() > 1) {
                LOG_POST("Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            Tests->SetFile(*fname);
            x_ProcessFile(*fname, *Tests);
        }
        Tests->Summarize();
        if (m_AutoFix) {
            x_Autofix(Tests->GetTests());
            for (auto it: m_Objects) {
                x_OutputObject(x_ConstructAutofixName(it.first), *it.second);
            }
        }
    }
    m_Xml ? x_OutputXml(outname, *Tests) : x_Output(outname, *Tests);
    if (m_Print) {
        m_Xml ? Tests->OutputXML(cout) : Tests->OutputText(cout, true, m_Fat);
    }
}


void CDiscRepApp::x_Output(const string& filename, CDiscrepancySet& tests)
{
    bool summary = GetArgs()["S"].AsBoolean();
    CNcbiOfstream out(filename.c_str(), ofstream::out);
    tests.OutputText(out, m_Fat, summary, m_Ext, m_Big);
}


void CDiscRepApp::x_OutputXml(const string& filename, CDiscrepancySet& tests)
{
    CNcbiOfstream out(filename.c_str(), ofstream::out);
    tests.OutputXML(out);
}


void CDiscRepApp::x_Autofix(const TDiscrepancyCaseMap& tests)
{
    ITERATE (TDiscrepancyCaseMap, tst, tests) {
        const TReportItemList& list = tst->second->GetReport();
        ITERATE (TReportItemList, it, list) {
            (*it)->Autofix(*m_Scope);
        }
    }
}


void CDiscRepApp::x_OutputObject(const string& filename, const CSerialObject& obj)
{
    CNcbiOfstream out(filename.c_str(), ofstream::out);
    out << MSerial_AsnText << obj;
}


int CDiscRepApp::Run(void)
{
    const CArgs& args = GetArgs();

    //EGroup group = eNone;
    char group = 0;

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
            else if (s[i] == 'q' || s[i] == 'b' || s[i] == 'u') {
                if (group && group != s[i]) {
                    ERR_POST(string("-P options are not compatible: ") + group + " and " + s[i]);
                    return 1;
                }
                group = s[i];
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
        ITERATE (list<string>, s, List) {
            string name = GetDiscrepancyCaseName(*s);
            if (name.empty()) {
                ERR_POST("Test name not found: " + *s);
                return 1;
            } else {
                Tests.insert(name);
            }
        }
        if (group) {
            LOG_POST(string("Option ignored: -P ") + group);
        }
    }
    else if (!args["N"]) {
        vector<string> AllTests;
        switch (group) {
            case 'q':
                AllTests = GetDiscrepancyNames(eSmart);
                break;
            case 'u':
                AllTests = GetDiscrepancyNames(eSubmitter);
                m_Big = true;
                break;
            case 'b':
                AllTests = GetDiscrepancyNames(eBig);
                m_Big = true;
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
        ITERATE (list<string>, s, List) {
            string name = GetDiscrepancyCaseName(*s);
            if (name.empty()) {
                ERR_POST("Test name not found: " + *s);
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
        ITERATE (list<string>, s, List) {
            if (*s != "ALL") {
                ERR_POST("Unknown option: " + *s);
                return 1;
            }
            else {
                m_Ext = true;
            }
        }
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
        if (GetArgs()["a"] && GetArgs()["a"].AsString() == "c") {
            ERR_POST("Options -F and -a c are mutually exclusive");
            return 1;
        }
        m_AutoFix = args["F"].AsBoolean();
    }

    CRef<CObjectManager> ObjMgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *ObjMgr, CDataLoadersUtil::fDefault | CDataLoadersUtil::fGenbankOffByDefault);
    m_Scope.Reset(new CScope (*ObjMgr));
    m_Scope->AddDefaults();

    // run tests
    if (args["o"]) {
        if (abs_input_path == CDirEntry::CreateAbsolutePath(args["o"].AsString())) {
            ERR_POST("Input and output files should be different");
            return 1;
        }
        x_ProcessAll(args["o"].AsString());
    }
    else {
        int count = 0;
        ITERATE (vector<string>, f, m_Files) {
            ++count;
            if (m_Files.size() > 1) {
                LOG_POST("Processing file " + to_string(count) + " of " + to_string(m_Files.size()));
            }
            x_ProcessOne(*f);
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

// some functions for use when in a debugger
using namespace ncbi;
using namespace objects;

void PS(const CSerialObject *obj, CNcbiOstream *out_strm = &cout)
{
    CNcbiOstream & real_strm = ( out_strm ? *out_strm : cout );
    if( obj ) {
        real_strm << noskipws << MSerial_AsnText << *obj << endl;
    } else {
        real_strm << "NULL" << endl;
    }
}

CTempString *TMP_MK(const char *a_str)
{
    return new CTempString(a_str);
}
