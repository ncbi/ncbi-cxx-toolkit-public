/*
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
* Author:  Amelia Fong
*
* File Description:
*   Unit test module for MT .
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>


#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <algo/blast/format/blast_async_format.hpp>


#include <algo/blast/format/build_archive.hpp>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace ncbi::blast;
using namespace ncbi::objects;


#ifdef NCBI_THREADS
BOOST_AUTO_TEST_SUITE(blast_mt)

void s_GenerateGiList(CRef<CSeqDB> & seqdb, vector<TGi> & gis)
{
	gis.clear();
    int num_seqs = seqdb->GetNumSeqs();
	int kNumTestOids = MIN(5000, num_seqs/4);
	while(gis.size() <= kNumTestOids) {
       	int oid = rand() % num_seqs;
       	TGi gi;
       	seqdb->OidToGi(oid, gi);
       	if(gi > 0){
       		gis.push_back(gi);
       	}
       	else {
       		continue;
       	}
	}
}

void s_GenerateAccsList(CRef<CSeqDB> & seqdb, vector<string> & ids)
{
	ids.clear();
    int num_seqs = seqdb->GetNumSeqs();
	int kNumTestOids = MIN(5000, num_seqs/4);
	while(ids.size() <= kNumTestOids) {
       	int oid = rand() % num_seqs;
       	list<CRef<CSeq_id> > t = seqdb->GetSeqIDs(oid);
       	CRef<CSeq_id> t_id = t.back();
       	ids.push_back(t_id->GetSeqIdString());
       	if(!t.empty()){
       		CRef<CSeq_id> t_id = t.back();
       		ids.push_back(t_id->GetSeqIdString());
       	}
       	else {
       		continue;
       	}
	}
}


class CDLTestThread : public CThread
{
public:
    CDLTestThread(vector<TGi> & gis, string & db, bool isProtein):
    	m_gis(gis), m_db(db), m_isProtein(isProtein){ }

    virtual void* Main(void) {
        CRef<CSeqDB> seqdb(new CSeqDB(m_db, m_isProtein?CSeqDB::eProtein:CSeqDB::eNucleotide));
        CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*obj_mgr));
        CBlastDbDataLoader::RegisterInObjectManager(*obj_mgr, seqdb, true, CObjectManager::eDefault, CBlastDatabaseArgs::kSubjectsDataLoaderPriority);
        CBlastDbDataLoader::SBlastDbParam param(seqdb);
        string ld(CBlastDbDataLoader::GetLoaderNameFromArgs(param));
        scope->AddDataLoader(ld);
        for (int i=0; i < m_gis.size(); i++) {
    		CSeq_id id (CSeq_id::e_Gi, m_gis[i]);
    	    CBioseq_Handle  bioseq_handle = scope->GetBioseqHandle(id);
    	    CBioseq_Handle::TId tmp = bioseq_handle.GetId();
    	    ITERATE(CBioseq_Handle::TId, itr, bioseq_handle.GetId()) {
   	            CConstRef<CSeq_id> next_id = itr->GetSeqId();
    	    }
    	    wait(random());
        }
    	return NULL;
    }
    ~CDLTestThread() {}
private:
    vector<TGi> m_gis;
    string m_db;
    bool m_isProtein;
};

void s_MTDataLoaderTest(string & db, bool isProtein)
{
    CRef<CSeqDB> seqdb(new CSeqDB(db, isProtein?CSeqDB::eProtein:CSeqDB::eNucleotide));
	vector<TGi> gis;
	s_GenerateGiList(seqdb, gis);

	const int kNumThreads=64;
	vector<CDLTestThread*> threads;
	for (int i=0; i < kNumThreads; i++) {
		threads.push_back(new CDLTestThread(gis, db, isProtein));
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Run();
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Join();
	}
}

BOOST_AUTO_TEST_CASE(MT_DataLoaderForEachThread_16S)
{
	string db = "rRNA_typestrains/16S_ribosomal_RNA";
	bool isProtein = false;
	BOOST_REQUIRE_NO_THROW(s_MTDataLoaderTest(db, isProtein));
}

BOOST_AUTO_TEST_CASE(MT_DataLoaderForEachThread_RefseqProt)
{
	string db = "refseq_select_prot";
	bool isProtein = true;
	BOOST_REQUIRE_NO_THROW(s_MTDataLoaderTest(db, isProtein));
}

/****************************************************************************************/
class CDLTest2Thread : public CThread
{
public:
    CDLTest2Thread(vector<TGi> & gis, string & ld):
    	m_gis(gis), m_dataloader(ld){ }

