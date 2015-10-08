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

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuTaxClient.hpp>
#include <objects/id1/Entry_complexities.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/effsearchspace_calc.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objtools/blast/services/blast_services.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuCdUpdateParameters.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuCdUpdater.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuHitsDistributor.hpp>
//#include <algo/structure/cd_utils/cuBlastUtils.hpp>
#include <algo/structure/cd_utils/cuSequenceTable.hpp>
#include <algo/structure/cd_utils/cuPssmMaker.hpp>
#include <algo/structure/cd_utils/cuBlockIntersector.hpp>
#include <algo/structure/cd_utils/cuBlockFormater.hpp>
#include <objects/scoremat/Pssm.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
//#include <objects/seq/seqlocinfo.hpp>
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

CDUpdateStats::CDUpdateStats() : numBlastHits(0), numRedundant(0), numObsolete(0),
	numFilteredByOverlap(0){}

string CDUpdateStats::toString(bool detailed)
{
	int added = numBlastHits - envSeq.size() - fragmented.size() - overlap.size() 
		- noSeq.size() - badAlign.size() - numRedundant - oldNewPairs.size();
	string result = "Total number of sequences added to the pending list of the Cd:" + toString(added)
		+". ";
	if (numFilteredByOverlap > 0)
	{
		result += "Total number of pending sequences  that are not moved to normal alignment because of insufficient overlapping:"
			+ toString(numFilteredByOverlap) + ". ";
	}
	if (!detailed)
		return result;
	result += "Number of Blast Hits = ";
	result += toString(numBlastHits); 
	result += ". ";
	
	result += toString(envSeq, "Environmental Sequences");
	result += toString(fragmented, "Sequence Fragments");
	result += toString(overlap, "Alignments overlapping a row already in CD");
	result += toString(noSeq, "Alignments with no sequence data");
	result += toString(badAlign, "Alignments that are corrupted or do not match with the CD");
	if (numRedundant > 0)
	{
		result += "Alignments removed due to redundancy:";
		result += toString(numRedundant);
		result += ". ";
	}
	result += toString(oldNewPairs, "New sequences that can replace old sequences (in  parentheses) already in CD");
	result += ". ";
	if (numObsolete > 0)
	{
		result += "Numer of obsolete sequences removed:";
		result += toString(numObsolete);
		result += ". ";
	}
	return result;
}

string CDUpdateStats::toString(vector<TGi>& gis, string type)
{
	if (gis.size() <= 0)
		return "";
	string result = "Number of " + type + " =";
	result += toString((int)gis.size()); 
	result += ":\n";
	result += toString(gis);
	result += "\n";
	return result;
}

string CDUpdateStats::toString(vector<TGi>& gis)
{
	string result;
	for(unsigned int i = 0; i < gis.size(); i++)
	{
		result += toString(GI_TO(int, gis[i]));
		result += ",";
	}
	return result;
}

string CDUpdateStats::toString(int num)
{
	char buf[100];
	sprintf(buf, "%d", num);
	return string(buf);
}

string CDUpdateStats::toString(vector<OldNewGiPair>& giPairs, string type)
{
	if (giPairs.size() <= 0)
		return "";
	string result = "Number of " + type + " =";
	result += toString((int)giPairs.size()); 
	result += ":\n";
	for (unsigned int i = 0 ; i < giPairs.size(); i++)
	{
		result += toString(GI_TO(int, giPairs[i].second));
		result += "(";
		result += toString(GI_TO(int, giPairs[i].first));
		result += ")";
		result += ",";
	}
	result += "\n";
	return result;
}


/*---------------------------------------------------------------------------------*/
/*                     UpdaterInterface                                                   */
/*---------------------------------------------------------------------------------*/

list<UpdaterInterface*> UpdaterInterface::m_updaterList;

void UpdaterInterface::addUpdater(UpdaterInterface* updater)
{
	m_updaterList.push_back(updater);
}

bool UpdaterInterface::IsEmpty()
{
	return m_updaterList.empty();
}

void UpdaterInterface::removeUpdaters(const vector<CCdCore*>& cds)
{
	for (unsigned int i = 0; i < cds.size(); i++)
	{
		for (list<UpdaterInterface*>::iterator lit = m_updaterList.begin();
			lit != m_updaterList.end(); lit++)
		{
			if ((*lit)->hasCd(cds[i]))
			{
				UpdaterInterface* cup = *lit;
				m_updaterList.erase(lit);
				if (cup)
				{
					//cup->getCd()->ClearUpdateInfo();
					delete cup;
				}
				break;
			}
		}
	}
}

void UpdaterInterface::removeUpdaters(const vector<UpdaterInterface*>& updaters)
{
	for (unsigned int i = 0; i < updaters.size(); i++)
	{
		for (list<UpdaterInterface*>::iterator lit = m_updaterList.begin();
			lit != m_updaterList.end(); lit++)
		{
			if ((*lit) == updaters[i])
			{
				UpdaterInterface* cup = *lit;
				m_updaterList.erase(lit);
				if (cup)
				{
					//cup->getCd()->ClearUpdateInfo();
					delete cup;
				}
				break;
			}
		}
	}
}

int UpdaterInterface::checkAllBlasts(vector<UpdaterInterface*>& blasted)
{
	list<UpdaterInterface*>::iterator lit = m_updaterList.begin();
	while(lit != m_updaterList.end())
	{
		UpdaterInterface* updater = *lit;
		if(updater->getBlastHits())
		{
			blasted.push_back(updater);
		}
		lit++;
	}
	return blasted.size();
}

/*---------------------------------------------------------------------------------*/
/*                     GroupUpdater                                                   */
/*---------------------------------------------------------------------------------*/

GroupUpdater::GroupUpdater(vector<CCdCore*>& cds, CdUpdateParameters& config)
{
	for (unsigned int i = 0; i < cds.size(); i++)
		m_cdUpdaters.push_back(new CDUpdater(cds[i], config));
}

GroupUpdater::~GroupUpdater() //delete all in m_cdUpdaters
{
	for (unsigned int i = 0; i < m_cdUpdaters.size(); i++)
		delete (m_cdUpdaters[i]);
}
	
	//UpdaterInterface
int GroupUpdater::submitBlast(bool wait, int row)
{
    int count = 0;
    for (unsigned int i = 0; i < m_cdUpdaters.size(); i++)
	{
        if (!(m_cdUpdaters[i])->submitBlast(wait,row)) {
			return 0;  // return 0 if one fails so legacy "if" statements still work.
        }
        else {
            count++;
        }
	}
	return count;
}

bool GroupUpdater::getBlastHits()
{
	bool allDone = true;
	for (unsigned int i = 0; i < m_cdUpdaters.size(); i++)
	{
		if (!(m_cdUpdaters[i]->getBlastHits()))
			allDone = false;
	}
	return allDone;
}

bool GroupUpdater::processBlastHits()
{
	if (!getBlastHits())
	{
		LOG_POST("Not all BLASTs on the group are done.  Thus updating this group can't be done at this time.\n");
		return false;
	}
	//distribute
	HitDistributor dist;
	for (unsigned int i = 0; i < m_cdUpdaters.size(); i++)
	{
		dist.addBatch(m_cdUpdaters[i]->getAlignments());
	}
	//dist.dump("DistTab.txt");
	dist.distribute();
	//update individual CDs
	bool allDone = true;
	for (unsigned int i = 0; i < m_cdUpdaters.size(); i++)
	{
		if (!(m_cdUpdaters[i]->processBlastHits()))
			allDone = false;
	}
	return allDone;
}

void GroupUpdater::getCds(vector<CCdCore*>& cds)
{
	for (unsigned int i = 0; i < m_cdUpdaters.size(); i++)
	{
		m_cdUpdaters[i]->getCds(cds);
	}
}

bool GroupUpdater::hasCd(CCdCore* cd)
{
	for (unsigned int i = 0; i < m_cdUpdaters.size(); i++)
	{
		if (m_cdUpdaters[i]->hasCd(cd))
			return true;
	}
	return false;;
}

/*---------------------------------------------------------------------------------*/
/*                     CDUpdater                                                   */
/*---------------------------------------------------------------------------------*/

CDUpdater::CDUpdater(CCdCore* cd, CdUpdateParameters& config) 
: m_config(config), m_cd(cd), m_guideAlignment(0), m_processPendingThreshold(-1), m_hitsNeeded(-1),
  m_blastQueryRow(0)
{
}


CDUpdater::~CDUpdater()
{
	if (m_guideAlignment)
		delete m_guideAlignment;
}

