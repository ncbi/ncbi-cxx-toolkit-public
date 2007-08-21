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
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignmentFactory.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuClassicalGuideAlignment.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideToFamilyMember.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuTwoCDGuideAlignment.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuWithinFamilyGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)


const int CGuideAlignmentFactory::m_defaultOverlapPercent = 75;

bool CGuideAlignmentFactory::SetOverlapPercentage(int overlapPercentage)
{
    if (overlapPercentage < 0 || overlapPercentage > 100) return false;
    m_overlapPercentage = overlapPercentage;
    return true;
}

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



CGuideAlignment_Base* CGuideAlignmentFactory::MakeGuide(const CCdCore* cd1, const CCdCore* cd2)
{
    return MakeGuideInstance(cd1, cd2, NULL, false, false);
/*
    //  Only two class instances possible when no family is available.
    CGuideAlignment_Base* ga = (m_makeDegenerateGuides) ? new CDegenerateGuide() : new CTwoCDGuideAlignment(m_overlapPercentage);
    if (ga && !ga->Make(cd1, cd2)) {
        delete ga;
        ga = NULL;
    }

    //  It is the caller's responsibility to see if the creation succeeded by calling CGuideAlignment methods.
    return ga;
*/
}

CGuideAlignment_Base* CGuideAlignmentFactory::MakeGuide(const CCdCore* cd1, const CCdCore* cd2, const CDFamily* family)
{
    //  The casts are harmless; SGuideInput is only a container to pass into MakeGuide,
    //  which ultimately uses the CDs as arguments to Make(const cd1, const cd2).
    CDFamily* castedFamily = const_cast<CDFamily*>(family);
    CGuideAlignment_Base::SGuideInput input1(const_cast<CCdCore*>(cd1), castedFamily, kMax_UInt, 0);
    CGuideAlignment_Base::SGuideInput input2(const_cast<CCdCore*>(cd2), castedFamily, kMax_UInt, 0);
    return MakeGuideInstance(input1, input2);
}

CGuideAlignment_Base* CGuideAlignmentFactory::MakeGuide(const CCdCore* cd1, const CDFamily* family1, const CCdCore* cd2, const CDFamily* family2)
{
    //  The casts are harmless; SGuideInput is only a container to pass into MakeGuide,
    //  which ultimately uses the CDs as arguments to Make(const cd1, const cd2).
    CGuideAlignment_Base::SGuideInput input1(const_cast<CCdCore*>(cd1), const_cast<CDFamily*>(family1), kMax_UInt, 0);
    CGuideAlignment_Base::SGuideInput input2(const_cast<CCdCore*>(cd2), const_cast<CDFamily*>(family2), kMax_UInt, 0);
    return MakeGuideInstance(input1, input2);
}

//  Used only by MakeGuide, below, which we assume has made all necessary sanity checks.
CGuideAlignment_Base* CGuideAlignmentFactory::MakeGuideInstance(const CCdCore* cd1, const CCdCore* cd2, const CDFamily* family, bool cd1InFam, bool cd2InFam)
{
    CGuideAlignment_Base* guide = NULL;
    if (m_makeDegenerateGuides) {
        guide = new CDegenerateGuide();
    } else if (cd1InFam && cd2InFam) {
        guide = new CWithinFamilyGuideAlignment(family, m_overlapPercentage);
    } else if ((cd1InFam && !cd2InFam) || (cd2InFam && !cd1InFam)) {
        guide = new CGuideToFamilyMember(family, m_overlapPercentage);
    } else {  //  both false...
        guide = new CTwoCDGuideAlignment(m_overlapPercentage);
    }

    bool madeGuide;
    if (guide) {
        if (cd2InFam && !cd1InFam) {
            madeGuide = guide->Make(cd2, cd1);  //  in this case, cd2 has to appear first as it is the family member
        } else {
            madeGuide = guide->Make(cd1, cd2);
        }
        if (!madeGuide) {
            delete guide;
            guide = NULL;
        }
    }
    return guide;
}

//  Infers the proper type of guide alignment needed based on the contents of the guide inputs,
//  then uses the virtual Make method to create objects of the associated class.
CGuideAlignment_Base* CGuideAlignmentFactory::MakeGuideInstance(const CGuideAlignment_Base::SGuideInput& guideInput1, const CGuideAlignment_Base::SGuideInput& guideInput2)
{
    CGuideAlignment_Base* guide = NULL;
    CDFamilyIterator fam1End, fam2End;
    bool hasFam1 = (guideInput1.fam != NULL), hasFam2 = (guideInput2.fam != NULL);
    bool sameFam = (guideInput1.fam == guideInput2.fam);  //  includes two null families
    bool diffValidFams = (hasFam1 && hasFam2 && (guideInput1.fam != guideInput2.fam));

    if (hasFam1) fam1End = guideInput1.fam->end();
    if (hasFam2) fam2End = guideInput2.fam->end();

    bool cd1InFam1= (hasFam1 && guideInput1.fam->findCD(guideInput1.cd) != fam1End);
    bool cd1InFam2= (hasFam2 && guideInput2.fam->findCD(guideInput1.cd) != fam2End);
    bool cd2InFam1= (hasFam1 && guideInput1.fam->findCD(guideInput2.cd) != fam1End);
    bool cd2InFam2= (hasFam2 && guideInput2.fam->findCD(guideInput2.cd) != fam2End);

    //  A pre-condition for two different families, require that each CD be from the family of the corresponding input
    if (diffValidFams && (!cd1InFam1 || !cd2InFam2) ) {
//        m_errors.push_back("When different families passed, CDs must below to their corresponding family.\n");
        return guide;
    }

    
    if (sameFam) {
        guide = MakeGuideInstance(guideInput1.cd, guideInput2.cd, guideInput1.fam, cd1InFam1, cd2InFam1);
    } else {
        if (hasFam1 && !hasFam2) {
            guide = MakeGuideInstance(guideInput1.cd, guideInput2.cd, guideInput1.fam, cd1InFam1, cd2InFam1);
        } else if (hasFam2 && !hasFam1) {
            guide = MakeGuideInstance(guideInput1.cd, guideInput2.cd, guideInput2.fam, cd1InFam2, cd2InFam2);
        } else if (hasFam1 && hasFam2) {

            //  Will only get here if the pre-condition above (cd in its corresponding family) passes
            //  Until have a derived class that takes two families, try to use CGuideToFamilyMember
        
            guide = new CGuideToFamilyMember(guideInput1.fam, m_overlapPercentage);
            if (guide && !guide->Make(guideInput1.cd, guideInput2.cd)) {
                delete guide;
                //  try with other family...
                guide = new CGuideToFamilyMember(guideInput2.fam, m_overlapPercentage);
                if (guide && !guide->Make(guideInput2.cd, guideInput1.cd)) {
                    delete guide;
                    guide = NULL;
                }
            }

        }  //  no final case -- won't get to 'else' if have two null families
    }

    return guide;
}


//  Special case of the above when the family is known to be valid.
//  cd1 and cd2 are required to be members of the family, which can not be NULL.
CGuideAlignment_Base* CGuideAlignmentFactory::MakeGuideFromValidatedHierarchy(const CCdCore* cd1, const CCdCore* cd2, const CDFamily* validatedFamily)
{
    //  Special case for validated alignment (Make performs necessary sanity checks)
    CGuideAlignment_Base* ga;
    if (m_makeDegenerateGuides) 
        ga = new CDegenerateGuide();
    else 
        ga = new CValidatedHierarchyGuideAlignment(validatedFamily);

    if (!ga->Make(cd1, cd2)) {
        delete ga;
        ga = NULL;
    }

    //  It is the caller's responsibility to see if the creation succeeded by calling CGuideAlignment methods.
    return ga;
}

void CGuideAlignmentFactory::MakeAllGuidesToRoot(CDFamily* family, GuideMap& guideMap)
{
    guideMap.clear();
    if (!family) return;

    CValidatedHierarchyGuideAlignment* ga;
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
            ga = new CValidatedHierarchyGuideAlignment(family);
            if (ga) {
                ga->MakeGuideToRoot(thisCd, true);   //  cache the multiple alignment 
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
    CValidatedHierarchyGuideAlignment::ClearAlignmentCache();

    //  Put consensus masters back if present.
    RestoreConsensusMasters(*family);
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
