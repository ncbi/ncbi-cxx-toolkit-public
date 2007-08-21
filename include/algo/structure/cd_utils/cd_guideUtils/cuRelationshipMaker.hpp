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
 *          Singleton class that makes parent/child relationships between CDs.
 *
 * ===========================================================================
 */

#ifndef CU_RELATIONSHIP_MAKER__HPP
#define CU_RELATIONSHIP_MAKER__HPP


#include <objects/cdd/Domain_parent.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignment.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuChildDomain.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)


//  This singleton class does the heavy-lifting to compute guide
//  alignments, create new CDs and manage parent/child relationships
//  for CDs.  Where possible, the type of parent needed is inferred
//  although this may not always be possible.
//
//  There are three basic scenarios:
//      a)  parent CD exists; child CD does not exist  (use CreateChild methods)
//      b)  child CD exists; parent CD does not exist  (use CreateParent methods)
//      c)  child and parent CDs both exist                 (use CreateRelationship methods)

class CRelationshipMaker
{
    static const int m_defaultOverlapPercentage;

public:

    enum ErrorCode {
        RELMAKER_SUCCESS = 0,
		RELMAKER_SHARED_SEQUENCE_GUIDE = 1,
		RELMAKER_GUIDE_ALIGNMENT,
		RELMAKER_NO_INTERSECTION_WITH_BLOCK_MODEL,
		RELMAKER_BAD_BLOCK_MODEL_RANGE,
		RELMAKER_LINK_NOT_ADDED,
		RELMAKER_NO_GUIDE_TO_EXISTING_PARENT,
		RELMAKER_UNSPECIFIED_ERROR
    };

    //  Return the single instance, with lazy-initialization.
    static CRelationshipMaker* Get();

    static string AncestorsToString(const CCdCore* cd);

    //  code > 0 refers to an error.  code = 0 refers to success
    static string ResultCodeToString(int code);

    //  If the last call to Create* succeeded, the returned string is empty.
    string GetLastMessage() const { return m_lastErrorMsg;}

    //  It may be necessary to know the entire family in order to make
    //  a guide alignment between the parent and child.
    void SetParentFamily(const ncbi::cd_utils::CDFamily* family) { m_parentFamily = family; }
    const ncbi::cd_utils::CDFamily* GetParentFamily() const { return m_parentFamily; }
    void SetChildFamily(const ncbi::cd_utils::CDFamily* family) { m_childFamily = family; }
    const ncbi::cd_utils::CDFamily* GetChildFamily() const { return m_childFamily; }

    //  Control amount of overlap of a specified range required when making guide.
    //  true if set value is in range [0, 100]; return false if out of range    
    static bool SetOverlapPercentage(int overlapPercentage);  
    static int GetOverlapPercentage() {return m_overlapPercentage;}

    //  Control whether or not the child and parent CDs are required to
    //  have a shared (i.e., overlapping) sequence interval. 
    //  [Implementation note:  this can be ensured by only trying to create an 
    //   instance of CTwoCDGuideAlignment; failure indicates either no shared
    //   interval or the shared interval was too short based on m_overlapPercentage.]
    static void SetForceSharedInterval(bool shareInterval) { m_forceSharedSequenceInterval = shareInterval;}
    static bool GetForceSharedInterval(){return m_forceSharedSequenceInterval;}

    //  When set to false, any existing parents are removed before adding new ancestor links to a child.
    //  Otherwise, they are kept.
    static void SetKeepExistingParents(bool keepExistingParents) { m_keepExistingParents = keepExistingParents;}
    static bool GetKeepExistingParents(){ return m_keepExistingParents;}

    //  Each successful call to Create* will automatically insert a new ancestor unless auto-update 
    //  child is set to 'false'.  In that case, you must manually call UpdateChildParentage() to have
    //  any created relationships added to the child.
    static void SetAutoUpdateChild(bool autoUpdateChild) { m_autoUpdateChild = autoUpdateChild;}
    static bool GetAutoUpdateChild(){ return m_autoUpdateChild;}

    //  This class is responsible for cleaning up the returned pointer; do not delete it yourself.
    //  However, you may force a cleanup by calling Reset.
    CChildDomain* GetChildDomain() { return m_childDomain; }

    //  If not auto-updating child CD, this allows use to manual update child's ancestors.
    void UpdateChildParentage() { if (m_childDomain) m_childDomain->UpdateParentage(m_keepExistingParents);}

