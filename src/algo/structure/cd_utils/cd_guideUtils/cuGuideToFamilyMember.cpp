//  $Id$

#include <ncbi_pch.hpp>

#include <algo/structure/cd_utils/cd_guideUtils/cuGuideToFamilyMember.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuTwoCDGuideAlignment.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuWithinFamilyGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


//////////////////////////////////////////////////////////////////////////////////////////////////
//
//    CGuideToFamilyMember
//
//////////////////////////////////////////////////////////////////////////////////////////////////
/*
void CGuideToFamilyMember::Initialize() {
    CGuideAlignment_Base::Initialize();
}

void CGuideToFamilyMember::Cleanup() {
    CGuideAlignment_Base::Cleanup();
    if (m_cleanupFamily) delete m_family;
}
*/

CGuideAlignment_Base* CGuideToFamilyMember::Copy() const {
    //  the copy will have m_cleanupFamily set to false --> only the original is responsible for cleaning up m_family!!
    CGuideToFamilyMember* guideCopy = new CGuideToFamilyMember(m_family1, m_overlapPercentage1);
    CopyBase(guideCopy);
    return guideCopy;
}

bool CGuideToFamilyMember::SanityCheck(const SGuideInput& familyMemberInput)
{
    bool result = true;
    string err;

    //  Check that the family contains the CD.
    if (familyMemberInput.fam && familyMemberInput.cd) {
        if (familyMemberInput.fam->findCD(familyMemberInput.cd) == familyMemberInput.fam->end()) {
            err = "CGuideToFamilyMember:  CD " + familyMemberInput.cd->GetAccession() + " not in family.";
            m_errors.push_back(err);
            result = false;
        }
    } else {
        err = "CGuideToFamilyMember:  invalid CD and/or family pointer.";
        m_errors.push_back(err);
        result = false;
    }
    return result;
}


bool CGuideToFamilyMember::MakeSameFamily(const SGuideInput& familyMemberInput, const SGuideInput& otherCdInput)
{
    //  If this is being called, already did all of the sanity checks.
    //  Since both CDs refer to the same family, don't need to set members related to the 2nd family in 'guide'.
    CWithinFamilyGuideAlignment guide(familyMemberInput.fam, m_overlapPercentage1);
    SGuideInput modOtherCdInput(otherCdInput.cd, familyMemberInput.fam);
    bool result = guide.Make(familyMemberInput.cd, modOtherCdInput.cd);
    if (result) {
//        guide.CopyBase(this);
        CGuideAlignment_Base::CopyBase(&guide, this);
    }
    return result;
}