    virtual void* Main(void) {
        CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
        CRef<CScope> scope(new CScope(*obj_mgr));
        scope->AddDataLoader(m_dataloader);
        for (int i=0; i < m_gis.size(); i++) {
    		CSeq_id id (CSeq_id::e_Gi, m_gis[i]);
    	    CBioseq_Handle  bioseq_handle = scope->GetBioseqHandle(id);
    		CConstRef<CBioseq> t = bioseq_handle.GetCompleteBioseq();
    	    CBioseq_Handle::TId tmp = bioseq_handle.GetId();
    	    wait(random());
        }
    	return NULL;
    }
    ~CDLTest2Thread() {}
private:
    vector<TGi> m_gis;
    string m_dataloader;
};

void s_MTDataLoaderTest2(string & db, bool isProtein)
{
    CRef<CSeqDB> seqdb(new CSeqDB(db, isProtein?CSeqDB::eProtein:CSeqDB::eNucleotide));
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CBlastDbDataLoader::RegisterInObjectManager(*obj_mgr, seqdb, true, CObjectManager::eDefault, CBlastDatabaseArgs::kSubjectsDataLoaderPriority);
    CBlastDbDataLoader::SBlastDbParam param(seqdb);
    string ld(CBlastDbDataLoader::GetLoaderNameFromArgs(param));

	vector<TGi> gis;
	s_GenerateGiList(seqdb, gis);

	const int kNumThreads=64;
	vector<CDLTest2Thread*> threads;
	for (int i=0; i < kNumThreads; i++) {
		threads.push_back(new CDLTest2Thread(gis, ld));
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Run();
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Join();
	}

}

BOOST_AUTO_TEST_CASE(MT_SingleDataLoaderForAllThreads_16s)
{
	{
		string db = "rRNA_typestrains/16S_ribosomal_RNA";
		bool isProtein = false;
		s_MTDataLoaderTest2(db, isProtein);
	}
}

BOOST_AUTO_TEST_CASE(MT_SingleDataLoaderForAllThreads_ProtSelect)
{
	{
		string db = "refseq_select_prot";
		bool isProtein = true;
		s_MTDataLoaderTest2(db, isProtein);
	}
}

/***********************************************************************/
class CSeqDBTestThread : public CThread
{
public:
    CSeqDBTestThread(string & db, bool isProtein, vector<string> & ids):
    	 m_db(db), m_isProtein(isProtein), m_Ids(ids){}

    virtual void* Main(void) {
        CRef<CSeqDB> seqdb(new CSeqDB(m_db, m_isProtein?CSeqDB::eProtein:CSeqDB::eNucleotide));
        for (int i=0; i < m_Ids.size(); i++) {
        	vector<int> oids;
        	string output;
        	seqdb->AccessionToOids(m_Ids[i], oids);
        	seqdb->GetSequenceAsString(oids[0], output);
        }
    	return NULL;
    }
    ~CSeqDBTestThread() {}
private:
    string m_db;
    bool m_isProtein;
    vector<string> m_Ids;
};

void s_MTSeqDBTest(string & db, bool isProtein)
{
    CRef<CSeqDB> seqdb(new CSeqDB(db, isProtein?CSeqDB::eProtein:CSeqDB::eNucleotide));
    int num_seqs = seqdb->GetNumSeqs();
	int kNumTestOids = MIN(5000, num_seqs/4);
	vector<string> ids;
	s_GenerateAccsList(seqdb, ids);

	const int kNumThreads=64;
	vector<CSeqDBTestThread*> threads;
	for (int i=0; i < kNumThreads; i++) {
		threads.push_back(new CSeqDBTestThread(db, isProtein, ids));
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Run();
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Join();
	}

}

