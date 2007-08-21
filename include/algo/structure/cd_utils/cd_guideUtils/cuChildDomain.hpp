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
 *          General representation of a child domain and the links
 *          to its parent domains.
 *
 * ===========================================================================
 */

#ifndef CU_CHILD_DOMAIN__HPP
#define CU_CHILD_DOMAIN__HPP


#include <objects/cdd/Domain_parent.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)


//  This class hierarchy allows the CChildDomain class to not need to know
//  about the details of its parentage.  Each instance will contain a guide
//  alignment and maintain knowledge of the type of parent it is.  Also, this
//  class provides an interface to mask the guide alignment, to deal with the
//  restrictions that may exist in a domain with multiple or non-classical parents.
class CLinkToParent
{
public:

    CLinkToParent(const CGuideAlignment_Base* guideAlignment, CDomain_parent::EParent_type type) {
        m_guideAlignment = guideAlignment;
        m_linkType = (guideAlignment) ? type : CDomain_parent::eParent_type_other;
    }

    virtual ~CLinkToParent() {
        m_maskedRegions.clear();
        delete m_guideAlignment;
        m_guideAlignment = NULL;
    }

    CDomain_parent::EParent_type GetType() { return m_linkType; }

    //  The block model passed *must* be in the coordinates of the guide's master.
    //  No residues implied by the block model are allowed in the masked guide.
    //  Returns false if there's no guide, no overlap or seqIds don't match.
    bool AddMask(const BlockModel& bm);

    //  Simpler version of the above to specify a single block.
    //  The from/to values *must* be in the coordinates of the guide's master.
    //  Returns false if there's no guide, no overlap or (to < from).
    bool AddMask(unsigned int from, unsigned int to);

    bool HasMask() const { return (m_maskedRegions.size() > 0);}

    //  The returned guide is the result after applying all masks.
    //  Caller takes ownership of the returned pointer.
    virtual CGuideAlignment_Base* GetMaskedGuide() const;

    //  The full unmasked guide is returned.
    const CGuideAlignment_Base* GetFullGuide() const { return m_guideAlignment;}

protected:
    //  This is the full guide alignment; any masking is applied on-the-fly to this object.
    const CGuideAlignment_Base* m_guideAlignment;
    CDomain_parent::EParent_type m_linkType;

    //  The block models should be in the coordinates of the guide's master.
    //  Mechanism to constrain the region of the guide alignment.
    vector<BlockModel> m_maskedRegions;

};


//  This class can represent any domain with one or more parents (classical,
//  fusion, etc.).  The goal of this class is to manage the parentage
//  of a CD in one location.  If the class is instantiated with a domain
//  with parents already declared, the class will cache that information
//  in case it becomes necessary to rollback to the original state.
//
//  Changes to the 'ancestors' field of the underlying child CCdCore object
//  are made only through the 'UpdateParentage' method.
//
class CChildDomain
{
public:

    //  If parent id is not of type 'Gid', return empty string.
    static string GetDomainParentAccession(const CDomain_parent& domainParent);

    CChildDomain(CCdCore* cd) : m_childCD(NULL) {
        Initialize(cd);
    }

    CChildDomain(int overlapPercentage) {
    }

    virtual ~CChildDomain() {
        Cleanup();
    }

    //  Get the CD.  Reflects the state of the parents after the
    //  most recent UpdateParentage/ResetAncestors/RollbackAncestors calls.
    //  Any added parent links after that time are not included.
    CCdCore* GetChildCD() { return m_childCD;}

    unsigned int GetNumOriginalAncestors() const { return m_originalAncestors.size();}
    const CCdd::TAncestors& GetOriginalAncestors() const { return m_originalAncestors;}

    //  So that we can construct a guide to an existing *classical* parent, if needed;
    //  all other types of parents should already have a guide declared.
    //  Return true when added; false if 'parent' is not a pre-existing parent of m_childCD.
    bool AddLinkToOriginalAncestor(CLinkToParent* link);

    //  This includes any ancestors added since the last call to UpdateParentage.
    //  It does *not* include any original ancestor relationships.
    unsigned int GetNumAddedAncestors() const { return m_addedAncestors.size();}

    const CCdd::TAncestors& GetAddedAncestors() const { return m_addedAncestors;}

    //  Verifies that the *master* of the guide in the link is the same as m_childDomain
    //  (convention is that in a parent/child type alignment, the child CD is the master);
    //  returns false otherwise.
    //  Note:  this only adds the link to CChildDomain object; a new ancestor is 
    //  NOT added to the underlying CD.  That is done by UpdateParentage, below.
    bool AddParentLink(CLinkToParent* link);

    //  This method modifies the 'ancestors' field of the underlying
    //  CD to reflect the links made.  Any original ancestors (i.e., those
    //  present when object was instantiated) are kept -- even if they'd
    //  been previously removed -- unless keepOriginalParents is set to false.
    //  The CD is returned; if the update fails, NULL is returned.
    CCdCore* UpdateParentage(bool keepOriginalParents);

    //  Merely clears the 'ancestors' field of the underlying CD; 
    //  does not touch any other member variables.
    void ResetAncestors();

    //  Restores the original state of the ancestors to the CD.
    //  Optionally clears out added parent links and associated CDomain_parent objects.
    void RollbackAncestors(bool purgeAddedLinks);

    //  Purges links in m_parentLinks AND those in m_preExistingParents.
    void PurgeAddedLinks();

    //  ***  not implemented yet ***
    //  For the current parentage, perform basic validation checks.
    //  Important:  any links added after the most recent call to
    //  UpdateParentage are ignored, unless doUpdateFirst is true.
    bool IsParentageValid(bool doUpdateFirst = false) const;

protected:


private:

    typedef multimap<string, CLinkToParent*> TPreExistParentMap;
    typedef TPreExistParentMap::iterator TPEPIt;
    typedef TPreExistParentMap::value_type TPEPVT;

    CCdCore* m_childCD;
    vector<CLinkToParent*> m_parentLinks;        //  these are links to newly added ancestors
    TPreExistParentMap m_preExistingParents;  //  needed so can make a guide for a pre-existing parent

    //  This is intended as a cache of the parentage state at instantiation.
    //  To be used if need to rollback, and should never be modified.
    //  TAncestors == list<CRef< CDomain_parent >>
    CCdd::TAncestors m_originalAncestors;

    //  Put new CDomain_parent objects created from links in m_parentLinks here.
    //  Be sure to maintain correspondence between index in m_parentLinks and
    //  the order in the list.  (Want to avoid an extra map data structure to maintain
    //  such a mapping so I can just append m_addedAncestors to the ancestors field
    //  in m_childDomain.)
    CCdd::TAncestors m_addedAncestors;

    void Cleanup();
    void Initialize(CCdCore* cd);
    bool CreateDomainParent(CLinkToParent* link);

    //  Makes guide types consistent with current contingent of established parent relationships.
    //  Unless were multiple parents initially, will need to update guide types 
    //  when additional links are added.  However, we will not be able to add a
    //  guide alignment for pre-existing ancestors - the best we can do is switch
    //  the type in that case.
    //  <does not deal w/ complications regarding deletion/perm parents yet>
    void UpdateAncestorListInChild();
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif    //  CU_CHILD_DOMAIN__HPP
