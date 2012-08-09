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
* Author:  Tom Madden, NCBI
*
* File Description:
*   Unit tests for remote blast services.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/blast/services/blast_services.hpp>
#include <corelib/ncbienv.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/exception.hpp>
#include <util/range.hpp>
#include <objects/blast/Blast4_database.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_ext.hpp>



// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#undef NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
using namespace ncbi::objects;

static bool
s_HasRawSequence(const CBioseq& bioseq)
{
     if (bioseq.GetInst().CanGetSeq_data() == true)
         return true;
     else if (bioseq.GetInst().IsSetExt())
     {
         if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta)
         {
              bool is_raw = true;
              ITERATE (CSeq_inst::TExt::TDelta::Tdata, iter,
                      bioseq.GetInst().GetExt().GetDelta().Get()) {
                 if ((*iter)->Which() == CDelta_seq::e_Loc) {
                     is_raw = false;
                     break;
                 }
              }
              return is_raw;
         }
     }
     return false;
}

BOOST_AUTO_TEST_SUITE(blast_services)

NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    // printf("Initialization function executed\n");
}

NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
    // printf("Finalization function executed\n");
}

BOOST_AUTO_TEST_CASE(GetInformationAboutInvalidBlastDatabaseRemotely)
{
    CBlastServices remote_svc;
    bool found = remote_svc.IsValidBlastDb("dummy", true);
    BOOST_REQUIRE(found == false);
}

BOOST_AUTO_TEST_CASE(MultipleDatabaseValidityCheck)
{
    CBlastServices remote_svc;
    bool found = remote_svc.IsValidBlastDb("nt wgs", false);
    BOOST_REQUIRE(found == true);
}

BOOST_AUTO_TEST_CASE(EmptyStringValidityCheck)
{
    CBlastServices remote_svc;
    bool found = remote_svc.IsValidBlastDb("", false);
    BOOST_REQUIRE(found == false);
}

BOOST_AUTO_TEST_CASE(OneBadDbValidityCheck)
{
    CBlastServices remote_svc;
    bool found = remote_svc.IsValidBlastDb("nt dummy", false);
    BOOST_REQUIRE(found == false);
}
#ifdef UNIT_TEST_REPEAT_DB
BOOST_AUTO_TEST_CASE(GetRepeatsFilteringDatabases)
{
    CBlastServices remote_svc;

    CRef<CBlast4_database> blastdb(new CBlast4_database);
    blastdb->SetName("repeat/repeat_9606");
    blastdb->SetType(eBlast4_residue_type_nucleotide);

    CRef<CBlast4_database_info> dbinfo = remote_svc.GetDatabaseInfo(blastdb);
    BOOST_REQUIRE(dbinfo.NotEmpty());
    BOOST_REQUIRE(dbinfo->GetDatabase() == *blastdb);

    const string title = "Homo sapiens";
    BOOST_REQUIRE_EQUAL(title, dbinfo->GetDescription());

    BOOST_REQUIRE_EQUAL((Int8)1362371, dbinfo->GetTotal_length());
    BOOST_REQUIRE_EQUAL((Int8)1267, dbinfo->GetNum_sequences());

    // Get all the databases
    vector< CRef<CBlast4_database_info> > repeat_dbs =
        remote_svc.GetOrganismSpecificRepeatsDatabases();
    // considered too fragile a test...
    //const size_t kNumAvailableRepeatsDbs = 16;
    //BOOST_REQUIRE_EQUAL(kNumAvailableRepeatsDbs, repeat_dbs.size());

    // Make sure these databases are present
    // Obtained by running 'blastdbcmd -recursive -list $BLASTDB/repeat -list_outfmt %t'
    typedef map<string, bool> TFoundDbs;
    TFoundDbs repeat_dbs_found;
    repeat_dbs_found["sapiens"] = false;
    repeat_dbs_found["rodent"] = false;
    repeat_dbs_found["thaliana"] = false;
    repeat_dbs_found["sativa"] = false; // rice
    repeat_dbs_found["mammal"] = false;
    repeat_dbs_found["fungi"] = false;
    repeat_dbs_found["elegans"] = false;
    repeat_dbs_found["gambiae"] = false;
    repeat_dbs_found["danio"] = false;
    repeat_dbs_found["melanogaster"] = false;
    repeat_dbs_found["fugu"] = false;

    ITERATE(vector< CRef<CBlast4_database_info> >, db_info, repeat_dbs) {
        NON_CONST_ITERATE(TFoundDbs, itr, repeat_dbs_found) {
            if (NStr::FindNoCase((*db_info)->GetDescription(), itr->first) != NPOS) {
                itr->second = true;
                break;
            }
        }
    }

    ITERATE(TFoundDbs, itr, repeat_dbs_found) {
        string msg("Did not find ");
        msg += itr->first + " repeats database";
        BOOST_REQUIRE_MESSAGE(itr->second, msg);
    }
}
#endif
BOOST_AUTO_TEST_CASE(GetWindowMaskedTaxIds)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    CBlastServices remote_svc;
    //remote_svc.SetVerbose();
    objects::CBlast4_get_windowmasked_taxids_reply::Tdata reply =
        remote_svc.GetTaxIdWithWindowMaskerSupport();
    BOOST_REQUIRE(!reply.empty());
    BOOST_REQUIRE(reply.find(9606) != reply.end());
}

