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
 * Authors: Christiam Camacho
 *
 */

/** @file bdb_unit_test.cpp
 * Unit tests for the BLAST database data loader
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader_rmt.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>   // for SeqDB_ReadGiList
#include <corelib/ncbithr.hpp>                      // for CThread
#include <util/random_gen.hpp>
#include <objmgr/seq_vector.hpp>        // for CSeqVector
#include <objects/seq/Seq_ext.hpp>
#include <corelib/ncbi_limits.hpp>

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/// Auxiliary class to register the BLAST database data loader of choice
/// (information provided in the constructor) and deactivate it on object
/// destruction
class CAutoRegistrar {
public:
    CAutoRegistrar(const string& dbname, bool is_protein,
                   bool use_fixed_slice_size, 
                   bool use_remote_blast_db_loader = false) {
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        if (use_remote_blast_db_loader) {
            loader_name = CRemoteBlastDbDataLoader::RegisterInObjectManager
                        (*om, dbname, is_protein 
                         ? CBlastDbDataLoader::eProtein
                         : CBlastDbDataLoader::eNucleotide,
                         use_fixed_slice_size,
                         CObjectManager::eDefault, 
                         CObjectManager::kPriority_NotSet)
                        .GetLoader()->GetName();
        } else {
            loader_name = CBlastDbDataLoader::RegisterInObjectManager
                        (*om, dbname, is_protein 
                         ? CBlastDbDataLoader::eProtein
                         : CBlastDbDataLoader::eNucleotide,
                         use_fixed_slice_size,
                         CObjectManager::eDefault, 
                         CObjectManager::kPriority_NotSet)
                        .GetLoader()->GetName();
        }
        om->SetLoaderOptions(loader_name, CObjectManager::eDefault);
    }

    void RegisterGenbankDataLoader() {
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        gbloader_name = 
            CGBDataLoader::RegisterInObjectManager(*om).GetLoader()->GetName();
    }

    ~CAutoRegistrar() {
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        om->SetLoaderOptions(loader_name, CObjectManager::eNonDefault);
        if ( !gbloader_name.empty() ) {
            om->SetLoaderOptions(gbloader_name, CObjectManager::eNonDefault);
        }
    }

private:
    string loader_name;
    string gbloader_name;
};

BOOST_AUTO_TEST_SUITE(bdbloader)

/// This function tests the ways in which the sequence data is populated in a
/// CBioseq loaded by the object manager via its data loaders
void TestCSeq_inst(const CBioseq& bioseq) {
    BOOST_REQUIRE(bioseq.CanGetInst());
    switch (bioseq.GetInst().GetRepr()) {
    case CSeq_inst::eRepr_raw:
        BOOST_REQUIRE(bioseq.GetInst().CanGetSeq_data());
        break;
    case CSeq_inst::eRepr_delta:
        BOOST_REQUIRE( !bioseq.GetInst().CanGetSeq_data() );
        BOOST_REQUIRE( bioseq.GetInst().CanGetExt() );
        BOOST_REQUIRE( bioseq.GetInst().GetExt().IsDelta() );
        break;
    default:
        break;
    }
}

void RetrieveGi555WithTimeOut(bool is_remote)
{
    const CSeq_id id(CSeq_id::e_Gi, 555);

    const string db("nt");
    const bool is_protein = false;
    const bool use_fixed_slice_size = true;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size, is_remote);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();
    TSeqPos len = sequence::GetLength(id, scope);
    const TSeqPos kLength(624);
    BOOST_REQUIRE_EQUAL(kLength, len);

    CBioseq_Handle bh = scope->GetBioseqHandle(id);
    CRef<CBioseq> retval(const_cast<CBioseq*>(&*bh.GetCompleteBioseq()));
    BOOST_REQUIRE_EQUAL(kLength, retval->GetLength());
    BOOST_REQUIRE(retval->CanGetInst());
    TestCSeq_inst(*retval);
}

BOOST_AUTO_TEST_CASE(RetrieveGi555WithTimeOut_Local)
{
    RetrieveGi555WithTimeOut(false);
}
BOOST_AUTO_TEST_CASE_TIMEOUT(RetrieveGi555WithTimeOut_Local, 5);

BOOST_AUTO_TEST_CASE(RetrieveGi555WithTimeOut_Remote)
{
    RetrieveGi555WithTimeOut(true);
}
BOOST_AUTO_TEST_CASE_TIMEOUT(RetrieveGi555WithTimeOut_Remote, 5);

void RetrieveLargeProteinWithTimeOut(bool is_remote)
{
    const CSeq_id id(CSeq_id::e_Gi, 1212992);

    const string db("nr");
    const bool is_protein = true;
    const bool use_fixed_slice_size = false;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size, is_remote);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();
    TSeqPos len = sequence::GetLength(id, scope);
    const TSeqPos kLength(26926);
    BOOST_REQUIRE_EQUAL(kLength, len);

    CBioseq_Handle bh = scope->GetBioseqHandle(id);
    CRef<CBioseq> retval(const_cast<CBioseq*>(&*bh.GetCompleteBioseq()));
    BOOST_REQUIRE_EQUAL(kLength, retval->GetLength());
    BOOST_REQUIRE(retval->CanGetInst());
    TestCSeq_inst(*retval);
}

#ifdef _DEBUG
    const time_t kTimeOutLargeProtein = 60*5;   // 5 minutes
#else
    const time_t kTimeOutLargeProtein = 15;
#endif /* _DEBUG */

