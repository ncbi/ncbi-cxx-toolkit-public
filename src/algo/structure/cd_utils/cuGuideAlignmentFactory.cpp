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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *          Factory for generation of guide alignment objects.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
//#include <algo/structure/cd_utils/cuUtils.hpp>
//#include <algo/structure/cd_utils/cuGuideAlignment.hpp>
//#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
#include <algo/structure/cd_utils/cuGuideAlignmentFactory.hpp>
#include <algo/structure/cd_utils/cuClassicalGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

/*
//  True only if seqId is a local string identifier that starts with the specified string.
//  False is startingStr is empty.
bool LocalIdStartsWith(const CRef< CSeq_id >& seqId, const string& startingStr)
{
    bool result = (startingStr.length() > 0 && seqId.NotEmpty());
    if (result && seqId->IsLocal() && seqId->GetLocal().IsStr()) {
        const string& localId = seqId->GetLocal().GetStr();
        result = NStr::StartsWith(localId, startingStr);
    }
    return result;
}

//  Any seq-id name that starts with 'consensus' is replaced with newName.
void RenameConsensus(CRef< CSeq_align >& seqAlign, const string& newName) 
{
    static const string consensus = "consensus";

    TDendiag* ddSet;
    TDendiag_it ddIt, ddEnd;
    CDense_diag::TIds ids;
    CDense_diag::TIds::iterator idIt, idEnd;

    if (GetDDSetFromSeqAlign(*seqAlign, ddSet)) {
        ddEnd = ddSet->end();
        for (ddIt = ddSet->begin(); ddIt != ddEnd; ++ddIt) {

            idEnd = (*ddIt)->SetIds().end();
            for (idIt = (*ddIt)->SetIds().begin(); idIt != idEnd; ++idIt) {
                if (LocalIdStartsWith(*idIt, consensus)) {
                    (*idIt)->SetLocal().SetStr() = newName;
                }
            }
        }
    }
}

//  Change 'consensus' to 'consensus_<cdAccession>' in seqannot and sequences ASN.1 fields
//  (initially, not dealing w/ the annotations) 
//  'revert' = true means to return the modified CD to using the string 'consensus'.
bool RenameConsensus(CCdCore* cd, bool revert = false)
{
    static const string consensus = "consensus";
    static const string consensusUnderscore = "consensus_";

    bool result = (cd && cd->HasConsensus());
//    bool consIsMaster;
    unsigned int nRows, nConsSeqs, nPending;
    string newName;
    vector<int> consSeqs;
    CRef< CSeq_align > seqAlign;
    CRef< CSeq_id > seqId;
    list< CRef< CSeq_id > > seqIds;
    list< CRef< CSeq_id > >::iterator seqIdIt, seqIdEnd;

    if (result) {

        newName = (revert) ? consensus : consensusUnderscore + cd->GetAccession();

        nRows = cd->GetNumRows();
        nPending = cd->GetNumPending();

//        consIsMaster = cd->UsesConsensusSequenceAsMaster();

        //  change name in each normal seq-align; skip i = 0 as i = 1 gets the same seq-align.
        for (unsigned int i = 1; i < nRows; ++i) {
            if (GetSeqAlign(i, seqAlign)) {
                RenameConsensus(seqAlign, newName);
            }
        }

        //  check pending rows for consensus
        if (nPending > 0) {
            CCdd::TPending pending= cd->SetPending();
            CCdd::TPending::iterator pit = pending.begin(), pend = pending.end();
            for (; pit != pend; ++pit) {
                if ((*pit)->IsSetSeqannot() && (*pit)->SetSeqannot().SetData().IsAlign()) {
                    list< CRef< CSeq_align > >& seqAligns = (*pit)->SeqSeqannot().SetData().SetAlign();
                    list< CRef< CSeq_align > >::iterator saIt = seqAligns.begin(), saEnd = seqAligns.end();
                    for (; saIt != saEnd; ++saIt) {
                        RenameConsensus(*saIt, newName);
                    }
                }
            }
        }


        //  replace the name in the sequence list
        if (cd->FindConsensusInSequenceList(&consSeqs)) {
            nConsSeqs = consSeqs.length();
            for (unsigned int i = 0; i < nConsSeqs; ++i) {
                if (cd->GetSeqIDs(consSeqs[i], seqIds)) {
                    seqIdEnd = seqIds.end();
                    for (seqIdIt = seqIds.begin(); seqIdIt != seqIdEnd; ++seqIdIt) {
                        if (LocalIdStartsWith(*seqidIt, consensus)) {
                            (*seqIdIt)->SetLocal().SetStr() = newName;
                        }
                    }
                }
            }
        }

        //  not replacing name in the annotations....

    }
    return result;
}
*/


void CGuideAlignmentFactory::RemoveConsensusMasters(CDFamily& family)
{
    CCdCore* cd;
    CDFamilyIterator famIt = family.begin(), famEnd = family.end();

	for (;  famIt != famEnd;  ++famIt) {
        cd = famIt->cd;
		if (cd->UsesConsensusSequenceAsMaster()) {
			ReMasterCdWithoutUnifiedBlocks(cd, 1,false);
			list< CRef< CSeq_align > >& seqAligns = cd->GetSeqAligns();
			m_consensusStore.insert(ConsensusStoreVT(cd, *(seqAligns.begin())));
			seqAligns.erase(seqAligns.begin());
		}
	}
}

