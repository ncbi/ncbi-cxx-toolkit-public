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
* File Description:  treealg.cpp
*
*   Implementation of a single-linkage clustering algorithm.
*   Produces a tree with a FAKE root.
*   Estimates branch lengths as half node-to-node distance.
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include "cuSimpleSlc.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


const int CUTreeNode::NOT_SET = -2;

CUTreeNode::CUTreeNode()
{
	init(NOT_SET, 0.0, 0.0);
}

//  The expected format of idDist is "id:distance"; set id = NOT_SET if problem parsing that field,
//  set distance = 0 if problem parsing that field.
CUTreeNode::CUTreeNode(const string& idDist)
{
	init(NOT_SET, 0.0, 0.0);

	//get delimiter position
	int colonPos = idDist.find(':');

	if (colonPos >= 0) //found name
	{
        try {
            //  Nothing before the colon; no id.
            if (colonPos > 0) {
    		    id = atoi(idDist.substr(0, colonPos).c_str());
            }
        } catch (...) {
        }
        try {
    		distance = atof(idDist.substr(colonPos+1).c_str());
        } catch (...) {
        }
	}
}

CUTreeNode::CUTreeNode(int id, CDistBasedClusterer::TDist dist, CDistBasedClusterer::TDist distToRoot)
{
	init(id, dist, distToRoot);
}

CUTreeNode::CUTreeNode(const CUTreeNode& rhs) {
    init(rhs.id, rhs.distance, rhs.distanceToRoot);
}

void CUTreeNode::init(int id, CDistBasedClusterer::TDist dist, CDistBasedClusterer::TDist distToRoot)
{
	this->id = id;
	this->distance = dist;
	this->distanceToRoot = distToRoot;
}


ostream &operator<<(ostream& os, const CUTreeNode& cuti) {

    os << "  ID: " << cuti.id << " Distance = " << cuti.distance;
    if (cuti.distanceToRoot != 0) os << "    Dist to Root = " << cuti.distanceToRoot;
    os << std::endl;
    return os;
}

bool CUTreeNode::operator==(const CUTreeNode& rhs) const {
    return (id == rhs.id && distance == rhs.distance && distanceToRoot == rhs.distanceToRoot);
}


const int CSimpleSLCAlgorithm::USED_ROW = -1;
const int CSimpleSLCAlgorithm::BAD_INDEX = -1;
const CDistBasedClusterer::TDist CSimpleSLCAlgorithm::REPLACE_NEG_DIST = 0.001;
const CDistBasedClusterer::TDist CSimpleSLCAlgorithm::INIT_MINIMA  = kMax_Double;


CSimpleSLCAlgorithm::CSimpleSLCAlgorithm() : m_dm(NULL), m_dim(0), m_nextNode(0) {
}

//  Initialize a list of nodes that will be placed in a tree structure.
//  (For a rooted tree with N leafs, there are 2N-1 total nodes, of which
//   N-2 are internal and one is the root.)
//  The node will be identified by its one-based row indices in the distance matrix
//  (assumed to correspond to rows an accompanying alignment).  Nodes with 'rowID'
//  > m_dim serve to hold composite nodes created during the SLC procedure.
//
//  The root node will have length 0 and often called the 'hub' node.
//
//  NOTE:  If the distance matrix row does not map directly to the row indices of an
//  alignment, the caller is responsible for re-mapping rowID to the correct
//  alignment row number.
//
//  NOTE:  Names are initially assigned to be the row ID as a string.

void CSimpleSLCAlgorithm::initializeNodes() {

    unsigned int nnodes;
    CUTreeNode* sip;

    for (unsigned int i=0; i < m_items.size(); ++i) {
        if (m_items[i]) {
            delete m_items[i];
        }
    }

    m_iters.clear();
    m_items.clear();
    m_nextNode = 0;

    nnodes = 2*m_dim - 1;
    m_iters = TCUTItVec(nnodes);

    for (unsigned int i=0; i<nnodes; ++i) {
        sip = new CUTreeNode((int)i);
        m_items.push_back(sip);
    }
}