BOOST_AUTO_TEST_CASE(RetrieveLargeProteinWithTimeOut_Local)
{
    RetrieveLargeProteinWithTimeOut(false);
}
BOOST_AUTO_TEST_CASE_TIMEOUT(RetrieveLargeProteinWithTimeOut_Local, 
                             kTimeOutLargeProtein);

BOOST_AUTO_TEST_CASE(RetrieveLargeProteinWithTimeOut_Remote)
{
    RetrieveLargeProteinWithTimeOut(true);
}
BOOST_AUTO_TEST_CASE_TIMEOUT(RetrieveLargeProteinWithTimeOut_Remote, 
                             kTimeOutLargeProtein);

void RetrieveLargeNuclSequence(bool is_remote)
{
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 291045224));

    const string db("nucl_dbs");
    const bool is_protein = false;
    const bool use_fixed_slice_size = false;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size, is_remote);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();
    TSeqPos len = sequence::GetLength(*id, scope);
    const TSeqPos kLength(101520);
    BOOST_REQUIRE_EQUAL(kLength, len);

    CBioseq_Handle bh = scope->GetBioseqHandle(*id);
    CRef<CBioseq> retval(const_cast<CBioseq*>(&*bh.GetCompleteBioseq()));
    BOOST_REQUIRE_EQUAL(kLength, retval->GetLength());
    TestCSeq_inst(*retval);
}

BOOST_AUTO_TEST_CASE(RetrieveLargeNuclSequence_Local)
{
    RetrieveLargeNuclSequence(false);
}

BOOST_AUTO_TEST_CASE(RetrieveLargeNuclSequence_Remote)
{
    RetrieveLargeNuclSequence(true);
}

