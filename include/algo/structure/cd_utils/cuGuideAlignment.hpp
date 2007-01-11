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

#ifndef CU_GUIDEALIGNMENT_HPP
#define CU_GUIDEALIGNMENT_HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/cdd/Domain_parent.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuCdFamily.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class MultipleAlignment;

//  A link in the chain of CDs that will form a TGuideChain
//  'blockModelIndex' and 'familyIndex' may be set depending on the
//  guide alignment type being generated.  (E.g., for fusion, etc.)
struct SGuideAlignmentLink
{
    const CCdCore* cd;
    unsigned int row;       // cd row used to make guide to parent; kMax_UInt if there was a problem
    unsigned int parentRow; // row in parent used to make guide to cd; kMax_UInt if there was a problem
    int blockModelIndex;    // Optional; ignored if <0
    int familyIndex;        // Optional; ignored if <0

    SGuideAlignmentLink(const CCdCore* cdIn, unsigned int rowIn = 0, unsigned int prowIn = 0, int bmiIn = -1, int fiIn = -1) : cd(cdIn), row(rowIn), parentRow(prowIn), blockModelIndex(bmiIn), familyIndex(fiIn) {}
};


//  Represent the chain of relationships linking a CD to its ancestor.
//  By default, the first entry in the chain is the CD in the guide, and
//  the last CD is the common ancestor for the two CDs in the guide.  
//  Note that a chain may span more than one hierarchy if non-classical relationships 
//  are modeled, in which case the familyIndex would change between links.
typedef vector<SGuideAlignmentLink> TGuideChain;


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

    bool IsOK() const {return m_isOK;}

    //  Override to specify parent relationship underlying this guide.
    virtual CDomain_parent::EParent_type GetType() const = 0;

    //  Most general case; default implementations below calls this method.
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2) = 0;

    //  Simplest case by default:  single hierarchy, use masters to generate a guide.
    //  No special handling if a consensus master is present --> error likely unless subclass
    //  overrides this method.
    virtual bool Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family);

    //  Simplest two family case by default:  two hierarchies, use masters to generate a guide.
    //  No special handling if a consensus master is present --> error likely unless subclass
    //  overrides this method.
    virtual bool Make(const CCdCore* cd1, CDFamily* family1, const CCdCore* cd2, CDFamily* family2);

    //  Convenience method for when one of the two CDs is the family's root node.
    //  Caches the multiple alignment object if 'cache' is true.
    //  Has special handling of consensus masters in one or more of the family's CDs.
//    bool MakeGuideToRoot(const CCdCore* cd, CDFamily* family, bool cache = false);
//    static void ClearAlignmentCache();

    const BlockModelPair& GetGuide() const {return m_guideBlockModelPair;}
    BlockModelPair GetGuide() {return m_guideBlockModelPair;}
    CRef<CSeq_align> GetGuideAsSeqAlign() const {return m_guideBlockModelPair.toSeqAlign();}

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

    //  These access the last entry in m_chain1
    const CCdCore* GetCommonCD() const;
    string GetCommonCDAcc() const;
    string GetCommonRowIdStr() const;
    unsigned int GetCommonCDRow() const;

    virtual string ToString() const;

    /// Reset the whole object
    virtual void Reset(void) {
        Initialize();
    }

protected:

    bool m_isOK;
    vector<string> m_errors;

    //  Make this mutable because mapping functions need to call BlockModelPair::reverse
    mutable BlockModelPair m_guideBlockModelPair;

    TGuideChain m_chain1;   //  chain for the master of the guide
    TGuideChain m_chain2;   //  chain for the slave of the guide
    vector<CDFamily*> m_families;       //  indices in m_chainX reference this container
    vector<BlockModel*> m_blockModels;  //  indices in m_chainX reference this container

//    static map<CDFamily*, MultipleAlignment*> m_alignmentCache;

    virtual void Initialize();
    virtual void Cleanup();

    //  In cases where the cache is used, pass the cached alignment.
//    bool Make(const CCdCore* cd1, const CCdCore* cd2, MultipleAlignment* ma);
    //  Build the TGuideChain structures:  default implementation just has start and, if commonCd
    //  is non-NULL, the end.
    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd);
//(const CCdCore* cd1, const CCdCore* cd2, const CCdCore* commonCd, CDFamily* family1, CDFamily* family2);

private:

    bool Replace(bool replaceMaster, const CRef< CSeq_align >& seqAlign, bool useFirst);

};


//  This class encapsulates the simplest case:  the creation of a guide alignment between
//  the *masters* of two CDs in a single *validated* classical hierarchy:  this should 
//  always be possible.  The resulting guide is between the masters of
//  the two CDs, mapped to the alignment of their first common ancestor (or the family
//  root if 'MakeGuideToRoot' is used).  The IsOK() method will return 'false' if
//  the guide could not be made.  
//  Consensus masters are not treated specially --> the friend factory class helps hide the
//  details of managing consensus, or getting a guide between a pair of consensus masters.
class NCBI_CDUTILS_EXPORT CMasterGuideAlignment : public CGuideAlignment_Base
{
    friend class CGuideAlignmentFactory;

//    friend ostream& operator <<(ostream& os, const CGuideAlignment& ga);

public:

