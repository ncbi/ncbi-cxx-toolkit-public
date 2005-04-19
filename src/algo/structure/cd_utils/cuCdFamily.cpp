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
 * File Description: Implement cdt_cd_family.hpp.
 *   part of CDTree app
 */
#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cuCdFamily.hpp>
#include <algorithm>

//using namespace std;
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


CDNode::CDNode(CCdCore* theCd) : cd(theCd), comParents(), selected(true) {
}

CDFamily::CDFamily(CCdCore* rootCD) :
   CDFamilyBase()
{
	   setRootCD(rootCD);
}


CDFamily::CDFamily() : 
	m_rootCD(0), CDFamilyBase()
{
}

CDFamilyIterator CDFamily::setRootCD(CCdCore* rootCD)
{
	if (!rootCD)
		return end();
	if (begin() != end())  //can't add root if there is already one  
		return end();
	m_rootCD = rootCD;
	return insert(begin(), m_rootCD);
}

CCdCore* CDFamily::getRootCD() const
{
	return m_rootCD;
}


//operations to build the family tree
CDFamilyIterator CDFamily::addChild(CCdCore* cd, CCdCore* parentCD)
{
	CDFamilyIterator pit;
	if (parentCD == 0)
		pit = end();
	else
		pit = findCD(parentCD);
	if (pit == end())
		return pit;
    else 
		return append_child(pit, CDNode(cd));
}

bool CDFamily::removeChild(CCdCore* cd)
{
	CDFamilyIterator cDIterator = findCD(cd);
	if (cDIterator == end())
		return false;
	else
	{
		erase(cDIterator);
		return true;
	}
}

//operations to navigate the family tree
CDFamilyIterator CDFamily::findCD(CCdCore* cd) const
{
	return find(begin(), end(), CDNode(cd));
}

//parent=0, get from root
void CDFamily::getChildren(vector<CCdCore*>& cds, CCdCore* parentCD) const
{
	CDFamilyIterator pit;
	if (parentCD == 0)
		pit = begin();
	else
		pit = findCD(parentCD);
	getChildren(cds, pit);
}

void CDFamily::getChildren(vector<CCdCore*>& cds, CDFamilyIterator pit) const
{
	if (pit == end())
		return;
	CDFamily::sibling_iterator sit = pit.begin();
	while (sit != pit.end())
	{
		cds.push_back(sit->cd);
		++sit;
	}
}

void CDFamily::getChildren(vector<CDFamilyIterator>& cdITs, CDFamilyIterator pit) const
{
	if (pit == end())
		return;
	CDFamily::sibling_iterator sit = pit.begin();
	while (sit != pit.end())
	{
		//can be a problem because push_back() takes a reference
		cdITs.push_back(CDFamilyIterator(sit));
		++sit;
	}
}

void CDFamily::getDescendants(vector<CCdCore*>& cds, CCdCore* parentCd) const {
    vector<CCdCore*> tmpCds;
	getChildren(tmpCds, parentCd);
    for (int i = 0; i < tmpCds.size(); ++i) {
        cds.push_back(tmpCds[i]);
        getDescendants(cds, tmpCds[i]);
    }
}

void CDFamily::getDescendants(set<CCdCore*>& cds, CCdCore* parentCd) const {
    vector<CCdCore*> tmpCds;
    cds.clear();
	getDescendants(tmpCds, parentCd);
    for (int i = 0; i < tmpCds.size(); ++i) {
        cds.insert(tmpCds[i]);
    }
}


CCdCore* CDFamily::getClassicalParent(CCdCore* childCD) const
{
	CDFamilyIterator it = findCD(childCD);
	if (it == end())
		return 0;
	CDFamilyIterator pit = parent(it);
	if (pit.node)
		return pit->cd;
	else
		return 0;
}




void CDFamily::selectCDs(const vector< CCdCore* > & cds) 
{
	for (CDFamilyIterator fit = begin(); fit != end(); ++fit)
	{
		if (find(cds.begin(), cds.end(), fit->cd) != cds.end())
			fit->selected = true;
		else
			fit->selected = false;
	}
}

void CDFamily::selectAllCDs() 
{
	for (CDFamilyIterator fit = begin(); fit != end(); ++fit)
	{
		fit->selected = true;
	}
}

int CDFamily::getSelectedCDs(vector<CCdCore*>& cds)
{
	for (CDFamilyIterator fit = begin(); fit != end(); ++fit)
	{
		if (fit->selected)
			cds.push_back(fit->cd);
	}
	return cds.size();
}


int CDFamily::getCDCounts() const
{
	return size();
}

int CDFamily::getAllCD(vector<CCdCore*>& cds) const
{
	CDFamilyIterator cit = begin();
	for (; cit != end(); ++cit)
		cds.push_back(cit->cd);
	return cds.size();
}

//  Return list CDs on the direct (classical) path from initialCD to the root.  
//  'initialCD' is the first in the path.
int CDFamily::getPathToRoot(CCdCore* initialCD, vector<CCdCore*>& path) const {

    CCdCore* currentCd;
    CCdCore* parentCd   = initialCD;

    path.clear();

    //  Make sure initialCD is in the family.
    if (findCD(initialCD) != end()) {
        while (parentCd) {
            currentCd = parentCd;
            path.push_back(currentCd);
            parentCd = getClassicalParent(currentCd);
        }
    }

    return path.size();
}

