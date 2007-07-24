//  $Id$

#include <ncbi_pch.hpp>

#include <algo/structure/cd_utils/cd_guideUtils/cuTwoCDGuideAlignment.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuWithinFamilyGuideAlignment.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
//#include <algo/structure/cd_utils/cuBlockFormater.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


bool CombineBlockModelPairs(BlockModelPair& bmp1, bool masterIsCommon1, BlockModelPair& bmp2, bool masterIsCommon2, int overlapPercentage)
{
    bool result = false;

    if (!masterIsCommon1) bmp1.reverse();
    if (!masterIsCommon2) bmp2.reverse();

    int lastAligned1 = bmp1.getMaster().getLastAlignedPosition() + 1;
    int lastAligned2 = bmp2.getMaster().getLastAlignedPosition() + 1;
    int seqLen = (lastAligned1 > lastAligned2) ? lastAligned1 : lastAligned2;

    //  bool arguments are true as we've explicitly done the reverse already.
    BlockModelPair* intersectedPair = GuideViaBlockFormatter(bmp1, true, bmp2, true, seqLen, overlapPercentage);
    if (intersectedPair && intersectedPair->isValid() && intersectedPair->getMaster().getTotalBlockLength() > 0) {
        result = true;
        bmp1 = *intersectedPair;
    }
    delete intersectedPair;

    return result;
}