int CDUpdater::submitBlast(bool wait, int row)
{
	m_blastQueryRow = row;
	bool blasted = false;
	try {
		blasted = blast(wait, row);
	}
	catch (blast::CBlastException& be) {
		blasted = false;
		m_lastError = "Blast exception in submitBlast for row " + NStr::IntToString(row) + ":\n";
        m_lastError += be.ReportAll();
	}
	catch (CException& e) {
		blasted = false;
		m_lastError = "NCBI C++ Toolkit exception in submitBlast for row " + NStr::IntToString(row) + ":\n";
        m_lastError += e.ReportAll();
	}
    catch (...) {
        blasted = false;
		m_lastError = "Unknown exception in submitBlast for row " + NStr::IntToString(row) + "\n";
    }
		
	if (blasted)
	{
		LOG_POST("RID of Blast for the update of " << m_cd->GetAccession() << " is " << getRid());
	}
	else
	{
		LOG_POST("Update of " << m_cd->GetAccession() << " failed due to error\n" << getLastError());
	}

    return(blasted ? 1 : 0);
}

bool CDUpdater::getBlastHits()
{
	if (!m_hits.Empty())
		return true;
	else
		return getHits(m_hits);
}

bool CDUpdater::processBlastHits()
{
	bool updated = false;
	
	//m_Data.cacheCD(m_cd->GetAccession().c_str(),"Updated by Blast",EDITTYPE_ALIGNMENT);
	if (!m_hits.Empty())
	{
		updated = true;
		update(m_cd, *m_hits);
		//LOG_POST("Stats of Updating "<<m_cd->GetAccession()<< ":\n"<<getStats().toString());
		unsigned numNoAlignment = getStats().badAlign.size();
		if (numNoAlignment > 0)
			LOG_POST("There are "<<numNoAlignment
			<<" hits whose alignments do not overlap with the CD.  This may indicate there are long insert to the CD alignment.  You can find the GIs for those hits in the log.");
	}
	else
	{
		LOG_POST("Found no BLAST hits to process for CD " << m_cd->GetAccession() << ". Will try again to retrieve the hits.\n");
	}
	//write update date and stats to scrapbook
	/*
	string updateStr("This CD is updated on ");
	CTime cur(CTime::eCurrent);
	CDate curDate(cur, CDate::ePrecision_day);
	string dateStr;
    curDate.GetDate(&dateStr);
	updateStr += dateStr;
	updateStr += ". ";
	updateStr += getStats().toString(false);
	updateStr += " ";
	updateStr += m_config.toString();
	list< CRef< CCdd_descr > > & descList = m_cd->SetDescription().Set();
	list< CRef< CCdd_descr > >::iterator it = descList.begin();
	for(; it != descList.end(); it++)
	{
	    if((*it)->IsScrapbook())
		    break;
	}
	CRef< CCdd_descr > scrapbook;
	if (it != descList.end())
		scrapbook = *it;
	else
	{
		scrapbook = new CCdd_descr;
		descList.push_back(scrapbook);
	}
	scrapbook->SetScrapbook().push_back(updateStr);*/
	return updated;
}

void CDUpdater::getCds(vector<CCdCore*>& cds)
{
	cds.push_back(m_cd);
}

bool CDUpdater::hasCd(CCdCore* cd)
{
	return m_cd == cd;
}

bool CDUpdater::blast(bool wait, int row)
{
	blast::CRemoteBlast* rblast;
	blast::CBlastProteinOptionsHandle* blastopt;
	CPSIBlastOptionsHandle * psiopt = NULL;
	if (m_config.blastType == eBLAST)
	{
		blastopt = new blast::CBlastProteinOptionsHandle(blast::CBlastOptions::eRemote);
		rblast = new blast::CRemoteBlast(blastopt);
	}
	else
	{
		psiopt = new blast::CPSIBlastOptionsHandle(blast::CBlastOptions::eRemote);
		//psiopt->SetCompositionBasedStats(eNoCompositionBasedStats);
		rblast = new blast::CRemoteBlast(psiopt);
		blastopt = psiopt;
	}
	//rblast-> SetVerbose();
	blastopt->SetSegFiltering(false);
	if (m_config.numHits > 0)
		blastopt->SetHitlistSize(m_config.numHits);
	if (m_config.evalue > 0)
		blastopt->SetEvalueThreshold(m_config.evalue);
	if (m_config.identityThreshold > 0) //does not seem to do anything with RemoteBlast
		blastopt->SetPercentIdentity((double)m_config.identityThreshold);
	rblast->SetDatabase(CdUpdateParameters::getBlastDatabaseName(m_config.database));

	string entrezQuery;
	if (m_config.organism != eAll_organisms)
	{
		entrezQuery += CdUpdateParameters::getOrganismName(m_config.organism);
		entrezQuery += "[Organism]";
	}
	if (m_config.entrezQuery.size() > 0)
		entrezQuery += m_config.entrezQuery;
	if (!entrezQuery.empty())
		rblast->SetEntrezQuery(entrezQuery.c_str());

	//set PSSM here
	if (m_config.blastType == eBLAST)
	{
		CRef<objects::CBioseq_set> bioseqs(new CBioseq_set);
		list< CRef< CSeq_entry > >& seqList = bioseqs->SetSeq_set();
		CRef< CSeq_entry > seqOld;

		if (!m_cd->GetSeqEntryForRow(row, seqOld)) {
            delete rblast;
			return false;
        }

		seqList.push_back(seqOld);
		CRef< CSeq_id > seqId = seqOld->SetSeq().SetId().front();
		TMaskedQueryRegions masks;
		int lo = m_cd->GetLowerBound(row);
		int hi = m_cd->GetUpperBound(row);
		int len = m_cd->GetSequenceStringByRow(row).length();
		if (lo > 0)
			masks.push_back(CRef<CSeqLocInfo>( new CSeqLocInfo(new CSeq_interval(*seqId, 0,lo-1),0)));
		if (hi < (len-1))
			masks.push_back(CRef<CSeqLocInfo> ((new CSeqLocInfo(new CSeq_interval(*seqId, hi + 1, len - 1),0))));
		if (masks.size() > 0)
		{
			TSeqLocInfoVector masking_locations;
			masking_locations.push_back(masks);
			rblast->SetQueries(bioseqs,masking_locations);
		}
		else
			rblast->SetQueries(bioseqs);
		//debug
		/*
		string err;
		if (!WriteASNToFile("blast_query", *bioseqs, false,&err))
			LOG_POST("Failed to write to blast_query");
		*/
		//end of debug
	}
	else //psi-blast
	{
		bool useConsensus = true;
		PssmMaker pm(m_cd, useConsensus, true);
		PssmMakerOptions config;
		config.unalignedSegThreshold = 35;
		config.requestFrequencyRatios = true;
		pm.setOptions(config);
		CRef<CPssmWithParameters> pssm = pm.make();

		// debugging *** ***
//		string err;
//		WriteASNToFile("pssm_debug", *pssm, false, &err);
		// debugging *** ***

		m_guideAlignment = new BlockModelPair(pm.getGuideAlignment());
		m_guideAlignment->degap();
		m_guideAlignment->reverse(); //keep con::master
		m_consensus = pm.getConsensus();
		//pssm->SetMatrix().SetIdentifier().SetStr("BLOSUM62");
		//debug
		psiopt->SetPseudoCount(pm.getPseudoCount());
		/*
		string err;
		string fname = m_cd->GetAccession();
		fname += ".pssm";
		if (!WriteASNToFile(fname.c_str(), *pssm, false,&err))
			LOG_POST("Failed to write to %s because of %s\n", fname.c_str(), err.c_str());
		else
			LOG_POST("PSSM is written to %s\n", fname.c_str());
		*/
		rblast->SetQueries(pssm);
	}


    //  Submit and, if requested, wait for blast results.
    //  Trap any exceptions and return 'false' in all such cases.
	bool blasted = false;
	try {
		if (wait) {
			blasted = rblast-> SubmitSync();
			m_rid = rblast->GetRID();
			//LOG_POST("RID="<<m_rid);
			getBlastHits();
		}

		blasted = rblast->Submit();

        if (!blasted) {
            m_lastError = rblast->GetErrors();
        }

	} catch (CRemoteBlastException& e) {
		m_lastError = "RemoteBlast exception in CDUpdater::blast() for row " + NStr::IntToString(row) + ":\n";
        m_lastError += e.ReportAll();
//		string err = e.GetErrCodeString();
//		LOG_POST("RemoteBlast exception in CDUpdater::blast() for row " << NStr::IntToString(row) << ":  error code = " << err);
	}
	catch (CException& e) {
		m_lastError = "NCBI C++ Toolkit exception in CDUpdater::blast() for row " + NStr::IntToString(row) + ":\n";
        m_lastError += e.ReportAll();
	}
    catch (...) {
		m_lastError = "Unknown exception in CDUpdater::blast() for row " + NStr::IntToString(row) + "\n";
    }

	m_rid = rblast->GetRID();
	delete rblast;
	return blasted;
}

