//  $Id$


#include <ncbi_pch.hpp>
#include <algorithm>

#include <algo/structure/cd_utils/cd_guideUtils/cuTwoCDGuideAlignment.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuBlockFormater.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

void CTwoCDGuideAlignment::Initialize() {
    CGuideAlignment_Base::Initialize();
}

void CTwoCDGuideAlignment::Cleanup() {
    CGuideAlignment_Base::Cleanup();
}

CGuideAlignment_Base* CTwoCDGuideAlignment::Copy() const {
    CTwoCDGuideAlignment* guideCopy = new CTwoCDGuideAlignment(m_overlapPercentage);
    CopyBase(guideCopy);
    return guideCopy;
}

//  In the returned BMP, the slave of bmp1 is the master and the slave of bmp2 is the slave.
BlockModelPair* GuideViaBlockFormatter(BlockModelPair& bmp1, bool masterIsCommon1, BlockModelPair& bmp2, bool masterIsCommon2, int seqLen, int overlapPercentage)
{
    BlockModelPair* guideBMP = NULL;
    CRef< CSeq_align > sa1Formatted(new CSeq_align), sa2Formatted(new CSeq_align);

    //  Put the common sequence as the master of each BMP.
    if (!masterIsCommon1) bmp1.reverse();
    if (!masterIsCommon2) bmp2.reverse();

    sa1Formatted = bmp1.toSeqAlign();
    sa2Formatted = bmp2.toSeqAlign();

    vector< CRef< CSeq_align > > seqAlignVec;
    seqAlignVec.push_back(sa1Formatted);
    seqAlignVec.push_back(sa2Formatted);

    cd_utils::BlockFormater bf(seqAlignVec, seqLen);

    CNcbiOstrstream oss;
    list< CRef< CSeq_align > > seqAlignList;
    list< CRef< CSeq_align > >::iterator saIt;

    //  Do not want the intersection to have blocks that span existing block boundaries,
    //  as rows not considered here may have insertions not present in these seq-aligns.
    set<int> forcedCTerminiInIntersection;
    unsigned int j, nBlocks = bmp1.getMaster().getBlocks().size();
    for (j = 0; j < nBlocks - 1; ++j) {  //  '-1' as I don't care about end of the C-terminal block
        forcedCTerminiInIntersection.insert(bmp1.getMaster().getBlock(j).getEnd());
    }

    nBlocks = bmp2.getMaster().getBlocks().size();
    for (j = 0; j < nBlocks - 1; ++j) {  //  '-1' as I don't care about end of the C-terminal block
        forcedCTerminiInIntersection.insert(bmp2.getMaster().getBlock(j).getEnd());
    }

    int numGood = bf.findIntersectingBlocks(overlapPercentage);
    bf.formatBlocksForQualifiedRows(seqAlignList, &forcedCTerminiInIntersection);

    //  Make the guide alignment, linking the *non-common* sequence in each original Seq-align.
    if (numGood == 2 && seqAlignList.size() == 2) {

        guideBMP = new BlockModelPair();

        saIt = seqAlignList.begin();
        BlockModel bm1(*saIt, true);
        ++saIt;
        BlockModel bm2(*saIt, true);

        guideBMP->getMaster() = bm1;
        guideBMP->getSlave() = bm2;
    } else {
        oss << "number of qualified rows is off:  " << numGood << "; seqAlignList.size() = " << seqAlignList.size() << NcbiEndl;
    }


    oss << "bmp1 common sequence\n" << bmp1.getMaster().toString() << NcbiEndl;
    oss << "bmp1 other  sequence\n" << bmp1.getSlave().toString() << NcbiEndl;
    oss << "bmp2 common sequence\n" << bmp2.getMaster().toString() << NcbiEndl;
    oss << "bmp2 other  sequence\n" << bmp2.getSlave().toString() << NcbiEndl;
    if (guideBMP) {
        oss << "intersection master\n" << guideBMP->getMaster().toString() << NcbiEndl;
        oss << "intersection slave\n" << guideBMP->getSlave().toString() << NcbiEndl;
    } else {
        oss << "no intersection\n";
    }

    string msg = CNcbiOstrstreamToString(oss);
//    cdLog::ErrorPrintf("GuideViaBlockFormatter results:\n%s\n", msg.c_str());

    return guideBMP;
}

