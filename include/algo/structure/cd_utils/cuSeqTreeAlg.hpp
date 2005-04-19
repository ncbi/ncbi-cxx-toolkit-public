#ifndef CU_TREEALG__HPP
#define CU_TREEALG__HPP

/*  $Id$
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
*   Virtual base class for representing sequence-tree calculation algorithms.
*   Implementations of specific algorithms should be subclasses that define,
*   minimally, the ComputeTree method.
*
*/

#include <algo/structure/cd_utils/cuSeqTreeStream.hpp>
#include <algo/structure/cd_utils/cuDistmat.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

const int NUMBER_OF_TREE_ALGORITHMS = 2;
const string TREE_ALGORITHM_NAMES[] = {
		"",
		"Single Linkage Clustering",
		"Neighbor Joining"
		//"Fast Minimum Evolution"
};

enum ETreeMethod {
	eNoTreeMethod=0,
	eSLC=1,
	eNJ=2
	//eME=3
};  //  add name to array above and increment # algorithms
const ETreeMethod GLOBAL_DEFAULT_ALGORITHM = eSLC;

string GetTreeAlgorithmName(ETreeMethod algorithm);
	

const double RESET_WITH_TINY_DISTANCE = 0.0001;  //  used to reset zero-distance leafs in MPRooting
void MidpointRootSeqTree(const SeqTree& oldTree, SeqTree& newTree);
//SeqTree MidpointRootSeqTree(const SeqTree& oldTree);
bool GetMaxPath(const SeqTree& atree, double& dMax, SeqTree::iterator& end1, SeqTree::iterator& end2);
bool GetMaxPath(const SeqTree::iterator& cursor, double& dMax, double& dBranch1, SeqTree::iterator& end1, double& dBranch2, SeqTree::iterator& end2);


class TreeAlgorithm {

public:

    enum Rootedness {
        eUnrooted=0,
        eRooted=1,
		eMidpointRooted=2
    };

private:

    static const string NO_NAME;
    static const Rootedness DEF_ROOTED;

public:

    typedef SeqTree::iterator TSeqIt;
    typedef SeqTree::sibling_iterator TChildIt;
    typedef vector< TSeqIt > TTreeIt;
/*
    TreeAlgorithm(string name=NO_NAME) : m_tree(NULL), m_tmethod(eNone) {
        m_rootedness = DEF_ROOTED;
    }
*/
    TreeAlgorithm(Rootedness rooted = DEF_ROOTED, ETreeMethod tmethod = eNoTreeMethod) 
        : m_tree(NULL), m_rootedness(rooted), m_tmethod(tmethod) {
    }


    const SeqTree* GetTree() const {
        return m_tree;
    }
/*
    void SetName(string name) {
        m_name = name;
    }
    const string GetName() const {
        return m_name;
    }
	*/

	string GetName() {
		return TREE_ALGORITHM_NAMES[m_tmethod];
	}
	ETreeMethod GetTreeMethod() {
		return m_tmethod;
	}

    void SetRootedness(Rootedness rooted) {
        m_rootedness = rooted;
    }
    Rootedness GetRootedness() {
        return m_rootedness;
    }
    bool isRooted() {
        bool r = (m_rootedness == eUnrooted) ? false : true;
        return r;
    }

	void SetMidpointRooting(bool rooting) {
		if (rooting) {
			m_rootedness = eMidpointRooted;
		} else if (m_rootedness == eMidpointRooted) {
			m_rootedness = DEF_ROOTED;
		}
	}
	bool useMidpointRooting() {
		return (m_rootedness == eMidpointRooted);
	}
			

    virtual ~TreeAlgorithm(){}
    virtual long GetNumLoopsForTreeCalc() = 0;
    virtual void ComputeTree(SeqTree* tree, pProgressFunction pFunc) = 0;
    virtual string toString(){return kEmptyStr;}

    //virtual void SetCDD(CCd* cdd){};
    virtual void SetDistMat(DistanceMatrix* dm){};

	//void fillSeqNames(SeqTree* treeData, CCd* pCd);

    const DistanceMatrix* GetDistMat() const {
        return m_dm;
    }

protected:

    SeqTree*   m_tree;   

    DistanceMatrix* m_dm;     //  Distance matrix used to compute the tree
    //CCd*            m_cdd;    //  Optionally defined CD for which this tree is generated.

//    string      m_name;
    Rootedness  m_rootedness;
	ETreeMethod m_tmethod;

	void midpointRootIfNeeded();
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */

#endif  /* CU_TREEALG__HPP */

