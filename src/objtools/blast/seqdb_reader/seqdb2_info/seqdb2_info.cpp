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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Minimalistic application, with command-line arguments' processing
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbsqlite.hpp>

#include <ctime>
#include <algorithm>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#include <sstream>
using std::ostringstream;


/////////////////////////////////////////////////////////////////////////////
//  CSeqdb2InfoApplication::


class CSeqdb2InfoApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Initialize arguments


void CSeqdb2InfoApplication::Init(void)
{
    // Create command-line argument descriptions class.
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context.
    arg_desc->SetUsageContext(
            GetArguments().GetProgramBasename(),
            "seqdb2_info"
    );

    // Add required keys.
    arg_desc->AddKey(
            "db",
            "in_db",
            "Database (SQLite)",
            CArgDescriptions::eString
    );

    // Add flags.
    arg_desc->AddFlag(
            "volinfo",
            "Display database source volume information",
            true
    );

    // Add optional keys.
    arg_desc->AddOptionalKey(
            "entry",
            "accession_or_OID",
            "'all' selects all entries, OID if numeric, otherwise accession",
            CArgDescriptions::eString
    );

    arg_desc->AddOptionalKey(
            "entry_batch",
            "accession_list_file",
            "Name of file containing accessions to be searched, "
            "one accession per line",
            CArgDescriptions::eInputFile
    );

    // Set dependencies.
    arg_desc->SetDependency(
            "entry",
            CArgDescriptions::eExcludes,
            "entry_batch"
    );
    arg_desc->SetDependency(
            "entry_batch",
            CArgDescriptions::eExcludes,
            "volinfo"
    );
    arg_desc->SetDependency(
            "volinfo",
            CArgDescriptions::eExcludes,
            "entry"
    );

    // Setup arg.descriptions for this application.
    SetupArgDescriptions(arg_desc.release());
}


// Calculate time interval from two calls to C library function clock(),
// which returns CPU seconds (not live time) from the beginning of the
// process, in clock ticks.  CLOCKS_PER_SEC is defined by the C library
// for the specific hardware the process is running on.
// This function is equivalent to C library function difftime(), which
// measures the live time between two calls to time(), in whole seconds.
double diffclock(clock_t toc, clock_t tic)
{
    return (double) (toc - tic) / (double) CLOCKS_PER_SEC;
}


// Return true if string can be parsed as a valid positive integer.
bool isPositiveInteger(const string& s)
{
    if (s.empty()) {
        return false;
    }
    for (size_t i = 0; i < s.size(); ++i) {
        if (!isdigit(s[i])) {
            return false;
        }
    }
    return true;
}


// Convert a signed integer to its string equivalent.
string itos(const int n)
{
    // Trivial case of zero.
    if (n == 0) return "0";
    // Need absolute value of n.
    int na = (n < 0) ? -n : n;
    // This loop forms the string, LS digit to MS digit.
    string s;
    while (na != 0) {
        s = string(1, ((char) (na % 10) + '0')) + s;
        na /= 10;
    }
    // Prepend the '-' sign if necessary.
    if (n < 0) s = '-' + s;
    return s;
}


// Strip leading and trailing whitespace off a C-style string, and return
// the result as a C++ string object.
string trim(char* c)
{
    // Point to last (non-null) character.
    char* e = &c[strlen(c) - 1];
    // Strip off whitespace in the back.
    while (isspace(*e)  &&  (e >= c)) {
        *e-- = '\0';
    }
    // Point to first character.
    e = c;
    // Move pointer forward past whitespace.
    while (isspace(*e)  &&  (*e != '\0')) {
        ++e;
    }
    // Return what remains as string.
    return string(e);
}


/////////////////////////////////////////////////////////////////////////////
//  Run application