CSimpleSLCAlgorithm::~CSimpleSLCAlgorithm() {

    m_iters.clear();
    for (unsigned int i=0; i<(2*m_dim - 1); ++i) {
        delete m_items[i];
    }
    m_items.clear();
}



unsigned int CSimpleSLCAlgorithm::ij2Index(unsigned int i, unsigned int j) {
    return ij2Index(i, j, m_dim);
}

unsigned int CSimpleSLCAlgorithm::ij2Index(unsigned int i, unsigned int j, unsigned int dim) {
    unsigned int result = kMax_UInt;
    unsigned int ii = (i >= j) ? i : j;
    unsigned int jj = (i >= j) ? j : i;
    if (ii < dim && jj < dim) {
        result = (jj*(2*dim - jj - 1))/2 + ii;
    }
    _ASSERT(result <= dim*(dim+1)/2);
    return result;
}

//  Main computational engine for the SLC method

void CSimpleSLCAlgorithm::ComputeTree(CUTree* atree, TSlc_DM dm, unsigned int dim) {

    unsigned int i, j, k, k1, k2;
    unsigned int imin, jmin;
    int idmin, jdmin;
    CDistBasedClusterer::TDist ilen, jlen;

    CDistBasedClusterer::TDist tmpDist1, tmpDist2, minval;

    if (!atree || dim == 0 || !dm) {
        if (atree) delete atree;
        atree = NULL;
        m_tree = NULL;
        m_dim = 0;
        m_dm = NULL;
        return;
    }

    m_tree = atree;
    m_dim = dim;
    m_dm  = dm;
    initializeNodes();


//    string newickStr = "";
/*
#if _DEBUG
    ofstream ofs;
	ofs.open("slc_info.txt");
#endif
  */

    m_iters[0] = m_tree->insert(m_tree->begin(), *m_items[0]);

    //  For composite nodes, save the 1/2 dist. between the two component nodes.
    //  When this node becomes part of a composite node in a later iteration, that
    //  distance is subtracted from the orig-composite-node to hub distance.

    CDistBasedClusterer::TDist* internalDistCorrection = new CDistBasedClusterer::TDist[m_dim];

    //  Map rows in distance matrix to index of corresponding node in m_iters;
    //  when there is no corresponding distance matrix row, enter USED_ROW.

    int* indexMap = new int[m_dim];

    m_nextNode = m_dim + 1;

    if (indexMap == 0 || internalDistCorrection == 0) {
        m_tree->clear();
        return;
    } else {
        for (i=0; i<m_dim; ++i) {
            indexMap[i] = i+1;
            internalDistCorrection[i] = 0.0;
        }
    }

    /*
    // speeding up tree-calc, pre-fetch all distances
    CDistBasedClusterer::TDist** ppDists;
    ppDists = new CDistBasedClusterer::TDist*[m_dim];
    for (i=0; i<m_dim; i++) {
        ppDists[i] = new CDistBasedClusterer::TDist[m_dim];
    }
    for (i=0; i<m_dim; i++) {
        for (j=0; j<m_dim; j++) {
            ppDists[i][j] = m_dm->FastGet(i, j);
        }
    }
    */

    //  Iterations loop
    for (unsigned int it=1; it<=m_dim-1; ++it) {

        minval = INIT_MINIMA;
        i = 0;
        j = 0;

        //  Find minimum distance in active part of the matrix
        for (k = 0; k < m_dim*(m_dim + 1)/2; ++k) {

            _ASSERT(j < m_dim && i < m_dim);

            if (indexMap[i] != USED_ROW && indexMap[j] != USED_ROW) {
                if (m_dm[k] < minval && i != j) {  //  don't want to look at the diagonals, which should be zero
                    minval = m_dm[k];
                    imin = i;
                    jmin = j;
                }
            }

            ++j;
            if (j == m_dim && i < m_dim - 1) {
                ++i;
                j = i;
            }
        }
/*
        for (j=1; j<m_dim; ++j) {
            if (indexMap[j] != USED_ROW) {
                for (i=0; i<j; ++i) {
                    if (indexMap[i] != USED_ROW) {
                        if (m_dm[j][i] < minval) {
                            minval = m_dm[j][i];
//                        if (m_dm[i*m_dim + j] < minval) {
//                            minval = m_dm[i*m_dim + j];
                            imin = i;
                            jmin = j;
                        }
                    }
                }
            }
        }
*/
        _ASSERT(imin < m_dim && jmin < m_dim);
        idmin = indexMap[imin];
        jdmin = indexMap[jmin];

//         if (idmin == USED_ROW || jdmin == USED_ROW) {
//             ERR_POST(ncbi::Error << "Indexing Error:  " << idmin << " or " << jdmin << " is not a valid row index "
//                  << " for DM indices " << imin << " and " << jmin << ", respectively.\n\n");
//             break;
//         }

        //  Assign length of new branches to be 1/2 m_dm(imin, jmin)

        tmpDist1 = m_dm[ij2Index(imin, jmin)];
        ilen = 0.5*tmpDist1 - internalDistCorrection[imin];
        jlen = 0.5*tmpDist1 - internalDistCorrection[jmin];
//        ilen = 0.5*m_dm[imin][jmin] - internalDistCorrection[imin];
//        jlen = 0.5*m_dm[imin][jmin] - internalDistCorrection[jmin];
/*
#if _DEBUG
        if (1) {
            ERR_POST(ncbi::Trace << "\nIteration " << it << ":  min distance = " << minval << endl);
            ERR_POST(ncbi::Trace << "Rows :    " << imin << "(node " << idmin << "; length= " << ilen
                 << ") and " << jmin << "(node " << jdmin << "; length= " << jlen << ")." << endl);
            ERR_POST(ncbi::Trace << " dist corrs:  " << internalDistCorrection[imin]
                 << " " << internalDistCorrection[jmin] << endl);
        }
#endif
*/
//  Connect nodes, creating them as needed.

        ilen = (ilen > 0) ? ilen : REPLACE_NEG_DIST;
        jlen = (jlen > 0) ? jlen : REPLACE_NEG_DIST;

        Join(idmin, jdmin, ilen, jlen);

//        internalDistCorrection[imin] = 0.5*m_dm[imin][jmin];
        internalDistCorrection[imin] = 0.5*m_dm[ij2Index(imin, jmin)];
        indexMap[imin] = m_nextNode++;
        indexMap[jmin] = USED_ROW;

        //  Fix-up distance matrix:  row jmin is zeroed out, imin row becomes m_dm(imin, jmin)
        for (k=0; k<m_dim; ++k) {
            if (indexMap[k] != USED_ROW) {
                k1 = ij2Index(k, imin);
                k2 = ij2Index(k, jmin);
                tmpDist1 = m_dm[k1];
                tmpDist2 = m_dm[k2];
                m_dm[k1] = (tmpDist1 < tmpDist2) ? tmpDist1 : tmpDist2;
                m_dm[k2] = 0.0;
//                m_dm[k][imin] = (m_dm[k][imin] < m_dm[k][jmin]) ? m_dm[k][imin] : m_dm[k][jmin];
//                m_dm[k][jmin] = 0.0;
//                m_dm[jmin][k] = 0.0;
//                m_dm[imin][k] = m_dm[k][imin];
            }
        }
        m_dm[ij2Index(imin, imin)] = 0.0;
//        m_dm[imin][imin] = 0.0;

    }  //  end iteration loop

    //  Need to deal with final node?  (should all be hooked to the hub node)
/*#if _DEBUG
	ofs.close();
#endif
*/
	midpointRoot();

    delete [] indexMap;
    delete [] internalDistCorrection;
}

