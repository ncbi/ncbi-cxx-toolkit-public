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
 *          Also contains a pure virtual base class for family-based guides.
 *
 * ===========================================================================
 */

#ifndef CU_GUIDEALIGNMENT_HPP
#define CU_GUIDEALIGNMENT_HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqalign/Seq_align.hpp>
//#include <objects/cdd/Domain_parent.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuCdFamily.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

//  A link in the chain of CDs that will form a TGuideChain
//  'blockModelIndex' and 'familyIndex' may be set depending on the
//  guide alignment type being generated.  (E.g., for fusion, etc.)
struct SGuideAlignmentLink
{
    const CCdCore* cd;
    unsigned int row;       // cd row used to make guide to parent; kMax_UInt if there was a problem
    int blockModelIndex;    // Optional; ignored if <0
    int familyIndex;        // Optional; ignored if <0

    SGuideAlignmentLink(const CCdCore* cdIn, unsigned int rowIn = 0, int bmiIn = -1, int fiIn = -1) : cd(cdIn), row(rowIn), blockModelIndex(bmiIn), familyIndex(fiIn) {}

    string Print(bool addNewline = false) const {
        string s = (cd) ? cd->GetAccession() : "<NULL CD>";
        s += ", row = " + NStr::UIntToString(row);
        if (blockModelIndex >= 0) {
            s+= "; block model index = " + NStr::IntToString(blockModelIndex);
        }
        if (familyIndex >= 0) {
            s+= "; family index = " + NStr::IntToString(familyIndex);
        }
        if (addNewline) s += "\n";
        return s;
    }
};


//  Represent the chain of relationships linking a CD to its ancestor.
//  By default, the first entry in the chain is the CD in the guide, and
//  when traversing a family, the last CD is the common ancestor for the two CDs 
//  in the guide.  Note that a chain may span more than one hierarchy if non-classical 
//  relationships are modeled, in which case the familyIndex would change between links.
typedef vector<SGuideAlignmentLink> TGuideChain;

NCBI_CDUTILS_EXPORT 
string GuideChainToString(const TGuideChain& chain);

// ==========================================
//      CGuideAlignment_Base declaration
// ==========================================

//  In most cases, it is best to use the CGuideAlignmentFactory class to instantiate 
//  a CGuideAlignment_Base-derived object.  

class NCBI_CDUTILS_EXPORT CGuideAlignment_Base
{

public:

    struct SGuideInput {
        SGuideInput(CCdCore* cdIn, CDFamily* famIn, unsigned int rowIn = 0, unsigned int parentIn = 0) : cd(cdIn), fam(famIn), rowInCd(rowIn), rootParentIndex(parentIn) {}

        CCdCore* cd;
        CDFamily* fam;
        unsigned int rowInCd;
        unsigned int rootParentIndex;  //  in non-classical families, index specifies which parent
    };

    CGuideAlignment_Base()  {Initialize();}
    virtual ~CGuideAlignment_Base() {
        Cleanup();
    }

    //  Subclass instances must be able to duplicate themselves, populating non-base-class members.
    virtual CGuideAlignment_Base* Copy() const = 0;

    //  Most general case.
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2) = 0;

    //  Subclasses will typically need to override the default implementation:  two CDs, no family information.
    virtual bool Make(const CCdCore* cd1, const CCdCore* cd2);

    //  Default implementation:  single hierarchy, use masters to generate a guide.
//    virtual bool Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family);

    //  Default implementation:  two hierarchies, use masters to generate a guide.