bool CDUpdater::getHits(CRef<CSeq_align_set> & hits)
{
    bool done = false;
	blast::CRemoteBlast rblast(getRid());
	try {
		//LOG_POST("Calling RemoteBlast::CheckDone().\n");
		done = rblast.CheckDone();
		//LOG_POST("Returned from RemoteBlast::CheckDone().\n");
		if (done)
		{
			hits = rblast.GetAlignments();
		}
	} catch (...) {
		LOG_POST("Exception while getting BLAST hits of CD " << m_cd->GetAccession() << " for RID " << getRid());
	}
	return done;
}

bool CDUpdater::checkDone()
{
    bool done = false;
	blast::CRemoteBlast rblast(getRid());
	try {
		//LOG_POST("Calling RemoteBlast::CheckDone().\n");
		done = rblast.CheckDone();
		//LOG_POST("Returned from RemoteBlast::CheckDone().\n");

			//CCdCore::UpdateInfo ui = m_cd->GetUpdateInfo();
			//ui.status = CCdCore::BLAST_DONE;
			//m_cd->SetUpdateInfo(ui);

	} catch (...) {
		LOG_POST("Exception during CheckDone for CD " << m_cd->GetAccession() << ", RID " << getRid());
	}
	return done;
}

bool CDUpdater::checkBlastAndUpdate()
{
	CRef<CSeq_align_set> seqAligns;
	if(getHits(seqAligns))
	{
		if(!seqAligns.Empty())
		{
			update(m_cd, *seqAligns);
			SetUpdateDate(m_cd);
			//LOG_POST("Stats of Updating "<<m_cd->GetAccession()<<" are "<<getStats().toString());
			unsigned numNoAlignment = getStats().badAlign.size();
			if (numNoAlignment > 0)
				LOG_POST("There are hits whose alignments do not overlap with the CD.  This may indicate there are long insert to the CD alignment.  You find the GIs for those hits in the log\n");
		}
		else
		{
			LOG_POST("Got no alignment for BLAST hits for CD " << m_cd->GetAccession() << ". will try again to retrieve the hits.\n");
			return true; 
		}
		return true;
	}
	else
		return false;
}

double CDUpdater::ComputePercentIdentity(const CRef< CSeq_align >& alignment, const string& queryString, const string& subjectString)
{
    double result = 0.0;
    unsigned int nIdent = 0;
    unsigned int qLen = queryString.length(), sLen = subjectString.length();
    unsigned int i, j, qStart, qStop, sStart, sStop;

    if (alignment.Empty() || qLen == 0 || sLen == 0) return result;

    //  Note that is is only %id in the aligned region, and doesn't factor in any non-identities
    //  implicit in any parts of the query N- and/or C-terminal to the alignment.
	const CSeq_align::C_Segs::TDenseg& denseg = alignment->GetSegs().GetDenseg();
    double denom = (denseg.GetSeqStop(0) - denseg.GetSeqStart(0) + 1);

    CDense_seg::TStarts starts = denseg.GetStarts();
    CDense_seg::TLens lens = denseg.GetLens();

    for (i = 0; i < lens.size(); ++i) {
        //  Do this check before implicit cast to unsigned int when assigning to qStart, sStart.
        if (starts[2*i] < 0 || starts[2*i + 1] < 0) continue;  //  gap
        qStart = starts[2*i];
        sStart = starts[2*i + 1];

        qStop = qStart + lens[i] - 1;
        sStop = sStart + lens[i] - 1;
        if (qStop >= qLen || sStop >= sLen) continue;  //  string index out of range

        for (j = 0; j < lens[i]; ++j) {
            if (queryString[qStart + j] == subjectString[sStart + j]) ++nIdent;
        }
    }
    result = 100.0*nIdent/denom;
//    LOG_POST(nIdent << " identities found (" << result << "%)\nquery:    " << queryString << "\nsubject:  " << subjectString);

    return result;
}