void CSimpleSLCAlgorithm::Join(int idmin, int jdmin, CDistBasedClusterer::TDist ilen, CDistBasedClusterer::TDist jlen) {

    CDistBasedClusterer::TDist tmpd  = ilen;
    int maxIndex = 2*m_dim - 2;  //  tot. # nodes (less the hub)

    if (idmin == jdmin) {
        ERR_POST(ncbi::Error << "Error:  You cannot join node " << idmin << " to itself.\n");
        return;
    }

    if (idmin < 0 || idmin > maxIndex || jdmin < 0 || jdmin > maxIndex || m_nextNode < 0 || m_nextNode > maxIndex+1) {
        ERR_POST(ncbi::Error << "Error:  Out of range index in Join:  " << idmin << " " << jdmin << " " << m_nextNode << "  Max allowed index:  " << maxIndex << ncbi::Endl());
        return;
    }

    TCUTChildIt cit;
    TCUTIt& hub = m_iters[0];

    //  New composite node (unless we're at the final two nodes)
    if (m_nextNode <= maxIndex) {
        m_iters[m_nextNode] = m_tree->append_child(hub, *m_items[m_nextNode]);
    }

    int tmpi = idmin;
    while (tmpi == idmin || tmpi == jdmin) {
        cit = m_iters[tmpi];
        if (m_nextNode <= maxIndex) {   //  unless the last two nodes...
            if (cit.is_valid()) {
                if (m_tree->parent(cit) == hub) {
                    cit->distance = tmpd;
                    TCUTChildIt nexit=cit;++nexit;
                    m_tree->reparent(m_iters[m_nextNode], cit, nexit);  // don't use ++ as it modifies cit
                } else {   //  this case should not occur
                    tmpi = -(idmin+jdmin);
                    ERR_POST(ncbi::Error << "Error:  iterator found (id= " << cit->id << ") not attached to hub.\n");
                }
            } else {
                m_items[tmpi]->distance = tmpd;
                m_iters[tmpi] = m_tree->append_child(m_iters[m_nextNode], *m_items[tmpi]);
            }
        } else {
            if (cit.is_valid()) {
                if (m_tree->parent(cit) == hub) {
                    cit->distance = tmpd;
                } else {   //  this case should not occur
                    tmpi = -(idmin+jdmin);
                    ERR_POST(ncbi::Info << "Error:  iterator found (id= " << cit->id << ") not attached to hub for last nodes.\n");
                }
            } else {
                m_items[tmpi]->distance = tmpd;
                m_iters[tmpi] = m_tree->append_child(m_iters[0], *m_items[tmpi]);
            }
        }
        tmpd = jlen;
        tmpi = (tmpi==idmin) ? jdmin : -(idmin+jdmin);
    }
}

