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
 * Authors:  Denis Vakatov
 *
 * File Description:
 *   Compose performance summary of testsuite results ('check.sh.log' files
 *   named as 'perf_view.EXT' where EXT is the name of a run).
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/xregexp/regexp.hpp>

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CPsgPerfApplication::

class CPsgPerfApplication : public CNcbiApplication
{
    typedef string TTime;  // Test timing
    typedef string TType;  // "OSG", "PSG" or "PSOS" (or "UNK")
    typedef string TName;  // "1_acc/0/S,BD"
    typedef string TSetup; // Extension of the run file 'perf_view.EXT'

    typedef map<TName, TTime>     TNameTime;
    typedef map<TType, TNameTime> TRun;
    void ProcessFile(const string& file_ext);
    void ProcessLine(const char* line, const string& file_ext);
    void PrintResult();

    typedef map<TSetup, TRun> TResults;
    TResults m_Results;

    string ColorMe(const string& value, const TSetup& color);

    set<TName> m_Names;

private:
    virtual void Init(void);
    virtual int  Run(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments

void CPsgPerfApplication::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Compose summary of 'check.sh.log' files");

    // Describe the expected command-line arguments

    // Whether to use JIRA colors
    arg_desc->AddFlag
        ("nocolor", "Do not use JIRA colors");

    // Set of file extensions (perf_view.EXT) to process
    arg_desc->AddExtra
        (1,  // one mandatory extra args
         8,  // up to 8 extra args
         "Files to process: list of extensions (as in 'perf_view.EXT')",
         CArgDescriptions::eString);
    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)

int CPsgPerfApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    // Printout obtained argument values
    string str;
    //cout << args.Print(str) << endl;

    for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
        ProcessFile(args[extra].AsString());
    }

    PrintResult();

    return 0;
}

void CPsgPerfApplication::ProcessFile(const string& file_ext)
{
    ifstream file("perf_view." + file_ext);
    if ( !file.good() )
        throw runtime_error("Error opening file perf_view." + file_ext);

    while ( file.good() ) {
        char line[1024];

        file.getline(line, sizeof(line));
        //cout << line << endl;
        ProcessLine(line, file_ext);
    }

    if ( !file.eof() )
        throw runtime_error("Error reading file perf_view." + file_ext);
}


void CPsgPerfApplication::ProcessLine(const char* line, const string& file_ext)
{
    if ( NStr::IsBlank(line) )
        return;

    CRegexpUtil re;
    TType       loader;
    TName       name;

    if ( CRegexpUtil(line).Extract("objmgr_perf_test").empty() ) {
        loader = "UNK";

        // TEST NAME
        re.Reset(line);
        re.Reset( re.Extract("-- .*") );
        name = re.Extract("] [^(]*");
        name.erase(0, 1);
        NStr::TruncateSpacesInPlace(name);
        //NStr::ReplaceInPlace(name, "-", "\\-");
        //NStr::ReplaceInPlace(name, "+", "\\+");
        //NStr::ReplaceInPlace(name, "_", "\\_");

        //cout << "NAME: '" << name << "'" << endl;
    }
    else {  // Important special case here:  OBJMGR_PERF_TEST
        // IDS
        re.Reset(line);
        re.Reset( re.Extract("-ids  *[^ ]*") );
        string ids = re.Extract("[^ ]*$");
        //cout << "IDS: '" << ids << "'" << endl;

        // THREADS
        re.Reset(line);
        re.Reset( re.Extract("-threads  *[^ ]*") );
        string threads = re.Extract("[^ ]*$");
        if ( threads.empty() )
            threads = "-";
        //cout << "THREADS: '" << threads << "'" << endl;

        // LOADER
        re.Reset(line);
        re.Reset( re.Extract("-loader  *[^ ]*") );
        loader = re.Extract("[^ ]*$");
        if (loader == "gb") {
            if ( CRegexpUtil(line).Extract("-pubseqos").empty() )
                loader = "OSG";
            else
                loader = "PSOS";
        } else if (loader == "psg") {
            loader = "PSG";
        } else {
            loader = "UNK";
        }
        //cout << "LOADER: '" << loader << "'" << endl;

        // SPLIT
        string split;
        if ( CRegexpUtil(line).Extract("-no_split").empty() )
            split = "S";
        else
            split = "N";
        //cout << "SPLIT: '" << split << "'" << endl;

        // BULK
        re.Reset(line);
        re.Reset( re.Extract("-bulk  *[^ ]*") );
        string bulk = re.Extract("[^ ]*$");
        if (bulk == "bioseq")
            bulk = "BB";
        else if (bulk == "data")
            bulk = "BD";
        //cout << "BULK: '" << bulk << "'" << endl;

        // TEST NAME
        name = ids + "/" + threads + "/" + split;
        if ( !bulk.empty() )
            name = name + "," + bulk;
    }
    m_Names.insert(name);

    // TIME (or status, if not OK)
    string stime;
    if ( CRegexpUtil(line).Extract("^OK").empty() ) {
        stime = string(line).substr(0,3); 
    }
    else {
        re.Reset(line);
        re.Reset( re.Extract("real  *[0-9]\\.*[0-9]*") );
        string xtime = re.Extract("[^ ]*$");
        //cout << "TIME: '" << time << "'" << endl;
        double dtime = NStr::StringToDouble(xtime, NStr::fConvErr_NoThrow);
        ostringstream os;
        os << setprecision(dtime < 100.0 ? 2 : 3) << dtime;
        stime = os.str();
        //cout << "XTIME: '" << xtime << "'" << endl;    
    }

    m_Results[file_ext][loader][name] = stime;
}


string CPsgPerfApplication::ColorMe(const string& value, const TSetup& color)
{
    if ( GetArgs()["nocolor"] )
        return value;

    static map<TSetup, string> m_Colors;

    if ( m_Colors[color].empty() ) {
        static size_t color_counter = 0;
        static const vector<string> kColors =
            { "black",  "red",  "blue",  "green",
              "orange", "pink", "brown", "purple" };

        m_Colors[color] = kColors[color_counter++ % kColors.size()];
    }

    if (value == "abs")
        return "{color:grey}abs{color}";

    string x_value = value;
    if (x_value.find_first_not_of("0123456789. *") != string::npos)
        x_value = "*" + x_value + "*";

    return "{color:" + m_Colors[color] + "}" + x_value + "{color}";
}


void CPsgPerfApplication::PrintResult()
{
    // Legend

    if ( GetArgs()["nocolor"] ) {
        cout << R"delimiter(
CXYZ = client "C" against server + "PSG-X.Y.Z"
where "C" is:
*  'T' - contemporary TRUNK
*  'S' - latest SC in SVN
*  'P' - contemporary full production build
)delimiter";
    }
    else {
        cout << R"delimiter(
{{{color:blue}*C*{color}{color:red}*XYZ*{color}}} = client {color:blue}{{*C*}}{color} against server + {{PSG-{color:red}*X.Y.Z*{color}}}, _where_ {color:blue}{{*C*}}{color}:
*    '{color:blue}{{T}}{color}' - contemporary TRUNK
*    '{color:blue}{{S}}{color}' - latest SC in SVN
*    '{color:blue}{{P}}{color}' - contemporary full production build
)delimiter";
    }

    cout << R"delimiter(

For the special case of OBJMGR_PERF_TEST the test name is decomposed and then
converted into a shorter form "IDs/Threads/Parameters" -- where:
* *IDs*: file of seq-ids
* *Thr*: number of threads (if specified)
* *Par*: N = No-Split, S = Split;  BD = Bulk-Data, BB = Bulk-Bioseq

)delimiter";

    vector<TType> loaders = { "PSG", "OSG", "PSOS", "UNK" };

    // Header
    cout << " || IDs/Thr/Par || ";
    size_t loader_idx = 0;
    for (const string& loader : loaders) {
        bool loader_empty = true;
        for (const auto& x_run : m_Results) {
            for (const auto& x_loader : x_run.second) {
                if (x_loader.first != loader  ||  x_loader.second.empty())
                    continue;
                loader_empty = false;
                break;
            }
            if ( !loader_empty )
                break;
        }
        if ( loader_empty ) {
            loaders[loader_idx].erase();
            loader_idx++;
            continue;
        }
        loader_idx++;

        cout << loader << ": ";
        for (const auto& run : m_Results) {
            cout << ColorMe(run.first, run.first) << " ";
        }
       cout << " || ";
    }
    cout << endl;

    // Values
    for (const TName& name : m_Names) {
        cout << " | " << name << " | ";
        for (const string& loader : loaders) {
            if ( loader.empty() )
                continue;

            // Mark min time value(s) with bold font
            string min_stime = " ";
            double min_dtime = numeric_limits<double>::max();
            for (const auto& run : m_Results) {
                double dtime = NStr::StringToDouble
                    (m_Results[run.first][loader][name],
                     NStr::fConvErr_NoThrow);
                if (dtime  &&  dtime < min_dtime) {
                    min_dtime = dtime;
                    min_stime = m_Results[run.first][loader][name];
                }
            }
            for (const auto& run : m_Results) {
                if (m_Results[run.first][loader][name] == min_stime)
                    m_Results[run.first][loader][name] =
                        "*" + m_Results[run.first][loader][name] + "*";
            }

            // Print time values
            bool first = true;
            for (const auto& run : m_Results) {
                if (!first)
                    cout << " - ";
                first = false;
                string& time = m_Results[run.first][loader][name];
                if ( time.empty() )
                    time = "abs";
                cout << ColorMe(time, run.first);
            }
            cout << " | ";
        }
        cout << endl;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CPsgPerfApplication().AppMain(argc, argv);
}
