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
 *          Factory for generation of guide alignment objects.
 *
 * ===========================================================================
 */

#ifndef CU_GUIDEALIGNMENTFACTORY_HPP
#define CU_GUIDEALIGNMENTFACTORY_HPP

#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)



class NCBI_CDUTILS_EXPORT CGuideAlignmentFactory {

    static const int m_defaultOverlapPercent;

public:

    typedef map<CCdCore*, CGuideAlignment_Base*> GuideMap;
    typedef GuideMap::iterator GuideMapIt;
    typedef GuideMap::value_type GuideMapVT;


    CGuideAlignmentFactory(int overlapPercentage = m_defaultOverlapPercent, bool allowConsensus = true) : m_allowConsensus(allowConsensus), m_makeDegenerateGuides(false), m_overlapPercentage(m_defaultOverlapPercent) { 
        SetOverlapPercentage(overlapPercentage);
    }
    ~CGuideAlignmentFactory() {};

    //  When m_allowConsensus is false, any guides made by the factory will first
    //  remove any consensus sequence found, remastering to the first non-consensus 
    //  row if the consensus is the master.  When true, a consensus can be used
    //  as the row for the guide alignment (although it needs to be temporarily
    //  removed in order to make a MultipleAlignment).
    bool IsAllowConsensus() const {return m_allowConsensus;}
    void SetAllowConsensus(bool allowConsensus) { m_allowConsensus = allowConsensus;}

    //  Forces the MakeGuide(...) methods to instantiate CDegenerateGuide instances, 
    //  in which a BlockModelPair is not created.  
    void SetMakeDegenerateGuides(bool makeDegenerate) {m_makeDegenerateGuides = makeDegenerate;}  
    bool GetMakeDegenerateGuides() const {return m_makeDegenerateGuides;}  

    //  Control amount of overlap of a specified range required when making guide.
    //  true if set value is in range [0, 100]; return false if out of range    
    bool SetOverlapPercentage(int overlapPercentage);  
    int GetOverlapPercentage() const {return m_overlapPercentage;}

    //   Destroys all CGuideAlignment_Base objects in the passed map and clears the map.
    void DestroyGuides(GuideMap& guideMap);

    //  Only uses sequences in the two CDs passed in to build a guide.
    CGuideAlignment_Base* MakeGuide(const CCdCore* cd1, const CCdCore* cd2);

    //  Uses the family to make a guide between the two CDs passed in.
    //  If neither CD is in the family, or the family is NULL, will still try to make the guide.
    CGuideAlignment_Base* MakeGuide(const CCdCore* cd1, const CCdCore* cd2, const CDFamily* family);

    //  For simplicity, cd1 is required to be in family1 and cd2 is required to be in family2
    //  when both are different non-null objects.  Returns NULL pointer if this pre-condition fails.
    CGuideAlignment_Base* MakeGuide(const CCdCore* cd1, const CDFamily* family1, const CCdCore* cd2, const CDFamily* family2);

    //  Special case of the above when the family is known to be valid (i.e., from CDD or CDTrack).
    //  cd1 and cd2 are required to be members of the family, which can not be NULL.
    //  (Unless m_makeDegenerateGuides has been set to true, all pointers returned are instances of CValidatedHierarchyGuideAlignment.)
    CGuideAlignment_Base* MakeGuideFromValidatedHierarchy(const CCdCore* cd1, const CCdCore* cd2, const CDFamily* validatedFamily);

    //  ** Validated hierarchy method.  **
    //  Get guide alignments to the root for all non-root members of the family.
    //  Caller should verify that the guides in the GuideMap are valid (a null pointer is not sufficient, so
    //  check for error messages or call the IsOK method).
    //  (All pointers returned are instances of CValidatedHierarchyGuideAlignment; this method
    //   overrides the m_makeDegenerateGuides flag.)
    void MakeAllGuidesToRoot(CDFamily* family, GuideMap& guideMap);

private:

    typedef map<CCdCore*, CRef< CSeq_align> > ConsensusStore;
    typedef ConsensusStore::iterator ConsensusStoreIt;
    typedef ConsensusStore::value_type ConsensusStoreVT;

    bool m_allowConsensus;
    bool m_makeDegenerateGuides;  //  use this toggle to integrate creation of degenerate guides vs. separate methods
    int m_overlapPercentage;
    ConsensusStore m_consensusStore;

    //  Infers the proper type of guide alignment needed based on the contents 
    //  of the guide inputs, then uses the virtual methods to create objects of the required 
    //  CGuideAlignment_Base-derived class.
    //  Caller should verify that the guide returned is valid (a non-null pointer is not sufficient, so
    //  check for error messages or call the IsOK method).
    CGuideAlignment_Base* MakeGuideInstance(const CGuideAlignment_Base::SGuideInput& guideInput1, const CGuideAlignment_Base::SGuideInput& guideInput2);

    //  Helper method for MakeGuide & the generic MakeGuideInstance method
    CGuideAlignment_Base* MakeGuideInstance(const CCdCore* cd1, const CCdCore* cd2, const CDFamily* family, bool cd1InFam, bool cd2InFam);

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


#endif // CU_GUIDEALIGNMENTFACTORY_HPP