void CGuideAlignmentFactory::RestoreConsensusMasters(CDFamily& family)
{
    CCdCore* cd;
    CDFamilyIterator famIt = family.begin(), famEnd = family.end();
    ConsensusStoreIt csIt, csEnd = m_consensusStore.end();

	for (;  famIt != famEnd;  ++famIt) {
        cd = famIt->cd;
        csIt = m_consensusStore.find(cd);
        if (csIt != csEnd) {
			list< CRef< CSeq_align > >& seqAligns = cd->GetSeqAligns();
			seqAligns.push_front(csIt->second);
			ReMasterCdWithoutUnifiedBlocks(cd, 1,false);
        }
    }

    m_consensusStore.clear();

}

bool CGuideAlignmentFactory::RemapGuideToConsensus(CGuideAlignment_Base& gaToMap, ConsensusStoreIt& master, ConsensusStoreIt& slave) {
    ConsensusStoreIt csEnd = m_consensusStore.end();
    bool isMasterCons  = (master  != csEnd);
    bool isSlaveCons = (slave != csEnd);
    bool result = false;

    BlockModelPair bmpMaster(master->second), bmpSlave(slave->second);
    bmpMaster.reverse();
    bmpSlave.reverse();

    _TRACE("RemapGuideToConsensus:  before:\n" << gaToMap.ToString());
    _TRACE("\nOriginal master:  consensus:\n" << bmpMaster.getMaster().toString());
    _TRACE("\nOriginal master:      row 1:\n" << bmpMaster.getSlave().toString());
    _TRACE("\nOriginal slave:   consensus:\n" << bmpSlave.getMaster().toString());
    _TRACE("\nOriginal slave:       row 1:\n" << bmpSlave.getSlave().toString());


    if (isMasterCons || isSlaveCons) {
        if (isMasterCons && isSlaveCons) {
            result = gaToMap.ReplaceMaster(master->second, false);  // consensus is the slave in master.second
            result &= gaToMap.ReplaceSlave(slave->second, false);   // consensus is the slave in slave.second
        } else if (isMasterCons) {  // isSlaveCons == false
            result = gaToMap.ReplaceMaster(master->second, false);  // consensus is the slave in master.second
        } else {                   // isSlaveCons == true; isMasterCons == false
            result = gaToMap.ReplaceSlave(slave->second, false);    // consensus is the slave in slave.second
        }
    }

    _TRACE("RemapGuideToConsensus:  after (result = " << result << "):\n" << gaToMap.ToString());

    return result;
} 

void CGuideAlignmentFactory::MakeAllGuidesToRoot(CDFamily* family, GuideMap& guideMap)
{
    guideMap.clear();
    if (!family) return;

    CMastersClassicalGuideAlignment* ga;
    CCdCore* thisCd;
    CCdCore* rootCd = family->getRootCD();
    CDFamilyIterator famIt = family->begin(), famEnd = family->end();
    ConsensusStoreIt csIt, csRootIt;

    //  This call modifies family to take out the consensus from the alignment.
    //  Needs to be done regardless of whether the consensus is allowed in the
    //  guide alignment, otherwise the MultipleAlignment object can be confused
    //  by multiple CDs each using the local id 'consensus'
    RemoveConsensusMasters(*family);
    csRootIt = m_consensusStore.find(rootCd);

	for (;  famIt != famEnd;  ++famIt) {
        ga = NULL;
        thisCd = famIt->cd;
        if (thisCd != rootCd) {
            ga = new CMastersClassicalGuideAlignment();
            if (ga) {
                ga->MakeGuideToRoot(thisCd, family, true);   //  cache the multiple alignment 
            }
        }

        //  If consensus master is allowed in the guide, and one exists, need to remap
        //  the guide alignment.  If the root has a consensus, still need to remap for
        //  all decendent CDs even if they do not have a consensus!!
        if (m_allowConsensus && ga) {
            csIt = m_consensusStore.find(thisCd);
            RemapGuideToConsensus(*ga, csRootIt, csIt);
        }

        //  It is the caller's responsibility to see if the creation succeeded by calling CGuideAlignment methods.
        guideMap.insert(GuideMapVT(thisCd, ga));
	}

    //  Clean up the static cache as noone else likely needs it any longer.
    CMastersClassicalGuideAlignment::ClearAlignmentCache();

    //  Put consensus masters back if present.
    RestoreConsensusMasters(*family);


}

//  Infers the proper type of guide alignment needed based on the contents of the guide inputs,
//  then uses the virtual Make method to create objects of the associated class.
CGuideAlignment_Base* CGuideAlignmentFactory::MakeGuide(const CGuideAlignment_Base::SGuideInput& guideInput1, const CGuideAlignment_Base::SGuideInput& guideInput2)
{
    //  Simple case when need a classical alignment (only type supported right now...)
    CGuideAlignment_Base* ga = new CMastersClassicalGuideAlignment();
    if (ga) {
        ga->Make(guideInput1, guideInput2);
    }

    //  It is the caller's responsibility to see if the creation succeeded by calling CGuideAlignment methods.
    return ga;
}

void CGuideAlignmentFactory::DestroyGuides(GuideMap& guideMap)
{
    GuideMapIt it = guideMap.begin(), itEnd = guideMap.end();
    for (; it != itEnd; ++it) {
        delete it->second;
    }
    guideMap.clear();
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