bool GetGuideBetweenRows(const CCdCore* cd, unsigned int masterRow, unsigned int slaveRow, BlockModelPair& bmp)
{
    bool result = (cd && cd->GetNumRows() > masterRow && cd->GetNumRows() > slaveRow);

    if (result) {

        //  Not assuming that the two rows have the same block model, hence I'm not
        //  simply setting the master/slave BlockModels in bmp (unless rows are equal).
        BlockModelPair bmpGuide(cd->GetSeqAlign(masterRow));  //  starts with master from row 0, slave from masterRow

        if (masterRow == slaveRow) {
            bmp.getMaster() = (masterRow == 0) ? bmpGuide.getMaster() : bmpGuide.getSlave();
            bmp.getSlave() = (masterRow == 0) ? bmpGuide.getMaster() : bmpGuide.getSlave();
        } else {
            if (slaveRow != 0) {
                BlockModelPair a(cd->GetSeqAlign(slaveRow));  //  starts with master from row 0, slave from slaveRow

                //  It's possible to fail here...
                if (masterRow != 0) {
                    result = (a.remaster(bmpGuide) > 0);  //  after this, a has masterRow as master; slaveRow as slave
                }
                bmpGuide = a;
            } else {  // slave is the CD master (master can't be); simply do a reverse
                bmpGuide.reverse();
            }
            bmp = bmpGuide;
        }
    }

    //  Probably redundant, but let's make sure.
    if (result) {
        result = (bmp.getMaster().getTotalBlockLength() > 0);
    }

    return result;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//
//    CWithinFamilyGuideAlignment
//
//////////////////////////////////////////////////////////////////////////////////////////////////
/*
void CWithinFamilyGuideAlignment::Initialize() {
    CGuideAlignment_Base::Initialize();
}

void CWithinFamilyGuideAlignment::Cleanup() {
    CGuideAlignment_Base::Cleanup();
    if (m_cleanupFamily) delete m_family;
}
*/

CGuideAlignment_Base* CWithinFamilyGuideAlignment::Copy() const {
    //  the copy will have m_cleanupFamily set to false --> only the original is responsible for cleaning up m_family!!
    CWithinFamilyGuideAlignment* guideCopy = new CWithinFamilyGuideAlignment(m_family1, m_overlapPercentage1);
    CopyBase(guideCopy);
    return guideCopy;
}

bool CWithinFamilyGuideAlignment::SanityCheck(const SGuideInput& guideInput1, const SGuideInput& guideInput2)
{
    string err;

    /*  only require fam1 == fam2, not that the m_family1 variable is used 
    if (guideInput1.fam != m_family1 || guideInput2.fam != m_family1) {
        err = "Families in both inputs should match the family used in the constructor!";
        m_errors.push_back(err);
        return false;
    }
    */

    //  Sanity check the family and CDs.
    if (guideInput1.fam == guideInput2.fam && guideInput1.fam && guideInput1.cd && guideInput2.cd) {
        ncbi::cd_utils::CDFamilyIterator familyEnd = guideInput1.fam->end();
        if (guideInput1.fam->findCD(guideInput1.cd) == familyEnd) {
            err = "CD " + guideInput1.cd->GetAccession() + " not in family.";
            m_errors.push_back(err);
        }
        if (guideInput1.fam->findCD(guideInput2.cd) == familyEnd) {
            err = "CD " + guideInput2.cd->GetAccession() + " not in family.";
            m_errors.push_back(err);
        }
        if (m_errors.size() > 0) {
            return false;
        }
    } else {
        err = "Input problem:  either using different families or invalid CD or family pointers.";
        m_errors.push_back(err);
        return false;
    }
    return true;
}

bool CWithinFamilyGuideAlignment::Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2)
{
    bool madeGuide = false;
    CCdCore* commonCd = NULL;
    CCdCore* cd1 = guideInput1.cd;
    CCdCore* cd2 = guideInput2.cd;
    ncbi::cd_utils::CDFamily* family = guideInput1.fam;
    string err;
    string overlapErrStr = " with the minimum overlap " + NStr::IntToString(m_overlapPercentage1);
    string acc1 = cd1->GetAccession(), acc2 = cd2->GetAccession(), accCommon = "No common ancestor";

    Cleanup();
    if (!SanityCheck(guideInput1, guideInput2)) {
        m_isOK = false;
        return false;
    }

    vector<CCdCore*> path1, path2;
    ncbi::cd_utils::CDFamilyIterator commonCdIt = family->convergeTo(cd1, cd2, path1, path2);
    if (commonCdIt == family->end()) {
        m_errors.push_back("No common ancestor for CDs " + cd1->GetAccession() + " and " + cd2->GetAccession());
        m_isOK = false;
        return false;
    }

    madeGuide = true;
    commonCd = commonCdIt->cd;
    if (commonCd) accCommon = commonCd->GetAccession();

//    cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::Make:\nbetween CD1 %s and CD2 %s, common CD %s\n", acc1.c_str(), acc2.c_str(), accCommon.c_str());

    BlockModelPair path1Guide;   //  master is the front of path1; slave is the back of path1 (i.e., common CD)
    BlockModelPair path2Guide;   //  master is the front of path2; slave is the back of path2 (i.e., common CD)
    if (!GetGuideSpanningPath(path1, path1Guide, m_chain1)) {
        m_errors.push_back("Could not find a partial guide between CD " + acc1 + " and ancestral CD " + accCommon + overlapErrStr);
        madeGuide = false;
    }
    if (!GetGuideSpanningPath(path2, path2Guide, m_chain2)) {
        m_errors.push_back("Could not find a partial guide between CD " + acc2 + " and ancestral CD " + accCommon + overlapErrStr);
        madeGuide = false;
    }

    if (madeGuide) {
//        cdLog::ErrorPrintf("Before final combine path1Guide\n%s\n%s\nChain1\n%s", path1Guide.getMaster().toString().c_str(), path1Guide.getSlave().toString().c_str(), GuideChainToString(m_chain1).c_str());
//        cdLog::ErrorPrintf("Before final combine path2Guide\n%s\n%s\nChain2\n%s", path2Guide.getMaster().toString().c_str(), path2Guide.getSlave().toString().c_str(), GuideChainToString(m_chain2).c_str());

        madeGuide = CombineBlockModelPairs(path1Guide, false, path2Guide, false, m_overlapPercentage1);
        if (madeGuide) {
            m_guideBlockModelPair = path1Guide;
//            cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::Make:\nAble to create guide for CD1 %s and CD2 %s\n", acc1.c_str(), acc2.c_str());
        } else {
            m_errors.push_back("Could not combine partial guides to make guide between " + acc1 + " and " + acc2 + overlapErrStr);
//            cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::Make:\nUNABLE to create guide for CD1 %s and CD2 %s\n", acc1.c_str(), acc2.c_str());
        }
//        cdLog::ErrorPrintf("After final combine path1Guide\n%s\n%s", path1Guide.getMaster().toString().c_str(), path1Guide.getSlave().toString().c_str());
    }

    m_isOK = madeGuide;
    return madeGuide;
}