//  Return list of all CDs in the family not on the direct (classical) path from initialCD to the root.  
//  Returns all CDs if initialCD is null.
int CDFamily::getCdsNotOnPathToRoot(CCdCore* initialCD, vector<CCdCore*>& notOnPath) const {

    notOnPath.clear();

    //  Make sure initialCD is in the family.
    if (findCD(initialCD) != end()) {
        for (CDFamilyIterator cit = begin(); cit != end(); ++cit) {
            if (initialCD) {
                if (cit->cd != initialCD && !isDirectAncestor(initialCD, cit->cd)) {
                    notOnPath.push_back(cit->cd);
                }
            } else {
                notOnPath.push_back(cit->cd);
            }
        }
    }
	return notOnPath.size();
}


bool CDFamily::isDirectAncestor(CCdCore* cd, CCdCore* potentialAncestorCd) const {

    bool isAncestor = false;
    CCdCore* parent;
    if (cd && potentialAncestorCd && cd != potentialAncestorCd) {
        if (potentialAncestorCd == getRootCD()) {  // root is everyone's ancestor
            isAncestor = true;
        } else {
            parent = getClassicalParent(cd);
            while (parent && !isAncestor) {
                if (parent == potentialAncestorCd) {
                    isAncestor = true;
                } else {
                    parent = getClassicalParent(parent);
                }
            }
        }
    }
    return isAncestor;
}

bool CDFamily::isDescendant(CCdCore* cd, CCdCore* potentialDescendantCd) const {

    return isDirectAncestor(potentialDescendantCd, cd);
}
    
    
bool CDFamily::IsFamilyValid(const CDFamily* family, string& err) {
    bool hasError = false;
    if (!family) {
        err.append("Null CDFamily Object.\n");
        hasError = true;
    }
    if (family->getRootCD() == NULL) {
        err.append("CDFamily Object Has No Root.\n");
        hasError = true;
    }
    if (family->getCDCounts() <= 0) {
        err.append("CDFamily Object With No CDs.\n");
        hasError = true;
    }
    return (!hasError);
}

void CDFamily::inspect()const
{
	CDFamilyIterator fit = begin();
	for(; fit != end(); ++fit)
	{
		string acc;
		acc = fit->cd->GetAccession();
	}
}

CDFamily::~CDFamily()
{
}

/*
int CDFamily::getFamilies(vector<CDFamily>& families, vector<CCdCore*>& cds, bool findChildren)
{
	families.clear();
	if (findChildren)	
		retrieveFamilies(cds, families);
	else
	{
		vector<CCdCore*> tmp(cds);
		createFamilies(tmp, families);
	}
	return families.size();
}*/

//


bool CDFamily::isDup(CDFamily& one, vector<CDFamily>& all)
{
	int occurrence = 0;
	CCdCore* cd = one.getRootCD();
	for (int i = 0; i < all.size(); i++)
	{
		if (all[i].findCD(cd) != all[i].end())
			occurrence++;
	}
	return occurrence > 1;  // 1 = find self only, not a dup
}

CDFamily* CDFamily::findFamily(CCdCore* cd, vector<CDFamily>& families)
{
	for (int i = 0; i < families.size(); i++)
	{
		if (families[i].findCD(cd) != families[i].end())
			return &(families[i]);
	}
	return 0;
}

//includeChildren = false
int CDFamily::createFamilies(vector<CCdCore*>& cds, vector<CDFamily>& families)
{
	vector<CCdCore*>::iterator cdIterator = cds.begin();
	while(cdIterator != cds.end())
	{
		CCdCore* cd = *cdIterator;
		if (!findParent(cd, cds))  //no parent, then use this cd as a root of family
		{
			CDFamily* cdFamily = new CDFamily(cd);
			cds.erase(cdIterator);
			extractFamily(cd, *cdFamily, cds);
			families.push_back(*cdFamily);
			cdIterator = cds.begin();
		}
		else
			++cdIterator;
	}
	return families.size(); //all cds should be added
}

void CDFamily::extractFamily(CCdCore* parentCD, CDFamily& cdFamily, vector<CCdCore*>& cds)
{
	set<int> children;
	//parentCD= cdFamily.getRootCD();
	if (findChildren(parentCD, cds, children))
	{
		//add children
		for (set<int>::iterator sit = children.begin(); sit != children.end(); ++sit)
		{
			cdFamily.addChild(cds[*sit], parentCD);
		}
		//remove added children from cds
		vector<CCdCore*> tmp(cds);
		cds.clear();
		for (int i = 0; i < tmp.size(); i++)
		{
			if (children.find(i) == children.end())
			{
				cds.push_back(tmp[i]);
			}
		}
		// extract the subfamily for each children
		for (set<int>::iterator sit = children.begin(); sit != children.end(); ++sit)
		{
			extractFamily(tmp[*sit], cdFamily, cds);
		}
	}
}

bool CDFamily::findParent(CCdCore* cd, vector<CCdCore*>& cds)
{
	string acc = cd->GetClassicalParentAccession();
	for (int i = 0; i < cds.size(); i++)
	{
		if (cds[i] != cd)
			if (acc.compare(cds[i]->GetAccession()) == 0)
				return true;
	}
	return false;
}

bool CDFamily::findChildren(CCdCore* cd, vector<CCdCore*>& cds, set<int>& children)
{
	string acc = cd->GetAccession();
	for (int i = 0; i < cds.size(); i++)
	{
		if (cds[i] != cd)
			if (acc.compare(cds[i]->GetClassicalParentAccession()) == 0)
				children.insert(i);
	}
	return children.size() > 0;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */

