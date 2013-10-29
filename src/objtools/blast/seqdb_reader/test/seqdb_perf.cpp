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
 * Author: Christiam Camacho
 *
 */

/** @file seqdb_perf.cpp
 * Command line tool to measure the performance of CSeqDB.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#ifdef _OPENMP
#include <omp.h>
#endif /* OPENMP */
#include <numeric>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
#endif

/// Auxiliary structure to keep
struct SMemUsage {
    size_t total, resident, shared;
    SMemUsage() : total(0), resident(0), shared(0) {}

    SMemUsage& operator+=(const SMemUsage& other) {
        this->total += other.total;
        this->resident += other.resident;
        this->shared += other.shared;
        return *this;
    }

    SMemUsage operator+(const SMemUsage& other) {
        SMemUsage retval(*this);
        retval += other;
        return retval;
    }

    friend ostream& operator<<(ostream& out, const SMemUsage& mu) {
        out << "Total memory usage: " << NStr::UInt8ToString_DataSize(mu.total) << endl;
        out << "Resident memory usage: " << NStr::UInt8ToString_DataSize(mu.resident) << endl;
        out << "Shared memory usage: " << NStr::UInt8ToString_DataSize(mu.shared) << endl;
        return out;
    }
};

/// The application class
class CSeqDBPerfApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CSeqDBPerfApp() {}
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
    
    typedef vector< CRef<CSeqDBExpert> > TDbHandles;
    /// Handle to BLAST database
    CRef<CSeqDBExpert> m_BlastDb;
    /// Is the database protein
    bool m_DbIsProtein;
    /// Vector of distinct CSeqDB objects accessing the same database
    TDbHandles m_DbHandles;
    /// Container of memory usage objects
    vector<SMemUsage> m_MemoryUsage;

    /// Initializes the application's data members
    void x_InitApplicationData();
    void x_UpdateMemoryUsage(const int thread_id = 0);
    void x_ReportMemUsage() const;

    /// Prints the BLAST database information (e.g.: handles -info command line
    /// option)
    int x_PrintBlastDatabaseInformation();

    /// Processes all requests except printing the BLAST database information
    /// @return 0 on success; 1 if some sequences were not retrieved
    int x_ScanDatabase();
};

void 
CSeqDBPerfApp::x_UpdateMemoryUsage(const int thread_id /* = 0 */)
{
    SMemUsage mu;
    if (GetMemoryUsage(&mu.total, &mu.resident, &mu.shared)) {
        m_MemoryUsage[thread_id] += mu;
    }
}

void 
CSeqDBPerfApp::x_ReportMemUsage() const
{
    SMemUsage total = std::accumulate(m_MemoryUsage.begin(),
                                      m_MemoryUsage.end(), SMemUsage());
    cout << total << endl;
}

int
CSeqDBPerfApp::x_ScanDatabase()
{
    CStopWatch sw;
    sw.Start();
    Uint8 num_letters = m_BlastDb->GetTotalLength();
    const bool kScanUncompressed = GetArgs()["scan_uncompressed"];
    vector<int> oids2iterate;
    for (int oid = 0; m_DbHandles.front()->CheckOrFindOID(oid); oid++) {
        oids2iterate.push_back(oid);
    }
    LOG_POST(Info << "Will go over " << oids2iterate.size() << " sequences");

    #pragma omp parallel default(none) num_threads(m_DbHandles.size()) \
                         shared(oids2iterate) if(m_DbHandles.size() > 1)
    { 
#ifdef _OPENMP
        int thread_id = omp_get_thread_num();
#endif
        #pragma omp for schedule(static, (oids2iterate.size()/m_DbHandles.size())) nowait
        for (size_t i = 0; i < oids2iterate.size(); i++) {
            int oid = oids2iterate[i];
            const char* buffer = NULL;
            int seqlen = 0;
            if (m_DbIsProtein || kScanUncompressed) {
                int encoding = m_DbIsProtein ? 0 : kSeqDBNuclBlastNA8;
                m_DbHandles[thread_id]->GetAmbigSeq(oid, &buffer, encoding);
                seqlen = m_DbHandles[thread_id]->GetSeqLength(oid);
            } else {
                m_DbHandles[thread_id]->GetSequence(oid, &buffer);
                seqlen = m_DbHandles[thread_id]->GetSeqLength(oid) / 4;
            }
            for (int i = 0; i < seqlen; i++) {
                char base = buffer[i];
                base = base;    // dummy statement
            }
            if (m_DbIsProtein || kScanUncompressed) {
                m_DbHandles[thread_id]->RetAmbigSeq(&buffer);
            } else {
                m_DbHandles[thread_id]->RetSequence(&buffer);
            }
        }
        x_UpdateMemoryUsage(thread_id);
    } // end of omp parallel

    sw.Stop();
    uint64_t bases = static_cast<uint64_t>(num_letters / sw.Elapsed());
    cout << "Scanning rate: " 
         << NStr::NumericToString(bases, NStr::fWithCommas) 
         << " bases/second" << endl;
    return 0;
}

