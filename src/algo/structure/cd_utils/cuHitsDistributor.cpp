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
 *       Distribute PSI-BLAST hits among CDs
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuHitsDistributor.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

GiFootPrint::GiFootPrint(CRef<CSeq_align> seqAlign)
{
	const CSeq_id& seqId = seqAlign->GetSeq_id(1);
	gi = 0;
	if (seqId.IsGi())
		gi = seqId.GetGi();
	from = seqAlign->GetSeqStart(1);
	to = seqAlign->GetSeqStop(1);
}

HitDistributor::HitDistributor()
{

}

HitDistributor::~HitDistributor()
{
}

void HitDistributor::addBatch(CRef<CSeq_align_set> seqAlignSet)
{
	if (!seqAlignSet.Empty())
	{
		m_batches.push_back(seqAlignSet);
		if (seqAlignSet->IsSet())
		{
			list< CRef< CSeq_align > >& seqAlignList = seqAlignSet->Set();
			list< CRef< CSeq_align > >::iterator lit = seqAlignList.begin();
			//cdLog::Printf(0, "This batch has %d hits.\n", seqAlignList.size());
			for(; lit != seqAlignList.end(); lit++)
			{
				GiFootPrint gfp(*lit);
				if (gfp.gi > 0)
					m_hitTable[gfp].push_back(&(*lit));
				else
					ERR_POST("A SeqAlign without a GI is detected.");  //should not happen normally for BLAST hits
			}
		}
		else
			ERR_POST("No hit for this Blast.");
	}
	else
		ERR_POST("No hit for this Blast.");
}

void HitDistributor::distribute()
{
	FootprintToHitMap::iterator fit = m_hitTable.begin();
	for (; fit != m_hitTable.end(); fit++)
	{
		vector< CRef< CSeq_align >* >& hits = fit->second;
		if (hits.size() <= 1)
			continue;
		CRef< CSeq_align >* seqAlignRef = (hits[0]);
		double min_evalue, evalue;
		int min_id = 0;
		if (!(*seqAlignRef)->GetNamedScore("e_value", min_evalue))
		{
			ERR_POST("Can't get evalue from SeqAlign.  Something is wrong");
			continue;
		}
		for (int i = 1; i < (int) hits.size(); i++)
		{
			seqAlignRef = hits[i];
			if (!(*seqAlignRef)->GetNamedScore("e_value", evalue))
			{
				ERR_POST("Can't get evalue from SeqAlign.  Something is wrong");
				continue;
			}
			if (evalue < min_evalue)
			{
				min_evalue = evalue;
				min_id = i;
			}
		}
		for (int i = 0; i < (int) hits.size(); i++)
		{
			seqAlignRef = hits[i];
			if (i != min_id)
			{
				/*
				cdLog::Printf(0, "Remove (%d:%d-%d)from %s\n", fit->first.gi,  fit->first.from, 
					fit->first.to, (*seqAlignRef)->GetSeq_id(0).AsFastaString().c_str());*/
				seqAlignRef->Reset();
			}
			/*
			else
				cdLog::Printf(0, "Keep (%d:%d-%d)for %s\n", fit->first.gi,  fit->first.from, 
					fit->first.to, (*seqAlignRef)->GetSeq_id(0).AsFastaString().c_str());*/
		}
	}
	//remove all Empty CRef from m_batches
	for (unsigned int b =0; b < m_batches.size(); b++)
	{
		list< CRef< CSeq_align > >& seqAlignList = m_batches[b]->Set();
		list< CRef< CSeq_align > >::iterator lit = seqAlignList.begin();
		int num = 0;
		while(lit != seqAlignList.end())
		{
			if ( lit->Empty() )
			{
				lit = seqAlignList.erase(lit);
				num++;
			}
			else
				lit++;
		}
		//ERR_POST("Number of hitremoved from this batch.  It now has %d hits.\n",num, seqAlignList.size()); 
	}
}

void HitDistributor::dump(string filename)
{
	CNcbiOfstream outStream(filename.c_str(), IOS_BASE::out | IOS_BASE::binary);
	string err;
    if (!outStream) {
        err = "Cannot open file for writing";
		return;
    }
	FootprintToHitMap::iterator fit = m_hitTable.begin();
	for (; fit != m_hitTable.end(); fit++)
	{
		vector< CRef< CSeq_align >* >& hits = fit->second;
		const GiFootPrint& gfp = fit->first;
		outStream<<"GI-Footprint"<<gfp.gi<<':'<<gfp.from<<'-'<<gfp.to<<endl;
		for (unsigned int i = 0; i < hits.size(); i++)
		{
			if (!WriteASNToStream(outStream, **hits[i], false,&err))
				LOG_POST("Failed to write to "<<filename<<" because of "<<err);
		}
	}
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
