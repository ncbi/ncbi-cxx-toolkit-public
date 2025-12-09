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
 * Author:  Christiam Camacho
 *
 * File Description:
 *   Demo/test program for BLAST database index detection
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>

USING_NCBI_SCOPE;

class CTestIndexDetectionApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int Run(void);
};

void CTestIndexDetectionApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test BLAST database index detection");
    
    arg_desc->AddKey("db", "database", "BLAST database name",
                     CArgDescriptions::eString);
    
    arg_desc->AddKey("dbtype", "type", "Database type",
                     CArgDescriptions::eString);
    arg_desc->SetConstraint("dbtype", &(*new CArgAllow_Strings, "prot", "nucl"));
    
    SetupArgDescriptions(arg_desc.release());
}

int CTestIndexDetectionApp::Run(void)
{
    const CArgs& args = GetArgs();
    string dbname = args["db"].AsString();
    char dbtype = (args["dbtype"].AsString() == "prot") ? 'p' : 'n';
    
    cout << "Checking database: " << dbname << endl;
    
    // Resolve and display the database path
    string resolved_path = SeqDB_ResolveDbPathNoExtension(dbname, dbtype);
    if (!resolved_path.empty()) {
        cout << "Database found at: " << resolved_path << endl;
    } else {
        cout << "Database path could not be resolved" << endl;
    }
    
    cout << "Database type: " << (dbtype == 'p' ? "protein" : "nucleotide") << endl;
    cout << endl;
    
    // Check for indexed megablast (nucleotide only)
    if (dbtype == 'n') {
        bool has_idx = SeqDB_DBIndexExists(dbname, dbtype, eIndexedMegablast);
        cout << "Indexed Megablast (.idx, .shd): " 
             << (has_idx ? "YES" : "NO") << endl;
    }
    
    // Check for kmer index (protein only)
    if (dbtype == 'p') {
        bool has_kmer = SeqDB_DBIndexExists(dbname, dbtype, eKmerIndex);
        cout << "Kmer Index (.pki, .pkd): " 
             << (has_kmer ? "YES" : "NO") << endl;
        
        bool has_prot_idx = SeqDB_DBIndexExists(dbname, dbtype, eProteinIndex);
        cout << "Protein Index (.pix): " 
             << (has_prot_idx ? "YES" : "NO") << endl;
    }
    
    return 0;
}

int main(int argc, const char* argv[])
{
    return CTestIndexDetectionApp().AppMain(argc, argv);
}