BOOST_AUTO_TEST_CASE(GetDatabaseInfo)
{
    CBlastServices remote_svc;

    CRef<CBlast4_database> blastdb(new CBlast4_database);
    blastdb->SetName("nr");
    blastdb->SetType(eBlast4_residue_type_nucleotide);

    CRef<CBlast4_database_info> dbinfo = remote_svc.GetDatabaseInfo(blastdb);
    BOOST_REQUIRE(dbinfo.NotEmpty());
    BOOST_REQUIRE(dbinfo->GetDatabase() == *blastdb);

    const string nt_title =
        "All GenBank+EMBL+DDBJ+PDB sequences (but no EST, STS, "
        "GSS,environmental samples or phase 0, 1 or 2 HTGS sequences)";
    BOOST_REQUIRE_EQUAL(nt_title, dbinfo->GetDescription());

    BOOST_REQUIRE(dbinfo->GetTotal_length() > (Int8)15e+9);
    BOOST_REQUIRE(dbinfo->GetNum_sequences() > (Int8)35e+5);

    bool all_found;
    vector< CRef<CBlast4_database_info> > dbinfo_v = remote_svc.GetDatabaseInfo("nr", false, &all_found);
    BOOST_REQUIRE(dbinfo_v.empty() == false);
    BOOST_REQUIRE(all_found == true);
    dbinfo = dbinfo_v.front();
    BOOST_REQUIRE(dbinfo.NotEmpty());
    BOOST_REQUIRE(dbinfo->GetDatabase() == *blastdb);

    BOOST_REQUIRE_EQUAL(nt_title, dbinfo->GetDescription());

    BOOST_REQUIRE(dbinfo->GetTotal_length() > (Int8)15e+9);
    BOOST_REQUIRE(dbinfo->GetNum_sequences() > (Int8)35e+5);

    // Try fetching swissprot
    blastdb->SetName("swissprot");
    blastdb->SetType(eBlast4_residue_type_protein);

    dbinfo = remote_svc.GetDatabaseInfo(blastdb);
    BOOST_REQUIRE(dbinfo.NotEmpty());
    BOOST_REQUIRE(dbinfo->GetDatabase() == *blastdb);

    const string swissprot_title("Non-redundant UniProtKB/SwissProt sequences.");
    BOOST_REQUIRE_EQUAL(swissprot_title, dbinfo->GetDescription());

    BOOST_REQUIRE(dbinfo->GetTotal_length() > (Int8)7e+7);
    BOOST_REQUIRE(dbinfo->GetNum_sequences() > (Int8)15e+4);

    // Try fetching a non-existent database
    blastdb->SetName("junk");
    blastdb->SetType(eBlast4_residue_type_protein);
    dbinfo = remote_svc.GetDatabaseInfo(blastdb);
    BOOST_REQUIRE(dbinfo.Empty());

    // Try a frozen database
    blastdb->SetName("ecoli");
    blastdb->SetType(eBlast4_residue_type_nucleotide);
    dbinfo = remote_svc.GetDatabaseInfo(blastdb);
    BOOST_REQUIRE(dbinfo.NotEmpty());

    BOOST_REQUIRE(dbinfo->GetDatabase() == *blastdb);
    //BOOST_REQUIRE(dbinfo->GetDescription().empty());
    //BOOST_REQUIRE_EQUAL(string("12/12/2002 14:17:00"),
    //                     dbinfo->GetLast_updated());
    BOOST_REQUIRE_EQUAL((Int8)400, dbinfo->GetNum_sequences());
    BOOST_REQUIRE_EQUAL((Int8)4662239, dbinfo->GetTotal_length());

    // Try unknown residue type
    blastdb->SetName("patnt");
    blastdb->SetType(eBlast4_residue_type_unknown);
    dbinfo = remote_svc.GetDatabaseInfo(blastdb);
    BOOST_REQUIRE(dbinfo.Empty());

    // Fetch the structure group's favorite RPS-BLAST database
    blastdb->SetName("cdd");
    blastdb->SetType(eBlast4_residue_type_protein);
    dbinfo = remote_svc.GetDatabaseInfo(blastdb);
    BOOST_REQUIRE(dbinfo.NotEmpty());
    BOOST_REQUIRE(dbinfo->GetDatabase() == *blastdb);

// This is no longer true - (why?)
    //BOOST_REQUIRE(dbinfo->GetDescription().find("cdd.v") != NPOS);
}