bool CDUpdater::update(CCdCore* cd, CSeq_align_set& alignments)
{
	if ( !cd || (!alignments.IsSet()))
		return false;
	
    double pidScore = 0.0;
	list< CRef< CSeq_align > >& seqAligns = alignments.Set();
	m_stats.numBlastHits = seqAligns.size();
	vector< CRef< CBioseq > > bioseqs;
	LOG_POST("Got "<<m_stats.numBlastHits<<" blast hits for CD " << cd->GetAccession() << ".");
	retrieveAllSequences(alignments, bioseqs);

	SequenceTable seqTable;
	CDRefresher* refresher = 0;
	if (m_config.replaceOldAcc)
		refresher = new CDRefresher(cd);
	vector< CRef< CBioseq > > bioseqVec;
	for (unsigned int i = 0; i < bioseqs.size(); i++)
	{
		bioseqVec.clear();
		SplitBioseqByBlastDefline (bioseqs[i], bioseqVec);
		seqTable.addSequences(bioseqVec, true); //as a group
	}

    // debugging *** ***
    //string err;
    //WriteASNToFile("alignments.txt", alignments, false, &err);

	// debugging *** ***
	//seqTable.dump("seqTable.txt");
	//LOG_POST("Retrieved "<<bioseqs.size()<<" Bioseqs for blast hits\n"); 
	//LOG_POST("Process BLAST Hits and add them to "<<cd->GetAccession());
	int completed = 0;
	list< CRef< CSeq_align > >::iterator it = seqAligns.begin();
	//CID1Client id1Client;
	//id1Client.SetAllowDeadEntries(false);

	CRef< CSeq_id > seqID, querySeqID;
	CRef<CSeq_entry> seqEntry;
    CRef< CBioseq > queryBioseq(new CBioseq);
    string queryString, subjectString;

    //  Get bioseq corresponding to the query.
    if (it != seqAligns.end()) {
        querySeqID = (*it)->SetSegs().SetDenseg().GetIds()[0];
        if (!cd->CopyBioseqForSeqId(querySeqID, queryBioseq)) {
            queryBioseq.Reset();

            //  This message isn't relevant when the query is a PSSM.
            if (m_config.blastType == eBLAST)
                LOG_POST("No bioseq found in CD " << cd->GetAccession() << " for update query.");
        } else {
            queryString = GetRawSequenceString(*queryBioseq);
        }
    }


	//for BLAST, if the master is PDB, the master seqid (gi from BLAST) needs to be changed
    if (m_config.blastType == eBLAST && it != seqAligns.end())
	{
		CSeq_align::C_Segs& oldSegs = (*it)->SetSegs();
		CRef< CSeq_align::C_Segs::TDenseg> denseg( &(oldSegs.SetDenseg()) );
		vector< CRef< CSeq_id > >& seqIds= denseg->SetIds();
		CRef< CSeq_entry > masterSeq;
		cd->GetSeqEntryForRow(m_blastQueryRow, masterSeq);
		vector< CRef< CSeq_id > > pdbIds;
		GetAllIdsFromSeqEntry(masterSeq, pdbIds, true);
		if ((pdbIds.size() > 0) && SeqEntryHasSeqId(masterSeq, *seqIds[0]) && (!seqIds[0] ->IsPdb()))
			m_masterPdb = pdbIds[0];
	}
	for(; it != seqAligns.end(); it++)
    {
        pidScore = 0.0;
		CRef< CSeq_align > seqAlignRef = *it;
		//seqAlign from BLAST is in Denseg
		CSeq_align::C_Segs::TDenseg& denseg = seqAlignRef->SetSegs().SetDenseg();

        //  9/25/08:  CRemoteBlast dropped the identity count from the scores, and is a 
        //  pending issue (JIRA ticket http://jira.be-md.ncbi.nlm.nih.gov/browse/SB-114).
        //  Workaround is to compute the % identity directly, as done below in
        //  the function ComputePercentIdentity.
		if (false && m_config.identityThreshold > 0)
		{

            //  Note:  if the type isn't in the seq-align, pidScore remains 0.0 and the
            //  scan of seqAligns will be aborted.
            seqAlignRef->GetNamedScore(CSeq_align::eScore_IdentityCount, pidScore);

			int start = denseg.GetSeqStart(0);
			int stop = denseg.GetSeqStop(0);
			pidScore = 100*pidScore/(stop - start + 1);
			if ((int)pidScore < m_config.identityThreshold)
				break; //stop
		}
		//the second is slave
		if (denseg.GetDim() > 1)
			seqID = denseg.GetIds()[1];
		else
			break; //should not be here
        TGi gi  = seqID->GetGi();
		//if (gi == 50302132)
		//	int ttt = 1;
		//if(retrieveSeq(id1Client, seqID, seqEntry)) 
		vector< CRef< CBioseq > > bioseqVec;
		if(seqTable.findSequencesInTheGroup(seqID, bioseqVec) > 0)
		{
			//one SeqAlign returned from BLAST may represent the hits on several
			//different Bioseq that all have the same seq_data
			//pick the Seq_id from the most useful Bioseq
			if (bioseqVec.size() > 1)
			{
				int index = pickBioseq(refresher, seqAlignRef, bioseqVec); 
				seqEntry = new CSeq_entry;
				seqEntry->SetSeq(*bioseqVec[index]);
				seqID = denseg.GetIds()[1];
				gi  = seqID->GetGi();
                subjectString = GetRawSequenceString(*bioseqVec[index]);
			}
			else
			{
				seqEntry = new CSeq_entry;
				seqEntry->SetSeq(*bioseqVec[0]);
                subjectString = GetRawSequenceString(*bioseqVec[0]);
			}

            //  when there's no identity count in the seq-align, compute it directly
    		if (m_config.identityThreshold > 0)
	    	{
                pidScore = ComputePercentIdentity(seqAlignRef, queryString, subjectString);
        		if ((int)pidScore < m_config.identityThreshold)
		    		break; //stop
            }

			//change seqAlign from denseg to dendiag
			//use pdb_id if available
			if(!modifySeqAlignSeqEntry(cd, *it, seqEntry))
			{
				m_stats.badAlign.push_back(gi);
				continue;
			}
			
			bool passed = true;
			if (!m_config.noFilter)
				passed = passedFilters(cd, *it, seqEntry);
			if(passed) //not framented; not environmental seq
			{
				// add merge fragment later here

				//remaster the seqAlign from consensus to CD.master
				if (m_config.blastType == ePSI_BLAST)
				{
					BlockModelPair bmp(*it);
					bmp.remaster(*m_guideAlignment);
					CRef<CSeq_align> saRef = bmp.toSeqAlign();
					if (saRef.Empty())
					{
						m_stats.badAlign.push_back(gi);
						//LOG_POST("No valid alignment after remastering to the CD for maste.  gi|%d is ignored", gi);
						continue;
					}
					else
						(*it) = saRef;
				}
				//check to see if it is necessary to replace old sequences
				TGi replacedGi = INVALID_GI;
				if (m_config.replaceOldAcc)
					replacedGi = refresher->refresh(seqAlignRef, seqEntry);
				if (replacedGi > ZERO_GI)
				{
					m_stats.oldNewPairs.push_back(CDUpdateStats::OldNewGiPair(replacedGi, gi));
				}
				else
				{
					cd->AddPendingSeqAlign(*(it));
					//just add sequence now.  redundanticy will be removed later
					//if (cd->GetSeqIndex(seqID) < 0)
						cd->AddSequence(seqEntry);
				}
			}
		}
		else
			m_stats.noSeq.push_back(gi);
		completed++;
		if (m_hitsNeeded > 0)
		{
			if (completed >= m_hitsNeeded)
				break;
		}
		if ((completed % 500) == 0)
			LOG_POST("Processed "<<completed<<" of "<<m_stats.numBlastHits<<" hits.");
	}

    //  always keep normal rows w/ automatic NR
    LOG_POST("Finishing processing hits for "<<cd->GetAccession());
	/*
	if(m_config.nonRedundify)
	{
		progbar.Update(0, "Remove redundant and obsolete sequences");
		LOG_POST("Non-redundify pending in ", cd->GetAccession().c_str());
		m_stats.numRedundant = removeRedundantPending2(cd, true);
		LOG_POST("Finish non-redundifying pending in %s \n", cd->GetAccession().c_str());
	}
	progbar.Update(50);*/
	/*
	if (m_config.refresh)
	{
		vector<int> obsoleted;
		cdWorkshop::getRowsWithObsoleteGIs(cd->GetAccession(), obsoleted);
		vector<int> oldNonMaster;
		if (obsoleted.size() > 0)
		{
			//check if master is obsolete
			
			//collect child cd masters
			vector<cd_utils::BlockModel> masters;
			CDFamily* cdFamily = cdWorkshop::getSubFamily(cd->GetAccession());
			CDFamilyIterator famit;
			for (famit = cdFamily->begin(); famit != cdFamily->end(); ++famit) 
			{
				masters.push_back( cd_utils::BlockModel(*(famit->cd->GetSeqAligns().begin()),false) );
			}
			for(int i = 0; i < obsoleted.size();i++)
			{
				if (obsoleted[i] > 0)
				{
					BlockModel oldBm(cd->GetSeqAlign(obsoleted[i]));
					bool found = false;
					for (int k = 0; k < masters.size(); k++)
					{
						if (oldBm.overlap(masters[k]))
						{
							found = true;
							break;
						}
					}
					if (!found)
						oldNonMaster.push_back(obsoleted[i]);
				}
			}
		}
		if (oldNonMaster.size() < obsoleted.size())
			cdLog::WarningPrintf("The master sequence of %s or its descendant CD is obsolete.  It is not deleted at this time.\n", cd->GetAccession().c_str());
		m_stats.numObsolete = oldNonMaster.size();
		if ((m_stats.numObsolete > 0) && ((cd->GetNumRows() - m_stats.numObsolete) >= 2) )
			cd->EraseTheseRows(oldNonMaster);
	}*/
	//progbar.Update(100);
	//cd->EraseSequences(); 
	if (m_processPendingThreshold > 0)
	{
		m_stats.numFilteredByOverlap = mergePending(cd, m_processPendingThreshold,true);
	}
	if (refresher)
		delete refresher;
	return true;
}

int CDUpdater::mergePending(CCdCore* cd, int threshold, bool remaster)
{
	int excluded = processPendingToNormal(threshold, cd);
	if (remaster)
	{//check and remaster if necessary
		CRef< CSeq_entry > seqEntry;
		cd->GetSeqEntryForRow(0,seqEntry);
		vector< CRef< CSeq_id > > seqIds;
		GetAllIdsFromSeqEntry(seqEntry,seqIds, true);
		if (seqIds.size() == 0)
		{
			int nRows = cd->GetNumRows();
			int i = 1;
			for (; i < nRows; i++)
			{	
				CRef< CSeq_id >  SeqID;
				if (cd->GetSeqIDForRow(i-1,1, SeqID))
				{
					if (SeqID->IsPdb())
						break;
				}
			}
			if (i < nRows)
			{
				string err;
				ReMasterCdWithoutUnifiedBlocks(cd, i, true);
			}
		}
	}
	return excluded;
}


int CDUpdater::pickBioseq(CDRefresher* refresher, CRef< CSeq_align > seqAlignRef, 
						   vector< CRef< CBioseq > >&  bioseqVec)
{
	CSeq_align::C_Segs::TDenseg& denseg = seqAlignRef->SetSegs().SetDenseg();
	//the second is slave
	vector< CRef< CSeq_id > >& seqIdVec = denseg.SetIds();
	CRef< CSeq_id > seqID;
	assert(denseg.GetDim() > 1);
	seqID = seqIdVec[1];
	int index = -1;
	//if the current seqId's bioseq has a PDB , no change
	for (int i = 0; i < (int) bioseqVec.size(); i++)
	{
		if(BioseqHasSeqId(*(bioseqVec[i]), *seqID))
		{
			index = i;
			const CBioseq::TId& ids = bioseqVec[i]->GetId();
			CBioseq::TId::const_iterator it = ids.begin(), itend = ids.end();
			for (; it != itend; ++it) 
			{
				if ((*it)->IsPdb()) 
				{
					 return index;
				}
			}
		}
	}
	assert(index >= 0);
	//use other PDB if there is one.
	for (int i = 0; i < (int) bioseqVec.size(); i++)
	{
		if (i==index)
			continue;
		const CBioseq::TId& ids = bioseqVec[i]->GetId();
		CBioseq::TId::const_iterator it = ids.begin(), itend = ids.end();
		CRef< CSeq_id > giId;
		bool foundPDB =false;
		for (; it != itend; ++it) 
		{
			if ((*it)->IsPdb()) 
				foundPDB = 1;
			else if ((*it)->IsGi()) 
				giId = *it;
		}
		if (foundPDB)
		{
			//debug
			//LOG_POST("seqId before =%s\n",seqIdVec[1]->AsFastaString().c_str()); 
			seqIdVec[1] = giId; //replace id in SeqAlign
			//LOG_POST("seqId After =%s\n",seqIdVec[1]->AsFastaString().c_str()); 
			return i;
		}
	}
	
	//use the one whose older version is already in CD
	//this can be used to replace the old one later
	if (refresher)
	{
		for (int i = 0; i < (int) bioseqVec.size(); i++)
		{
			if (refresher->hasOlderVersion(bioseqVec[i]))
			{
				const CBioseq::TId& ids = bioseqVec[i]->GetId();
				CBioseq::TId::const_iterator it = ids.begin(), itend = ids.end();
				for (; it != itend; ++it) 
				{
					if ((*it)->IsGi()) 
					{
						seqIdVec[1] = (*it); //replace
						return i;
					}
				}
			}
		}
	}
	return index;
}

