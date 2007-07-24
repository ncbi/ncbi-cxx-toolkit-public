//  $Id$

#ifndef CU_GUIDE_TO_FAMILY_MEMBER__HPP
#define CU_GUIDE_TO_FAMILY_MEMBER__HPP


//#include "algCdFamily.hpp"
//#include "algAlignmentCollection.hpp"

#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

extern bool CombineBlockModelPairs(BlockModelPair& bmp1, bool masterIsCommon1, BlockModelPair& bmp2, bool masterIsCommon2, int overlapPercentage);

//  Use this class to build a guide alignment between a CD from a given family
//  and a second CD that may or may not be from that family.
//  The resulting guide is between the two masters.
//  Uses CTwoCDGuideAlignment and CWithinFamilyGuideAlignment classes internally.
class CGuideToFamilyMember: public CFamilyBasedGuide
{
public:

    CGuideToFamilyMember(const ncbi::cd_utils::CDFamily* family) : CFamilyBasedGuide(family) {
    }

    CGuideToFamilyMember(const ncbi::cd_utils::CDFamily* family, int overlapPercentage) : CFamilyBasedGuide(family, overlapPercentage) {
    }

    virtual ~CGuideToFamilyMember() {
        Cleanup();
    }

    virtual CGuideAlignment_Base* Copy() const;

    //  Uses the family from the first input argument, and NOT from the constructor!
    //  Family in 2nd input argument is ignored.
    //  If SGuideInput.rowInCd is kMax_UInt for both CDs, use first possible overlapping rows in constructing the guide.
    //  Returns true if a guide alignment was successfully constructed.
    //  Returns false if couldn't make a guide, or if bad inputs (e.g., in first arg, inconsistent cd & fam).
    virtual bool Make(const SGuideInput& familyMemberInput, const SGuideInput& otherCdInput);

    //  Make a guide using these two CDs, if possible.  Deduce the rows to use from the CDs.
    //  The first CD *must* be from the family given in the constructor for the calculation to proceed.
    //  The second CD may or may not be from the family given in the constructor
    bool Make(const CCdCore* familyMember, const CCdCore* otherCd);

    //  Make a guide of the given CD -- that may or may not be in the family -- to the hierarchy root.
    //  Will start with the root model, but if no guide is possible this method will descend the tree until
    //  a valid guide is found, and map the result back to the root CD.
    //  bool MakeGuideToRoot(const CCdCore* cd);

protected:

    //  Build the TGuideChain structures.
//    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd);

//    virtual void Initialize();
//    virtual void Cleanup();

private:

    bool SanityCheck(const SGuideInput& familyMemberInput);

    bool MakeSameFamily(const SGuideInput& familyMemberInput, const SGuideInput& otherCdInput);

    //  Recursively try and build a guide along the path of CDs:  assumes children are
    //  before parents in the vector.  The result 'bmpGuide' has the master of the first CD
    //  in the path as master block model, the master of the last CD in the path as slave block model.
    //  A path of length one returns a dummy guide of the master to itself.
    bool GetGuideSpanningPath(vector<CCdCore*>& path, BlockModelPair& bmpGuide, TGuideChain& pathChain);

};


END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif    //  CU_GUIDE_TO_FAMILY_MEMBER__HPP