    /*
    //  When using CreateChild or CreateParent, a copy of the passed CD is created.
    //  To restrict the rows in the copy, call this method before invoking Create*
    void SetRowsForCDCreation(const set<unsigned int>& rows) { m_rowsInCopy = rows;}
    void ClearRowsForCDCreation() {m_rowsInCopy.clear();}

    //  ** IMPORTANT **
    //  All 'CreateChild' methods clean up any existing child CD, and create a new underlying 
    //  CCdCore object, and put it into m_childDomain.
    //  All 'CreateParent' and 'CreateRelationship' methods install the passed pointer into m_childDomain.
    //  If the underlying CD pointers are not the same, pre-existing m_childDomain is cleaned up first.

    //  Scenario:  child CD does not yet exist.
    //  From the parent-to-be CD, make a copy of the CD but only aligned in the region(s) given.
    //  (First two versions useful only for deletion or classical children; next two versions
    //   intended for combining two existing CDs to create a new fusion model; last version
    //   simply uses the full parent alignment in the child.)
    //  NULL pointer returned on failure.
    CCdCore* CreateChild(CCdCore* parent, unsigned int from, unsigned int to, const string& childAccession, const string& childShortname = "");
    CCdCore* CreateChild(CCdCore* parent, BlockModel& blocksOnParent, const string& childAccession, const string& childShortname = "");
    CCdCore* CreateChild(vector<CCdCore*>& parents, vector<unsigned int>& from, vector<unsigned int>& to, const string& childAccession, const string& childShortname = "");
    CCdCore* CreateChild(vector<CCdCore*>& parents, vector<BlockModel>& blocksOnParents, const string& childAccession, const string& childShortname = "");
    CCdCore* CreateChild(CCdCore* parent, const string& childAccession, string childShortname = "");

    //  Scenario:  parent CD does not yet exist.
    //  From the child-to-be CD, make a copy of the CD to serve as a parent aligned
    //  only over the specified region of the child's alignment.  Last version simply uses
    //  the full span of the child's alignment to define a parent.
    //  NULL pointer returned on failure.
    CCdCore* CreateParent(CCdCore* child, unsigned int from, unsigned int to, const string& parentAccession, const string& parentShortname = "");
    CCdCore* CreateParent(CCdCore* child, BlockModel& blocksOnParent, const string& parentAccession, const string& parentShortname = "");
    CCdCore* CreateParent(CCdCore* child, const string& parentAccession, const string& parentShortname = "");
*/

    //  Scenario:  both parent and child CD exist.
    //  From existing parent and child CDs, create a relationship over the indicated
    //  range of the parent's alignment.  Last version tries to use the full span of the parent's 
    //  alignment to define a relationship to the child.  
    //  On success, return value is 0 and child's ancestors are updated.  An ErrorCode > 0 is returned on failure.
    //  When the 'degenerate' parameter is true, creates a relationship w/o a seqAlign.
    int CreateRelationship(CCdCore* child, CCdCore* parent, unsigned int fromOnParentMaster, unsigned int toOnParentMaster, bool degenerate = false);
    int CreateRelationship(CCdCore* child, CCdCore* parent, const BlockModel* blocksOnParentMaster, bool degenerate = false);
    int CreateRelationship(CCdCore* child, CCdCore* parent, bool degenerate = false);

    //  Cleans up results of any prior calculation.  
    //  You will need to reset the child/parent families after calling this method, and the rows to copy.
    //  Does not modify static member variables.
    void Reset();

private:

    static CRelationshipMaker* m_instance;
    static int m_overlapPercentage;  //  used when making guides
    static bool m_forceSharedSequenceInterval;  //  the child and parent *must* have an overlapping sequence interval
    static bool m_keepExistingParents;
    static bool m_autoUpdateChild;  //  update the parents in the child after every successful link added.

    CChildDomain* m_childDomain;
    const ncbi::cd_utils::CDFamily* m_parentFamily;
    const ncbi::cd_utils::CDFamily* m_childFamily;

    string m_lastErrorMsg;

    //  Defines the rows to use to make a new CD in CreateChild and CreateParent methods.
    //  When empty, all rows will be used.
    set<unsigned int> m_rowsInCopy;

    CRelationshipMaker() { 
        m_childDomain = NULL;
        Reset();
    }
    ~CRelationshipMaker() { Reset();}

    //  For m_childDomain, determine the parent type for the proposed parent CD.  
    //  Returns CDomain_parent::eParent_type_other if can't figure it out or parent is otherwise invalid
    //  (including when m_childDomain hasn't been created via InstallChild).
    CDomain_parent::EParent_type InferRelationshipTypeNeeded(CCdCore* parent, const BlockModel* blocksOnParentMaster);

    //  Creates the m_childDomain object for the 'child' pointer.
    //  When 'child' has a pre-existing parent w/o a guide, it tries to provide one to m_childDomain so it can fill it in.
    void InstallChild(CCdCore* child);

    BlockModel* MakeClippedBlockModelFromMaster(CCdCore* cd, unsigned int from, unsigned int to);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif    //  CU_RELATIONSHIP_MAKER__HPP