BOOST_AUTO_TEST_CASE(GetBioseq)
{
    CBlastServices::TBioseqVector results;
    string errors, warnings;
    CBlastServices::TSeqIdVector queries;
    queries.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Gi, 555)));
    CBlastServices::GetSequences(queries, "nt", 'n', results, errors, warnings);
    const size_t kResultsSize = 1;
    BOOST_REQUIRE_EQUAL(kResultsSize, results.size());
    BOOST_REQUIRE_EQUAL((unsigned int)624, results[0]->GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(FetchQuerySequence_NonExistentDb)
{   
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    CBlastServices::TBioseqVector results;
    string errors, warnings;
    CBlastServices::TSeqIdVector queries;
    queries.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Gi, 555)));
        
    CBlastServices::GetSequences(queries, "junk", 'p', results, errors,
                               warnings/*, true*/);
    const string kExpectedError("Failed to open databases: [junk]");
    BOOST_REQUIRE_EQUAL(kExpectedError, errors);
    BOOST_REQUIRE(warnings.empty());
    BOOST_REQUIRE(results.empty());
}   
    
BOOST_AUTO_TEST_CASE(FetchQuerySequence_NoQueries)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    CBlastServices::TBioseqVector results;
    string errors, warnings;
    CBlastServices::TSeqIdVector queries;
    CBlastServices::GetSequences(queries, "nt", 'p', results, errors, warnings);
    BOOST_REQUIRE_EQUAL(string("Error: no sequences requested."), errors);
    BOOST_REQUIRE(warnings.empty());
    BOOST_REQUIRE(results.empty());
}

BOOST_AUTO_TEST_CASE(FetchMultipleSequences)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string kDbName("ecoli");
    const char kSeqType('n');
    int ecoli_gis[] = { 1786181, 1786192, 2367095, 1786217, 1786230, 1786240,
        1786250, 1786262, 1786283, 1786298 };

    CBlastServices::TSeqIdVector queries;
    size_t i = 0;
    for (i = 0; i < sizeof(ecoli_gis)/sizeof(*ecoli_gis); i++) {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, ecoli_gis[i]));
        queries.push_back(id);
    }

    string warnings, errors;
    CBlastServices::TBioseqVector results;

    CBlastServices::GetSequences(queries, kDbName, kSeqType, results, errors,
                               warnings/*, true*/);
    BOOST_REQUIRE_EQUAL(queries.size(), results.size());
    BOOST_REQUIRE(errors.empty());
    BOOST_REQUIRE(warnings.empty());

    i = 0;
    ITERATE(CBlastServices::TBioseqVector, bs, results) {
        const CBioseq::TId& ids = (*bs)->GetId();
        ITERATE(CBioseq::TId, id, ids) {
            if ((*id)->IsGi()) {
                BOOST_REQUIRE_EQUAL(ecoli_gis[i++], (*id)->GetGi());
                break;
            }
        }
        BOOST_REQUIRE(s_HasRawSequence(**bs));
    }
}