bool CWithinFamilyGuideAlignment::Make(const CCdCore* cd1, const CCdCore* cd2)
{
    string err;
    if (!InSameFamily(cd1, cd2, m_family1)) {
        m_isOK = false;
        if (cd1 && cd2 && m_family1) {
            err = "CWithinFamilyGuideAlignment:  guide not made.\nCDs " + cd1->GetAccession() + " and " + cd2->GetAccession() + " are not from the same family.";
        } else {
            err = "CWithinFamilyGuideAlignment:  guide not made.\nCD or family pointer is undefined.\n";
        }
        m_errors.push_back(err);
        return false;
    }

    //  First, try and make the guide w/o reference to the family.
    CTwoCDGuideAlignment simpleGuide(m_overlapPercentage1);
    if (simpleGuide.Make(cd1, cd2)) {
//        m_isOK = true;
//        m_guideBlockModelPair = simpleGuide.GetGuide();
//        m_chain1 = simpleGuide.GetMasterChain();
//        m_chain2 = simpleGuide.GetSlaveChain();
        CGuideAlignment_Base::CopyBase(&simpleGuide, this);
        return true;
    } else if (!m_family1) {
        return false;
    }

    SGuideInput input1(const_cast<CCdCore*>(cd1), const_cast<ncbi::cd_utils::CDFamily*>(m_family1), kMax_UInt, 0);
    SGuideInput input2(const_cast<CCdCore*>(cd2), const_cast<ncbi::cd_utils::CDFamily*>(m_family1), kMax_UInt, 0);

    bool result = Make(input1, input2);
    return result;
}


bool CWithinFamilyGuideAlignment::GetGuideSpanningPath(vector<CCdCore*>& path, BlockModelPair& bmpGuide, TGuideChain& pathChain)
{
    bool result = false;
    bool madeGuide = true;
    bool doCombine = true;
    unsigned int i, middle;
    unsigned int row1, row2;
    unsigned int pathLength = path.size();
    CCdCore* cd1;
    CCdCore* cd2;
    TGuideChain localChain1, localChain2;

    if (pathLength == 0) {
        return false;
    //  dummy alignment of master to itself
    } else if (pathLength == 1) {
        cd1 = path.front();
        if (cd1) {
            const CRef< CSeq_align>& sa1 = cd1->GetSeqAlign(0);
            BlockModelPair bmp(sa1);
            bmpGuide.getMaster() = bmp.getMaster();
            bmpGuide.getSlave() = bmp.getMaster();
//            pathChain.insert(pathChain.begin(), SGuideAlignmentLink(cd1, 0));
            pathChain.push_back(SGuideAlignmentLink(cd1, 0));
//            cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::GetGuideSpanningPath:\n    single CD guide for %s\n",cd1->GetAccession().c_str());
        }
        return (cd1 != NULL);
    } else {
        cd1 = path.front();
        cd2 = path.back();
        if (!cd1 || !cd2) {
            return false;
        }
//        cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::GetGuideSpanningPath:\n    path spanning CD1 %s and CD2 %s\n", cd1->GetAccession().c_str(), cd2->GetAccession().c_str());
    }

    //  in recursive steps, the path with be split into path1 and path2
    vector< CCdCore* > path1, path2;

    //  First, try and make a guide with the front and back CDs...
    SGuideInput input1(cd1, NULL, kMax_UInt, 0);
    SGuideInput input2(cd2, NULL, kMax_UInt, 0);
    CTwoCDGuideAlignment simpleGuide(m_overlapPercentage1);
    if (simpleGuide.Make(input1, input2)) {
        bmpGuide = simpleGuide.GetGuide();

        //  Fix up the chains for the entire guide.
        //  There should always be one entry in each local chain, but use more general code just in case of weirdness.
        localChain1 = simpleGuide.GetMasterChain();
        localChain2 = simpleGuide.GetSlaveChain();
        pathChain.insert(pathChain.begin(), localChain1.begin(), localChain1.end());
        for (i = 0; i < localChain2.size(); ++i) {
            pathChain.push_back(localChain2[i]);
        }

        result = true;
//        cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::GetGuideSpanningPath:\n    spanned path between CD1 %s and CD2 %s\n", cd1->GetAccession().c_str(), cd2->GetAccession().c_str());

    //  If there are two CDs and Make failed above, then splitting this won't work as we
    //  will not be able to recombine.
    } else if (pathLength > 2) {

        //  Split path in two; the middle CD is shared so that we're guaranteed to
        //  have a common CD if both paths are successfuly spanned.
        middle = pathLength/2;
        for (i = 0; i < pathLength; ++i) {
            if (i <= middle)
                path1.push_back(path[i]);
            if (i >= middle)
                path2.push_back(path[i]);
        }

        //  Find guide for each half
        BlockModelPair guide1, guide2;
        if (!GetGuideSpanningPath(path1, guide1, localChain1))  {
            madeGuide = false;
        }
        if (!GetGuideSpanningPath(path2, guide2, localChain2))  {
            madeGuide = false;
        }

        //  Combine the halves if possible
        //  pathXGuide:  master is master of pathX.front; slave is master of pathX.back (i.e., pathX.front's ancestor)
        //  by construction, path1.back == path2.front (although the rows used could be different)
        if (madeGuide) {

            if (localChain1.size() > 0 && localChain2.size() > 0) {
                //  Fix up the pathChain.
                pathChain.insert(pathChain.begin(), localChain1.begin(), localChain1.end());
                for (i = 0; i < localChain2.size(); ++i) {
                    pathChain.push_back(localChain2[i]);
                }

//                cdLog::ErrorPrintf("Before combine in GetGuideBetweenRows guide1\n%s\n%s\n\nBefore combine in GetGuideSpanningPath guide2\n%s\n%s\n", guide1.getMaster().toString().c_str(), guide1.getSlave().toString().c_str(), guide2.getMaster().toString().c_str(), guide2.getSlave().toString().c_str());

                //  If the rows for the common CD aren't the same,
                //  remaster guide2 to have the same master as guide1 so the combination can proceed.
                row1 = localChain1.back().row;
                row2 = localChain2.front().row;
                if (localChain1.back().cd == localChain2.front().cd && row1 != row2) {
                    BlockModelPair bmpGuide;  //  (needs row2 as master; row1 as slave)
                    doCombine = (GetGuideBetweenRows(localChain1.back().cd, row2, row1, bmpGuide) && guide2.remaster(bmpGuide) > 0);
                } else if (localChain1.back().cd != localChain2.front().cd) {
                    doCombine = false;
                }

//                cdLog::ErrorPrintf("Before combine in GetGuideSpanningPath guide1\n%s\n%s\n\nBefore combine in GetGuideSpanningPath guide2\n%s\n%s\n", guide1.getMaster().toString().c_str(), guide1.getSlave().toString().c_str(), guide2.getMaster().toString().c_str(), guide2.getSlave().toString().c_str());

                if (doCombine && CombineBlockModelPairs(guide1, false, guide2, true, m_overlapPercentage1)) {
                    bmpGuide = guide1;
                    result = true;
//                    cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::GetGuideSpanningPath:\n    spanned path between CD1 %s and CD2 %s by splitting\n", cd1->GetAccession().c_str(), cd2->GetAccession().c_str());
//                } else {
//                    cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::GetGuideSpanningPath:\n    could not combine BMPs spanning path between CD1 %s and CD2 %s (doCombine = %d)\n", cd1->GetAccession().c_str(), cd2->GetAccession().c_str(), doCombine);
                }

//            } else {
//                cdLog::ErrorPrintf("CWithinFamilyGuideAlignment::GetGuideSpanningPath:\n    one of the local chains is empty (%d, %d) in spanning path between CD1 %s and CD2 %s\n", localChain1.size(), localChain2.size(), cd1->GetAccession().c_str(), cd2->GetAccession().c_str());
            }
    }

    }
    return result;
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
