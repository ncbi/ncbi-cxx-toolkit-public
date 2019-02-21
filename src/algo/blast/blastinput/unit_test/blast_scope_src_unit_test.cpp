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
 */

/** @file blast_scope_src_unit_test.cpp
 */
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/entrez2/entrez2_client.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include "blast_input_unit_test_aux.hpp"
#if defined(NCBI_COMPILER_WORKSHOP) && defined(NDEBUG) && defined(NCBI_WITHOUT_MT) && defined(__i386) && NCBI_COMPILER_VERSION == 550
#  define BUGGY_COMPILER
#endif

#if !defined(SKIP_DOXYGEN_PROCESSING) && !defined(BUGGY_COMPILER)

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

const char* CAutoNcbiConfigFile::kSection = "BLAST";
const char* CAutoNcbiConfigFile::kDataLoaders = "DATA_LOADERS";
const char* CAutoNcbiConfigFile::kProtBlastDbDataLoader 
    = "BLASTDB_PROT_DATA_LOADER";
const char* CAutoNcbiConfigFile::kNuclBlastDbDataLoader
    = "BLASTDB_NUCL_DATA_LOADER";

BOOST_AUTO_TEST_SUITE(blast_scope_src)

BOOST_AUTO_TEST_CASE(RetrieveFromBlastDb_TestSequenceData) 
{
    CSeq_id seqid(CSeq_id::e_Gi, 129295);

    const char* seq =
"QIKDLLVSSSTDLDTTLVLVNAIYFKGMWKTAFNAEDTREMPFHVTKQESKPVQMMCMNNSFNVATLPAEKMKILELPFASGDLSMLVLLPDEVSDLERIEKTINFEKLTEWTNPNTMEKRRVKVYLPQMKIEEKYNLTSVLMALGMTDLFIPSANLTGISSAESLKISQAVHGAFMELSEDGIEMAGSTGVIEDIKHSPESEQFRADHPFLFLIKHNPTNTIVYFGRYWSP";

    CAutoNcbiConfigFile afc;
    afc.SetProteinBlastDbDataLoader(SDataLoaderConfig::kDefaultProteinBlastDb);
    SDataLoaderConfig dlconfig(true);
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName, 
                      string(SDataLoaderConfig::kDefaultProteinBlastDb));
    CBlastScopeSourceWrapper scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();
    string data_loader_name("BLASTDB_");
    data_loader_name += string(SDataLoaderConfig::kDefaultProteinBlastDb);
    data_loader_name += "Protein";
    BOOST_REQUIRE_EQUAL(data_loader_name,
                        scope_source.GetBlastDbLoaderName());

    CBioseq_Handle bh = scope->GetBioseqHandle(seqid);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK_EQUAL((char)seq[i], (char)sv[i]);
    }
}

// This is supposed to similate the absence of a .ncbirc file
BOOST_AUTO_TEST_CASE(RetrieveFromDefaultBlastDb_NoNcbirc) 
{
    CSeq_id seqid(CSeq_id::e_Gi, 129295);

    const char* seq =
"QIKDLLVSSSTDLDTTLVLVNAIYFKGMWKTAFNAEDTREMPFHVTKQESKPVQMMCMNNSFNVATLPAEKMKILELPFASGDLSMLVLLPDEVSDLERIEKTINFEKLTEWTNPNTMEKRRVKVYLPQMKIEEKYNLTSVLMALGMTDLFIPSANLTGISSAESLKISQAVHGAFMELSEDGIEMAGSTGVIEDIKHSPESEQFRADHPFLFLIKHNPTNTIVYFGRYWSP";

    CAutoEnvironmentVariable auto_env1("NCBI", "/dev/null");
    CAutoEnvironmentVariable auto_env2("BLASTDB", "/blast/db/blast");
    CNcbiApplication* app = CNcbiApplication::Instance();
    app->ReloadConfig(CMetaRegistry::fAlwaysReload, 0);

    SDataLoaderConfig dlconfig(true);
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName, 
                      string(SDataLoaderConfig::kDefaultProteinBlastDb));
    CBlastScopeSourceWrapper scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();
    string data_loader_name("BLASTDB_");
    data_loader_name += string(SDataLoaderConfig::kDefaultProteinBlastDb);
    data_loader_name += "Protein";
    BOOST_REQUIRE_EQUAL(data_loader_name,
                        scope_source.GetBlastDbLoaderName());

    CBioseq_Handle bh = scope->GetBioseqHandle(seqid);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK_EQUAL((char)seq[i], (char)sv[i]);
    }
    NcbiTestGetRWConfig().IncludeNcbircIfAllowed();
}

