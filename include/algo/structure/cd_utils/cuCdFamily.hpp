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
 * Author:  Charlie Liu
 *
 * File Description: Define a CD family in a tree structure.
 *   part of CDTree app
 */

#ifndef CU_CD_FAMILY_HPP
#define CU_CD_FAMILY_HPP
#include <algo/structure/cd_utils/tree_msvc7.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CDNode
{
public:
	CCdCore* cd;
	vector<CCdCore*> comParents;  // possible component parents
	bool selected;

	CDNode(CCdCore* theCd);  // populate the component parent vector
	void addComParent(CCdCore* pcd) {comParents.push_back(pcd);}
	bool operator==(const CDNode& rhs) {return cd == rhs.cd;};
	//cd pointers are not deleted here.  This is important.
	~CDNode(){};
};

typedef tree<CDNode> CDFamilyBase;
typedef CDFamilyBase::iterator CDFamilyIterator;

// define a family hiearchy of CDs 
class NCBI_CDUTILS_EXPORT CDFamily:public CDFamilyBase
{
public:
    CDFamily(CCdCore* rootCD);
	CDFamily();

	CDFamilyIterator setRootCD(CCdCore* rootCD);
	CCdCore* getRootCD() const;
	//operations to build the family tree
	CDFamilyIterator addChild(CCdCore* cd, CCdCore* parentCD); 
	bool removeChild(CCdCore* cd);
	void getChildren(vector<CCdCore*>& cds, CCdCore* parentCD) const;
	void getChildren(vector<CCdCore*>& cds, CDFamilyIterator pit) const;
	void getChildren(vector<CDFamilyIterator>& cdITs, CDFamilyIterator pit)const;
	void getDescendants(vector<CCdCore*>& cds, CCdCore* parentCD) const;
	void getDescendants(set<CCdCore*>& cds, CCdCore* parentCD) const;
	void subfamily(CDFamilyIterator cit, CDFamily*& subfam, bool childrenOnly=false);
    //   Was 'getParent'; component parents are stored in each CDNode and not
    //   represented in the tree structure.  Renamed to emphasize this.
    CCdCore* getClassicalParent(CCdCore* childCD) const;

	void selectCDs(const vector<CCdCore*>& cds);
	void selectAllCDs();
	int  getSelectedCDs(vector<CCdCore*>& cds);

	CDFamilyIterator findCD(CCdCore* cd) const;
    CDFamilyIterator findCDByAccession(CCdCore* cd) const;  //  in case have a different pointer than used to build family
    CDFamilyIterator findCDByAccession(const string& acc) const;  //  get the iterator to the CD w/ this accession

	int getCDCounts() const;
	int getAllCD(vector<CCdCore*>& cds) const;

    //  Return list CDs on the direct (classical) path from initialCD to the root.  
    //  'initialCD' is the first in the path.  Empty list returned if initialCD is null, or not in the family.
    int getPathToRoot(CCdCore* initialCD, vector<CCdCore*>& path) const;

    //  Return list of all CDs in the family not on the direct (classical) path from initialCD to the root.  
    //  Returns all CDs if initialCD is null.
    //  Empty list returned if initialCD is not in the family.
    int getCdsNotOnPathToRoot(CCdCore* initialCD, vector<CCdCore*>& notOnPath) const;

    //  true if 'potentialAncestorCd' can be found by tracing back 
    //  parent links of 'cd'.  While 'NULL' is technically the top of
    //  the tree, return false if either CD is null or cd == potentialAncestorCd.
    bool isDirectAncestor(CCdCore* cd, CCdCore* potentialAncestorCd) const;

    //  true if 'cd' is a direct ancestor of 'potentialDescendantCd',
    //  or alternatively, if potentialDescendantCd is in cd's subtree.
    bool isDescendant(CCdCore* cd, CCdCore* potentialDescendantCd) const;

    //  Return iterator to the deepest common ancestor in the tree for the input cds.
    //  Return 'end' if there is no common ancestor.
    //  If the 'byAccession' flag is true, cds are located in the family 
    //  by accession, otherwise, cds are located by pointer values (i.e., findCDByAccession vs. findCD).
	CDFamilyIterator convergeTo(CCdCore* cd1, CCdCore* cd2, bool byAccession = false)const;
	CDFamilyIterator convergeTo(const set<CCdCore*>& cds, bool byAccession = false)const;
    //  This version also returns the paths from cd1, cd2 to the common CD.
    CDFamilyIterator convergeTo(CCdCore* cd1, CCdCore* cd2, vector<CCdCore*>& path1, vector<CCdCore*>& path2) const;

    //  Output order is that of a pre-ordered depth-first search.
    string getNewickRepresentation() const;
    void getNewickRepresentation(std::ostream& os, const CDFamilyIterator& cursor) const;

    //  sanity check on pointer, root and number of nodes
    static bool IsFamilyValid(const CDFamily* family, string& err);
	void inspect()const;
    virtual ~CDFamily();
	
	static CDFamily* findFamily(CCdCore* cd, vector<CDFamily>& families);
	static bool isDup(CDFamily& one, vector<CDFamily>& all);

	//CDs in cds will be extracted into families; caller assumes ownership of the CDFamily pointers.
	static int createFamilies(vector<CCdCore*>& cds, vector<CDFamily*>& families);

	//CDs in cds will be extracted into families
	//static int createFamilies(vector<CCdCore*>& cds, vector<CDFamily>& families);
	static void extractFamily(CCdCore* parentCD, CDFamily& cdFamily, vector<CCdCore*>& cds);
	static bool findParent(CCdCore* cd, vector<CCdCore*>& cds);
	static bool findChildren(CCdCore* cd, vector<CCdCore*>& cds, set<int>& children);

	
private:
	CCdCore* m_rootCD;
	//CDFamilyIterator m_selectedIterator;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif
