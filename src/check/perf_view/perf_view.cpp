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
    string xtime;
    if ( CRegexpUtil(line).Extract("^OK").empty() ) {
        xtime = string(line).substr(0,3); 
    }
    else {
        re.Reset(line);
        re.Reset( re.Extract("real  *[0-9.]*") );
        string time = re.Extract("[^ ]*$");
        //cout << "TIME: '" << time << "'" << endl;
        double dtime = NStr::StringToDouble(time);
        ostringstream os;
        os << setprecision(dtime < 100.0 ? 2 : 3) << dtime;
        xtime = os.str();
        //cout << "XTIME: '" << xtime << "'" << endl;    
    }

    m_Results[file_ext][loader][name] = xtime;
}


void CPsgPerfApplication::PrintResult()
{
    cout << R"delimiter(
T235 = TRUNK client + {{PSG-2.3.5}} server
S235 = SC-25 client + {{PSG-2.3.5}} server
T212 = TRUNK client + {{PSG-2.1.2}} server
S212 = SC-25 client + {{PSG-2.1.2}} server

For the special case of OBJMGR_PERF_TEST the test name is decomposed and then
converted into a shorter form "IDs/Threads/Parameters" -- where:
* IDs:  File of seq-ids
* Thr:  Threads (if specified)
* Par:  N = No-Split, S = Split;  BD = Bulk-Data, BB = Bulk-Bioseq

)delimiter";

    vector<string> loaders = { "PSG", "OSG", "PSOS", "UNK" };

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
            cout << run.first << " ";
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

            bool first = true;
            for (const auto& run : m_Results) {
                if (!first)
                    cout << " - ";
                first =false;
                string& time = m_Results[run.first][loader][name];
                if ( time.empty() )
                    time = "ABS";
                cout << time;
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
