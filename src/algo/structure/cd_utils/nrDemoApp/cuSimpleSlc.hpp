#ifndef CU_SIMPLESLC__HPP
#define CU_SIMPLESLC__HPP

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
*   Implementation of a single-linkage clustering algorithm
*   Produces a tree with a FAKE root.
*   Estimates branch lengths as half node-to-node distance.
*
*/

#include <corelib/ncbistd.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <algo/structure/cd_utils/tree_msvc7.hpp>
#include <algo/structure/cd_utils/cuBaseClusterer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

/////////////////////////
//  Tree definitions...
/////////////////////////

class CUTreeNode
{
	friend ostream &operator<<(ostream& os, const CUTreeNode&);
public:

    static const int NOT_SET;

	CUTreeNode();
	CUTreeNode(const CUTreeNode& rhs);
    CUTreeNode(const string& idDist);  //The expected format of idDist is "id:distance"
	CUTreeNode(int id, CDistBasedClusterer::TDist dist=0.0, CDistBasedClusterer::TDist distToRoot=0.0);
	~CUTreeNode(){};

	bool operator==(const CUTreeNode& rhs) const;

	int id;
	CDistBasedClusterer::TDist distance;
	CDistBasedClusterer::TDist distanceToRoot;
private:
	void init(int id, CDistBasedClusterer::TDist dist, CDistBasedClusterer::TDist distToRoot);
};



typedef tree<CUTreeNode> CUTree;
typedef CUTree::iterator CUTreeIterator;

typedef CDistBasedClusterer::TDist* TSlc_DM ;

class CSimpleSLCAlgorithm {

    static const CDistBasedClusterer::TDist REPLACE_NEG_DIST;
    static const CDistBasedClusterer::TDist INIT_MINIMA;

public:

    typedef CUTree::iterator TCUTIt;
    typedef CUTree::sibling_iterator TCUTChildIt;
    typedef vector< TCUTIt > TCUTItVec;

    CSimpleSLCAlgorithm();

    virtual ~CSimpleSLCAlgorithm();
    virtual void ComputeTree(CUTree* tree, TSlc_DM dm, unsigned int dim);

    //  Convert (i, j) to linearized coordinate index for distance matrix.  Assumes
    //  a linearized DM from the upper triangular section, including the diagonal,
    //  implying i >= j.  If i < j, the underlying DM is assumed to be symmetric and
    //  the index for (j, i) is returned.  If either i or j exceeds dim-1, return kMax_UInt.
    //  Static version also requires a matrix dimension.
    //  **  i, j and index are all zero-based values **
    unsigned int ij2Index(unsigned int i, unsigned int j);
    static unsigned int ij2Index(unsigned int i, unsigned int j, unsigned int dim);
//    virtual string toString();
//    string toString(const TSeqIt& sit);


    //  stringification methods (modified from cuSeqTreeStream code)
    string toString(const CUTree& stree);
    string toNestedString(const CUTree& stree);
    void fromString(const string& str, CUTree& stree);

    //  i/o methods (modified from cuSeqTreeStream code)
    bool writeToFile(std::ofstream&ofs, const CUTree& stree);
    bool write(std::ostream&os, const CUTree& stree, const TCUTIt& cursor);
    bool read(std::istream& is, CUTree& cuTree);
    bool readFromFile(std::ifstream& ifs, CUTree& seqTree);

private:

    static const int USED_ROW;
    static const int BAD_INDEX;


    CUTree*  m_tree;
//    TSlc_DM  m_dm;                  //  input distance matrix  (double**)
    TSlc_DM  m_dm;                  //  *linearized* input distance matrix  (double*)

    unsigned int m_dim;             //  Number of sequences (dimension of underlying matrix)
    int m_nextNode;                 //  Index of next free node in m_seqiters.
    vector< CUTreeNode* > m_items;  //  Contents of a tree node.
    TCUTItVec  m_iters;             //  List of tree iterators pointing to tree nodes,
                                    //  identified using the row id from alignment provided.


    void initializeNodes();
    bool isHub(const TCUTIt& sit);
    bool isInternal(const TCUTIt& sit);
    void Join(int inode1, int inode2, CDistBasedClusterer::TDist len1, CDistBasedClusterer::TDist len2);  

    int  numChildren(const TCUTIt& sit);

    void midpointRoot();
    void MidpointRootCUTree(CUTree& tmpTree, CUTree& newTree);
    bool GetMaxPathCU(const CUTree& atree, CDistBasedClusterer::TDist& dMax, TCUTIt& end1, TCUTIt& end2);
    bool GetMaxPathCU(const TCUTIt& cursor, CDistBasedClusterer::TDist& dMax, CDistBasedClusterer::TDist& dBranch1, TCUTIt& end1, CDistBasedClusterer::TDist& dBranch2, TCUTIt& end2);

    bool isDelimiter(char ch);
    void readToDelimiter(std::istream& is, string& str);

};

//  Hub node is node 0.
inline
bool CSimpleSLCAlgorithm::isHub(const TCUTIt& it) {
    return (it == m_iters[0]);
}

inline
bool CSimpleSLCAlgorithm::isInternal(const TCUTIt& it) {
    return (it.is_valid() && !isHub(it));
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif   /*  CU_SIMPLESLC__HPP  */