BOOST_AUTO_TEST_CASE(FetchMultipleSequences_NotFound)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string kDbName("ecoli");
    const char kSeqType('n');
    const int kGiNotFound(555); // this GI shouldn't be found in ecoli
    int ecoli_gis[] = { 1786181, 1786192, kGiNotFound, 1786217, 1786230,
        1786240, 1786250, 1786262, 1786283, 1786298 };

    CBlastServices::TSeqIdVector queries;
    for (size_t i = 0; i < sizeof(ecoli_gis)/sizeof(*ecoli_gis); i++) {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, ecoli_gis[i]));
        queries.push_back(id);
    }

    string warnings, errors;
    CBlastServices::TBioseqVector results;

    CBlastServices::GetSequences(queries, kDbName, kSeqType, results, errors,
                               warnings/*, true*/);
    BOOST_REQUIRE_EQUAL(queries.size() - 1, results.size());
    BOOST_REQUIRE( !errors.empty() );
    BOOST_REQUIRE( errors.find("Failed to fetch sequence") != NPOS );
    BOOST_REQUIRE( errors.find(NStr::IntToString(kGiNotFound)) != NPOS );
    BOOST_REQUIRE(warnings.empty());
}

BOOST_AUTO_TEST_CASE(FetchQuerySequenceInfo_NonExistentDb)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    CBlastServices::TBioseqVector results;
    string errors, warnings;
    CBlastServices::TSeqIdVector queries;
    queries.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Gi, 555)));

    CBlastServices::GetSequencesInfo(queries, "junk", 'p', results, errors,
                               warnings/*, true*/);
    const string kExpectedError("Failed to open databases: [junk]");
    BOOST_REQUIRE_EQUAL(kExpectedError, errors);
    BOOST_REQUIRE(warnings.empty());
    BOOST_REQUIRE(results.empty());
}

BOOST_AUTO_TEST_CASE(FetchQuerySequenceInfo_NoQueries)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    CBlastServices::TBioseqVector results;
    string errors, warnings;
    CBlastServices::TSeqIdVector queries;
    CBlastServices::GetSequencesInfo(queries, "nt", 'p', results, errors,
                                   warnings);
    BOOST_REQUIRE_EQUAL(string("Error: no sequences requested."), errors);
    BOOST_REQUIRE(warnings.empty());
    BOOST_REQUIRE(results.empty());
}

BOOST_AUTO_TEST_CASE(FetchQuerySequenceInfo)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");

    CRef<CSeq_id> seqid(new CSeq_id(CSeq_id::e_Gi, 129295));
    CBlastServices::TSeqIdVector getseq_queries;
    getseq_queries.push_back(seqid);
    
    // Now fetch the sequence.
    
    string warnings, errors;
    CBlastServices::TBioseqVector results;
    
    CBlastServices::GetSequencesInfo(getseq_queries, "nr", 'p', results, errors,
                                   warnings);
    
    BOOST_REQUIRE_EQUAL(getseq_queries.size(), results.size());
    BOOST_REQUIRE(results[0].NotEmpty());
    int length = results[0]->GetLength();
    BOOST_REQUIRE_EQUAL(232, length);
    BOOST_REQUIRE(!s_HasRawSequence(*results[0]));
}

BOOST_AUTO_TEST_CASE(FetchQuerySequenceInfo_NotFound)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const int kGi(129295);
    CRef<CSeq_id> seqid(new CSeq_id(CSeq_id::e_Gi, kGi));
    CBlastServices::TSeqIdVector getseq_queries;
    getseq_queries.push_back(seqid);
    
    string warnings, errors;
    CBlastServices::TBioseqVector results;
    
    CBlastServices::GetSequencesInfo(getseq_queries, "nr", 'n',
                               results,   // out
                               errors,    // out
                               warnings/*,  // out
                               true*/); // out
    
    BOOST_REQUIRE(results.empty());
    BOOST_REQUIRE( !errors.empty() );
    BOOST_REQUIRE( errors.find("Failed to fetch sequence") != NPOS );
    BOOST_REQUIRE( errors.find(NStr::IntToString(kGi)) != NPOS );
    BOOST_REQUIRE(warnings.empty());
}

BOOST_AUTO_TEST_CASE(FetchMultipleSequencesInfo)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string kDbName("ecoli");
    const char kSeqType('n');
    int ecoli_gis[] = { 1786181, 1786192, 2367095, 1786217, 1786230, 1786240,
        1786250, 1786262, 1786283, 1786298 };

    CBlastServices::TSeqIdVector queries;
    size_t i = 0;
    for (i = 0; i < sizeof(ecoli_gis)/sizeof(*ecoli_gis); i++) {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, ecoli_gis[i]));
        queries.push_back(id);
    }

    string warnings, errors;
    CBlastServices::TBioseqVector results;

    CBlastServices::GetSequencesInfo(queries, kDbName, kSeqType, results, errors,
                               warnings/*, true*/);
    BOOST_REQUIRE_EQUAL(queries.size(), results.size());
    BOOST_REQUIRE(errors.empty());
    BOOST_REQUIRE(warnings.empty());

    i = 0;
    ITERATE(CBlastServices::TBioseqVector, bs, results) {
        const CBioseq::TId& ids = (*bs)->GetId();
        ITERATE(CBioseq::TId, id, ids) {
            if ((*id)->IsGi()) {
                BOOST_REQUIRE_EQUAL(ecoli_gis[i++], (*id)->GetGi());
                break;
            }
        }
        BOOST_REQUIRE( !s_HasRawSequence(**bs) );
    }
}