void RetrievePartsOfLargeChromosome(bool is_remote)
{
    const string kAccession("NC_000001");
    CRef<CSeq_id> id(new CSeq_id(kAccession));
    // retrieves this range in the sequence
    const TSeqRange kRange(15100, 15500); 
    CRef<CSeq_loc> sl(new CSeq_loc(*id, kRange.GetFrom(), kRange.GetTo()));

    const string db("nucl_dbs");
    const bool is_protein = false;
    const bool use_fixed_slice_size = true;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size, is_remote);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();

    const TSeqPos kLength(249250621);
    TSeqPos len = sequence::GetLength(*id, scope);
    BOOST_REQUIRE_EQUAL(kLength, len);
    len = sequence::GetLength(*sl, scope);
    const TSeqRange kTestRange = sl->GetTotalRange();
    BOOST_REQUIRE_EQUAL(kRange, kTestRange);
    BOOST_REQUIRE_EQUAL(kTestRange.GetLength(), len);

    CBioseq_Handle bh = scope->GetBioseqHandle(*sl);
    BOOST_REQUIRE(bh);
    BOOST_REQUIRE_EQUAL(is_protein, bh.IsAa());
    // N.B.: don't call GetCompleteBioseq or you'll fetch the whole enchilada!
    CRef<CBioseq> retval(const_cast<CBioseq*>(&*bh.GetBioseqCore()));
    BOOST_REQUIRE(retval->IsSetLength());
    BOOST_REQUIRE_EQUAL(kLength, retval->GetLength());
    BOOST_REQUIRE(retval->CanGetInst());
    BOOST_REQUIRE(retval->GetInst().CanGetSeq_data() == false);
    //{ofstream o("junk.asn"); o << MSerial_AsnText << *retval;}

    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    BOOST_REQUIRE(sv.size() == kLength);
    string buffer;
    // N.B.: GetSeqData's stop argument is exclusive
    sv.GetSeqData(kTestRange.GetFrom(), kTestRange.GetTo(), buffer);
    BOOST_REQUIRE_EQUAL(kTestRange.GetLength() - 1 , buffer.size());

    // Obtained with the following command:
    // blastdbcmd -db nucl_dbs -entry NC_000001 -range 100-500 -outfmt %s
    char seq_data[] = 
        "GTCCCCACCCCCATGACACTCCCCAGCCCTCCAAGGCCACTGTGTTTCCCAGTTAGCTCAGAGCCTCAGTCGATCCCTGACCCAGCACCGGGCACTGATGAGACAGCGGCTGTTTGAGGAGCCACCTCCCAGCCACCTCGGGGCCAGGGCCAGGGTGTGCAGCACCACTGTACAATGGGGAAACTGGCCCAGAGAGGTGAGGCAGCTTGCCTGGGGTCACAGAGCAAGGCAAAAGCAGCGCTGGGTACAAGCTCAAAACCATAGTGCCCAGGGCACTGCCGCTGCAGGCGCAGGCATCGCATCACACCAGTGTCTGCGTTCACAGCAGGCATCATCAGTAGCCTCCAGAGGCCTCAGGTCCAGTCTCTAAAAATATCTCAGGAGGCTGCAGTGGCTGACC";
    BOOST_REQUIRE_EQUAL(sizeof(seq_data)/sizeof(seq_data[0]),
                        buffer.size()+1);
    for (size_t i = 0; i <= buffer.size(); i++) {
        const string ref(1, seq_data[i]);
        const string test(1, buffer[i]);
        string msg("Unexpected base at position ");
        msg += NStr::IntToString(i+kRange.GetFrom()) + ": '" + ref + "' vs. '";
        msg += test + "'";
        BOOST_REQUIRE_MESSAGE(ref == test, msg);
    }
}

BOOST_AUTO_TEST_CASE(RetrievePartsOfLargeChromosome_Local)
{
    RetrievePartsOfLargeChromosome(false);
}

BOOST_AUTO_TEST_CASE(RetrievePartsOfLargeChromosome_Remote)
{
    RetrievePartsOfLargeChromosome(true);
}

#ifndef _DEBUG
/* Only execute this in release mode as debug might be too slow */
void RetrieveLargeChromosomeWithTimeOut(bool is_remote)
{
    const string kAccession("NC_000001");

    const string db("nucl_dbs");
    const bool is_protein = false;
    const bool use_fixed_slice_size = false;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size, is_remote);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();
    CRef<CSeq_id> id(new CSeq_id(kAccession));
    TSeqPos len = sequence::GetLength(*id, scope);
    const TSeqPos kLength(249250621);
    BOOST_REQUIRE_EQUAL(kLength, len);

    CBioseq_Handle bh = scope->GetBioseqHandle(*id);
    CRef<CBioseq> retval(const_cast<CBioseq*>(&*bh.GetCompleteBioseq()));
    BOOST_REQUIRE_EQUAL(kLength, retval->GetLength());
    TestCSeq_inst(*retval);
}

BOOST_AUTO_TEST_CASE(RetrieveLargeChromosomeWithTimeOut_Local)
{
    RetrieveLargeChromosomeWithTimeOut(false);
}
BOOST_AUTO_TEST_CASE_TIMEOUT(RetrieveLargeChromosomeWithTimeOut_Local, 30);