void
CSeqDBPerfApp::x_InitApplicationData()
{
    CStopWatch sw;
    sw.Start();
    const CArgs& args = GetArgs();
    const CSeqDB::ESeqType kSeqType = ParseMoleculeTypeString(args["dbtype"].AsString());
    const string& kDbName(args["db"].AsString());
    m_BlastDb.Reset(new CSeqDBExpert(kDbName, kSeqType));
    m_DbIsProtein = static_cast<bool>(m_BlastDb->GetSequenceType() == CSeqDB::eProtein);

    int kNumThreads = 1;
#if (defined(_OPENMP) && defined(NCBI_THREADS))
    kNumThreads = args["num_threads"].AsInteger();
#endif
    m_DbHandles.reserve(kNumThreads);
    m_DbHandles.push_back(m_BlastDb);
    if (kNumThreads > 1) {
        for (int i = 1; i < kNumThreads; i++) {
            m_BlastDb.Reset(new CSeqDBExpert(kDbName, kSeqType));
            m_DbHandles.push_back(m_BlastDb);
        }
    }
    m_MemoryUsage.assign(kNumThreads, SMemUsage());

    sw.Stop();
    cout << "Initialization time: " << sw.AsSmartString() << endl;
}

int
CSeqDBPerfApp::x_PrintBlastDatabaseInformation()
{
    CStopWatch sw;
    sw.Start();
    _ASSERT(m_BlastDb.NotEmpty());
    static const NStr::TNumToStringFlags kFlags = NStr::fWithCommas;
    const string kLetters = m_DbIsProtein ? "residues" : "bases";
    const CArgs& args = GetArgs();

    CNcbiOstream& out = args["out"].AsOutputFile();

    // Print basic database information
    out << "Database: " << m_BlastDb->GetTitle() << endl
        << "\t" << NStr::IntToString(m_BlastDb->GetNumSeqs(), kFlags) 
        << " sequences; "
        << NStr::UInt8ToString(m_BlastDb->GetTotalLength(), kFlags)
        << " total " << kLetters << endl << endl
        << "Date: " << m_BlastDb->GetDate() 
        << "\tLongest sequence: " 
        << NStr::IntToString(m_BlastDb->GetMaxLength(), kFlags) << " " 
        << kLetters << endl;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // Print filtering algorithms supported
    out << m_BlastDb->GetAvailableMaskAlgorithmDescriptions();
#endif
    x_UpdateMemoryUsage();

    // Print volume names
    vector<string> volumes;
    m_BlastDb->FindVolumePaths(volumes,false);
    out << endl << "Volumes:" << endl;
    ITERATE(vector<string>, file_name, volumes) {
        out << "\t" << *file_name << endl;
    }
    sw.Stop();
    cout << "Time to get BLASTDB metadata: " << sw.AsSmartString() << endl;
    return 0;
}

void CSeqDBPerfApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                  "CSeqDB performance testing client");

    arg_desc->SetCurrentGroup("BLAST database options");
    arg_desc->AddDefaultKey("db", "dbname", "BLAST database name", 
                            CArgDescriptions::eString, "nr");

    arg_desc->AddDefaultKey("dbtype", "molecule_type",
                            "Molecule type stored in BLAST database",
                            CArgDescriptions::eString, "guess");
    arg_desc->SetConstraint("dbtype", &(*new CArgAllow_Strings,
                                        "nucl", "prot", "guess"));

    arg_desc->SetCurrentGroup("Retrieval options");
    arg_desc->AddFlag("scan_uncompressed", 
                      "Do a full database scan of uncompressed sequence data", true);
    arg_desc->AddFlag("scan_compressed", 
                      "Do a full database scan of compressed sequence data", true);
    arg_desc->AddFlag("get_metadata", 
                      "Retrieve BLAST database metadata", true);
    
    arg_desc->SetDependency("scan_compressed", CArgDescriptions::eExcludes, 
                            "scan_uncompressed");
    arg_desc->SetDependency("scan_compressed", CArgDescriptions::eExcludes, 
                            "get_metadata");
    arg_desc->SetDependency("scan_uncompressed", CArgDescriptions::eExcludes, 
                            "get_metadata"); 

    arg_desc->AddDefaultKey("num_threads", "number", 
                            "Number of threads to use (requires OpenMP)",
                            CArgDescriptions::eInteger, "1");
    arg_desc->SetConstraint("num_threads", new CArgAllow_Integers(0, kMax_Int));
    //arg_desc->AddFlag("one_db_handle", "Build only 1 CSeqDB object?", true);
    //arg_desc->SetDependency("one_db_handle", CArgDescriptions::eRequires, "num_threads");

    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey("out", "output_file", "Output file name", 
                            CArgDescriptions::eOutputFile, "-");

    SetupArgDescriptions(arg_desc.release());
}

int CSeqDBPerfApp::Run(void)
{
    int status = 0;

    try {
        x_InitApplicationData();
        if (GetArgs()["get_metadata"]) {
            status = x_PrintBlastDatabaseInformation();
        } else {
            status = x_ScanDatabase();
        }
        x_ReportMemUsage();
    } catch (const CSeqDBException& e) {
        LOG_POST(Error << "BLAST Database error: " << e.GetMsg());
        status = 1;
    } catch (const exception& e) {
        LOG_POST(Error << "Error: " << e.what());
        status = 1;
    } catch (...) {
        cerr << "Unknown exception!" << endl;
        status = 1;
    }
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CSeqDBPerfApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