// Motivated by JIRA WB-235
BOOST_AUTO_TEST_CASE(RetrieveFromBlastDbOnly_TestSequenceDataWithAccession) 
{
    vector<string> accessions;
    // note the order, this comes second in the BLAST DB
    accessions.push_back("CCD68076");   
    accessions.push_back("NP_001024503");

    const string kNonStdDb("prot_dbs");
    SDataLoaderConfig dlconfig(kNonStdDb, true,
                               SDataLoaderConfig::eUseBlastDbDataLoader);
    BOOST_CHECK(dlconfig.m_BlastDbName == kNonStdDb);
    CBlastScopeSourceWrapper scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();
    string data_loader_name("BLASTDB_");
    data_loader_name += kNonStdDb;
    data_loader_name += "Protein";
    BOOST_REQUIRE_EQUAL(data_loader_name,
                        scope_source.GetBlastDbLoaderName());

    ITERATE(vector<string>, acc, accessions) {
    CSeq_id id(*acc);

    const char* seq ="\
MADDSENFVLTVDIGTTTIRSVVYDSKCKERGSYQEKVNTIYTTRNDDEVLVEIEPEQLFLQFLRVIKKAYETLPPNAHV\
DVGLCTQRNSIVLWNKRTLKEETRIICWNDKRANNKCHNLNNSFLLKALNLAGGFLHFVTRKNRFLAAQRLKFLGGMVSH\
RLMVTIDRSEKLKLMKADGDLCYGSLETWLLMRSSKSNILCVEASNISPSGMFDPWIGAYNTLIMKIIGFPTDMLFPIVD\
SNLKDMNKLPIIDSSHIGKEFTISSIIADQQAAMFGCGTWERGDVKITLGTGTFVNVHTGKVPYASMSGLYPLVGWRING\
ETDFIAEGNAHDTAVILHWAQSIGLFNDVTETSDIALSVNDSNGVVFIPAFCGIQTPINDETACSGFMCIRPDTTKVHMV\
RAILESIAFRVYQIYAAAESEVNINKNSPVRICGGVSNNNFICQCIADLLGRKVERMTDSDHVAARGVALLTGFSSGIWT\
KEKLRELVTVEDIFTPNYESRKGLLKTFQTWKKAVDRCLGFYH";

    CBioseq_Handle bh = scope->GetBioseqHandle(id);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

        CNcbiOstrstream os;
        os << "Failed on accession " << *acc;
        const string msg = CNcbiOstrstreamToString(os);
    for (size_t i = 0; i < sv.size(); i++) {
            BOOST_REQUIRE_MESSAGE((char)seq[i] == (char)sv[i], msg);
        }
    }
}

