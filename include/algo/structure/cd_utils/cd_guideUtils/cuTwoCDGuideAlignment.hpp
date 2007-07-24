//  $Id$

#ifndef CU_TWO_CD_GUIDE_ALIGNMENT__HPP
#define CU_TWO_CD_GUIDE_ALIGNMENT__HPP

#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


//  This class only examines the two CDs presented.   The guide is between the two masters.
class CTwoCDGuideAlignment: public CGuideAlignment_Base
{
public:

    CTwoCDGuideAlignment() : m_overlapPercentage(100) {
        Initialize();
    }

    CTwoCDGuideAlignment(int overlapPercentage) : m_overlapPercentage(100) {
        SetOverlapPercentage(overlapPercentage);
        Initialize();
    }

    virtual ~CTwoCDGuideAlignment() {
        Cleanup();
    }

    virtual CGuideAlignment_Base* Copy() const;

    //  If guideInput.rowInCd is kMax_UInt for both CDs, try use first possible overlapping rows in constructing the guide.
    //  Returns true if a guide alignment was successfully constructed.
    //  Returns false if couldn't make a guide, or if bad inputs (e.g., the families in the two SGuideInput arguments differ).
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2);

    //  Simply make a guide using these two CDs, if possible.  Deduce the rows to use from the CDs.
    virtual bool Make(const CCdCore* cd1, const CCdCore* cd2);

    //  Minimum overlap required when creating a guide.  An integer between one and one hundred
    //  Returns false if the input was out of range, in which case nothing is changed.
    bool SetOverlapPercentage(int percentage);
    int GetOverlapPercentage() const { return m_overlapPercentage;}

protected:

    //  Build the TGuideChain structures.
    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd) {};

    virtual void Initialize();
    virtual void Cleanup();

private:

    int m_overlapPercentage;  //  Between 0 and 100; ultimately used in BlockFormatter

};


END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif    //  CU_TWO_CD_GUIDE_ALIGNMENT__HPP