/*
unsigned int CDUpdater::removeRedundantPending2(CCdCore* cd, bool keepNormalRows)
{
    if (!cd) return 0;
    //  Make sure the CD updates its internal list of alignments and sequences
    cd->GetSeqAlignsIntoVector(true);
    cd->GetSeqEntriesIntoVector(true);

    string errs, msgs;
    int row;
    set<int> redundantPendingRows, redundantPendingRowsReindexed;

    //  Do not want the non-redundifier to silently toss structures.  Ask for verification,
    //  temporarily changing cfgNonRedundifier.keepFilters to keep all structures if so
    //  directed by the user.
    int tmpKeepFilters = cfgNonRedundifier.keepFilters;
    if (! (tmpKeepFilters & CCdCoreTreeNRCriteria::eKeep3dStructure)) {

        int answer = cdLog::Question("Yes|No", "Override Removal of Redundant Structures?", "The 'Keep All Structures' non-redundifier preference is not\nset, and new recruited structures may not be kept.\n\nDo you want to temporarily override your preference,\nand keep all recruited structures?");
        if (answer == 0) {
            cfgNonRedundifier.keepFilters |= CCdCoreTreeNRCriteria::eKeep3dStructure;
        }
    }

    NonRedundifier2* nr2 = cdWorkshop::DoNonredundifyCD(cd, redundantPendingRows, errs, CCdCore::USE_ALL_ALIGNMENT, keepNormalRows, &msgs);

    //  Return preference to original state.
    cfgNonRedundifier.keepFilters = tmpKeepFilters;

    for (set<int>::const_iterator sit = redundantPendingRows.begin(); sit != redundantPendingRows.end(); sit++) {
        row = (*sit >= CCdCore::PENDING_ROW_START) ? *sit - CCdCore::PENDING_ROW_START : *sit;
        redundantPendingRowsReindexed.insert(row);
	}

    delete nr2;

    //  this calls cd->EraseSequences() after removing rows
	cd->ErasePendingRows(redundantPendingRowsReindexed);
	return redundantPendingRowsReindexed.size();
}*/

bool CDUpdater::findRowsWithOldSeq(CCdCore* cd, CBioseq& bioseq)
{
	/*
	int gi  = bioseq.GetFirstId()->GetGi();
	if (gi == 45645177)
	{
		string err;
		WriteASNToFile("bioseq.dat", bioseq, false,&err);
	}*/
	CRef< CBioseq > bioseqRef(&bioseq);
	TGi giNew = getGi(bioseqRef);
	int num = cd->GetNumRows();
	vector<int> rows;
	string nAcc;
	int nVer=0;	
	CRef< CSeq_id > seqId;
	if (!GetAccAndVersion(bioseqRef, nAcc, nVer, seqId))
		return false;
	bool foundOldSeq = false;
	for (int i = 0; i < num; i++)
	{
		cd->GetBioseqForRow(i, bioseqRef);
		string oAcc;
		int oVer=0;
		if (GetAccAndVersion(bioseqRef, oAcc, oVer,seqId))
		{
			TGi giOld = getGi(bioseqRef);
			if ((oAcc == nAcc) && (giNew != giOld))
			{
				rows.push_back(i);
				m_stats.oldNewPairs.push_back(CDUpdateStats::OldNewGiPair(giOld, giNew));
				foundOldSeq = true;
			}
		}
	}
	return foundOldSeq;
}

bool CDUpdater::passedFilters(CCdCore* cd, CRef< CSeq_align > seqAlign, 
						CRef< CSeq_entry > seqEntry)
{
	//filter environmental seq
	CRef< CBioseq > bioseq;
	TGi gi =  getGi(seqEntry);
	if (!GetOneBioseqFromSeqEntry(seqEntry, bioseq))
	{
		m_stats.noSeq.push_back(gi);
		return false; //no seq is not acceptable
	}
/*    if (m_config.excludingTaxNodes == eEnvironmental_sequence && TaxDataSource::isEnvironmentalSeq(*bioseq))
	{
		m_stats.envSeq.push_back(gi);
		return false;
	}*/
	//filter fragmented
	if (m_config.missingResidueThreshold > 0 && isFragmentedSeq(cd, seqAlign, seqEntry))
	{
		m_stats.fragmented.push_back(gi);
		return false;
	}
    //  filter overlapping updates unless disabled
	if (m_config.allowedOverlapWithCDRow >= 0 && overlapWithCDRow(cd, seqAlign))
	{
		m_stats.overlap.push_back(gi);
		return false;
	}
	return true;
}

bool CDUpdater::overlapWithCDRow(CCdCore* cd,CRef< CSeq_align > seqAlign)
{
    //  Ignore overlaps by disabling this check when requested.
    int overlap = m_config.allowedOverlapWithCDRow;
    if (overlap < 0) return false;

    bool result = false;
	BlockModel bm(seqAlign);
    int lo, hi;
	int lastPos = bm.getLastAlignedPosition();
	int firstPos = bm.getFirstAlignedPosition();
	CRef< CSeq_id > seqId = bm.getSeqId();
	CRef< CSeq_id > seqIdRow;

    //  Scan until the first overlap of significant size found.
    //  Do not return 'false' after first seq-id match in case there are repeats.
	for(int i = 0; !result && i < cd->GetNumRows(); i++)
	{
		if(cd->GetSeqIDFromAlignment(i, seqIdRow))
		{
			if (SeqIdsMatch(seqId, seqIdRow))
			{
				lo = cd->GetLowerBound(i);
				hi = cd->GetUpperBound(i);
				if (lo + overlap <= firstPos)
                    result = (hi - overlap >= firstPos);
				else
					result = (lo + overlap <lastPos);

                if (result) {
                    if (overlap > 0) {
                        LOG_POST("CD sequence " << i << " [" << lo << ", " << hi << "] and proposed update with range [" << firstPos << ", " << lastPos << "] exceed maximum allowed overlap = " << overlap);
                    } else {
                        LOG_POST("Disallowed overlap of CD sequence " << i << " [" << lo << ", " << hi << "] and proposed update with range [" << firstPos << ", " << lastPos << "]");
                    }
                }
//				if (lo <= firstPos)
//					return hi >=firstPos;
//				else
//					return lo <lastPos;
			}
		}
	}
	return result;
}

bool CDUpdater::isFragmentedSeq(CCdCore* cd, CRef< CSeq_align > seqAlign, 
						CRef< CSeq_entry > seqEntry)
{
	int pssmLen = m_consensus.size(); //equal to PSSM length
	int lenAligned = GetNumAlignedResidues(seqAlign);
	if (lenAligned >= pssmLen)
		return false;
	BlockModel master(seqAlign, false);
	int mGapToN = master.getGapToNTerminal(0);
	//master is consensus at this point
	int mGapToC = master.getGapToCTerminal(master.getBlocks().size() -1, m_consensus.size());
	BlockModel slave(seqAlign);
	CRef< CBioseq > bioseq;
	if (!GetOneBioseqFromSeqEntry(seqEntry, bioseq))
		return true; //no seq is a fragmented seq
	int seqLen = GetSeqLength(*bioseq);
	int sGapToN = slave.getGapToNTerminal(0);
	int sGapToC = slave.getGapToCTerminal(slave.getBlocks().size() - 1, seqLen);
	int allowed = m_config.missingResidueThreshold;
	if ( ( mGapToN - sGapToN > allowed) ||
		 (mGapToC - sGapToC > allowed))
		 return true;
	else
		return false;
}