BOOST_AUTO_TEST_CASE(ConfigFileTest_RetrieveFromBlastDb_TestSequenceData) 
{
    CSeq_id seqid(CSeq_id::e_Gi, 129295);

    const char* seq =
"QIKDLLVSSSTDLDTTLVLVNAIYFKGMWKTAFNAEDTREMPFHVTKQESKPVQMMCMNNSFNVATLPAEKMKILELPFASGDLSMLVLLPDEVSDLERIEKTINFEKLTEWTNPNTMEKRRVKVYLPQMKIEEKYNLTSVLMALGMTDLFIPSANLTGISSAESLKISQAVHGAFMELSEDGIEMAGSTGVIEDIKHSPESEQFRADHPFLFLIKHNPTNTIVYFGRYWSP";


    CAutoNcbiConfigFile acf(SDataLoaderConfig::eUseBlastDbDataLoader);
    acf.SetProteinBlastDbDataLoader(SDataLoaderConfig::kDefaultProteinBlastDb);
    SDataLoaderConfig dlconfig(true);
    BOOST_CHECK(dlconfig.m_UseBlastDbs == true);
    BOOST_CHECK(dlconfig.m_UseGenbank == false);
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName,
                string(SDataLoaderConfig::kDefaultProteinBlastDb));
    CBlastScopeSourceWrapper scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();
    string data_loader_name("BLASTDB_");
    data_loader_name += string(SDataLoaderConfig::kDefaultProteinBlastDb);
    data_loader_name += "Protein";
    BOOST_REQUIRE_EQUAL(data_loader_name,
                        scope_source.GetBlastDbLoaderName());

    CBioseq_Handle bh = scope->GetBioseqHandle(seqid);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK_EQUAL((char)seq[i], (char)sv[i]);
    }
}

BOOST_AUTO_TEST_CASE
    (ConfigFileTest_RetrieveFromNonStandardBlastDb_Config_TestSequenceData) 
{
    CSeq_id seqid(CSeq_id::e_Gi, 1787519);

    const char* seq = "MKAIFVLKGWWRTS";

    const string kNonStdDb("ecoli");
    CAutoNcbiConfigFile acf(SDataLoaderConfig::eUseBlastDbDataLoader);
    acf.SetProteinBlastDbDataLoader(kNonStdDb);
    SDataLoaderConfig dlconfig(true);
    BOOST_CHECK(dlconfig.m_UseBlastDbs == true);
    BOOST_CHECK(dlconfig.m_UseGenbank == false);
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName, kNonStdDb);
    CBlastScopeSourceWrapper scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();
    string data_loader_name("BLASTDB_");
    data_loader_name += kNonStdDb + "Protein";
    BOOST_REQUIRE_EQUAL(data_loader_name,
                        scope_source.GetBlastDbLoaderName());

    CBioseq_Handle bh = scope->GetBioseqHandle(seqid);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK_EQUAL((char)seq[i], (char)sv[i]);
    }
}

// Since this uses the SDataLoaderConfig constructor which specifies the BLAST
// database, it doesn't matter what the config value has
BOOST_AUTO_TEST_CASE
    (ConfigFileTest_RetrieveFromNonStandardBlastDb_ForcedDb_TestSequenceData) 
{
    CSeq_id seqid(CSeq_id::e_Gi, 1787519);

    const char* seq = "MKAIFVLKGWWRTS";

    const string kNonStdDb("ecoli");
    CAutoNcbiConfigFile acf(SDataLoaderConfig::eUseBlastDbDataLoader);
    acf.SetProteinBlastDbDataLoader("dummy db");
    SDataLoaderConfig dlconfig(kNonStdDb, true,
                               SDataLoaderConfig::eUseBlastDbDataLoader);
    BOOST_CHECK(dlconfig.m_UseBlastDbs == true);
    BOOST_CHECK(dlconfig.m_UseGenbank == false);
    BOOST_CHECK(dlconfig.m_BlastDbName == kNonStdDb);
    CBlastScopeSourceWrapper scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();
    string data_loader_name("BLASTDB_");
    data_loader_name += kNonStdDb + "Protein";
    BOOST_REQUIRE_EQUAL(data_loader_name,
                        scope_source.GetBlastDbLoaderName());

    CBioseq_Handle bh = scope->GetBioseqHandle(seqid);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK_EQUAL((char)seq[i], (char)sv[i]);
    }
}

