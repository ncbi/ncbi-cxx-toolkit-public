#ifndef CU_COMPONENT_HPP
#define CU_COMPONENT_HPP

#include <corelib/ncbistd.hpp>
#include <objects/cdd/Domain_parent.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class CCdCore;

enum EParentType {
	PARENT_CLASSIC=0,
	PARENT_FUSION,
	PARENT_DELETION,
	PARENT_PERMUTATION,
	PARENT_OTHER
};


//  Does the CD have parents declared according to CDTree rules??
//  True if:
//  a)  either the 'ancestors' field or the 'parent' field is declared, or
//      neither are set
//     AND
//  b1)  there is a single classical parent, OR
//  b2)  there are component/other parents present and no classical parents.
bool obeysParentTypeConstraints(const CCdCore* pCD);

//  Return the number of times the pChild declares pParent as a parent of specified type.
//  If the parent type is unspecified, counts parents of any type.
//  Zero means that pParent was not declared by pChild as a parent.
int getNumRepeats(const CCdCore* pParent, const CCdCore* pChild, const CDomain_parent::EParent_type* parentType = NULL);

//  Return the repeat number corresponding to the given guide alignment index (zero-based; this implicitly
//  defines the ancestor CD) of the child.
//  If the parent type is unspecified, counts parents of any type.
//  Zero means that the index was out of range or otherwise no parents of specified type.
int getRepeatNumberForGuideIndex(const CCdCore* pChild, int guideAlignmentIndex, const CDomain_parent::EParent_type* parentType = NULL);


//  Return the parent type recorded by the child CD (or eParent_type_other if the child claims there
//  is no component parent/child relation).
//  Since repeats allow a parent to exist > 1 time, returns the type of the first repeat by default.
//  If nThRepeat > 0, return the type of the nThRepeat of the parent in the child's list of
//  parents, where nThRepeat == 1 is the first (equivalent to the default, 0), 2 is the second, etc.
//  If nThRepeat > # occurrences, return 'eParent_type_other' to signal no such parent.
CDomain_parent::EParent_type getParentType(const CCdCore* pParent, const CCdCore* pChild, int nThRepeat = 0);
CDomain_parent::EParent_type getParentTypeByIndex(const CCdCore* pChild, int guideAlignmentIndex);

//  Fill in a vector containing all parent types declared by the child CD, in order in child's list
//  (or an empty vector if there is no parent/child relationships).  
//  Return total number of parents.
int getParentTypes(const CCdCore* pChild, vector<CDomain_parent::EParent_type>& parentTypes);

int algAssignComponentParent(CCdCore * pCD, CCdCore * pNewParentCD,CCdeCore * pOldParentCD,int mergeMethod,int parType,string & lMsg);
//bool translatePendingList2ComponentRelation(CCdCoreCore * pRelCD,CCdCoreCore * pDstCD,string & lMsg);
//bool translateComponentRelation2PendingList(CCdCoreCore * pSrCCdCoreCore, CCdCoreCore * pRelCD,string & lMsg);

//   If returnAll == false, 'allNames' and 'pars' do not include duplicate copies of parents,
//   in the case of repeats.  If returnAll == true, duplicates instances of a parent are included.
//   Note:  classical parents are returned if present!!
int  getComponentParents(const CCdCore * pSrCCdCore, string * allNames, vector < CCdCore* > * pars, bool returnAll = false);
int  getComponentParents(const CCdCore * pSrCCdCore, string * allNames, vector < string > * pars=0, bool returnAll = false);
CCdCore* getComponentParentByIndex(const CCdCore * pSrCCdCore, int guideAlignmentIndex);

//  If a child has declared repeats of parent domain pSrCCdCore, returns duplicate strings in children,
//  and the return value counts total number of times pSrCCdCore is a parent (repeats included).
int  getComponentChildren(CCdCore * pSrCCdCore, string * allNames, vector < CCdCore* > * children);
int  getComponentChildren(CCdCore * pSrCCdCore, string * allNames, vector < string > * children=0);

//   Convenience methods...
bool isClassicalParent(const CCdCore* potentialClassParent, const CCdCore* childCD);
bool isComponentParentType(CDomain_parent::EParent_type type);
bool isComponentParent(const CCdCore* potentialCompParent, const CCdCore* childCD);
bool isComponentChild(const CCdCore* potentialCompChild, const CCdCore* parentCD);
bool hasParentChildRelationship(const CCdCore* potentialParent, const CCdCore* potentialChild);

CDomain_parent * findComponentParent(CCdCore * pSrCCdCore, string acc, int * pCnt=0);
bool algRemoveComponentParent(CCdCore * pCD, CCdCore * pOldParentCD,string & lMsg);

//   Retrieve a copy of the seq_align for the index-th component parent of pCD. 
//   False if a) no such index, b) there is no guide alignment for index,
//   c) CD has a classical parent. 
bool getGuideAlignment(const CCdCore* pCD, int index, CRef< CSeq_align >& seqAlign); 
bool getGuideAlignmentByRepeat(const CCdCore* pParent, const CCdCore* pChild, CRef< CSeq_align >& seqAlign, int nThRepeat = 1); 

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif