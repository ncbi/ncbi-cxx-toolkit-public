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
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cuGuideAlignment.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

// ============================================
//      CGuideAlignment_Base implementation
// ============================================

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


END_SCOPE(cd_utils)
END_NCBI_SCOPE
