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
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuConsensusMaker.hpp>
#include "objects/seqalign/Seq_align_set.hpp"
#include <algo/structure/cd_utils/cuSequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)
/*
ConsensusMaker::ConsensusMaker(CRef<CSeq_align_set> seqAlign, CCdCore* cd) :
	m_cd(cd), m_seqAligns(&(seqAlign->Set())), m_consensus(), m_rp()
{
	addRows();
	makeConsensus();
}*/

ConsensusMaker::ConsensusMaker(CCdCore* cd) :
	m_cd(cd), m_consensus(), m_rp(), m_seqAligns(cd->GetSeqAligns())
{
	addRows();
	CRef< CSeq_id > seqId;
	cd->GetSeqIDFromAlignment(0, seqId);
	if (!IsConsensus(seqId))
		makeConsensus();
}

ConsensusMaker::~ConsensusMaker()
{
}

void ConsensusMaker::addRows()
{
	list<CRef< CSeq_align > >::iterator lit = m_seqAligns.begin();
	if (lit == m_seqAligns.end())
		return;
	//build Residue profiles indexed by master
	string mSeq, sSeq;
	int seqinx = 0;
	// add slaves
	for(; lit != m_seqAligns.end(); lit++)
	{
		BlockModelPair bmPair(*(lit));
		if (lit == m_seqAligns.begin())
		{
			m_masterSeqId = bmPair.getMaster().getSeqId();
			if (IsConsensus(m_masterSeqId))
			{
				vector<int> seqIndice;
				m_cd->FindConsensusInSequenceList(&seqIndice);
				if (seqIndice.size() > 0)
					mSeq = m_cd->GetSequenceStringByIndex(seqIndice[0]);
			}
			else
			{
				seqinx = m_cd->GetSeqIndex(m_masterSeqId);
				mSeq = m_cd->GetSequenceStringByIndex(seqinx);
			}
			m_conSeqId = new CSeq_id(CSeq_id::e_Local, "consensus", "");
		}
		sSeq.clear();
		seqinx = m_cd->GetSeqIndex(bmPair.getSlave().getSeqId());
		sSeq = m_cd->GetSequenceStringByIndex(seqinx);
		m_rp.addOneRow(bmPair, mSeq, sSeq);
	}
}
void ConsensusMaker::makeConsensus()
{
	m_rp.calculateRowWeights();
	m_consensus = m_rp.makeConsensus();
	BlockModelPair& guideAlignment = m_rp.getGuideAlignment();
	guideAlignment.getMaster().setSeqId(m_masterSeqId);
	guideAlignment.getSlave().setSeqId(m_conSeqId);
}

const string& ConsensusMaker::getConsensus()
{
	return m_consensus;
}

CRef< CSeq_entry > ConsensusMaker::getConsensusSeqEntry()
{
	CRef< CSeq_entry > result(new CSeq_entry);
	CBioseq& bioseq = result->SetSeq();
	list< CRef< CSeq_id > >& idList = bioseq.SetId();
	idList.push_back(m_conSeqId);
	CSeq_inst& seqInst = bioseq.SetInst();
	seqInst.SetRepr(CSeq_inst::eRepr_raw);
	seqInst.SetMol(CSeq_inst::eMol_aa);
	seqInst.SetLength(m_consensus.size());
	CSeq_data& seqData = seqInst.SetSeq_data();
	seqData.SetNcbieaa(*(new CSeq_data::TNcbieaa(m_consensus)));
	//*(new CSeq_data(m_consensus, CSeq_data::e_Ncbieaa)));
	return result;
}

const BlockModelPair& ConsensusMaker::getGuideAlignment()
{
	return m_rp.getGuideAlignment();
}

CRef< CSeq_align > ConsensusMaker::getGuideSeqAlign()
{
	const BlockModelPair& guideAlignment = m_rp.getGuideAlignment();
	return guideAlignment.toSeqAlign();
}