int CSeqdb2InfoApplication::Run(void)
{
    // Initialize SQLite package.
    CSQLITE_Global::Initialize();

    // Get arguments.
    const CArgs& args = GetArgs();

    // Open SQLite database.
    CSeqDBSqlite sqldb(args["db"].AsString());
    sqldb.SetCacheSize(256L << 20);     // 256 MB

    if (args["entry_batch"].HasValue()) {

        // A text file of accessions, one per line, has been passed to
        // this application.
        string accList = args["entry_batch"].AsString();
        ifstream is(accList);
        list<string> accessions;
        char acc[64];
        // Copy each accession into a list container.
        while (true) {
            is.getline(acc, sizeof acc);
            if (strlen(acc) == 0) {
                break;
            }
            accessions.push_back(string(acc));
        }
        int numAccessions = accessions.size();

        // Get OIDs for accession list.  OIDs will be mapped one-to-one
        // with their accessions.
        clock_t tic, toc;
        tic = clock();
        vector<int> oids = sqldb.GetOids(accessions);
        toc = clock();

        // Display results, counting all found entries only (signaled by
        // *it being non-negative).
        int nf = 0;
        list<string>::iterator ir = accessions.begin();
        vector<int>::iterator it = oids.begin();
        while (ir != accessions.end()  &&  it != oids.end()) {
            if (*it >= 0) {
                ++nf;
            }
            cout << *ir++ << " " << *it++ << endl;
        }
        cerr << "Found " << nf << " OIDs out of "
                << accessions.size() << " accessions" << endl;
        double elapsed = diffclock(toc, tic);
        if (elapsed > 0.0) {
            cerr << (double) numAccessions / elapsed
                    << " accessions per CPU second" << endl;
        }

    } else if (args["entry"].HasValue()) {

        // Fetch argument-list string following -entry.
        string entry = args["entry"].AsString();
        if (entry == "all") {

            // Special parameter "all" means all records will be dumped.
            unsigned int numRecords = 0;
            string acc;
            int ver;
            int oid;
            while (sqldb.StepAccessions(&acc, &ver, &oid)) {
                cout << acc << "." << ver << " " << oid << endl;
                ++numRecords;
            }

        } else if (isPositiveInteger(entry)) {

            // If parameter is a valid non-negative integer, it won't be
            // a valid accession, so we treat it as an OID and perform
            // a reverse-search (OID-to-accessions).
            int oid = atoi(args["entry"].AsString().c_str());
            list<string> accs = sqldb.GetAccessions(oid);
            list<string>::iterator it = accs.begin();
            while (it != accs.end()) {
                cout << *it++ << endl;
            }

        } else {

            // Assume this is a string accession.
            int oid = sqldb.GetOid(entry);
            if (oid == CSeqDBSqlite::kAmbiguous) {
                cout << "Accession.version ambiguous" << endl;
            } else if (oid == CSeqDBSqlite::kNotFound) {
                cout << "Accession not found" << endl;
            } else {
                cout << oid << endl;
            }

        }

    } else {

        // If "volinfo" is selected, display the volume info
        // from the database first.
        if (args["volinfo"].AsBoolean()) {
            struct SVol {
                string m_path;
                string m_modtime;
                string m_volume;
                string m_numoids;
                SVol(string p, int m, int v, int n) :
                    m_path(p),
                    m_volume(itos(v)),
                    m_numoids(itos(n))
                {
                    long lt = (long) m;
                    m_modtime = trim(ctime(&lt));
                }
            };
            list<SVol> vols;

            // Track maximum widths of columns.
            const string sep("|");
            const string label_path("path");
            const string label_modtime("modtime");
            const string label_volume("volume");
            const string label_numoids("numoids");
            size_t l_path    = label_path.size();
            size_t l_modtime = label_modtime.size();
            size_t l_volume  = label_volume.size();
            size_t l_numoids = label_numoids.size();

            string path;
            int modtime, volume, numoids;
            while (sqldb.StepVolumes(&path, &modtime, &volume, &numoids)) {
                SVol svol(path, modtime, volume, numoids);
                l_path    = max(l_path,    svol.m_path.size());
                l_modtime = max(l_modtime, svol.m_modtime.size());
                l_volume  = max(l_volume,  svol.m_volume.size());
                l_numoids = max(l_numoids, svol.m_numoids.size());
                vols.push_back(svol);
            }

            // Add padding to column headers.
            string buf = label_path;
            int pad = l_path - label_path.size();
            if (pad > 0) buf += string(pad, ' ');
            buf += sep + label_modtime;
            pad = l_modtime - label_modtime.size();
            if (pad > 0) buf += string(pad, ' ');
            buf += sep + label_volume;
            pad = l_volume - label_volume.size();
            if (pad > 0) buf += string(pad, ' ');
            buf += sep + label_numoids;
            pad = l_numoids - label_numoids.size();
            if (pad > 0) buf += string(pad, ' ');
            cout << buf << endl;
            cout << string(buf.size(), '-') << endl;

            // ADd padding to column data.
            list<SVol>::iterator it = vols.begin();
            while (it != vols.end()) {
                buf = it->m_path;
                pad = l_path - it->m_path.size();
                if (pad > 0) buf += string(pad, ' ');
                buf += sep;
                pad = l_modtime - it->m_modtime.size();
                if (pad > 0) buf += string(pad, ' ');
                buf += it->m_modtime + sep;
                pad = l_volume - it->m_volume.size();
                if (pad > 0) buf += string(pad, ' ');
                buf += it->m_volume + sep;
                pad = l_numoids - it->m_numoids.size();
                if (pad > 0) buf += string(pad, ' ');
                buf += it->m_numoids;
                cout << buf << endl;
                ++it;
            }
        }

        // Count up the total number of accessions.
        unsigned int numRecords = 0;
        while (sqldb.StepAccessions(NULL, NULL, NULL)) {
            ++numRecords;
        }
        // Print summary.
        cout << "\n" << numRecords << " entries found" << endl;

    }

    // Finalize SQLite package.
    CSQLITE_Global::Finalize();

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CSeqdb2InfoApplication::Exit(void)
{
    // Do your after-Run() cleanup here
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    // Execute main application function; change argument list to
    // (argc, argv, 0, eDS_Default, 0) if there's no point in trying
    // to look for an application-specific configuration file.
    return CSeqdb2InfoApplication().AppMain(
            argc, argv, 0, eDS_Default, 0
    );
}