bool CDUpdater::retrieveSeq(CID1Client& client, CRef<CSeq_id> seqID, CRef<CSeq_entry>& seqEntry)
{
	/*int gi = 0;
	if (seqID->IsGi())
		gi = seqID->GetGi();
	else
		gi = client.AskGetgi(seqID.GetObject());
	if (gi == 0)
		return false;
	CID1server_maxcomplex maxplex;
	maxplex.SetMaxplex(eEntry_complexities_entry);
	maxplex.SetGi(gi);*/
	try {
		//seqEntry = client.AskGetsefromgi(maxplex);
		seqEntry = client.FetchEntry(*seqID, 1);
	} catch (...)
	{
		return false;
	}
	return true;
}

bool CDUpdater::findSeq(CRef<CSeq_id> seqID, vector< CRef< CBioseq > >& bioseqs, CRef<CSeq_entry>& seqEntry)
{
    for (unsigned int i = 0; i < bioseqs.size(); i++)
	{
		const CBioseq::TId& ids = bioseqs[i]->SetId();
        CBioseq::TId::const_iterator it = ids.begin(), itend = ids.end();
        for (; it != itend; ++it) {
		    if (SeqIdsMatch(seqID, *it)) 
		    {
			    seqEntry = new CSeq_entry;
			    seqEntry->SetSeq(*bioseqs[i]);
			    return true;
		    }
        }
		/*
		int gi1 = seqID->GetGi();
		int gi2 = getGi(bioseqs[i]);
		if (gi1 == gi2)
		{
			seqEntry = new CSeq_entry;
			seqEntry->SetSeq(*bioseqs[i]);
			return true;
		}*/
	}
	return false;
}

void CDUpdater::retrieveAllSequences(CSeq_align_set& alignments, vector< CRef< CBioseq > >& bioseqs)
{
	vector< CRef<CSeq_id> > seqids;
	unsigned int batchSize = 500;
	unsigned int maxBatchSize = 2000;
	list< CRef< CSeq_align > >& seqAligns = alignments.Set();
	list< CRef< CSeq_align > >::iterator lit = seqAligns.begin();
	for (; lit != seqAligns.end(); lit++)
	{
		seqids.push_back((*lit)->SetSegs().SetDenseg().GetIds()[1]);
		list< CRef< CSeq_align > >::iterator next = lit;
		next++;
		//the batch is full or reach the end
		if (seqids.size() >= batchSize || (next == (seqAligns.end())) )
		{
			string errors, warnings;
			vector< CRef< CBioseq > > bioseqBatch;
			try {
				//LOG_POST("Calling CBlastServices::GetSequences().\n");
				CBlastServices::GetSequences(seqids, "nr", 'p', bioseqBatch, errors,warnings);
				LOG_POST("Returned from CBlastServices::GetSequences() with a batch of " << bioseqBatch.size() << " sequences.");
			}
			catch (blast::CBlastException& be)
            {
				if (seqids.size() > maxBatchSize)
				{
					seqids.clear(); //give up on retrieving sequence on these hits.
					//LOG_POST("Retrieving sequences from RemoteBlast failed after repeated tries. Giving up on these %d blast hits");
				}
				else
					LOG_POST("Retrieving sequences from RemoteBlast failed with an exception of "<<be.GetErrCodeString());
				continue;
			} catch (...) 
            {
                LOG_POST("Unspecified exception during CBlastServices::GetSequences().  Skipping to next Seq-align.\n");
                continue;
            }

			if (seqids.size()!= bioseqBatch.size())
			{
				LOG_POST("Ask for "<< seqids.size()<<" sequences.  Got "<<bioseqBatch.size()<<" back\n");
				LOG_POST("Error="<<errors<<"\nWarnings="<<warnings);
			}
			seqids.clear();
			for (unsigned int i = 0 ; i < bioseqBatch.size(); i++)
			{
				bioseqs.push_back(bioseqBatch[i]);
			}
		}
	}
}

TGi CDUpdater::getGi(CRef< CSeq_entry > seqEntry)
{
	vector< CRef< CSeq_id > > seqIds;
	GetAllIdsFromSeqEntry(seqEntry, seqIds);
	for (unsigned int i = 0; i < seqIds.size(); i++)
		if (seqIds[i]->IsGi())
			return seqIds[i]->GetGi();
	return ZERO_GI;
}

TGi CDUpdater::getGi(CRef<CBioseq> bioseq)
{
	const list< CRef< CSeq_id > >& seqIds = bioseq->GetId();
	list< CRef< CSeq_id > >::const_iterator cit = seqIds.begin();
	for (; cit != seqIds.end(); cit++)
		if ((*cit)->IsGi())
			return (*cit)->GetGi();
	return ZERO_GI;
}

//change seqAlign from denseg to dendiag
//remaster back to the master.
//use pdb_id if available
bool CDUpdater::modifySeqAlignSeqEntry(CCdCore* cd, CRef< CSeq_align >& seqAlign, 
						CRef< CSeq_entry > seqEntry)
{
	CSeq_align::C_Segs& oldSegs = seqAlign->SetSegs();
	CRef< CSeq_align::C_Segs::TDenseg> denseg( &(oldSegs.SetDenseg()) );
	vector< CRef< CSeq_id > >& seqIds= denseg->SetIds();
	if(seqIds.size() <= 1)
		return false;

	if (!m_masterPdb.Empty())
	{
		seqIds[0].Reset(m_masterPdb.GetPointer());	
	}

	//if slave has a pdb-id use it in seqAlign
	vector< CRef< CSeq_id > > slaveIds;
	GetAllIdsFromSeqEntry(seqEntry, slaveIds, true); //pdb only
	if (slaveIds.size() > 0)
		seqIds[1].Reset( (slaveIds[0]).GetPointer() );
	
	if (seqEntry->IsSet())
	{
		//pick the right BioSeq from the Set
		CRef< CBioseq > bioseq;
		if (GetOneBioseqFromSeqEntry(seqEntry, bioseq, seqIds[1].GetPointer()))
		{
			if (!reformatBioseq(bioseq, seqEntry, m_client))
				return false;
			seqEntry->SetSeq(*bioseq);
		}
		else
			return false;
	}
	else
	{
		CRef< CBioseq > bioseq(&seqEntry->SetSeq());
		if (!reformatBioseq(bioseq, seqEntry, m_client))
			return false;
	}

	CSeq_align::C_Segs::TDendiag& dendiag = seqAlign->SetSegs().SetDendiag();
	Denseg2DenseDiagList(*denseg, dendiag);
	/*
	BlockModelPair bmp(seqAlign);
	bmp.remaster(*m_guideAlignment);
	seqAlign = bmp.toSeqAlign();*/

	return true;
}