int CSimpleSLCAlgorithm::numChildren(const TCUTIt& sit) {

    int n = 0;
    if (sit.is_valid()) {
        n = m_tree->number_of_children(sit);
    }
    return n;

}


/*=======================================*/
/*    Midpoint-root related functions    */
/*=======================================*/

void CSimpleSLCAlgorithm::midpointRoot() {

    static const CDistBasedClusterer::TDist RESET_WITH_TINY_DISTANCE = 0.0001;

    if (m_tree) {

		//  In case any nodes have zero distance (e.g., if there are identical sequences)
        //  reset distance to some small value.
        //  (midpoint root can become confused in such cases...)
		for (TCUTIt stit = m_tree->begin(); stit != m_tree->end(); ++stit) {
			if (stit->distance < RESET_WITH_TINY_DISTANCE && m_tree->parent(stit).is_valid()) {
				stit->distance = RESET_WITH_TINY_DISTANCE;
			}
		}

		CUTree tmpTree = *m_tree;
		MidpointRootCUTree(tmpTree, *m_tree);
		if (m_tree->size() == 0) {
		    *m_tree = tmpTree;
		}
    }
}

void CSimpleSLCAlgorithm::MidpointRootCUTree(CUTree& tmpTree, CUTree& newTree) {

	bool result = true;
    bool found  = false;
	CDistBasedClusterer::TDist midpointDist = 0.0;
	CDistBasedClusterer::TDist dToEnd1 = 0.0;
	CDistBasedClusterer::TDist dmax = 0.0;
	CDistBasedClusterer::TDist dToNewParent, dToNewChild;
    int nTopLevelNodes;

    CUTreeNode item;
	TCUTIt end1, end2;
	TCUTIt cursor1, cursor2, newTreeRoot, newTreeCursor;
    TCUTChildIt  tmpSibling, tmpTreeCursor;

	if (! &tmpTree) {
		return;
	}
	newTree.clear();

	if (GetMaxPathCU(tmpTree, dmax, end1, end2)) {

        //  Find where midpoint is in the tree; insert a new node there which will
        //  be the new root node.  By definition, the longer path is to 'end1'.

		midpointDist = dmax/2.0;
        cursor1 = end1;
		while (cursor1.is_valid() && dToEnd1 < midpointDist) {// != tmpTree.begin()) {
            dToEnd1 += cursor1->distance;
            cursor2 = cursor1;
            cursor1 = tmpTree.parent(cursor2);
        }

        //  Don't bother if the new root would be either between old root and
        //  the head of the tree or between some node and the old root.
        if (!cursor1.is_valid() || !tmpTree.parent(cursor1).is_valid()) {
        	newTree.clear();
            newTree = tmpTree;
            return;
        }

        item = CUTreeNode(-1, dToEnd1-midpointDist);
        if (cursor1.is_valid()) {  //  midpoint between two nodes
            cursor2->distance -= item.distance;
            if (dToEnd1 == midpointDist) { //midpoint lands on existing (non-head) node
                newTreeRoot = cursor1;
                cursor1     = tmpTree.parent(newTreeRoot);
            } else {
                newTreeRoot = tmpTree.append_child(cursor1, item);
                tmpTree.reparent(newTreeRoot, cursor2, tmpTree.next_sibling(cursor2));
            }

            //  Starting at newTreeRoot, adjust distances in CUTreeNodes on path
            //  to the head of tmpTree.

            cursor2 = newTreeRoot;
            dToNewParent = cursor2->distance;
            cursor2->distance = 0.0;
            while (cursor1.is_valid()) {
                tmpSibling = tmpTree.previous_sibling(cursor2);
                if (!tmpSibling.is_valid()) {
                    tmpSibling = tmpTree.next_sibling(cursor2);
                }
                dToNewChild  = cursor1->distance;   // dist to current parent (which will become the new child...)
                cursor1->distance = dToNewParent;
                dToNewParent = dToNewChild;
                cursor2 = cursor1;
                cursor1 = tmpTree.parent(cursor2);
            }

            //  If old root will not be in new tree, adjust distance on its children
            if (tmpTree.number_of_children(cursor2) == 2) {
                tmpTreeCursor = cursor2.begin();
                while (tmpTreeCursor != tmpSibling) {
                    ++tmpTreeCursor;
                }
                tmpTreeCursor->distance += cursor2->distance;
            }

        }

        //  Now deal with any other top-level nodes (cursor2 is attached to 'head')
        nTopLevelNodes = tmpTree.number_of_siblings(tmpTree.begin());
        if (nTopLevelNodes == 2) {  //  sibling will attach directly to cursor2
            if (tmpTree.next_sibling(cursor2) != tmpTree.end()) {
                cursor1 = tmpTree.next_sibling(cursor2);
            } else if (tmpTree.previous_sibling(cursor2) != tmpTree.end()) {
                cursor1 = tmpTree.previous_sibling(cursor2);
            } else {
                cursor1 = tmpTree.end();
            }
            dToNewChild = 0.0;
            cursor1->distance += dToNewParent;
        } else         if (nTopLevelNodes > 2) {  // an intermediate node will be inserted as a child of cursor2 below
            item = *cursor2;
            dToNewChild = cursor2->distance;
        }

    	newTree.clear();
        newTree.operator=(tmpTree.reroot(newTreeRoot));

        //  Fix up the distance of internal node corresp. to head of old tree if needed.
        if (nTopLevelNodes > 2) {

            CUTreeNode nullItem = CUTreeNode();
            TCUTChildIt sit;
            int count = 0;
            newTreeCursor = newTree.begin();
            while (!found && newTreeCursor != newTree.end()) {
                if (*newTreeCursor == item) {
                    sit = newTreeCursor.begin();
                    while (sit != newTreeCursor.end()) {
                        if (*sit == nullItem) {
                            sit->distance = dToNewParent;
                            ++count;
                        }
                        ++sit;
                    }
                    found = (count == 1) ? true : false;
                }
                ++newTreeCursor;
            }
            if (!found) {  //  the empty former-head node was not found.
				result = false;
            }
        }
	} else {
		result = false;
	}

	if (!result) {
		newTree.clear();
	}
}