BOOST_AUTO_TEST_CASE(FetchMultipleSequencesInfo_NotFound)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string kDbName("ecoli");
    const char kSeqType('n');
    const int kGiNotFound(555); // this GI shouldn't be found in ecoli
    int ecoli_gis[] = { 1786181, 1786192, kGiNotFound, 1786217, 1786230,
        1786240, 1786250, 1786262, 1786283, 1786298 };

    CBlastServices::TSeqIdVector queries;
    for (size_t i = 0; i < sizeof(ecoli_gis)/sizeof(*ecoli_gis); i++) {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, ecoli_gis[i]));
        queries.push_back(id);
    }

    string warnings, errors;
    CBlastServices::TBioseqVector results;

    CBlastServices::GetSequencesInfo(queries, kDbName, kSeqType, results, errors,
                               warnings/*, true*/);
    BOOST_REQUIRE_EQUAL(queries.size() - 1, results.size());
    BOOST_REQUIRE( !errors.empty() );
    BOOST_REQUIRE( errors.find("Failed to fetch sequence") != NPOS );
    BOOST_REQUIRE( errors.find(NStr::IntToString(kGiNotFound)) != NPOS );
    BOOST_REQUIRE(warnings.empty());
}

BOOST_AUTO_TEST_CASE(FetchQuerySequenceParts_NonExistentDb)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    CBlastServices::TSeqIntervalVector queries;
    CBlastServices::TSeqDataVector results;
    string errors, warnings;
    CBlastServices::TSeqIdVector ids;
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 555));
    queries.push_back(CRef<CSeq_interval>(new CSeq_interval(*id, 0, 2)));

    CBlastServices::GetSequenceParts(queries, "junk", 'p', ids, results, errors,
                               warnings/*, true*/);
    const string kExpectedError("Failed to open databases: [junk]");
    BOOST_REQUIRE_EQUAL(kExpectedError, errors);
    BOOST_REQUIRE(warnings.empty());
    BOOST_REQUIRE(results.empty());
    BOOST_REQUIRE(ids.empty());
}

BOOST_AUTO_TEST_CASE(FetchQuerySequenceParts_NoQueries)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    CBlastServices::TSeqIntervalVector queries;
    CBlastServices::TSeqDataVector results;
    string errors, warnings;
    CBlastServices::TSeqIdVector ids;

    CBlastServices::GetSequenceParts(queries, "nt", 'n', ids, results, errors,
                                   warnings);
    BOOST_REQUIRE_EQUAL(string("Error: no sequences requested."), errors);
    BOOST_REQUIRE(warnings.empty());
    BOOST_REQUIRE(results.empty());
    BOOST_REQUIRE(ids.empty());
}

BOOST_AUTO_TEST_CASE(FetchQuerySequenceParts)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");

    CBlastServices::TSeqIntervalVector queries;
    CBlastServices::TSeqDataVector results;
    string errors, warnings;
    CBlastServices::TSeqIdVector ids;
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 556));
    const TSeqRange range(100,200);
    queries.push_back(CRef<CSeq_interval>(new CSeq_interval(*id, 
                                                            range.GetFrom(),
                                                            range.GetTo())));

    CBlastServices::GetSequenceParts(queries, "nt", 'n', ids, results, errors,
                                   warnings);
    
    BOOST_REQUIRE(results.size());
    BOOST_REQUIRE(results[0].NotEmpty());
    BOOST_REQUIRE(results[0]->IsNcbi4na());
    BOOST_REQUIRE_EQUAL((TSeqPos)range.GetLength()/2,
                        results[0]->GetNcbi4na().Get().size());
}