//get org-ref from seqEntry if bioseq does not have one
//remove all unnecessary fields
//replace ftable with mmdb-id
bool CDUpdater::reformatBioseq(CRef< CBioseq > bioseq, CRef< CSeq_entry > seqEntry, CEntrez2Client& client )
{
	//get BioSource if there is none in bioseq
	CSeq_descr& seqDescr = bioseq->SetDescr();
	bool hasSource = false;
	bool hasTitle = false;
	//reset all fields except the source field

	//need trim even if bioseq is not a Set
	if (seqDescr.IsSet())
	{
		list< CRef< CSeqdesc > >& descrList = seqDescr.Set();
		list< CRef< CSeqdesc > >::iterator cit = descrList.begin();
		while (cit != descrList.end())
		{
			if ((*cit)->IsSource() && (!hasSource)) //only keep one source field
			{
				hasSource = true;
				cit++;
			}
			else if ( (*cit)->IsTitle())
			{
				cit++;
				hasTitle = true;
			}
			 //extract taxid/taxname from "TaxNamesData" field
			//blastdb uses it to send tax info
			else if ((*cit)->IsUser() && (!hasSource))
			{
				if ((*cit)->SetUser().SetType().SetStr() == "TaxNamesData")
				{
					vector< CRef< CUser_field > >& fields = (*cit)->SetUser().SetData();
					if ( fields.size() > 0)
					{
						CRef< CUser_field > field = fields[0];
						int taxid = field->GetLabel().GetId();
						string taxname = field->GetData().GetStrs()[0];
						//create a source seedsc and add it
						CRef< CSeqdesc > source(new CSeqdesc);
						COrg_ref& orgRef = source->SetSource().SetOrg();
						orgRef.SetTaxId(taxid);
						orgRef.SetTaxname(taxname);
						descrList.push_back(source);
						hasSource = true;
					}
				}
				cit = descrList.erase(cit);
			}
			else
				cit = descrList.erase(cit);
		}
	}
	if (!hasSource)
	{
		//get source or org-ref from seqEntry
		if (seqEntry->IsSet())
		{
			const list< CRef< CSeqdesc > >& descrList = seqEntry->GetSet().GetDescr().Get();
			list< CRef< CSeqdesc > >::const_iterator cit = descrList.begin();
			for (; cit != descrList.end(); cit++)
			{
				if ((*cit)->IsSource())
				{
					seqDescr.Set().push_back(*cit);
					break;
				}
			}
		}
	}
	// if bioSeq is pdb
	//replace annot field with mmdb-id
	//otherwise reset annot field
	bioseq->ResetAnnot();
	const list< CRef< CSeq_id > >& seqIds = bioseq->GetId();
	list< CRef< CSeq_id > >::const_iterator cit = seqIds.begin();
	bool isPdb = false;
	for (; cit != seqIds.end(); cit++)
	{
		if ((*cit)->IsPdb())
		{
			isPdb = true;
			break;
		}
	}
	if (isPdb)
	{
		//CEntrez2Client client;
		vector<TIntId> uids;
		string pdb = (*cit)->GetPdb().GetMol().Get();
		pdb += "[ACCN]";
		try {
			client.Query(pdb, "structure", uids);
		} catch (CException& e)
		{
			LOG_POST("\nFailed to retrieve mmdb-id for "<<pdb<<" because the error:\n "<<e.ReportAll());
			return false;
		}
		int mmdbId = 0;
		if (uids.size() > 0)
		{
			mmdbId = uids[0];
			CRef<CSeq_id> mmdbTag (new CSeq_id);
			CSeq_id::TGeneral& generalId = mmdbTag->SetGeneral();
			generalId.SetDb("mmdb");
			generalId.SetTag().SetId(mmdbId);
			CRef< CSeq_annot> seqAnnot (new CSeq_annot);
			seqAnnot->SetData().SetIds().push_back(mmdbTag);
			bioseq->SetAnnot().push_back(seqAnnot);
		}
		if (!hasTitle)
		{
			CRef< CPDB_block > pdbBlock;
			if (GetPDBBlockFromSeqEntry(seqEntry, pdbBlock))
			{
				CRef< CSeqdesc > seqDesc(new CSeqdesc);
				if (pdbBlock->CanGetCompound())
				{
					const list< string >& compounds = pdbBlock->GetCompound();
					if (compounds.size() != 0)
						seqDesc->SetTitle(*(compounds.begin()));
					seqDescr.Set().push_back(seqDesc);
				}
			}
		}
	}
	return true;
	
}

int CDUpdater::GetAllIdsFromSeqEntry(CRef< CSeq_entry > seqEntry, 
		vector< CRef< CSeq_id > >& slaveIds, bool pdbOnly)
{
	if (seqEntry->IsSeq())
	{
		const list< CRef< CSeq_id > >& seqIdList = seqEntry->GetSeq().GetId();
		list< CRef< CSeq_id > >::const_iterator lsii;
		for (lsii = seqIdList.begin(); lsii != seqIdList.end(); ++lsii) 
		{
			if (pdbOnly) 
			{
				if ((*lsii)->IsPdb())
					slaveIds.push_back(*lsii);
			}
			else
				slaveIds.push_back(*lsii);
		}
		return slaveIds.size();
	}
	else
	{
		list< CRef< CSeq_entry > >::const_iterator lsei;
		const list< CRef< CSeq_entry > >& seqEntryList = seqEntry->GetSet().GetSeq_set();  
		for (lsei = seqEntryList.begin(); lsei != seqEntryList.end(); ++lsei) 
		{
			 GetAllIdsFromSeqEntry(*lsei, slaveIds, pdbOnly);  //  RECURSIVE!!
		} 
		return slaveIds.size();
	}
}
//get only protein
bool CDUpdater::GetOneBioseqFromSeqEntry(CRef< CSeq_entry > seqEntry, 
		CRef< CBioseq >& bioseq,const CSeq_id* seqId)
{
	if (seqEntry->IsSeq())
	{
		if (seqEntry->GetSeq().IsAa())
		{
			if (seqId)
			{
				if (SeqEntryHasSeqId(seqEntry, *seqId))
				{
					bioseq.Reset(&seqEntry->SetSeq());
					return true;
				}
				else
					return false;
			}
			else
			{
				bioseq.Reset(&seqEntry->SetSeq());
				return true;
			}
		}
		else
			return false;

	}
	else
	{
		list< CRef< CSeq_entry > >::const_iterator lsei;
		const list< CRef< CSeq_entry > >& seqEntryList = seqEntry->GetSet().GetSeq_set();  
		for (lsei = seqEntryList.begin(); lsei != seqEntryList.end(); ++lsei) 
		{
			 if (GetOneBioseqFromSeqEntry(*lsei, bioseq, seqId))  //  RECURSIVE!!
				 return true;
		} 
		return false;
	}
}

bool CDUpdater::SeqEntryHasSeqId(CRef< CSeq_entry > seqEntry, const CSeq_id& seqId)
{
	vector< CRef< CSeq_id > > seqIds;
	GetAllIdsFromSeqEntry(seqEntry, seqIds,false);
	for (unsigned int i = 0; i < seqIds.size(); i++)
	{
		if (seqIds[i]->Match(seqId))
			return true;
	}
	return false;
}

bool CDUpdater::BioseqHasSeqId(const CBioseq& bioseq, const CSeq_id& seqId)
{
	const CBioseq::TId& ids = bioseq.GetId();
    CBioseq::TId::const_iterator it = ids.begin(), itend = ids.end();
    for (; it != itend; ++it) 
	{
	    if ((*it)->Match(seqId)) 
	    {
		    return true;
	    }
    }
	return false;
}


int CDUpdater::SplitBioseqByBlastDefline (CRef<CBioseq> orig, vector< CRef<CBioseq> >& bioseqs)
{
	CRef<CBlast_def_line_set> blastDefLine = GetBlastDefline(*orig);
	RemoveBlastDefline(*orig);
	list< CRef< CBlast_def_line > >& deflines = blastDefLine->Set();
	//most cases
	if (deflines.size() <= 1)
	{
		bioseqs.push_back(orig);
		return 1;
	}
	//PDBs likely
	int order = 0;
	for (list< CRef< CBlast_def_line > >::iterator iter = deflines.begin();
		iter != deflines.end(); iter++)
	{
		CRef<CBioseq> splitBioseq(new CBioseq);
		splitBioseq->Assign(*orig);
		reformatBioseqByBlastDefline(splitBioseq, *iter, order);
		bioseqs.push_back(splitBioseq);
		order++;
	}
	return deflines.size();
}

void CDUpdater::reformatBioseqByBlastDefline(CRef<CBioseq> bioseq, CRef< CBlast_def_line > blastDefline, int order)
{
	CSeq_descr& seqDescr = bioseq->SetDescr();
	int sourceOrder = 0;
	if (seqDescr.IsSet())
	{
		list< CRef< CSeqdesc > >& descrList = seqDescr.Set();
		list< CRef< CSeqdesc > >::iterator cit = descrList.begin();
		while (cit != descrList.end())
		{
			if ((*cit)->IsSource()) //only keep one source field
			{
				if (sourceOrder == order)
					cit++; //keep
				else
					cit = descrList.erase(cit);

                //  Do this for both cases; if sourceOrder == order must increment
                //  otherwise will keep all sources *after* order.
                sourceOrder++;
			}
			else if ( (*cit)->IsTitle())
				cit = descrList.erase(cit);
		}
		//add the title from the defLine
		CRef< CSeqdesc > title(new CSeqdesc);
		title->SetTitle(blastDefline->GetTitle());
		descrList.push_back(title);
	}
	
	//add seq_ids from the defline
	bioseq->SetId().assign(blastDefline->GetSeqid().begin(), blastDefline->GetSeqid().end());
}

//  IMPORTANT:  This code is being forked from src/objtools/align_format/align_format_util.cpp.
//  Check for changes in original source if this forked version misbehaves in the future.
/// Efficiently decode a Blast-def-line-set from binary ASN.1.
/// @param oss Octet string sequence of binary ASN.1 data.
/// @param bdls Blast def line set decoded from oss.
void CDUpdater::OssToDefline(const CUser_field::TData::TOss & oss, CBlast_def_line_set& bdls)
{
    typedef const CUser_field::TData::TOss TOss;
    
    const char * data = NULL;
    size_t size = 0;
    string temp;
    
    if (oss.size() == 1) {
        // In the single-element case, no copies are needed.
        
        const vector<char> & v = *oss.front();
        data = & v[0];
        size = v.size();
    } else {
        // Determine the octet string length and do one allocation.
        
        ITERATE (TOss, iter1, oss) {
            size += (**iter1).size();
        }
        
        temp.reserve(size);
        
        ITERATE (TOss, iter3, oss) {
            // 23.2.4[1] "The elements of a vector are stored contiguously".
            temp.append(& (**iter3)[0], (*iter3)->size());
        }
        
        data = & temp[0];
    }
    
    CObjectIStreamAsnBinary inpstr(data, size);
    inpstr >> bdls;
}



