/* $Id$
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
 * Author:  Charlie Liu
 *
 * File Description:
 *
 *       Update CDs
 *
 * ===========================================================================
 */

#ifndef CU_CDUPDATER_HPP
#define CU_CDUPDATER_HPP

#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/general/User_field.hpp>
#include <algo/structure/cd_utils/cuCppNCBI.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuCdUpdateParameters.hpp>
#include "objects/id1/id1_client.hpp"
#include "objects/seqalign/Seq_align_set.hpp"
#include "objects/entrez2/entrez2_client.hpp"
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuSequenceTable.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CDRefresher
{
public:
	CDRefresher (CCdCore* cd);

	//return the gi that's replaced; return -1 if none is replaced
	int refresh(CRef< CSeq_align> seqAlign, CRef< CSeq_entry > seqEntry);
	//bool hasBioseq(CRef< CBioseq > bioseq);
	bool hasOlderVersion(CRef< CBioseq > bioseq);

private:
	CCdCore* m_cd;
	typedef map<string, CRef< CBioseq > > AccessionBioseqMap;
	AccessionBioseqMap m_accSeqMap;

	void addSequences(CSeq_entry& seqEntry);
	void addSequence(CRef< CBioseq > bioseq);
	//cd_utils::SequenceTable m_seqTable;
};

struct NCBI_CDUTILS_EXPORT CDUpdateStats
{
	int numBlastHits;
	vector<int> envSeq;
	vector<int> fragmented;
	vector<int> overlap;
	vector<int> noSeq;
	vector<int> badAlign;
	vector<int> redundant;
	int numRedundant;
	typedef pair<int, int> OldNewGiPair;
	vector<OldNewGiPair> oldNewPairs;
	int numObsolete;
	int numFilteredByOverlap;

public:
	CDUpdateStats();
	string toString(bool detailed=true);
private:
	string toString(vector<int>& gis, string type);
	string toString(vector<int>& gis);
	string toString(int num);
	string toString(vector<OldNewGiPair>& giPairs, string type);
};

class NCBI_CDUTILS_EXPORT UpdaterInterface
{
public:
    virtual ~UpdaterInterface() {};
	virtual int  submitBlast(bool wait=false, int row = 0) = 0;
	virtual bool getBlastHits() = 0;
	virtual bool processBlastHits() = 0; //true: new sequences recruited.
	virtual void getCds(vector<CCdCore*>&) = 0;
	virtual bool hasCd(CCdCore*) =0;
	
	// maniplate the updater store
	static void addUpdater(UpdaterInterface* updater);
	static bool IsEmpty();
	static int checkAllBlasts(vector< UpdaterInterface* >& blasted);
	static void removeUpdaters(const vector<CCdCore*>& cds);
	static void removeUpdaters(const vector<UpdaterInterface*>& updaters);

private:
	static list<UpdaterInterface*> m_updaterList;
};

class GroupUpdater;

class NCBI_CDUTILS_EXPORT CDUpdater : public UpdaterInterface
{
public:
	CDUpdater(CCdCore* cd, CdUpdateParameters& config);
	//CDUpdater(const string& rid);
	virtual ~CDUpdater();
	
	//UpdaterInterface
	int  submitBlast(bool wait = false, int row = 0);
	bool getBlastHits();
	bool processBlastHits();
	void getCds(vector<CCdCore*>&);
	bool hasCd(CCdCore*);

    // submit a remote blast query
    // if failed or any exception was encountered, returns false (call getLastError to see message)
	bool blast(bool wait = false, int row = 0);

	const string getRid() {return m_rid;}
	const string getLastError() {return m_lastError;}
	bool getHits(CRef<CSeq_align_set> & hits);
	bool checkDone();
	CCdCore* getCd() {return m_cd;}
	CRef<CSeq_align_set> getAlignments() {return m_hits;}
	//drive update
	bool checkBlastAndUpdate();
	void setHitsNeeded(int num) {m_hitsNeeded = num;}
	bool update(CCdCore* cd, CSeq_align_set& alignments);

	//for making a new CD
	void requireProcessPending(int threshold) {m_processPendingThreshold = threshold;};
	//return the number of pending rows filtered out
	static int processPendingToNormal(int overlap, CCdCore* cd);
	static int mergePending(CCdCore* cd, int threshold, bool remaster);