BOOST_AUTO_TEST_CASE(SeqDBTest_AccsToOid_nr)
{
	{
		string db = "nr";
		bool isProtein = true;
		BOOST_REQUIRE_NO_THROW(s_MTSeqDBTest(db, isProtein));
	}
}

BOOST_AUTO_TEST_CASE(SeqDBTest_AccsToOid_nt)
{
	{
		string db = "nt";
		bool isProtein = false;
		BOOST_REQUIRE_NO_THROW(s_MTSeqDBTest(db, isProtein));
	}
}

/*********************************************************************/


class CSeqDBTest2Thread : public CThread
{
public:
    CSeqDBTest2Thread(string & db, bool isProtein, vector<int> & oids):
    	m_db(db), m_isProtein(isProtein), m_Oids(oids){ }

    virtual void* Main(void) {
        CRef<CSeqDB> seqdb(new CSeqDB(m_db, m_isProtein?CSeqDB::eProtein:CSeqDB::eNucleotide));
        for(int i =0; i < m_Oids.size(); i++) {
        	const char* buf;
        	seqdb->GetSequence(m_Oids[i], &buf);
        }
    	return NULL;
    }
    ~CSeqDBTest2Thread() {}
private:
    string m_db;
    bool m_isProtein;
	vector<int> & m_Oids;
};

void s_MTSeqDBTest2(string & db, bool isProtein)
{
	const int kNumThreads=64;
	const int kNumTestOids = 5000;
	vector<CSeqDBTest2Thread*> threads;
	vector<int> oids;
    CRef<CSeqDB> seqdb(new CSeqDB(db, isProtein?CSeqDB::eProtein:CSeqDB::eNucleotide));
    seqdb->SetNumberOfThreads(1, true);
    int num_seqs = seqdb->GetNumSeqs();
	for(int i=0; i < kNumTestOids; i++) {
       	int oid = rand() % num_seqs;
       	oids.push_back(oid);
	}

	for (int i=0; i < kNumThreads; i++) {
		threads.push_back(new CSeqDBTest2Thread(db, isProtein, oids));
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Run();
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Join();
	}

}

BOOST_AUTO_TEST_CASE(SeqDBTest2)
{
	{
		string db = "nr";
		bool isProtein = true;
		BOOST_REQUIRE_NO_THROW(s_MTSeqDBTest2(db, isProtein));
	}
}

class CSeqDBTest3Thread : public CThread
{
public:
    CSeqDBTest3Thread(CRef<CSeqDB> seqdb, vector<int> & oids):
    	m_SeqDB(seqdb), m_Oids(oids){ }

    virtual void* Main(void) {
        for(int i =0; i < m_Oids.size(); i++) {
        	const char* buf;
        	m_SeqDB->GetSequence(m_Oids[i], &buf);
        	wait (rand());
        	m_SeqDB->RetSequence(&buf);
        }
    	return NULL;
    }
    ~CSeqDBTest3Thread() {}
private:
    CRef<CSeqDB>   m_SeqDB;
	vector<int> &  m_Oids;
};

void s_MTSeqDBTest3(string & db, bool isProtein)
{
	const int kNumThreads=64;
	const int kNumTestOids = 5000;
	vector<CSeqDBTest3Thread*> threads;
	vector<int> oids;
    CRef<CSeqDB> seqdb(new CSeqDB(db, isProtein?CSeqDB::eProtein:CSeqDB::eNucleotide));
    seqdb->SetNumberOfThreads(kNumThreads);

    int num_seqs = seqdb->GetNumSeqs();
	for(int i=0; i < kNumTestOids; i++) {
       	int oid = rand() % num_seqs;
       	oids.push_back(oid);
	}

	for (int i=0; i < kNumThreads; i++) {
		threads.push_back(new CSeqDBTest3Thread(seqdb, oids));
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Run();
	}
	for (int i=0; i < kNumThreads; i++) {
		threads[i]->Join();
	}

}

BOOST_AUTO_TEST_CASE(SeqDBTest3)
{
	{
		string db = "nt";
		bool isProtein = false;
		BOOST_REQUIRE_NO_THROW(s_MTSeqDBTest3(db, isProtein));
	}
}
BOOST_AUTO_TEST_SUITE_END()
#endif
