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

#include <algo/structure/cd_utils/cuGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)



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
    //  Get all classical guide alignments to the root of the family.
    //  Caller should verify that the guides in the GuideMap are valid (a null pointer is not sufficient, so
    //  check for error messages or call the IsOK method).
    void MakeAllGuidesToRoot(CDFamily* family, GuideMap& guideMap);

    //   Destroys all CGuideAlignment_Base objects in the passed map and clears the map.
    void DestroyGuides(GuideMap& guideMap);

    //  ULTIMATELY, will infers the proper type of guide alignment needed based on the contents 
    //  of the guide inputs, then uses the virtual methods to create objects of the required 
    //  CGuideAlignment_Base-derived class.
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


#endif // CU_GUIDEALIGNMENTFACTORY_HPP

/*
 * ===========================================================================
 *
 * $Log$
 *
 * ===========================================================================
 */