BOOST_AUTO_TEST_CASE(RetrieveLargeChromosomeWithTimeOut_Remote)
{
    RetrieveLargeChromosomeWithTimeOut(true);
}
BOOST_AUTO_TEST_CASE_TIMEOUT(RetrieveLargeChromosomeWithTimeOut_Remote, 330);
#endif /* _DEBUG */

#if 0 /* Boost doesn't support MT unit tests */
#ifdef NCBI_THREADS
class CGiFinderThread : public CThread
{
public:
    CGiFinderThread(CRef<CScope> scope, const vector<int>& gis2find)
        : m_Scope(scope), m_Gis2Find(gis2find) {}

    virtual void* Main() {
        CRandom random;
        for (TSeqPos i = 0; i < m_Gis2Find.size(); i++) {
			int gi = m_Gis2Find[random.GetRand() % m_Gis2Find.size()];
            CSeq_id id(CSeq_id::e_Gi, gi);
            TSeqPos len = sequence::GetLength(id, m_Scope);
            if (len == numeric_limits<TSeqPos>::max()) {
                m_Scope->ResetHistory();
                continue;
            }
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(id);
            _ASSERT(bh);
            // Replace BOOST macros for _ASSERT's to avoid MT-safe problems
            //BOOST_REQUIRE(bh);
            CConstRef<CBioseq> bioseq = bh.GetCompleteBioseq();
            //BOOST_REQUIRE_EQUAL(len, bioseq->GetInst().GetLength());
            _ASSERT(len == bioseq->GetInst().GetLength());
            //TestCSeq_inst(*bioseq);
            m_Scope->ResetHistory();
        }
        return (void*)0;
    }

private:
    CRef<CScope> m_Scope;
    const vector<int>& m_Gis2Find;
};

void MultiThreadedAccess(bool is_remote)
{
    vector<int> gis;
    SeqDB_ReadGiList("data/ecoli.gis", gis);

    const string db("ecoli");
    const bool is_protein = true;
    const bool use_fixed_slice_size = true;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size, is_remote);
    //reg.RegisterGenbankDataLoader();

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();

    typedef vector< CRef<CGiFinderThread> > TTesterThreads;
    const TSeqPos kNumThreads = 4;
    TTesterThreads the_threads(kNumThreads);

    for (TSeqPos i = 0; i < kNumThreads; i++) {
        the_threads[i].Reset(new CGiFinderThread(scope, gis));
        BOOST_REQUIRE(the_threads[i].NotEmpty());
    }

    NON_CONST_ITERATE(TTesterThreads, thread, the_threads) {
        (*thread)->Run();
    }

    NON_CONST_ITERATE(TTesterThreads, thread, the_threads) {
        long result = 0;
        (*thread)->Join(reinterpret_cast<void**>(&result));
        BOOST_REQUIRE_EQUAL(0L, result);
    }
}

BOOST_AUTO_TEST_CASE(MultiThreadedAccess_Local)
{
    MultiThreadedAccess(false);
}

BOOST_AUTO_TEST_CASE(MultiThreadedAccess_Remote)
{
    MultiThreadedAccess(true);
}
#endif  /* NCBI_THREADS */
#endif 

void TestDataNotFound(bool is_remote)
{
    const CSeq_id id(CSeq_id::e_Gi, 555);

    const string db("pataa");
    const bool is_protein = true;
    const bool use_fixed_slice_size = true;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size, is_remote);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();
    TSeqPos len = sequence::GetLength(id, scope);
    BOOST_REQUIRE_EQUAL(numeric_limits<TSeqPos>::max(), len);
    CBioseq_Handle bh = scope->GetBioseqHandle(id);
    BOOST_REQUIRE( !bh );
}

BOOST_AUTO_TEST_CASE(TestDataNotFound_Local)
{
    TestDataNotFound(false);
}

BOOST_AUTO_TEST_CASE(TestDataNotFound_Remote)
{
    TestDataNotFound(true);
}
BOOST_AUTO_TEST_SUITE_END()
#endif /* SKIP_DOXYGEN_PROCESSING */