//    virtual bool Make(const CCdCore* cd1, CDFamily* family1, const CCdCore* cd2, CDFamily* family2);

    virtual string ToString() const;

    /// Reset the whole object
    virtual void Reset(void) {
        Initialize();
    }

    bool IsOK() const {return m_isOK;}

    const BlockModelPair& GetGuide() const {return m_guideBlockModelPair;}
    BlockModelPair GetGuide() {return m_guideBlockModelPair;}
    CRef<CSeq_align> GetGuideAsSeqAlign() const {return m_guideBlockModelPair.toSeqAlign();}

    //  Modify the guide by cutting out all residues not overlapping with the passed BlockModel,
    //  based on which BlockModel in the guide has a matching SeqId.
    //  If the SeqId does not match a SeqId in the existing guide, do nothing.
    //  The second parameter is ONLY used when the guide master and slave refer to the same sequence.
    //  NOTE:  the intersection may result in an empty set.  In such cases,  m_isOK will be set to false.
    void Intersect(const BlockModel& bm, bool basedOnMasterWhenSameSeqIdInGuide = true);

    //  Modify the guide by cutting out all residues overlapping with the mask,
    //  based on which BlockModel in the guide has a matching SeqId.
    //  If the SeqId does not match a SeqId in the existing guide, do nothing.
    //  The second parameter is ONLY used when the guide master and slave refer to the same sequence.
    //  NOTE:  the mask may remove all residues.  In such cases,  m_isOK will be set to false.
    void Mask(const BlockModel& mask, bool basedOnMasterWhenSameSeqIdInGuide = true);

    //  Swap the sense of the guide.
    void Reverse();

    //  Attempt to replace the master/slave of this guide with the alignment
    //  from seqAlign (the seqAlign's master if useFirst is true; the seqAlign's
    //  slave if useFirst is false).
    //  Returns false if the replacement can not be performed.
    bool ReplaceMaster(const CRef< CSeq_align >& seqAlign, bool useFirst);
    bool ReplaceSlave(const CRef< CSeq_align >& seqAlign, bool useFirst);

    //  Return -1 if can't be mapped
    int MapToMaster(unsigned int slavePos) const;
    int MapToSlave(unsigned int masterPos) const;

    bool HasErrors() const {return m_errors.size() > 0;}
    string GetErrors() const;

    //  These access the first entry in m_chain1
    const CCdCore* GetMasterCD() const;
    string GetMasterCDAcc() const;
    string GetMasterRowIdStr() const;
    unsigned int GetMasterCDRow() const;

    //  These access the first entry in m_chain2
    const CCdCore* GetSlaveCD() const;
    string GetSlaveCDAcc() const;
    string GetSlaveRowIdStr() const;
    unsigned int GetSlaveCDRow() const;

    const TGuideChain& GetMasterChain() const { return m_chain1;}
    const TGuideChain& GetSlaveChain() const { return m_chain2;}

    //  These access the last entry in m_chain1 and m_chain2.  If they are not the same,
    //  null pointer/empty string returned.
    const CCdCore* GetCommonCD() const;
    string GetCommonCDAcc() const;
    //  These access the last entry of m_chain1 (onMaster = true) or m_chain2 (onMaster = false).  
    //  If there is not a common CD, an empty string/kMax_UInt is returned.
    string GetCommonCDRowIdStr(bool onMaster) const;
    unsigned int GetCommonCDRow(bool onMaster) const;

protected:

    bool m_isOK;
    vector<string> m_errors;

    //  Make this mutable because mapping functions need to call BlockModelPair::reverse
    mutable BlockModelPair m_guideBlockModelPair;

    TGuideChain m_chain1;   //  chain for the master of the guide
    TGuideChain m_chain2;   //  chain for the slave of the guide
    vector<CDFamily*> m_families;       //  indices in m_chainX reference this container
    vector<BlockModel*> m_blockModels;  //  indices in m_chainX reference this container

    virtual void Initialize();
    virtual void Cleanup();

    //  Build the TGuideChain structures:  default implementation just has start and, if commonCd
    //  is non-NULL, the end.
    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd);

    //  Copies base-class members of this into guideCopy.  Does not create guideCopy!!
    void CopyBase(CGuideAlignment_Base* guideCopy) const;

    //  Use this to transfer base-class elements between instances of two different subclasses.
    static void CopyBase(CGuideAlignment_Base* from, CGuideAlignment_Base* to) { 
        if (from) 
            from->CopyBase(to);
    }

    bool IsBMPValid() const;

private:

    bool Replace(bool replaceMaster, const CRef< CSeq_align >& seqAlign, bool useFirst);

};

// ==========================================
//      CDegenerateGuide declaration
// ==========================================

//  A degenerate version of a 'guide alignment' that only relates a pair of domains
//  and does not actually specify how they are aligned.  To support cases where
//  a relationship is desired yet the details are not relevant.  [E.g., traditionally, 
//  classical parent/child relationships in CDTree do not require a mapping to
//  be created -- although all validated parent/child pairs in CDD ultimately do
//  obtain such a mapping.]

class NCBI_CDUTILS_EXPORT CDegenerateGuide : public CGuideAlignment_Base
{
public:
    CDegenerateGuide()  {Initialize();}
    virtual ~CDegenerateGuide() {
        Cleanup();
    }

    //  Subclass instances must be able to duplicate themselves, populating non-base-class members.
    virtual CGuideAlignment_Base* Copy() const;

    //  Most general case.
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2);

    virtual string ToString() const;

    bool IsOK() const {return m_isOK;}

//    const BlockModelPair& GetGuide() const {return m_guideBlockModelPair;}
//    BlockModelPair GetGuide() {return m_guideBlockModelPair;}
//    CRef<CSeq_align> GetGuideAsSeqAlign() const {return m_guideBlockModelPair.toSeqAlign();}

protected:

    //  Build the TGuideChain structures:  in this degenerate case, the commonCd is always ignored.
    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd);

};

// ==========================================
//      CFamilyBasedGuide declaration
// ==========================================

//  Adds a few member variables/methods common to making guides
//  based on a family.

class NCBI_CDUTILS_EXPORT CFamilyBasedGuide : public CGuideAlignment_Base
{
protected:

    static const int m_defaultOverlapPercentage;

public:

    CFamilyBasedGuide(const ncbi::cd_utils::CDFamily* family) : m_family1(family), m_overlapPercentage1(m_defaultOverlapPercentage), m_cleanupFamily1(false), m_family2(NULL), m_overlapPercentage2(m_defaultOverlapPercentage), m_cleanupFamily2(false) {
        Initialize();
    }

    CFamilyBasedGuide(const ncbi::cd_utils::CDFamily* family, int overlapPercentage) : m_family1(family), m_overlapPercentage1(m_defaultOverlapPercentage), m_cleanupFamily1(false), m_family2(NULL), m_overlapPercentage2(m_defaultOverlapPercentage), m_cleanupFamily2(false) {
        SetOverlapPercentage(overlapPercentage, true);
        Initialize();
    }


    virtual ~CFamilyBasedGuide() {
        Cleanup();
    }

    //  Subclass instances must be able to duplicate themselves, populating non-base-class members.
    virtual CGuideAlignment_Base* Copy() const = 0;

    //  Most general case.
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2) = 0;

    //  Since explicitly expect a family to be defined, subclasses *must* define this method
    //  as the base class default implementation is not applicable.
    //  'cd1' must come from m_family1; the requirements for 'cd2' will vary in the different subclasses.
    virtual bool Make(const CCdCore* cd1, const CCdCore* cd2) = 0;

//    virtual string ToString() const;

    //  Allow only the second family to be specified outside of the constructor.
    //  This class does not assume ownership of the passed family.
    void SetSecondFamily(const ncbi::cd_utils::CDFamily* secondFamily, int percentage = m_defaultOverlapPercentage);
    const ncbi::cd_utils::CDFamily* GetFamily(bool firstFamily = true) const { return (firstFamily) ? m_family1 : m_family2;}

    //  Set the minimum overlap required when creating a guide.  An integer between one and one hundred
    //  Returns false if the input was out of range, in which case nothing is changed.
    bool SetOverlapPercentage(int percentage, bool firstFamily = true);
    int GetOverlapPercentage(bool firstFamily = true) const { return (firstFamily) ? m_overlapPercentage1 : m_overlapPercentage2;}

    bool IsCleaningUpFamily(bool firstFamily) const { return (firstFamily) ? m_cleanupFamily1 : m_cleanupFamily2;}
    void SetCleanUpFamily(bool cleanup, bool firstFamily) { 
        if (firstFamily) { 
            m_cleanupFamily1 = cleanup;
        } else {
            m_cleanupFamily2 = cleanup;
        }
    }

    static bool InSameFamily(const CCdCore* cd1, const CCdCore* cd2, const CDFamily* family);

protected:

    const ncbi::cd_utils::CDFamily* m_family1;  //  this should always be non-NULL    
    int m_overlapPercentage1;  //  Between 0 and 100; ultimately used in BlockFormatter
    bool m_cleanupFamily1;     //  need to cleanup m_family if initialized w/ a ::CDFamily object.

    //  This set of members may or may not be used by derived classes.
    const ncbi::cd_utils::CDFamily* m_family2;  
    int m_overlapPercentage2;  //  Between 0 and 100; ultimately used in BlockFormatter
    bool m_cleanupFamily2;     //  need to cleanup m_family if initialized w/ a ::CDFamily object.


    virtual void Initialize();   // puts 2nd family members back to defaults.
    virtual void Cleanup();

    //  Build the TGuideChain structures:  default implementation just has start and, if commonCd
    //  is non-NULL, the end.
    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd) {};

private:

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // CU_GUIDEALIGNMENT_HPP
