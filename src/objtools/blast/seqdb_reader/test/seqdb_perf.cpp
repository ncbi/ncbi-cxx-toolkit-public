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
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
#endif

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
    
    /// Handle to BLAST database
    CRef<CSeqDBExpert> m_BlastDb;
    /// Is the database protein
    bool m_DbIsProtein;

    /// Initializes the application's data members
    void x_InitApplicationData();

    /// Prints the BLAST database information (e.g.: handles -info command line
    /// option)
    int x_PrintBlastDatabaseInformation();

    /// Processes all requests except printing the BLAST database information
    /// @return 0 on success; 1 if some sequences were not retrieved
    int x_ScanDatabase();
};

int
CSeqDBPerfApp::x_ScanDatabase()
{
    CStopWatch sw;
    sw.Start();
    Uint8 num_letters = m_BlastDb->GetTotalLength();

    if (m_DbIsProtein || GetArgs()["scan_uncompressed"]) {
        for (int oid = 0; m_BlastDb->CheckOrFindOID(oid); oid++) {
            const char* buffer = NULL;
            int encoding = m_DbIsProtein ? 0 : kSeqDBNuclBlastNA8;
            m_BlastDb->GetAmbigSeq(oid, &buffer, encoding);
            int seqlen = m_BlastDb->GetSeqLength(oid);
            for (int i = 0; i < seqlen; i++) {
                char base = buffer[i];
                base = base;    // dummy statement
            }
            m_BlastDb->RetAmbigSeq(&buffer);
        }
    } else {
        _ASSERT(GetArgs()["scan_compressed"]);
        for (int oid = 0; m_BlastDb->CheckOrFindOID(oid); oid++) {
            const char* buffer = NULL;
            m_BlastDb->GetSequence(oid, &buffer);
            int seqlen = m_BlastDb->GetSeqLength(oid);
            for (int i = 0; i < seqlen; i++) {
                char base = buffer[i];
                base = base;    // dummy statement
            }
            m_BlastDb->RetSequence(&buffer);
        }
    }
    sw.Stop();
    cout << setiosflags(ios::fixed) << setprecision(2) 
         << num_letters / sw.Elapsed() << " bases/second" << endl;
    return 0;
}

void
CSeqDBPerfApp::x_InitApplicationData()
{
    CStopWatch sw;
    sw.Start();
    const CArgs& args = GetArgs();
    CSeqDB::ESeqType seqtype = ParseMoleculeTypeString(args["dbtype"].AsString());
    m_BlastDb.Reset(new CSeqDBExpert(args["db"].AsString(), seqtype));
    m_DbIsProtein = static_cast<bool>(m_BlastDb->GetSequenceType() == CSeqDB::eProtein);
    sw.Stop();
    cout << "Initialization: " << sw.AsSmartString() << endl;
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

    // Print volume names
    vector<string> volumes;
    m_BlastDb->FindVolumePaths(volumes,false);
    out << endl << "Volumes:" << endl;
    ITERATE(vector<string>, file_name, volumes) {
        out << "\t" << *file_name << endl;
    }
    sw.Stop();
    cout << "Get BLASTDB metadata: " << sw.AsSmartString() << endl;
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