bool CSimpleSLCAlgorithm::GetMaxPathCU(const CUTree& atree, CDistBasedClusterer::TDist& dMax, TCUTIt& end1, TCUTIt& end2) {

    bool result = true;
    int foundCount = 0;
    CDistBasedClusterer::TDist rootDMax = 0.0, d1 = 0.0, d2 = 0.0;

    if (atree.size() < 2) {
        dMax = 0.0;
        end1 = atree.end();
        end2 = atree.end();
        return false;
    }

    CUTree tmpTree;
    TCUTIt tmpRoot = tmpTree.insert(tmpTree.begin(), CUTreeNode());
    TCUTIt rootEnd1 = tmpTree.begin(), rootEnd2 = tmpTree.begin();
    TCUTIt it = atree.begin();


    //  Since we cannot pass in the 'head' of the tree, make an dummy tree w/ a root node
    //  to which attach copy of the real tree.
    while (it.is_valid() && it != atree.end()) {
        tmpTree.append_child(tmpRoot, it);
        it = atree.next_sibling(it);
    }

    result = GetMaxPathCU(tmpRoot, rootDMax, d1, rootEnd1, d2, rootEnd2);

/*
    ERR_POST(ncbi::Trace << "longest two branches:\n" << *rootEnd1 << " d1= " << d1 << std::endl);
    ERR_POST(ncbi::Trace << *rootEnd2 << " d2= " << d2 << std::endl);
    ERR_POST(ncbi::Trace << "in GetMaxPathCU:  max distance = " << rootDMax << std::endl);
*/

//  find nodes in the original tree...
    it = atree.begin();
    while (it != atree.end() && foundCount < 2) {
        if (rootEnd1.is_valid() && *it == *rootEnd1) {
            rootEnd1 = it;
            ++foundCount;
        } else if (rootEnd2.is_valid() && *it == *rootEnd2) {
            rootEnd2 = it;
            ++foundCount;
        }
        ++it;
    }

    //  Didn't find the ends!
	if (foundCount < 2) {
        result = false;
    }

    if (result && rootEnd1 != rootEnd2 && rootDMax > 0) {
        dMax = rootDMax;
        end1 = rootEnd1;
        end2 = rootEnd2;
    } else {
        dMax = 0.0;
        end1 = atree.end();
        end2 = atree.end();
        result = false;
    }
    tmpTree.clear();
    return result;
}

