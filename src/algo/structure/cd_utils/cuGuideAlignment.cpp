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
 *          Base class for representing the guide alignment between two CDs.
 *          Default implementation assumes both are from a single CDFamily
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cuGuideAlignment.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

map<CDFamily*, MultipleAlignment*> CMasterGuideAlignment::m_alignmentCache;

void CGuideAlignment_Base::Initialize()
{
    m_isOK = false;
    m_errors.clear();

    Cleanup();
}

void CGuideAlignment_Base::Cleanup()
{
    m_chain1.clear();
    m_chain2.clear();
    m_families.clear();  //  class does not own CDFamily objects
    if (m_blockModels.size() > 0) {
        for (unsigned int i = 0; i < m_blockModels.size(); ++i) {
            delete m_blockModels[i];
        }
        m_blockModels.clear();
    }

    m_guideBlockModelPair.reset();
}

string CGuideAlignment_Base::GetErrors() const
{
    static const string slashN("\n");

    string allErrors;
    for (unsigned int i = 0; i < m_errors.size(); ++i) {
        allErrors += m_errors[i] + slashN;
    }
    return allErrors;
}


bool CGuideAlignment_Base::Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family)
{
    bool result = (family && cd1 && cd2);

    //  Remove any previous block model pair, so that in case of errors IsOK returns false.
    Reset();

    if (!result) {
        if (!family) {
            m_errors.push_back("CGuideAlignment error:  Null family - can't make guide alignment.\n");
        }
        if (!cd1) {
            m_errors.push_back("CGuideAlignment error:  First CD is null - can't make guide alignment.\n");
        }
        if (!cd2) {
            m_errors.push_back("CGuideAlignment error:  Second CD is null - can't make guide alignment.\n");
        }
        return false;
    }
    
    SGuideInput input1(const_cast<CCdCore*>(cd1), family);
    SGuideInput input2(const_cast<CCdCore*>(cd2), family);
    return Make(input1, input2);
}

bool CGuideAlignment_Base::Make(const CCdCore* cd1, CDFamily* family1, const CCdCore* cd2, CDFamily* family2)
{
    bool result = (family1 && family2 && cd1 && cd2);

    //  Remove any previous block model pair, so that in case of errors IsOK returns false.
    Reset();

    if (!result) {
        if (!family1) {
            m_errors.push_back("CGuideAlignment error:  Null first family - can't make guide alignment.\n");
        }
        if (!family2) {
            m_errors.push_back("CGuideAlignment error:  Null second family - can't make guide alignment.\n");
        }
        if (!cd1) {
            m_errors.push_back("CGuideAlignment error:  First CD is null - can't make guide alignment.\n");
        }
        if (!cd2) {
            m_errors.push_back("CGuideAlignment error:  Second CD is null - can't make guide alignment.\n");
        }
        return false;
    }
    
    SGuideInput input1(const_cast<CCdCore*>(cd1), family1);
    SGuideInput input2(const_cast<CCdCore*>(cd2), family2);
    result = Make(input1, input2);

    MakeChains(input1, input2, NULL);

    return result;
}


void CGuideAlignment_Base::MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd)
{

    m_chain1.clear();
    m_chain1.push_back(SGuideAlignmentLink(guideInput1.cd, guideInput1.rowInCd));
    if (commonCd) {
        m_chain1.push_back(SGuideAlignmentLink(commonCd));
    }

    m_chain2.clear();
    m_chain2.push_back(SGuideAlignmentLink(guideInput2.cd, guideInput2.rowInCd));
    if (commonCd) {
        m_chain2.push_back(SGuideAlignmentLink(commonCd));
    }
}

bool CGuideAlignment_Base::ReplaceMaster(const CRef< CSeq_align >& seqAlign, bool useFirst)
{
    return Replace(true, seqAlign, useFirst);
}

bool CGuideAlignment_Base::ReplaceSlave(const CRef< CSeq_align >& seqAlign, bool useFirst)
{
    return Replace(false, seqAlign, useFirst);
}

bool CGuideAlignment_Base::Replace(bool replaceMaster, const CRef< CSeq_align >& seqAlign, bool useFirst)
{
    bool result = seqAlign.NotEmpty() && m_guideBlockModelPair.isValid();
    if (result) {
        BlockModelPair guideCopy(m_guideBlockModelPair);
        BlockModelPair bmp(seqAlign);
        if (useFirst) bmp.reverse();  //  the row used from seqAlign needs to be the slave in the bmp

        if (!replaceMaster) guideCopy.reverse();  //  for slave, need to reverse to make it the master
        guideCopy.remaster(bmp);
        if (!replaceMaster) guideCopy.reverse();  //  put the slave back

        CRef< CSeq_align > tmpSeqAlign = guideCopy.toSeqAlign();
        if (tmpSeqAlign.NotEmpty()) {
            m_guideBlockModelPair = guideCopy;
        } else {
            result = false;
        }
    }
    return result;
}


int CGuideAlignment_Base::MapToMaster(unsigned int slavePos) const
{
    return m_guideBlockModelPair.mapToMaster((int) slavePos);
}

int CGuideAlignment_Base::MapToSlave(unsigned int masterPos) const
{
    m_guideBlockModelPair.reverse();
    int result = m_guideBlockModelPair.mapToMaster((int) masterPos);
    m_guideBlockModelPair.reverse();

    return result;
}

const CCdCore* CGuideAlignment_Base::GetMasterCD() const
{
    return (m_chain1.size() > 0) ? m_chain1[0].cd : NULL;
}

string CGuideAlignment_Base::GetMasterCDAcc() const 
{
    const CCdCore* cd = (m_chain1.size() > 0) ? m_chain1[0].cd : NULL;
    return (cd) ? cd->GetAccession() : "";
}

string CGuideAlignment_Base::GetMasterRowIdStr() const
{
    CRef< CSeq_id > seqId;
    string idStr = kEmptyStr;
    if (m_chain1.size() > 0 && m_chain1[0].cd->GetSeqIDFromAlignment(m_chain1[0].row, seqId)) {
        idStr = GetSeqIDStr(seqId);
    }
    return idStr;
}

unsigned int CGuideAlignment_Base::GetMasterCDRow() const
{
    return (m_chain1.size() > 0) ? m_chain1[0].row : kMax_UInt;
}

const CCdCore* CGuideAlignment_Base::GetSlaveCD() const
{
    return (m_chain2.size() > 0) ? m_chain2[0].cd : NULL;
}
string CGuideAlignment_Base::GetSlaveCDAcc() const
{
    const CCdCore* cd = (m_chain2.size() > 0) ? m_chain2[0].cd : NULL;
    return (cd) ? cd->GetAccession() : "";
}

string CGuideAlignment_Base::GetSlaveRowIdStr() const
{
    CRef< CSeq_id > seqId;
    string idStr = kEmptyStr;
    if (m_chain2.size() > 0 && m_chain2[0].cd->GetSeqIDFromAlignment(m_chain2[0].row, seqId)) {
        idStr = GetSeqIDStr(seqId);
    }
    return idStr;
}

unsigned int CGuideAlignment_Base::GetSlaveCDRow() const
{
    return (m_chain2.size() > 0) ? m_chain2[0].row : kMax_UInt;
}

const CCdCore* CGuideAlignment_Base::GetCommonCD() const
{
    unsigned int n = m_chain1.size();
    return (n > 0) ? m_chain1[n-1].cd : NULL;
}
string CGuideAlignment_Base::GetCommonCDAcc() const
{
    unsigned int n = m_chain1.size();
    const CCdCore* cd = (n > 0) ? m_chain1[n-1].cd : NULL;
    return (cd) ? cd->GetAccession() : "";
}
string CGuideAlignment_Base::GetCommonRowIdStr() const
{
    CRef< CSeq_id > seqId;
    unsigned int n = m_chain1.size();
    string idStr = kEmptyStr;
    if (n > 0 && m_chain1[n-1].cd->GetSeqIDFromAlignment(m_chain1[n-1].row, seqId)) {
        idStr = GetSeqIDStr(seqId);
    }
    return idStr;
}

unsigned int CGuideAlignment_Base::GetCommonCDRow() const
{
    unsigned int n = m_chain1.size();
    return (n > 0) ? m_chain1[n-1].row : kMax_UInt;
}

string CGuideAlignment_Base::ToString() const
{
    string s;
    string masterInfo = "Master CD " + GetMasterCDAcc() + "; row " + NStr::UIntToString(GetMasterCDRow()) + " (" + GetMasterRowIdStr() + ")";
    string slaveInfo  = "Slave  CD " + GetSlaveCDAcc() + "; row " + NStr::UIntToString(GetSlaveCDRow()) + " (" + GetSlaveRowIdStr() + ")";
    string commonInfo = "Common CD " + GetCommonCDAcc() + "; row " + NStr::UIntToString(GetCommonCDRow()) + " (" + GetCommonRowIdStr() + ")";

    s = masterInfo + "\n" + slaveInfo + "\n" + commonInfo + "\n";
    if (m_isOK) {
        const BlockModelPair& bmp = GetGuide();
        s += "master blocks:\n" + bmp.getMaster().toString();
        s += "slave  blocks:\n" + bmp.getSlave().toString();
    } else {
        s += "    --> guide alignment not OK!\n";
    }
    return s;
}


// ========================================
//      CMasterGuideAlignment
// ========================================


void CMasterGuideAlignment::ClearAlignmentCache()
{
    map<CDFamily*, MultipleAlignment*>::iterator cacheIt = m_alignmentCache.begin(), cacheEnd = m_alignmentCache.end();
    for (; cacheIt != cacheEnd; ++cacheIt) {
        delete cacheIt->second;
    }
    m_alignmentCache.clear();
}

bool CMasterGuideAlignment::Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family)
{
    bool result = (family && cd1 && cd2);

    //  Remove any previous block model pair, so that in case of errors IsOK returns false.
    Reset();

    if (!result) {
        if (!family) {
            m_errors.push_back("CMasterGuideAlignment error:  Null family - can't make guide alignment.\n");
        }
        if (!cd1) {
            m_errors.push_back("CMasterGuideAlignment error:  First CD is null - can't make guide alignment.\n");
        }
        if (!cd2) {
            m_errors.push_back("CMasterGuideAlignment error:  Second CD is null - can't make guide alignment.\n");
        }
        return false;
    }

    MultipleAlignment maAll;
    CDFamilyIterator fit = family->convergeTo(const_cast<CCdCore*>(cd1), const_cast<CCdCore*>(cd2));
    if (fit == family->end()) {
        m_errors.push_back("CMasterGuideAlignment error:  Common ancestor not found for CDs.\n");
        result = false;
    }

    if (result) {
        maAll.setAlignment(*family, fit);
        result = Make(cd1, cd2, &maAll);
        if (result) {
            MakeChains(cd1, cd2, fit->cd, family);
        }
    }
    return result;
}

bool CMasterGuideAlignment::MakeGuideToRoot(const CCdCore* cd, CDFamily* family, bool cache)
{
    bool result = false;
    MultipleAlignment* ma;
    map<CDFamily*, MultipleAlignment*>::iterator cacheIt;
    CCdCore* root = (family) ? family->getRootCD() : NULL;
    if (!root) {
        Reset();
        m_errors.push_back("CMasterGuideAlignment error:  Could not get root CD for family.\n");
    } else if (!cache) {
        result = Make(root, cd, family);
    } else {
        //  Use cached alignment for family if available.  Otherwise, create and cache it.
        cacheIt = m_alignmentCache.find(family);
        if ( cacheIt != m_alignmentCache.end()) {
            ma = cacheIt->second;
        } else {
            ma = new MultipleAlignment();
            if (ma) {
                CDFamilyIterator rootIt = family->begin();
//                LOG_POST("about to call ma->setAlignment\n");
                ma->setAlignment(*family, rootIt);  //  first iterator is to the root
//                LOG_POST("returned from ma->setAlignment\n");
                m_alignmentCache[family] = ma;
            }
        }

        Reset();
//        LOG_POST("about to call Make(root, cd, ma)\n");
        result = Make(root, cd, ma);
        if (result) {
            MakeChains(root, cd, root, family);
        }

    }
    return result;
}


bool CMasterGuideAlignment::Make(const CCdCore* cd1, const CCdCore* cd2, MultipleAlignment* ma)
{
    int master1, master2;
    string master1Str, master2Str;

    m_isOK = (cd1 && cd2 && ma);
    if (m_isOK) {
        master1 = ma->GetRowSourceTable().convertFromCDRow(const_cast<CCdCore*>(cd1), 0);
        master2 = ma->GetRowSourceTable().convertFromCDRow(const_cast<CCdCore*>(cd2), 0);
        cd1->Get_GI_or_PDB_String_FromAlignment(0, master1Str, false, 0);
        cd2->Get_GI_or_PDB_String_FromAlignment(0, master2Str, false, 0);

        LOG_POST("cd1 " << cd1->GetAccession() << " master " << master1 << "; " << master1Str);
        LOG_POST("cd2 " << cd2->GetAccession() << " master " << master2 << "; " << master2Str);

        if (master1 >= 0 && master2 >= 0) {
            m_guideBlockModelPair.getMaster() = ma->getBlockModel(master1);
            m_guideBlockModelPair.getSlave()  = ma->getBlockModel(master2);
            m_isOK = m_guideBlockModelPair.isValid();
        } else {
            string err;
            if (master1 < 0) {
                err = "CMasterGuideAlignment error:  the master of CD1 (" + cd1->GetAccession() + ") is either not present in its parents or is misaligned with respect to its parents\n";
            }
            if (master2 < 0) {
                err += "CMasterGuideAlignment error:  the master of CD2 (" + cd2->GetAccession() + ") is either not present in its parents or is misaligned with respect to its parents\n";
            }
            m_errors.push_back(err);
            m_isOK = false;
        }
    }

    return m_isOK;
}


bool CMasterGuideAlignment::Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2)
{
    bool result = (guideInput1.fam == guideInput2.fam);
    if (result) {
        result = Make(guideInput1.cd, guideInput2.cd, guideInput1.fam);
    } else {
        Reset();
        m_errors.push_back("CMasterGuideAlignment error:  different families passed to Make.\n");
    }
    return result;
}

void CMasterGuideAlignment::MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd)
{
    _ASSERT(guideInput1.fam == guideInput2.fam);
    MakeChains(guideInput1.cd, guideInput2.cd, commonCd, guideInput1.fam);
}

void CMasterGuideAlignment::MakeChains(const CCdCore* cd1, const CCdCore* cd2, const CCdCore* commonCd, CDFamily* family)
{
    vector<CCdCore*> path1, path2;
    int pathLen1 = family->getPathToRoot(const_cast<CCdCore*>(cd1), path1);
    int pathLen2 = family->getPathToRoot(const_cast<CCdCore*>(cd2), path2);

    for (int i = 0; i < pathLen1; ++i) {
        m_chain1.push_back(SGuideAlignmentLink(path1[i]));
        if (path1[i] == commonCd) {
            break;
        }
    }

    for (int i = 0; i < pathLen2; ++i) {
        m_chain2.push_back(SGuideAlignmentLink(path2[i]));
        if (path2[i] == commonCd) {
            break;
        }
    }

}

// ========================================
//      CGuideAlignmentFactory
// ========================================

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

    LOG_POST("RemapGuideToConsensus:  before:\n" << gaToMap.ToString());
    LOG_POST("\nOriginal master:  consensus:\n" << bmpMaster.getMaster().toString());
    LOG_POST("\nOriginal master:      row 1:\n" << bmpMaster.getSlave().toString());
    LOG_POST("\nOriginal slave:   consensus:\n" << bmpSlave.getMaster().toString());
    LOG_POST("\nOriginal slave:       row 1:\n" << bmpSlave.getSlave().toString());


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

    LOG_POST("RemapGuideToConsensus:  after (result = " << result << "):\n" << gaToMap.ToString());

    return result;
} 

void CGuideAlignmentFactory::MakeAllGuidesToRoot(CDFamily* family, GuideMap& guideMap)
{
    guideMap.clear();
    if (!family) return;

    CMasterGuideAlignment* ga;
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
            ga = new CMasterGuideAlignment();
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
    CMasterGuideAlignment::ClearAlignmentCache();

    //  Put consensus masters back if present.
    RestoreConsensusMasters(*family);


}

//  Infers the proper type of guide alignment needed based on the contents of the guide inputs,
//  then uses the virtual Make method to create objects of the associated class.
CGuideAlignment_Base* CGuideAlignmentFactory::MakeGuide(const CGuideAlignment_Base::SGuideInput& guideInput1, const CGuideAlignment_Base::SGuideInput& guideInput2)
{
    //  Simple case when need a classical alignment (only type supported right now...)
    CGuideAlignment_Base* ga = new CMasterGuideAlignment();
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
