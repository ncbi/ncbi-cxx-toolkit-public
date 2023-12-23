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
 *   Main() of Multipattern Search Code Generator
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/macro/Search_func.hpp>
#include <objects/macro/Suspect_rule.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <util/multipattern_search.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
using namespace ncbi::objects;


class CPrt2FsmApp : public CNcbiApplication
{
public:
    CPrt2FsmApp();
    void Init() override;
    int Run() override;
};


CPrt2FsmApp::CPrt2FsmApp() {}


void CPrt2FsmApp::Init()
{
    // Prepare command line descriptions
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Suspect Product Rules to FSM");
    arg_desc->AddOptionalKey("i", "InFile", "Input File", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("o", "OutFile", "Output File", CArgDescriptions::eOutputFile);
    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
}


string QuoteString(const string& s)
{
    string str;
    str.reserve(s.size() + 2);
    str += '\"';
    for (char c : s) {
        switch (c) {
            case '\n':
            case '\r':
                break;
            case '\"':
            case '\'':
            case '\\':
                str += '\\';
                NCBI_FALLTHROUGH;
            default:
                str += c;
        }
    }
    str += '\"';
    return str;
}

void QuoteBinary(std::ostream& ostr, const char* ptr, size_t size)
{
    bool needopen = false;
    string temp;
    ostr << "    \"";

    while(size) {
        unsigned char ch = *ptr++; --size;
        if (ch == '"' || ch < 32 || ch >= 128) {
            ostr << "\\x";
            NStr::NumericToString(temp, ch, 0, 16);
            if (temp.size() < 2)
               ostr << "0";
            ostr << temp;
            //ostr << "\"";
            needopen = true;
        } else {
            if (needopen) {
                ostr << "\" \"";
                needopen = false;
            }
            ostr << ch;
        }
    }

    ostr << "\"\n";
}

string QuoteBinary(const char* ptr, size_t size)
{
    ostringstream str;
    QuoteBinary(str, ptr, size);
    return str.str();
}

void QuoteBinaryHex(std::ostream& ostr, const char* ptr, size_t size)
{
    string temp;
    ostr << "    ";
    while (size)
    {
        unsigned char ch = *ptr++; --size;
        NStr::NumericToString(temp, ch, 0, 16);
        ostr << "0x" << temp << ", ";
    }
    ostr << "\n";
}

class CBinaryToCPP: public std::basic_streambuf<char>
{
public:
    using _MyBase = std::basic_streambuf<char>;
    static constexpr size_t m_buffer_size = 64;
    CBinaryToCPP(std::ostream& downstream) : m_ostr{&downstream}
    {

        m_buffer.reset(new char[m_buffer_size]);
        _MyBase::setp(m_buffer.get(), m_buffer.get() + m_buffer_size);
    }
    ~CBinaryToCPP()
    {
        sync();
    }
protected:
    int sync() override
    {
        xDump();
        return 0;
    }
    int_type overflow( int_type ch) override
    {
        xDump();
        if (ch != -1) {
            *pbase() = ch;
            _MyBase::pbump(1);
        }
        return ch;
    }

    void xDump() {
        size_t size = pptr() - pbase();
        if (size) {
            QuoteBinaryHex(*m_ostr, pbase(), size);
            _MyBase::setp(m_buffer.get(), m_buffer.get() + m_buffer_size);
        }
    }
    std::unique_ptr<char> m_buffer;
    std::ostream * m_ostr = nullptr;
};

static const pair<string, CMultipatternSearch::TFlags> FlagNames[] = {
    { "#NO_CASE", CMultipatternSearch::fNoCase },
    { "#BEGIN_STRING", CMultipatternSearch::fBeginString },
    { "#END_STRING", CMultipatternSearch::fEndString },
    { "#WHOLE_STRING", CMultipatternSearch::fWholeString },
    { "#BEGIN_WORD", CMultipatternSearch::fBeginWord },
    { "#END_WORD", CMultipatternSearch::fEndWord },
    { "#WHOLE_WORD", CMultipatternSearch::fWholeWord }
};

bool xTryProductRules(const string& filename, std::istream& file, std::ostream& ostr)
{
    unique_ptr<CObjectIStream> istr (CObjectIStream::Open(eSerial_AsnText, file));

    try
    {
        auto types = istr->GuessDataType({CSuspect_rule_set::GetTypeInfo()});
        if (types.size() != 1 || *types.begin() != CSuspect_rule_set::GetTypeInfo())
            return false;
    }
    catch(const CSerialException& e)
    {
        return false;
    }

    CSuspect_rule_set ProductRules;

    *istr >> ProductRules;

    CMultipatternSearch FSM;

    vector<string> patterns;
    for (auto rule : ProductRules.Get()) {
        patterns.push_back(rule->GetFind().GetRegex());
    }
    FSM.AddPatterns(patterns);

    ostr << "//\n"
        "// This code was generated by the prt2fsm application.\n"
        "// (see .../src/app/prt2fsm)\n"
        "//\n"
        "// Command line:\n"
        "//   prt2fsm";
    if (!filename.empty()) {
        ostr << " -i " << filename;
    }
    ostr << "\n";

    ostr << "//\n";
    ostr << "// Binary ASN.1 of CSuspect_rule_set object\n";
    ostr << "NCBI_FSM_RULES = {\n";
    std::string line;
    {
        CBinaryToCPP buf(ostr);
        std::ostream str(&buf);
        std::unique_ptr<CObjectOStream> objostr(CObjectOStream::Open(eSerial_AsnBinary, str));        
        *objostr << ProductRules;
    }
    ostr << "};\n";

    FSM.GenerateArrayMapData(ostr);

    return true;
}

bool xTryTextFile(const string& fname, std::istream& file, std::ostream& ostr)
{
	vector<pair<string, CMultipatternSearch::TFlags>> input;

    std::string line;
    size_t m;
    while (std::getline(file, line)) {
        // input line: <query> [<//comment>]
        //   /regex/   // comment ignored
        //   any text  // #NO_CASE #WHOLE_WORD etc...
        string comment;
        if ((m = line.find("//")) != string::npos) {
            comment = line.substr(m);
            line = line.substr(0, m);
        }
        if ((m = line.find_first_not_of(" \t")) != string::npos) {
            line = line.substr(m);
        }
        if ((m = line.find_last_not_of(" \t")) != string::npos) {
            line = line.substr(0, m + 1);
        }
        unsigned int flags = 0;
        for (auto f: FlagNames) {
            if (comment.find(f.first) != string::npos) {
                flags |= f.second;
            }
        }
        input.push_back(pair<string, unsigned int>(line, flags));
    }

    #if 0
    if ((m = fname.find_last_of("\\/")) != string::npos) {
        fname = fname.substr(m + 1);
    }

	for (size_t i = 1; i <= args.GetNExtra(); i++) {
        string param = args["#" + to_string(i)].AsString();
        params += " " + param;
        input.push_back(pair<string, unsigned int>(param, 0));
	}
    #endif

    CMultipatternSearch FSM;
    try {
        FSM.AddPatterns(input);
    }
    catch (string s) {
        cerr << s << "\n";
        return 1;
    }
    ostr << "//\n// This code was generated by the multipattern application.\n//\n// Command line:\n//   multipattern -A";
    if (!fname.empty()) {
        ostr << " -i " << fname;
    }
    ostr << "\n//\n";
    FSM.GenerateArrayMapData(ostr);

    return true;
}

int CPrt2FsmApp::Run()
{
    const CArgs& args = GetArgs();
    string fname;
    string params;
    if (!args["i"]) {
        ERR_POST("No input file");
        return 1;
    }
    fname = args["i"].AsString();
    //LOG_POST("Reading from " + fname);

    std::ostream* ostr = &std::cout;
    if (args["o"]) {
        ostr = &args["o"].AsOutputFile();
    }
    ifstream file(fname);
    if (!file) {
        ERR_POST("Cannot open file");
        return 1;
    }

    if (!xTryProductRules(fname, file, *ostr))
        xTryTextFile(fname, file, *ostr);

    return 0;
}


int main(int argc, const char* argv[])
{
    return CPrt2FsmApp().AppMain(argc, argv);
}