BlockModelPair* GuideViaBlockFormatter(const CRef< CSeq_align>& sa1, bool masterIsCommon1, const CRef< CSeq_align>& sa2, bool masterIsCommon2, int seqLen, int overlapPercentage)
{
    BlockModelPair bmp1(sa1), bmp2(sa2);
    if (seqLen <= 0 || overlapPercentage <= 0 || overlapPercentage > 100)
        return NULL;

    return GuideViaBlockFormatter(bmp1, masterIsCommon1, bmp2, masterIsCommon2, seqLen, overlapPercentage);
}


bool CTwoCDGuideAlignment::SetOverlapPercentage(int percentage)
{
    bool result = (percentage > 0 && percentage <= 100);
    if  (result)
        m_overlapPercentage = percentage;
    return result;
}

bool CTwoCDGuideAlignment::Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2)
{
    bool madeGuide = false;
    int row1, row2;

    //  Find common, overlapping rows between the two CDs.
    vector<int> overlappedRows1, overlappedRows2;
    unsigned int i, nOverlaps = 0;
    int seqLen;

    if (guideInput1.rowInCd == kMax_UInt && guideInput2.rowInCd == kMax_UInt) {
        nOverlaps = (int) GetOverlappedRows(guideInput1.cd, guideInput2.cd, overlappedRows1, overlappedRows2);
        if (nOverlaps == 0) {
            m_errors.push_back("No overlapping rows between " + guideInput1.cd->GetAccession() + " and " + guideInput2.cd->GetAccession());
        }
    } else if (guideInput1.rowInCd != kMax_UInt && guideInput2.rowInCd != kMax_UInt) {
        nOverlaps = 1;
        overlappedRows1.push_back(guideInput1.rowInCd);
        overlappedRows2.push_back(guideInput2.rowInCd);
    }

    for (i = 0; i < nOverlaps && !madeGuide; ++i) {

        m_guideBlockModelPair.reset();

        row1 = overlappedRows1[i];
        row2 = overlappedRows2[i];
        const CRef< CSeq_align>& sa1 = guideInput1.cd->GetSeqAlign(row1);
        const CRef< CSeq_align>& sa2 = guideInput2.cd->GetSeqAlign(row2);
        CRef< CSeq_align > saCopy1 = sa1;
        CRef< CSeq_align > saCopy2 = sa2;
        BlockModelPair bmp1(sa1);
        BlockModelPair bmp2(sa2);

        //  Want the bmp slave to always be the common row; the bmp master always the CD master.
        //  So if the common row is the CD master row == 0,  duplicate it in the bmp.
        if (row1 == 0) {
            bmp1.getSlave() = bmp1.getMaster();
            saCopy1 = bmp1.toSeqAlign();
        }
        if (row2 == 0) {
            bmp2.getSlave() = bmp2.getMaster();
            saCopy2 = bmp2.toSeqAlign();
        }

        seqLen = guideInput1.cd->GetSequenceStringByRow(row1).size();

        BlockModelPair* intersectedPair2 = GuideViaBlockFormatter(saCopy1, false, saCopy2, false, seqLen, m_overlapPercentage);
        if (intersectedPair2 && intersectedPair2->isValid() && intersectedPair2->getMaster().getTotalBlockLength() > 0) {
            m_guideBlockModelPair = *intersectedPair2;
            delete intersectedPair2;
            madeGuide = true;

            m_chain1.push_back(SGuideAlignmentLink(guideInput1.cd, row1));
            m_chain2.push_back(SGuideAlignmentLink(guideInput2.cd, row2));
//            cdLog::ErrorPrintf("After viaBlockFormatter\n%s\n", ToString().c_str());
        }

    }

    if (nOverlaps > 0 && !madeGuide) {
        string s = "None of the overlapping rows " + NStr::UIntToString(nOverlaps) + " between " + guideInput1.cd->GetAccession() + " and " + guideInput2.cd->GetAccession();
        s += "\nexceeded the minimum overlap percentage = " + NStr::IntToString(m_overlapPercentage);
        m_errors.push_back(s);
    }

    m_isOK = madeGuide;
    return madeGuide;
}

bool CTwoCDGuideAlignment::Make(const CCdCore* cd1, const CCdCore* cd2)
{
    SGuideInput input1(const_cast<CCdCore*>(cd1), NULL, kMax_UInt, 0);
    SGuideInput input2(const_cast<CCdCore*>(cd2), NULL, kMax_UInt, 0);
    return Make(input1, input2);
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