BOOST_AUTO_TEST_CASE(FetchQuerySequenceParts_NotFound)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const int kGi(129295);
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, kGi));
    const TSeqRange range(50, 300);
    CBlastServices::TSeqIntervalVector queries;
    queries.push_back(CRef<CSeq_interval>(new CSeq_interval(*id, 
                                                            range.GetFrom(),
                                                            range.GetTo())));
    CBlastServices::TSeqIdVector ids;
    string warnings, errors;
    CBlastServices::TSeqDataVector results;
    
    CBlastServices::GetSequenceParts(queries, "nr", 'n', ids, results, errors,
                                   warnings);
    
    BOOST_REQUIRE(results.empty());
    BOOST_REQUIRE( !errors.empty() );
    BOOST_REQUIRE( errors.find("Failed to fetch sequence") != NPOS );
    BOOST_REQUIRE( errors.find(NStr::IntToString(kGi)) != NPOS );
    BOOST_REQUIRE(warnings.empty());
    BOOST_REQUIRE(ids.empty());
}

BOOST_AUTO_TEST_CASE(FetchMultipleSequencesParts)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string kDbName("ecoli");
    const char kSeqType('p');
    int ecoli_gis[] = { 1786182, 1786183, 1786184, 1786185, 1786186, 1786187,
        1786188, 1786189, 1786190, 1786191
    };
    CBlastServices::TSeqIntervalVector queries;
    const TSeqRange range(0, 20); // all queries are at least 20 residues

    size_t i = 0;
    for (i = 0; i < sizeof(ecoli_gis)/sizeof(*ecoli_gis); i++) {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, ecoli_gis[i]));
        CRef<CSeq_interval> val(new CSeq_interval(*id, range.GetFrom(),
                                                  range.GetTo()));
        queries.push_back(val);
    }

    string warnings, errors;
    CBlastServices::TSeqDataVector results;
    CBlastServices::TSeqIdVector ids;

    CBlastServices::GetSequenceParts(queries, kDbName, kSeqType, ids, 
                                   results, errors, warnings/*, true*/);
    BOOST_REQUIRE_EQUAL(queries.size(), results.size());
    BOOST_REQUIRE(errors.empty());
    BOOST_REQUIRE(warnings.empty());

    ITERATE(CBlastServices::TSeqDataVector, seq_data, results) {
        BOOST_REQUIRE(seq_data->NotEmpty());
        BOOST_REQUIRE((*seq_data)->IsNcbistdaa());
        BOOST_REQUIRE_EQUAL(range.GetLength() - 1,
                            (*seq_data)->GetNcbistdaa().Get().size());
    }
    i = 0;
    ITERATE(CBlastServices::TSeqIdVector, id, ids) {
        if ((*id)->IsGi()) {
            BOOST_REQUIRE_EQUAL(ecoli_gis[i++], (*id)->GetGi());
        }
    }
}

BOOST_AUTO_TEST_CASE(FetchMultipleSequencesParts_NotFound)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string kDbName("ecoli");
    const char kSeqType('n');
    const int kGiNotFound(555); // this GI shouldn't be found in ecoli
    int ecoli_gis[] = { 1786181, 1786192, kGiNotFound, 1786217, 1786230,
        1786240, 1786250, 1786262, 1786283, 1786298 };

    CBlastServices::TSeqIntervalVector queries;
    const TSeqRange range(0, 20); // all queries are at least 20 residues

    size_t i = 0;
    for (i = 0; i < sizeof(ecoli_gis)/sizeof(*ecoli_gis); i++) {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, ecoli_gis[i]));
        CRef<CSeq_interval> val(new CSeq_interval(*id, range.GetFrom(),
                                                  range.GetTo()));
        queries.push_back(val);
    }

    string warnings, errors;
    CBlastServices::TSeqDataVector results;
    CBlastServices::TSeqIdVector ids;

    CBlastServices::GetSequenceParts(queries, kDbName, kSeqType, ids, 
                                   results, errors, warnings/*, true*/);
    BOOST_REQUIRE_EQUAL(queries.size() - 1, results.size());
    BOOST_REQUIRE_EQUAL(results.size(), ids.size());
    BOOST_REQUIRE( !errors.empty());
    BOOST_REQUIRE( errors.find("Failed to fetch sequence") != NPOS );
    BOOST_REQUIRE( errors.find(NStr::IntToString(kGiNotFound)) != NPOS );
    BOOST_REQUIRE(warnings.empty());
}
BOOST_AUTO_TEST_SUITE_END()