//  dBranch1 == longest branch to a child leaf
//  dBranch2 == 2nd longest branch to a child leaf
//  Join the two branches to form the longest leaf-to-leaf path.
bool CSimpleSLCAlgorithm::GetMaxPathCU(const TCUTIt& cursor, CDistBasedClusterer::TDist& dMax, CDistBasedClusterer::TDist& dBranch1, TCUTIt& end1, CDistBasedClusterer::TDist& dBranch2, TCUTIt& end2) {

//	return true;

	bool result = true;
    bool updatedPath = false;
	CDistBasedClusterer::TDist localMax, localBranch1, localBranch2;
    TCUTIt localEnd1;
	TCUTIt localEnd2;

	if (cursor.number_of_children() == 0)	{  // a leaf node
		dBranch1 = 0;
		dBranch2 = 0;
        dMax     = 0;
		end1     = cursor;
		end2     = TCUTIt();
        updatedPath = true;
	} else {

		CDistBasedClusterer::TDist sibDist, testDist;
		TCUTChildIt sibCursor = cursor.begin();

		//  Recusive call to GetMaxPathCU in loop
		while (result && sibCursor != cursor.end()) {
			sibDist = sibCursor->distance;
            localEnd1 = TCUTIt();
            localEnd2 = TCUTIt();
            localBranch1 = 0;
            localBranch2 = 0;
            localMax     = 0;
			result = GetMaxPathCU(sibCursor, localMax, localBranch1, localEnd1, localBranch2, localEnd2);
            if (result) {
                testDist = localBranch1 + sibDist + dBranch1;
                //  sibCursor contains both ends of the longest path yet seen
                if (localMax > dMax && localMax > testDist) {
                    updatedPath = true;
                    dMax = localMax;
                    end1 = localEnd1;
                    end2 = localEnd2;
                    dBranch1 = localBranch1 + sibDist;
                    dBranch2 = localBranch2 + sibDist;
                //  sibCursor contains one end of the longest path yet seen
                } else if (testDist > localMax && testDist > dMax) {
                    updatedPath = true;
                    dMax = testDist;
                    if (localBranch1 + sibDist > dBranch1) {
                        end2 = end1;
                        end1 = localEnd1;
                        dBranch2 = dBranch1;
                        dBranch1 = localBranch1 + sibDist;
                    } else {
                        end2 = localEnd1;
                        dBranch2 = localBranch1 + sibDist;
                    }
                //  keep the current longest path (sibCursor not on the longest path yet seen)
                } else if (dMax >= localMax && dMax >= testDist) {
					;
				//  should never see this condition
                } else {
					result = false;
                }

			}
            ++sibCursor;
		}
	}
	return result;
}