CRef<CSeq_align_set> ConsensusMaker::remasterWithConsensus()const
{
	CRef<CSeq_align_set> result(new CSeq_align_set);
	list<CRef< CSeq_align > >& resultList = result->Set();
	const BlockModelPair& guideAlignment = m_rp.getGuideAlignment();
	//add the consensus to master
	BlockModelPair rev(guideAlignment);
	rev.reverse();
	resultList.push_back(rev.toSeqAlign());

	//list<CRef< CSeq_align > >& seqAlignList = m_seqAligns->Set();
	list<CRef< CSeq_align > >::const_iterator lit = m_seqAligns.begin();
	for (; lit != m_seqAligns.end(); lit++)
	{
		BlockModelPair bmp(*lit);
		bmp.remaster(guideAlignment);
		resultList.push_back(bmp.toSeqAlign());
	}
	return result;
}

void ConsensusMaker::remasterWithConsensus(bool extended)
{
	if (m_cd->UsesConsensusSequenceAsMaster())
		return;
	BlockModelPair guideAlignment(m_rp.getGuideAlignment());
	list< CRef< CSeq_align > >& cdAlignList = (*(m_cd->SetSeqannot().begin()))->SetData().SetAlign();
	list<CRef< CSeq_align > >* seqAlignLp = &cdAlignList;
	list<CRef< CSeq_align > > extendedSeqAlignList;
	if (extended)
	{
		degapAlignment(m_cd, extendedSeqAlignList);
		guideAlignment.degap();
		seqAlignLp = &extendedSeqAlignList;
	}
	list<CRef< CSeq_align > >& seqAlignList = *seqAlignLp;
	list<CRef< CSeq_align > >::iterator lit = seqAlignList.begin(); 
	for (; lit != seqAlignList.end(); lit++)
	{
		BlockModelPair bmp(*lit);
		bmp.remaster(guideAlignment);
		(*lit) = bmp.toSeqAlign();
	}
	//add consensus::old_master pair
	guideAlignment.reverse();
	seqAlignList.push_front(guideAlignment.toSeqAlign());
	int sizeE = extendedSeqAlignList.size();
	int sizeL = seqAlignList.size();
	int sizeCd = cdAlignList.size();

	if (extended)
		cdAlignList.assign(seqAlignList.begin(), seqAlignList.end());
	m_cd->AddSequence(getConsensusSeqEntry());
}

CRef<CSeq_align_set> ConsensusMaker::degapAlignment(CCdCore* cd)
{
	CRef< CSeq_align_set > seqAligns (new CSeq_align_set());
	list< CRef< CSeq_align > >& seqAlignList = seqAligns->Set();
	degapAlignment(cd, seqAlignList);
	return seqAligns;
}

void ConsensusMaker::degapAlignment(CCdCore* cd, list< CRef< CSeq_align > >& seqAlignList)
{
	int num = cd->GetNumRows();
	//skip i=0, it is the master not a real seqAlign row
	for (int i = 1; i < num; i++)
	{
		BlockModelPair bp(cd->GetSeqAlign(i));
		bp.degap();
		seqAlignList.push_back(bp.toSeqAlign());
	}
}

void ConsensusMaker::degapCdAlignment(CCdCore* cd)
{
	list< CRef< CSeq_align > > seqAlignList;
	degapAlignment(cd, seqAlignList);
	list< CRef< CSeq_align > >& cdAlignList = (*(cd->SetSeqannot().begin()))->SetData().SetAlign();
	cdAlignList.assign(seqAlignList.begin(), seqAlignList.end());
}

void ConsensusMaker::degapCdAlignmentToPending(CCdCore* cd)
{
	//CRef<CSeq_align_set> seqAlignSet = degapAlignment(cd);
	//list< CRef< CSeq_align > >& seqAlignList = seqAlignSet->Set();
	list< CRef< CSeq_align > > seqAlignList;
	degapAlignment(cd, seqAlignList);
	list< CRef< CSeq_align > >::iterator lit = seqAlignList.begin();
	for (; lit != seqAlignList.end(); lit++)
	{
		cd->AddPendingSeqAlign(*lit);
	}
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