//  IMPORTANT:  This code is being forked from src/objtools/align_format/align_format_util.cpp.
//  That method uses the object manager, however, so we're not calling the function directly.
//  Check for changes in original source if this forked version misbehaves in the future.
CRef<CBlast_def_line_set>  CDUpdater::GetBlastDefline (const CBioseq& bioseq)
{
	static const string asnDeflineObjLabel = "ASN1_BlastDefLine";

    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set());
    if(bioseq.IsSetDescr()){
        const CSeq_descr& desc = bioseq.GetDescr();
        const list< CRef< CSeqdesc > >& descList = desc.Get();
        for (list<CRef< CSeqdesc > >::const_iterator iter = descList.begin(); iter != descList.end(); iter++){
      
            if((*iter)->IsUser()){
                const CUser_object& uobj = (*iter)->GetUser();
                const CObject_id& uobjid = uobj.GetType();
                if(uobjid.IsStr()){
   
                    const string& label = uobjid.GetStr();
                    if (label == asnDeflineObjLabel){
                        const vector< CRef< CUser_field > >& usf = uobj.GetData();

                        if(usf.front()->GetData().IsOss()){ //only one user field
                            typedef const CUser_field::TData::TOss TOss;
                            const TOss& oss = usf.front()->GetData().GetOss();
                            OssToDefline(oss, *bdls);
                        }         
                    }
                }
            }
        }
    }
    return bdls;
}

void CDUpdater::RemoveBlastDefline (CBioseq& handle)
{
	static const string asnDeflineObjLabel = "ASN1_BlastDefLine";
	if(handle.IsSetDescr())
	{
		CSeq_descr& desc = handle.SetDescr();
		list< CRef< CSeqdesc > >& descList = desc.Set();
		for (list<CRef< CSeqdesc > >::iterator iter = descList.begin(); iter != descList.end(); iter++)
		{
			if((*iter)->IsUser())
			{
				const CUser_object& uobj = (*iter)->GetUser();
				const CObject_id& uobjid = uobj.GetType();
				if(uobjid.IsStr())
				{
					const string& label = uobjid.GetStr();
					if (label == asnDeflineObjLabel)
					{
						descList.erase(iter);
						return;
					}
				}
			}
		}
	}
}

int CDUpdater::processPendingToNormal(int overlap, CCdCore* cd)
{
	AlignmentCollection ac(cd); //default:pending only
	int num = ac.GetNumRows();
	int seqlen = cd->GetSequenceStringByRow(0).size();
	if (seqlen <= 0)
		return num;
	vector< CRef< CSeq_align > > seqAlignVec;
	for (int i = 0; i < num; i++)
		seqAlignVec.push_back(ac.getSeqAlign(i));
	cd_utils::BlockFormater bf(seqAlignVec, seqlen);
	list< CRef< CSeq_align > >& seqAlignList = cd->GetSeqAligns();
	if (seqAlignList.size() > 0)
	{
		BlockModelPair bmp(*seqAlignList.begin());
		if (bmp.getMaster() == bmp.getSlave()) //aligned to self; used as a seed
			seqAlignList.erase(seqAlignList.begin());
		if (seqAlignList.size() > 0)
			bf.setReferenceSeqAlign(*seqAlignList.begin());
	}
	int numGood = bf.findIntersectingBlocks(overlap);
	bf.formatBlocksForQualifiedRows(seqAlignList);
	set<int> goodRows;
	vector<int> rows;
	bf.getQualifiedRows(rows);
	for (unsigned int r = 0; r < rows.size(); r++)
		goodRows.insert(rows[r]);
	cd->ErasePendingRows(goodRows);
	return num - numGood;
}


CDRefresher::CDRefresher (CCdCore* cd)
: m_cd(cd)
{
	addSequences(cd->SetSequences());
}

void CDRefresher::addSequences(CSeq_entry& seqEntry)
{
	if (seqEntry.IsSet())
	{
		list< CRef< CSeq_entry > >& seqSet = seqEntry.SetSet().SetSeq_set();
		list< CRef< CSeq_entry > >::iterator it = seqSet.begin();
		for (; it != seqSet.end(); it++)
			addSequences(*(*it));
	}
	else
	{
		CRef< CBioseq > bioseq(&(seqEntry.SetSeq()));
		addSequence(bioseq);
	}
}

void CDRefresher::addSequence(CRef< CBioseq > bioseq)
{
	string acc;
	int ver=0;	
	CRef< CSeq_id > textId;
	if (GetAccAndVersion(bioseq, acc, ver, textId))
		m_accSeqMap.insert(AccessionBioseqMap::value_type(acc, bioseq));
}

/*
bool CDRefresher::hasBioseq(CRef< CBioseq > bioseq)
{
	CRef< CBioseq > bioseqTmp;
	list< CRef< CSeq_id > >& seqIds = bioseq->SetId();
	for (list< CRef< CSeq_id > >::iterator it= seqIds.begin(); it != seqIds.end(); it++)
	{
		if (m_seqTable.findSequence(*it, bioseqTmp))
			return true;
	}
	return false;
}*/

bool CDRefresher::hasOlderVersion(CRef< CBioseq > bioseq)
{
	string nAcc;
	int nVer=0;	
	CRef< CSeq_id > textId;
	if (!GetAccAndVersion(bioseq, nAcc, nVer, textId))
		return false;
	AccessionBioseqMap::iterator it = m_accSeqMap.find(nAcc);
	if (it != m_accSeqMap.end())
	{
		TGi newgi = CDUpdater::getGi(bioseq);
		TGi oldgi = CDUpdater::getGi(it->second);
		return newgi != oldgi;
	}
	else
		return false;
}

	//return the gi that's replaced; return -1 if none is replaced
TGi CDRefresher::refresh(CRef< CSeq_align> seqAlign, CRef< CSeq_entry > seqEntry)
{
	CRef< CBioseq > bioseqRef;
	if (!CDUpdater::GetOneBioseqFromSeqEntry(seqEntry, bioseqRef))
		return INVALID_GI;
	if (!hasOlderVersion(bioseqRef))
		return INVALID_GI;
	string nAcc;
	int nVer=0;	
	CRef< CSeq_id > textId;
	if (!GetAccAndVersion(bioseqRef, nAcc, nVer, textId))
		return INVALID_GI;
	CRef< CBioseq > oldBioseq = m_accSeqMap[nAcc];
	string newStr, oldStr;
	GetNcbieaaString(*bioseqRef, newStr);
	GetNcbieaaString(*oldBioseq, oldStr);
	if (newStr.size() != oldStr.size())
		return INVALID_GI;
	//proceed to do the placement
	vector< CRef< CSeq_id > > newIds;
	CDUpdater::GetAllIdsFromSeqEntry(seqEntry, newIds);
	CRef< CSeq_id > giId, pdbId;
	for (unsigned int i = 0; i < newIds.size(); i++)
	{
		if (newIds[i]->IsGi())
			giId = newIds[i];
		else if (newIds[i]->IsPdb())
			pdbId = newIds[i];
	}
	list< CRef< CSeq_align > >& seqAlignList = m_cd->GetSeqAligns();
	bool replaced = false;
	for (list< CRef< CSeq_align > >::iterator lit = seqAlignList.begin(); lit != seqAlignList.end(); lit++)
	{
		CRef< CSeq_align >& seqAlign = *lit;
		CRef< CSeq_id > idInAlign;
		GetSeqID(seqAlign, idInAlign, true);
		if (CDUpdater::BioseqHasSeqId(*oldBioseq, *idInAlign))
		{
			BlockModelPair bmp(seqAlign);
			
			if (pdbId.NotNull())
			{
				bmp.getSlave().setSeqId(pdbId);
				seqAlign = bmp.toSeqAlign();
				replaced =true;
			}
			else if (giId.NotNull())
			{
				bmp.getSlave().setSeqId(giId);
				seqAlign = bmp.toSeqAlign();
				replaced = true;
			}
		}
	}
	if (replaced)
	{
		m_cd->AddSequence(seqEntry);
		return CDUpdater::getGi(oldBioseq);
	}
	else
		return INVALID_GI;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