/*=======================================*/
/*   End midpoint-root related functions */
/*=======================================*/

/*=======================================*/
/*   Start tree stream functions         */
/*=======================================*/

void CSimpleSLCAlgorithm::fromString(const std::string& str, CUTree& stree)
{
    ncbi::CNcbiIstrstream iss(str.c_str(), str.length()+1);

	read(iss, stree);
}

std::string CSimpleSLCAlgorithm::toString(const CUTree& stree)
{
    ncbi::CNcbiOstrstream oss;

	write(oss, stree, stree.begin());
    return ncbi::CNcbiOstrstreamToString(oss);
}

string CSimpleSLCAlgorithm::toNestedString(const CUTree& stree)
{
    static const int NESTED_INDENT = 4;

    int nesting_level = 0;
    std::string newString;
    std::string spacer, line;
    std::string s = toString(stree);
    std::string::const_iterator sci = s.begin();

    //  skip to opening paren
    while (*sci != '(' || sci == s.end()) {
        ++sci;
    }

    while (sci != s.end()) {

        //  print '(' on its own line properly indented, outputting any
        //  pre-exiting text first.
        if (*sci == '(') {
            if (nesting_level != 0 && !line.empty()) {
                newString.append(line + "\n");
                line.erase();
            }
            newString.append(std::string(NESTED_INDENT*nesting_level, ' ') + *sci + "\n");
            ++nesting_level;

        //  print ')' followed by any siblings on the same line, outputting
        //  any pre-existing text first.

        } else if (*sci == ')') {
            if (!line.empty()) {
                newString.append(line + "\n");
                line.erase();
            }
            --nesting_level;
            line = std::string(NESTED_INDENT*nesting_level, ' ') + *sci;

        //  print any other character; place space after comma for readability
        } else {
            if (line.length() == 0 && nesting_level > 0) {
                line = std::string(NESTED_INDENT*(nesting_level-1) + 2, ' ');
            }
            line += *sci;
            if (*sci == ',') {
                line += ' ';
            }
        }

        //  final closing ';'
        if (*sci == ';') {
            newString.append(line);
            line.erase();
            break;
        }
        ++sci;
    }

    //  if the ';' was forgotten, print out final text
    if (newString[newString.length() - 1] != ';') {
        if (line.length()==0 || line[line.length() - 1] != ';') {
            line.append(";");
        }
        newString.append(line);
    }

    return newString;
}

//string CSimpleSLCAlgorithm::toString() {
//    return CdTreeStream::toNestedString(*m_tree);
//}

bool CSimpleSLCAlgorithm::writeToFile(std::ofstream&ofs, const CUTree& stree)
{
	return write(ofs,stree, stree.begin());
}

bool CSimpleSLCAlgorithm::write(std::ostream&os, const CUTree& stree, const TCUTIt& cursor)
{
    double d;
	if (!os.good())
		return false;
	// if leaf, print leaf and return
	if (cursor.number_of_children() == 0)
	{
		os << cursor->id << ':' << cursor->distance;
		// if this node has sibling, print ","
		if (stree.number_of_siblings(cursor) > 1)
			os << ',';
		return true;
	}
	else
	{
		// print (
		os << '(';
		// print each child
		TCUTChildIt sib = cursor.begin();
		while (sib != cursor.end())
		{
			CSimpleSLCAlgorithm::write(os,stree,sib);  //recursive
			++sib;
		}
		// print ) <name>:<dist>;  accept negative distances
        d = cursor->distance;
		if (d > 0 || d < 0)
		{
			os << ") :" <<d;
			// if this node has sibling, print ","
			if (stree.number_of_siblings(cursor) > 1)
				os << ',';
 		}
		else  //root node has no distance
			os<< ");";
	}
	return true;
}