bool CGuideToFamilyMember::Make(const SGuideInput& familyMemberInput, const SGuideInput& otherCdInput)
{
    bool madeGuide = false;
    CCdCore* familyCd = familyMemberInput.cd;
    CCdCore* otherCd = otherCdInput.cd;
    ncbi::cd_utils::CDFamily* family = familyMemberInput.fam;
    string err;
    string acc1 = familyCd->GetAccession(), acc2 = otherCd->GetAccession(), accCommon = "No common ancestor";

    Cleanup();
    if (!SanityCheck(familyMemberInput)) {
        m_isOK = false;
        return false;
    }

    //  Handle the special case where otherCd happens to be from this family, using CWithinFamilyGuideAlignment.
    if (InSameFamily(familyCd, otherCd, family)) {
        return MakeSameFamily(familyMemberInput, otherCdInput);
    }

    //
    //  'otherCd' not in the family
    //

    //  First, see if we get lucky -- try and make the guide w/o reference to the family.
    CTwoCDGuideAlignment simpleGuide(m_overlapPercentage1);
    if (simpleGuide.Make(familyCd, otherCd)) {
//        simpleGuide.CopyBase(this);
        CGuideAlignment_Base::CopyBase(&simpleGuide, this);
        return true;
    }

    //  Next, see if there's any CD in m_family1 for which we can make a guide to 'otherCd'.
    //  If so, get the guide between that third CD and 'familyCd' and combine the two guides
    //  to produce a guide between the two CDs of interest.

    unsigned int i, nInChain;
    CCdCore* thirdCd = NULL;
    CTwoCDGuideAlignment* guideToThirdCd = NULL;
    CWithinFamilyGuideAlignment* withinFamilyGuide = NULL;
    CDFamilyBase::post_order_iterator famIt = m_family1->begin_post(), famEnd = m_family1->end_post();
    while (guideToThirdCd == NULL && famIt != famEnd) {        
        thirdCd = (*famIt).cd;
        ++famIt;
        if (familyCd == thirdCd) continue;    //  we've already looked at familyCd above

        guideToThirdCd = new CTwoCDGuideAlignment(m_overlapPercentage1);
        if (!guideToThirdCd->Make(thirdCd, otherCd)) {
            delete guideToThirdCd;
            guideToThirdCd = NULL;
        } else {
            //  Now, make the guide between this third cd and the original cd from the family.
            withinFamilyGuide = new CWithinFamilyGuideAlignment(m_family1, m_overlapPercentage1);
            if (!withinFamilyGuide->Make(familyCd, thirdCd)) {
                delete guideToThirdCd;
                delete withinFamilyGuide;
                guideToThirdCd = NULL;
                withinFamilyGuide = NULL;
            }
        }
    }

    //  Merge the (otherCd - thirdCd) and (thirdCd - familyCd) guides.
    //  Create the relevant chains.
    if (guideToThirdCd && withinFamilyGuide) {
        BlockModelPair withinFamilyBMP = withinFamilyGuide->GetGuide();     //  (familyCd/thirdCd)
        BlockModelPair twoCDBMP = guideToThirdCd->GetGuide();                  //  (thirdCd/otherCd)
        if (CombineBlockModelPairs(withinFamilyBMP, false, twoCDBMP, true, m_overlapPercentage1)) {

            madeGuide = true;

            m_guideBlockModelPair = withinFamilyBMP;

            //  Populate the chain from familyCd -> thirdCd
            const TGuideChain& chainWithin1 = withinFamilyGuide->GetMasterChain();
            m_chain1.insert(m_chain1.begin(), chainWithin1.begin(), chainWithin1.end());
            
            if (withinFamilyGuide->GetCommonCD() != thirdCd) {
                const TGuideChain& chain2 = withinFamilyGuide->GetSlaveChain();
                nInChain = chain2.size();
                for (i = nInChain; i > 0; --i) {
                    if (i == nInChain) continue;  //  the end of the chain is the common CD, already included.
                    m_chain1.push_back(chain2[i-1]);
                }
            }

            //  Populate the chain from otherCd -> thirdCd  
            //  (CTwoCDGuideAlignment has only one CD in each chain, so don't need to be more sophisticated about this)
            m_chain2.insert(m_chain2.begin(), guideToThirdCd->GetSlaveChain().begin(), guideToThirdCd->GetSlaveChain().end());
            m_chain2.insert(m_chain2.end(), guideToThirdCd->GetMasterChain().begin(), guideToThirdCd->GetMasterChain().end());

        } else {
            m_errors.push_back("CGuideToFamilyMember::Make:\nFailed to combine intermediate guides to CD " + thirdCd->GetAccession() + " for CD " + acc1 + " and CD " + acc2 + ".\n");
        }
    } else {
        if (withinFamilyGuide && withinFamilyGuide->HasErrors()) {
            m_errors.push_back(withinFamilyGuide->GetErrors());
        }
        if (guideToThirdCd && guideToThirdCd->HasErrors()) {
            m_errors.push_back(guideToThirdCd->GetErrors());
        }
        m_errors.push_back("CGuideToFamilyMember::Make:\nUNABLE to create guide between CD " + acc1 + " and CD " + acc2 + ", which are not in the same hierarchy.\n");
    }

    delete guideToThirdCd;
    delete withinFamilyGuide;

    m_isOK = madeGuide;
    return madeGuide;
}


bool CGuideToFamilyMember::Make(const CCdCore* familyMember, const CCdCore* otherCd)
{
    //  Assume otherCd not in family.  Will check for this case in the other Make function.
    SGuideInput input1(const_cast<CCdCore*>(familyMember), const_cast<ncbi::cd_utils::CDFamily*>(m_family1), kMax_UInt, 0);
    SGuideInput input2(const_cast<CCdCore*>(otherCd), NULL, kMax_UInt, 0);

    bool result = Make(input1, input2);
    return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