	bool isFragmentedSeq(CCdCore* cd, CRef< CSeq_align > seqAlign, 
						CRef< CSeq_entry > seqEntry);
	static bool retrieveSeq(CID1Client& client, CRef<CSeq_id> seqID, 
		CRef<CSeq_entry>& seqEntry);
	CDUpdateStats& getStats() {return m_stats;}
	static int pickBioseq(CDRefresher* refresher, CRef< CSeq_align > seqAlignRef, 
						   vector< CRef< CBioseq > >&  bioseqVec);
	static int GetAllIdsFromSeqEntry(CRef< CSeq_entry > seqEntry, 
		vector< CRef< CSeq_id > >& slaveIds, bool pdbOnly=false);
	static bool GetOneBioseqFromSeqEntry(CRef< CSeq_entry > seqEntry, 
		CRef< CBioseq >& bioseq, const CSeq_id* seqId=0);
	static int getGi(CRef< CSeq_entry > seqEntry);
	static int getGi(CRef<CBioseq> bioseq);
	static bool SeqEntryHasSeqId(CRef< CSeq_entry > seqEntry, const CSeq_id& seqId);
	static bool BioseqHasSeqId(const CBioseq& bioseq, const CSeq_id& seqId);

	//get org-ref from seqEntry if bioseq does not have one
	//remove all unnecessary fields
	//replace ftable with mmdb-id
	static bool reformatBioseq(CRef< CBioseq > bioseq, CRef< CSeq_entry > seqEntry, CEntrez2Client& client);

	//static unsigned int removeRedundantPending(CCdCore* cd);   //  uses original non-redundifier
	//static unsigned int removeRedundantPending2(CCdCore* cd, bool keepNormalRows);  //  uses new non-redundifier
	//copied from objtools/alnmgr/util/showalign.cpp
	static CRef<CBlast_def_line_set>  GetBlastDefline (const CBioseq& handle);
	static void RemoveBlastDefline (CBioseq& handle);
	static int SplitBioseqByBlastDefline (CRef< CBioseq > handle, vector< CRef<CBioseq> >& bioseqs);
	static void reformatBioseqByBlastDefline(CRef<CBioseq> bioseq, CRef< CBlast_def_line > blastDefline, int order);
private:
	bool passedFilters(CCdCore* cd, CRef< CSeq_align > seqAlign, 
						CRef< CSeq_entry > seqEntry);

    //  Ignore overlaps and return 'false' when overlap <= CDUpdateStats::allowedOverlapWithCDRow, or ignore
    //  *all* overlaps when CDUpdateStats::allowedOverlapWithCDRow < 0.
	bool overlapWithCDRow(CCdCore* cd,CRef< CSeq_align > seqAlign);
	bool modifySeqAlignSeqEntry(CCdCore* cd, CRef< CSeq_align >& seqAlign, 
						CRef< CSeq_entry > seqEntry);
	bool findRowsWithOldSeq(CCdCore* cd, CBioseq& bioseq);
	void retrieveAllSequences(CSeq_align_set& alignments, vector< CRef< CBioseq > >& bioseqs);
	bool findSeq(CRef<CSeq_id> seqID, vector< CRef< CBioseq > >& bioseqs, CRef<CSeq_entry>& seqEntry);

    double ComputePercentIdentity(const CRef< CSeq_align >& alignment, const string& queryString, const string& subjectString);

	CdUpdateParameters m_config;
	CDUpdateStats m_stats;
	string m_rid;
	CCdCore* m_cd;
	string m_lastError;
	cd_utils::BlockModelPair* m_guideAlignment; //consensus::master
	string m_consensus;
	CEntrez2Client m_client;
	int m_processPendingThreshold; //<0, don't do it
	CRef< CSeq_id > m_masterPdb;
	CRef<CSeq_align_set> m_hits;
	int m_hitsNeeded;
	int m_blastQueryRow;

    static void OssToDefline(const CUser_field::TData::TOss & oss, CBlast_def_line_set& bdls);
};

class NCBI_CDUTILS_EXPORT GroupUpdater : public UpdaterInterface
{
public:
	GroupUpdater(vector<CCdCore*>& cds, CdUpdateParameters& config);
	virtual ~GroupUpdater(); //delete all in m_cdUpdaters
	
	//UpdaterInterface
	int  submitBlast(bool wait=false, int row=0);
	bool getBlastHits();
	bool processBlastHits();
	void getCds(vector<CCdCore*>&);
	bool hasCd(CCdCore*);

private:
	vector<CDUpdater*> m_cdUpdaters;

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