BOOST_AUTO_TEST_CASE(ConfigFileTest_RetrieveFromGenbank_TestSequenceData) 
{
    CSeq_id seqid(CSeq_id::e_Gi, 129295);

    const char* seq =
"QIKDLLVSSSTDLDTTLVLVNAIYFKGMWKTAFNAEDTREMPFHVTKQESKPVQMMCMNNSFNVATLPAEKMKILELPFASGDLSMLVLLPDEVSDLERIEKTINFEKLTEWTNPNTMEKRRVKVYLPQMKIEEKYNLTSVLMALGMTDLFIPSANLTGISSAESLKISQAVHGAFMELSEDGIEMAGSTGVIEDIKHSPESEQFRADHPFLFLIKHNPTNTIVYFGRYWSP";


    CAutoNcbiConfigFile acf(SDataLoaderConfig::eUseGenbankDataLoader);
    SDataLoaderConfig dlconfig(true);
    BOOST_CHECK(dlconfig.m_UseBlastDbs == false);
    BOOST_CHECK(dlconfig.m_UseGenbank == true);
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName, kEmptyStr);
    CBlastScopeSourceWrapper scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();
    BOOST_REQUIRE(scope_source.GetBlastDbLoaderName() == kEmptyStr);

    CBioseq_Handle bh = scope->GetBioseqHandle(seqid);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK_EQUAL((char)seq[i], (char)sv[i]);
    }
}

// not the best thing to do, but supported nonetheless...
BOOST_AUTO_TEST_CASE(ConfigFileTest_UseNoDataLoaders) 
{
    CSeq_id seqid(CSeq_id::e_Gi, 129295);

    CAutoNcbiConfigFile acf(SDataLoaderConfig::eUseNoDataLoaders);
    SDataLoaderConfig dlconfig("pdbaa", true);
    BOOST_CHECK(dlconfig.m_UseBlastDbs == false);
    BOOST_CHECK(dlconfig.m_UseGenbank == false);
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName, kEmptyStr);
    CBlastScopeSourceWrapper scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();
    BOOST_REQUIRE(scope_source.GetBlastDbLoaderName() == kEmptyStr);

    CBioseq_Handle bh = scope->GetBioseqHandle(seqid);
    BOOST_CHECK(bh.State_NoData());
}

static 
void s_RetrieveSequenceLength(TGi gi, 
                              const string& dbname,
                              bool is_prot,
                              TSeqPos kExpectedLength) 
{
    const CSeq_id seqid(CSeq_id::e_Gi, gi);

    CAutoNcbiConfigFile afc;
    afc.SetProteinBlastDbDataLoader(dbname);
    afc.SetNucleotideBlastDbDataLoader(dbname);
    SDataLoaderConfig dlconfig(dbname, is_prot);
    BOOST_CHECK(dlconfig.m_UseBlastDbs == true);
    BOOST_CHECK(dlconfig.m_UseGenbank == true);
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName, dbname);
    CBlastScopeSourceWrapper scope_source(dlconfig);

    CRef<CScope> scope = scope_source.NewScope();
    TSeqPos length = sequence::GetLength(seqid, scope);
    BOOST_CHECK_EQUAL(kExpectedLength, length);
}

BOOST_AUTO_TEST_CASE(RetrieveFromBlastDb) {
    s_RetrieveSequenceLength(129295, "nr", true, 232);
}

// This gi has been removed from the BLAST databases
BOOST_AUTO_TEST_CASE(RetrieveFromGenbank) {
    s_RetrieveSequenceLength(7450545, "nr", true, 443);
}

BOOST_AUTO_TEST_CASE(RetrieveFromGenbank_NewlyAddedSequenceToGenbank) {

    // Get today's date
    CTime t;
    t.SetCurrent().SetFormat("Y/M/D");
    const string query_str(t.AsString() + "[PDAT]");

    // Search sequences publishe recently
    CEntrez2Client entrez;
    vector<TGi> results;
    entrez.Query(query_str, "nucleotide", results);
    if (results.empty()) {
        // No newly added sequences today :(
        return;
    }

    const CSeq_id seqid(CSeq_id::e_Gi, results.front());
    CBlastScopeSourceWrapper scope_source;

    TSeqPos length = sequence::GetLength(seqid, scope_source.NewScope());
    s_RetrieveSequenceLength(results.front(), "nt", false, length);
}

