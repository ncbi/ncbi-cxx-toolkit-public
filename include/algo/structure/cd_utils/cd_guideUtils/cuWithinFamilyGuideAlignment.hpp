//  $Id$

#ifndef CU_WITHIN_FAMILY_GUIDE_ALIGNMENT__HPP
#define CU_WITHIN_FAMILY_GUIDE_ALIGNMENT__HPP

#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

extern BlockModelPair* GuideViaBlockFormatter(BlockModelPair& bmp1, bool masterIsCommon1, BlockModelPair& bmp2, bool masterIsCommon2, int seqLen, int overlapPercentage);

//  Construct a BMP between any two rows of the same CD.  Fails if the guide can't be made (e.g., where
//  the rows have non-overlapping block models).
NCBI_CDUTILS_EXPORT 
bool GetGuideBetweenRows(const CCdCore* cd, unsigned int masterRow, unsigned int slaveRow, BlockModelPair& bmp);

//  Assumes the bmp1 and bmp2 have a common block model, specified by the two bool arguments.
//  The common sequence is removed, and a BlockModelPair created from the remaining block models
//  If the result it true, result of the combination is left in bmp1, with the non-common block model from
//  bmp1 input as the master and the non-common block model of bmp2 as the slave.
//  If the result is false, bmp1/2 are unchanged.
NCBI_CDUTILS_EXPORT 
bool CombineBlockModelPairs(BlockModelPair& bmp1, bool masterIsCommon1, BlockModelPair& bmp2, bool masterIsCommon2, int overlapPercentage);


//  This class will try and build a guide alignment for any two CDs in a single family.
//  The resulting guide is between the two masters.
//  Uses the CTwoCDGuideAlignment class internally as the basic operation.
class CWithinFamilyGuideAlignment: public CFamilyBasedGuide
{
public:

    CWithinFamilyGuideAlignment(const CDFamily* family) : CFamilyBasedGuide(family) {
    }

    CWithinFamilyGuideAlignment(const CDFamily* family, int overlapPercentage) : CFamilyBasedGuide(family, overlapPercentage)  {
    }

    virtual ~CWithinFamilyGuideAlignment() {
        Cleanup();
    }

    virtual CGuideAlignment_Base* Copy() const;

    //  Uses the family from the input arguments, and NOT from the constructor!
    //  If guideInput.rowInCd is kMax_UInt for both CDs, use first possible overlapping rows in constructing the guide.
    //  Returns true if a guide alignment was successfully constructed.
    //  Returns false if couldn't make a guide, or if bad inputs (e.g., the families in the two SGuideInput arguments differ).
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2);

    //  Make a guide using these two CDs, if possible.  Deduces the rows to use from the CDs.
    //  The two CDs must be from the family given in the constructor for the calculation to proceed.
    bool Make(const CCdCore* cd1, const CCdCore* cd2);


protected:

    //  Build the TGuideChain structures.
//    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd) {};

//    virtual void Initialize();
//    virtual void Cleanup();

private:


    bool SanityCheck(const SGuideInput& guideInput1, const SGuideInput& guideInput2);

    //  Recursively try and build a guide along the path of CDs:  assumes children are
    //  before parents in the vector.  The result 'bmpGuide' has the master of the first CD
    //  in the path as master block model, the master of the last CD in the path as slave block model.
    //  A path of length one returns a dummy guide of the master to itself.
    bool GetGuideSpanningPath(vector<CCdCore*>& path, BlockModelPair& bmpGuide, TGuideChain& pathChain);

};


END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif    //  CU_WITHIN_FAMILY_GUIDE_ALIGNMENT__HPP
