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

//  A link in the chain of CDs that will form a CGuideAlignmentPath.
//  'blockModelIndex' and 'familyIndex' may be set depending on the
//  guide alignment type being generated.  (E.g., for fusion, etc.)
struct SGuideAlignmentLink
{
    const CCdCore* cd;
    unsigned int row;       //  kMax_UInt if there was a problem
    int blockModelIndex;  // Optional; ignored if <0
    int familyIndex;      // Optional; ignored if <0

    SGuideAlignmentLink(const CCdCore* cdIn, unsigned int rowIn = 0, int bmiIn = -1, int fiIn = -1) : cd(cdIn), row(rowIn), blockModelIndex(bmiIn), familyIndex(fiIn) {}
};


//  Represent the chain of relationships linking a CD to its ancestor.
//  By default, the first entry in the chain is the CD in the guide, and
//  the last CD is the common ancestor for the two CDs in the guide.  
//  Note that a chain may span more than one hierarchy if non-classical relationships 
//  are modeled, in which case the familyIndex would change between links.
typedef vector<SGuideAlignmentLink> TGuideChain;

//  This class encapsulates the simplest case:  the creation of a guide alignment between
//  two CDs in a single classical hierarchy.  The guide is between the masters of
//  the two CDs, mapped to the alignment of their first common ancestor.
class NCBI_CDUTILS_EXPORT CGuideAlignment
{
//    friend class CGuideAlignmentFactory;

public:

    struct SGuideInput {
        SGuideInput(CCdCore* cdIn, CDFamily* famIn, unsigned int rowIn = 0, unsigned int parentIn = 0) : cd(cdIn), fam(famIn), rowInGuide(rowIn), rootParentIndex(parentIn) {}

        CCdCore* cd;
        CDFamily* fam;
        unsigned int rowInGuide;
        unsigned int rootParentIndex;  //  in non-classical families, index specifies which parent
    };


    CGuideAlignment()  {Initialize();}
    virtual ~CGuideAlignment() {
        Cleanup();
    }

    virtual bool IsOK() const {return m_isOK;}

    //  This base class deals only with classical hierarchies.
    //  Override in subclasses that deal with more complex relationships.
    virtual CDomain_parent::EParent_type GetType() const { return CDomain_parent::eParent_type_classical;}

    //  Simplest case by default:  single classical hierarchy, use masters to generate a guide.
    //  Never uses the cache as the cached alignment may represent a different subfamily than needed.
    bool Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family);

    //  Convenience method for when one of the two CDs is the family's root node.
    //  Caches the multiple alignment object if 'cache' is true.
    bool MakeGuideToRoot(const CCdCore* cd, CDFamily* family, bool cache = false);
    static void ClearAlignmentCache();

    //  Most general case; default implementation calls the above simple case, ignoring the 'row' in the
    //  guideInputs and always using the master of the specified CDs.
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2);

    const BlockModelPair& GetGuide() const {return m_guideBlockModelPair;}
    BlockModelPair GetGuide() {return m_guideBlockModelPair;}
    CRef<CSeq_align> GetGuideAsSeqAlign() const {return m_guideBlockModelPair.toSeqAlign();}

    bool HasErrors() const {return m_errors.size() > 0;}
    string GetErrors() const;

    //  These access the first entry in m_chain1
    const CCdCore* GetMasterCD() const;
    string GetMasterCDAcc() const;
    unsigned int GetMasterCDRow() const;

    //  These access the first entry in m_chain2
    const CCdCore* GetSlaveCD() const;
    string GetSlaveCDAcc() const;
    unsigned int GetSlaveCDRow() const;

    //  These access the last entry in m_chain1
    const CCdCore* GetCommonCD() const;
    string GetCommonCDAcc() const;
    unsigned int GetCommonCDRow() const;

    /// Reset the whole object
    virtual void Reset(void) {
        Initialize();
    }

protected:

    //  Informational class members for easy access
    bool   m_isOK;

    vector<string> m_errors;

    BlockModelPair m_guideBlockModelPair;

    TGuideChain m_chain1;   //  chain for the master of the guide
    TGuideChain m_chain2;   //  chain for the slave of the guide
    vector<CDFamily*> m_families;       //  indices in m_chainX reference this container
    vector<BlockModel*> m_blockModels;  //  indices in m_chainX reference this container

    static map<CDFamily*, MultipleAlignment*> m_alignmentCache;


    virtual void Initialize();
    void Cleanup();

private:

    //  In cases where the cache is used, pass the cached alignment.
    bool Make(const CCdCore* cd1, const CCdCore* cd2, MultipleAlignment* ma);
    //  Build the TGuideChain structures.
    void MakeChains(const CCdCore* cd1, const CCdCore* cd2, const CCdCore* commonCd, CDFamily* family);
};


class NCBI_CDUTILS_EXPORT CGuideAlignmentFactory {

public:

    typedef map<CCdCore*, CGuideAlignment*> GuideMap;
    typedef GuideMap::iterator GuideMapIt;
    typedef GuideMap::value_type GuideMapVT;


    CGuideAlignmentFactory() {};
    ~CGuideAlignmentFactory() {};

    //  ** Classical-parent only method.  **
    //  Uses the base guide alignment class for getting all guide alignments to the root of the family.
    //  Caller should verify that the guides in the GuideMap are valid (a null pointer is not sufficient, so
    //  check for error messages or call the IsOK method).
    void MakeAllGuidesToRoot(CDFamily* family, GuideMap& guideMap);

    //  ULTIMATELY, will infers the proper type of guide alignment needed based on the contents 
    //  of the guide inputs, then uses the virtual methods to create objects of the required 
    //  CGuideAlignment-derived class.
    //  Caller should verify that the guide returned is valid (a null pointer is not sufficient, so
    //  check for error messages or call the IsOK method).
    CGuideAlignment* MakeGuide(const CGuideAlignment::SGuideInput& guideInput1, const CGuideAlignment::SGuideInput& guideInput2);

private:
};



END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // CU_GUIDEALIGNMENT_HPP

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2006/12/18 17:04:54  lanczyck
 * initial version
 *
 *
 * ===========================================================================
 */