BOOST_AUTO_TEST_CASE(RetrieveFromGenbank_NoBlastDbDataLoader) {
    const CSeq_id seqid(CSeq_id::e_Gi, 7450545);
    CBlastScopeSourceWrapper scope_source;
    CRef<CScope> scope = scope_source.NewScope();
    TSeqPos length = sequence::GetLength(seqid, scope);
    const TSeqPos kExpectedLength = 443;
    BOOST_CHECK_EQUAL(kExpectedLength, length);
}

BOOST_AUTO_TEST_CASE(RetrieveFromGenbank_IncorrectBlastDbType) {
    s_RetrieveSequenceLength(7450545, "sts", false, 443);
}

struct SDiagRestorer {
    SDiagRestorer() { m_OriginalValue = SetDiagPostLevel(); }
    ~SDiagRestorer() { SetDiagPostLevel(m_OriginalValue); }
private:
    EDiagSev m_OriginalValue;
};

BOOST_AUTO_TEST_CASE(InvalidBlastDatabase) {
    try {
        SDiagRestorer d;
        s_RetrieveSequenceLength(129295, "dummy", true, 232);
    } catch (const CException& e) {
        const string kExpectedMsg(" BLAST database 'dummy' does not exist in");
        BOOST_CHECK(e.GetMsg().find(kExpectedMsg) != NPOS);
    }
}

BOOST_AUTO_TEST_CASE(ForceRemoteBlastDbLoader) {
    CAutoEnvironmentVariable env("BLASTDB", kEmptyStr);
    CAutoNcbiConfigFile acf(SDataLoaderConfig::eUseBlastDbDataLoader);
    acf.RemoveBLASTDBEnvVar();
    acf.SetProteinBlastDbDataLoader(SDataLoaderConfig::kDefaultProteinBlastDb);

    const CSeq_id seqid(CSeq_id::e_Gi, 129295);

    SDataLoaderConfig dlconfig(true);
    dlconfig.m_UseGenbank = false;
    BOOST_CHECK(dlconfig.m_UseBlastDbs == true);
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName,
                string(SDataLoaderConfig::kDefaultProteinBlastDb));
    CBlastScopeSourceWrapper scope_source(dlconfig);

    CRef<CScope> scope = scope_source.NewScope();
    TSeqPos length = sequence::GetLength(seqid, scope);
    const TSeqPos kExpectedLength = 232;
    BOOST_CHECK_EQUAL(kExpectedLength, length);

    string data_loader_name("REMOTE_BLASTDB_");
    data_loader_name += string(SDataLoaderConfig::kDefaultProteinBlastDb);
    data_loader_name += "Protein";
    BOOST_REQUIRE_EQUAL(data_loader_name,
                        scope_source.GetBlastDbLoaderName());
}

BOOST_AUTO_TEST_CASE(RetrieveSeqUsingPDBIds)
{
	const string dbname="data/pdb_test";
    CAutoNcbiConfigFile afc;
    afc.SetProteinBlastDbDataLoader(dbname);
    SDataLoaderConfig dlconfig(dbname, true);
    dlconfig.m_UseGenbank = false;
    BOOST_CHECK_EQUAL(dlconfig.m_BlastDbName, dbname);
    CBlastScopeSourceWrapper scope_source(dlconfig);
	CRef<CScope> scope = scope_source.NewScope();

	CNcbiIfstream ids_file("data/test.pdb_ids");
	string line;
    while (getline(ids_file, line)) {
    	vector<string>  d;
    	NStr::Split(line, " ", d);
	    const CSeq_id seqid(d[0]);
	    TSeqPos length = sequence::GetLength(seqid, scope);
	    BOOST_CHECK_EQUAL(NStr::StringToInt(d[1]), length);
    }
}

BOOST_AUTO_TEST_SUITE_END()
#endif /* SKIP_DOXYGEN_PROCESSING */
