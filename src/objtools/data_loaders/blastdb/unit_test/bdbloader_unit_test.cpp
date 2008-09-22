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
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>   // for SeqDB_ReadGiList
#include <corelib/ncbithr.hpp>                      // for CThread
#include <util/random_gen.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN    // this should only be defined here!
#include <boost/test/auto_unit_test.hpp>

#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CAutoRegistrar {
public:
    CAutoRegistrar(const string& dbname, bool is_protein,
                   bool use_fixed_slice_size) {
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        loader_name = CBlastDbDataLoader::RegisterInObjectManager
                    (*om, dbname, is_protein 
                     ? CBlastDbDataLoader::eProtein
                     : CBlastDbDataLoader::eNucleotide,
                     use_fixed_slice_size,
                     CObjectManager::eDefault).GetLoader()->GetName();
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

BOOST_AUTO_TEST_CASE(RetrieveGi555WithTimeOut)
{
    const time_t kTimeMax = 5;
    const CSeq_id id(CSeq_id::e_Gi, 555);

    const string db("nt");
    const bool is_protein = false;
    const bool use_fixed_slice_size = true;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();
    time_t start_time = CTime(CTime::eCurrent).GetTimeT();
    TSeqPos len = sequence::GetLength(id, scope);
    BOOST_REQUIRE_EQUAL((TSeqPos)624, len);

    CBioseq_Handle bh = scope->GetBioseqHandle(id);
    CRef<CBioseq> retval(const_cast<CBioseq*>(&*bh.GetCompleteBioseq()));
    time_t end_time = CTime(CTime::eCurrent).GetTimeT();

    ostringstream os;
    os << "Fetching " << id.AsFastaString() << " took " 
       << end_time-start_time << " seconds, i.e.: more than the "
       << kTimeMax << " second timeout";
    BOOST_REQUIRE_MESSAGE((end_time-start_time) < kTimeMax, os.str());
}

#ifndef _DEBUG
/* Only execute this in release mode as debug might be too slow */
BOOST_AUTO_TEST_CASE(RetrieveLargeChromosomeWithTimeOut)
{
    const time_t kTimeMax = 15;
    const string kAccession("NC_000001");

    const string db("nucl_dbs");
    const bool is_protein = false;
    const bool use_fixed_slice_size = false;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();
    CSeq_id id(kAccession);
    time_t start_time = CTime(CTime::eCurrent).GetTimeT();
    TSeqPos len = sequence::GetLength(id, scope);
    BOOST_REQUIRE_EQUAL((TSeqPos)247249719, len);

    CBioseq_Handle bh = scope->GetBioseqHandle(id);
    CRef<CBioseq> retval(const_cast<CBioseq*>(&*bh.GetCompleteBioseq()));
    time_t end_time = CTime(CTime::eCurrent).GetTimeT();

    ostringstream os;
    os << "Fetching " << kAccession << " took " 
       << end_time-start_time << " seconds, i.e.: more than the "
       << kTimeMax << " second timeout";
    BOOST_REQUIRE_MESSAGE((end_time-start_time) < kTimeMax, os.str());
}
#endif /* _DEBUG */

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
            BOOST_REQUIRE(bh);
            CConstRef<CBioseq> bioseq = bh.GetCompleteBioseq();
            BOOST_REQUIRE_EQUAL(len, bioseq->GetInst().GetLength());
            m_Scope->ResetHistory();
        }
        return (void*)0;
    }

private:
    CRef<CScope> m_Scope;
    const vector<int>& m_Gis2Find;
};

BOOST_AUTO_TEST_CASE(MultiThreadedAccess)
{
    vector<int> gis;
    SeqDB_ReadGiList("data/ecoli.gis", gis);

    const string db("ecoli");
    const bool is_protein = true;
    const bool use_fixed_slice_size = true;
    CAutoRegistrar reg(db, is_protein, use_fixed_slice_size);
    reg.RegisterGenbankDataLoader();

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
#endif

#endif /* SKIP_DOXYGEN_PROCESSING */