bool CSimpleSLCAlgorithm::isDelimiter(char ch)
{
	return (ch == '(') || (ch == ')') ||(ch == ',') ||(ch == ';');
}

bool CSimpleSLCAlgorithm::readFromFile(std::ifstream& ifs, CUTree& seqTree)
{
    return read(ifs, seqTree);
}

void CSimpleSLCAlgorithm::readToDelimiter(std::istream& is, std::string& str)
{
	//str.erase();
	char ch;
	while (is.get(ch) && (!isDelimiter(ch)))
		//if (!isspace((unsigned char) ch))
			str += ch;
	if (isDelimiter(ch))
		is.putback(ch);
}

bool CSimpleSLCAlgorithm::read(std::istream& is, CUTree& cuTree)
{
	if (!is.good())
		return false;
	char ch;
	is.get(ch);
	// skip to the first (
	while((ch != '(') && (is.good()))
	{
		is.get(ch);
	}
	if (!is.good())
		return false;
	CUTreeNode item;

	TCUTIt top = cuTree.insert(cuTree.begin(),item);
	TCUTIt cursor = top;
	std::string idDist;
	std::string distance;

	static std::string delimiters = "(),";
	while(is.get(ch))
	{
		if (!isspace((unsigned char) ch))
		{
			CUTreeNode tmp1;
			switch (ch)
			{
			case ';':  //end seq tree data
				return true;
				//break;
			case '(':  // start a node
				cursor = cuTree.append_child(cursor, tmp1);
				break;
			case ',':
				// end a sibling, nothing needs done, skip it.
				break;
			case ')':  //end a node; read and set the distance
				readToDelimiter(is,distance);
				if (distance.size() >0)
				{
                    //  allow interior nodes to be named
                    int colon_loc = distance.find_first_of(":");
                    if (colon_loc > 0) {
                        try {
                            cursor->id = atoi(distance.substr(0, colon_loc).c_str());
                        } catch (...) {
                            cursor->id = CUTreeNode::NOT_SET;
                        }
//                        cursor->name = distance.substr(0, colon_loc);
                        distance = distance.erase(0, colon_loc);
                    }

					//skip the leading :
                    //if not present, check if dealing w/ a final named internal node
                    if (distance[0] != ':') {
                        int semi_loc = distance.find_first_of(";");
                        if (semi_loc > 0) {
                            try {
                                cursor->id = atoi(distance.substr(0, semi_loc).c_str());
                            } catch (...) {
                                cursor->id = CUTreeNode::NOT_SET;
                            }
//                            cursor->name = distance.substr(0, semi_loc);
                            is.putback(distance[semi_loc]);
                        } else {
                            std::cout<<"length missing";
                        }
                    }
					else
					{
                        double dist = 0.0;
						distance.erase(0,1);
                        try {
						    dist = atof(distance.c_str());
                        } catch (...) {
                        }

                        if (dist < 0.0) {
                            std::cout << "Warning:  negative branch length! "
                                      << "  D = " << distance << std::endl;
//                                      << cursor->name << ", D = " << distance << std::endl;
                        }
						//set distance
						cursor->distance = dist;
						if (cursor == top)
						{
							std::cout<<"Warning:  already reached top before processing )";
							return false;
						}
						else
							cursor = cuTree.parent(cursor);
					}
				}
				distance.erase();
				break;
			default:  // non-delimiter starts a new leaf node
				idDist += ch;
				readToDelimiter(is, idDist);
				tmp1 = CUTreeNode(idDist);
				cuTree.append_child(cursor,tmp1); //don't move cursor
				//reset the word
				idDist.erase();
			}
		}
	}
	return true;
}

/*=======================================*/
/*     End tree stream functions         */
/*=======================================*/

END_SCOPE(cd_utils)
END_NCBI_SCOPE