    CMasterGuideAlignment() {
        Initialize();
    }

    virtual ~CMasterGuideAlignment() {
        Cleanup();
    }

    //  This class deals only with classical hierarchies.
    //  Override in subclasses that deal with more complex relationships.
    virtual CDomain_parent::EParent_type GetType() const { return CDomain_parent::eParent_type_classical;}

    //  This specialized class ignores everything except the 'cd' in the guideInputs; 
    //  always uses the master of the specified CDs.  
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2);

    //  Never uses the cache as the cached alignment may represent a different subfamily than needed.
    virtual bool Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family);

    //  Convenience method for when one of the two CDs is the family's root node.
    //  Caches the multiple alignment object if 'cache' is true.
    bool MakeGuideToRoot(const CCdCore* cd, CDFamily* family, bool cache = false);
    static void ClearAlignmentCache();

protected:

    static map<CDFamily*, MultipleAlignment*> m_alignmentCache;

    //  Build the TGuideChain structures.
    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd);

private:

    void MakeChains(const CCdCore* cd1, const CCdCore* cd2, const CCdCore* commonCd, CDFamily* family);

    //  In cases where the cache is used, pass the cached alignment.
    bool Make(const CCdCore* cd1, const CCdCore* cd2, MultipleAlignment* ma);

};


class NCBI_CDUTILS_EXPORT CGuideAlignmentFactory {

public:

    typedef map<CCdCore*, CGuideAlignment_Base*> GuideMap;
    typedef GuideMap::iterator GuideMapIt;
    typedef GuideMap::value_type GuideMapVT;


    CGuideAlignmentFactory(bool allowConsensus = true) : m_allowConsensus(allowConsensus) {};
    ~CGuideAlignmentFactory() {};

    //  When m_allowConsensus is false, any guides made by the factory will first
    //  remove any consensus sequence found, remastering to the first non-consensus 
    //  row if the consensus is the master.  When true, a consensus can be used
    //  as the row for the guide alignment (although it needs to be temporarily
    //  removed in order to make a MultipleAlignment).
    bool IsAllowConsensus() const {return m_allowConsensus;}
    void SetAllowConsensus(bool allowConsensus) { m_allowConsensus = allowConsensus;}

    //  ** Classical-parent only method.  **
    //  Uses the base guide alignment class for getting all guide alignments to the root of the family.
    //  Caller should verify that the guides in the GuideMap are valid (a null pointer is not sufficient, so
    //  check for error messages or call the IsOK method).
    void MakeAllGuidesToRoot(CDFamily* family, GuideMap& guideMap);

    //   Destroys all CGuideAlignment objects in the passed map and clears the map.
    void DestroyGuides(GuideMap& guideMap);

    //  ULTIMATELY, will infers the proper type of guide alignment needed based on the contents 
    //  of the guide inputs, then uses the virtual methods to create objects of the required 
    //  CGuideAlignment-derived class.
    //  Caller should verify that the guide returned is valid (a null pointer is not sufficient, so
    //  check for error messages or call the IsOK method).
    CGuideAlignment_Base* MakeGuide(const CGuideAlignment_Base::SGuideInput& guideInput1, const CGuideAlignment_Base::SGuideInput& guideInput2);

private:

    typedef map<CCdCore*, CRef< CSeq_align> > ConsensusStore;
    typedef ConsensusStore::iterator ConsensusStoreIt;
    typedef ConsensusStore::value_type ConsensusStoreVT;

    bool m_allowConsensus;
    ConsensusStore m_consensusStore;

    //  Use the specified entries in the ConsensusStore to remap the guide alignment.
    bool RemapGuideToConsensus(CGuideAlignment_Base& gaToMap, ConsensusStoreIt& master, ConsensusStoreIt& slave);
    
    //  The first method removes consensus master seq-aligns from all CDs in 'family',
    //  remastering the CD to what was row 1 prior to the removal.  The removed seq-align
    //  is placed in m_consensusStore, with the first sequence in that pairwise alignment
    //  being the new master and the second being the consensus (i.e., the original master).
    //  The second method replaces the seq-aligns from the m_consensusStore and remasters
    //  back to the consensus.
    void RemoveConsensusMasters(CDFamily& family);
    void RestoreConsensusMasters(CDFamily& family);
};



END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // CU_GUIDEALIGNMENT_HPP

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2007/01/11 21:51:45  lanczyck
 * last minute pre-Subversion conversion changes
 *
 * Revision 1.2  2007/01/09 19:28:32  lanczyck
 * refactor guide alignment class; add some method to guide alignment factory
 *
 * Revision 1.1  2006/12/18 17:04:54  lanczyck
 * initial version
 *
 *
 * ===========================================================================
 */
