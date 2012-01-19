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
*   Virtual base class for representing sequence-tree calculation algorithms.
*   Implementations of specific algorithms should be subclasses that define,
*   minimally, the ComputeTree method.
*
*/

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqTreeAlg.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const string TreeAlgorithm::NO_NAME = "<unnamed algorithm>";
const TreeAlgorithm::Rootedness TreeAlgorithm::DEF_ROOTED = TreeAlgorithm::eUnrooted;

#pragma warning (4 : 4701 )

/*=======================================*/
/*    Midpoint-root related functions    */
/*=======================================*/
#pragma warning (4 : 4018 )

void MidpointRootSeqTree(const SeqTree& oldTree, SeqTree& newTree) {

    SeqTree tmpTree;

	bool result = true;
    bool found  = false;
	double midpointDist = 0.0;
	double dToEnd1 = 0.0;
	double dmax = 0.0;
	double dToNewParent = 0, dToNewChild = 0;
    int nTopLevelNodes;

    SeqItem item;
	SeqTree::iterator cursor1, cursor2, newTreeRoot, newTreeCursor;
    SeqTree::sibling_iterator  tmpSibling, tmpTreeCursor;
	SeqTree::iterator end1, end2;//

	newTree.clear();
	if (! &oldTree) {
		return;
	}

	tmpTree = oldTree;

	if (GetMaxPath(tmpTree, dmax, end1, end2)) {

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
            newTree = oldTree;
            return;
        }

        item = SeqItem("New Root", -1, dToEnd1-midpointDist);
        if (cursor1.is_valid()) {  //  midpoint between two nodes
            cursor2->distance -= item.distance;
            if (dToEnd1 == midpointDist) { //midpoint lands on existing (non-head) node
                newTreeRoot = cursor1;
                cursor1     = tmpTree.parent(newTreeRoot);
            } else {  
                newTreeRoot = tmpTree.append_child(cursor1, item);
                tmpTree.reparent(newTreeRoot, cursor2, tmpTree.next_sibling(cursor2));
            }
            
            //  Starting at newTreeRoot, adjust distances in SeqItems on path 
            //  to the head of tmpTree.  *** How deal w/ head potentially becoming
            //  a new node?? ***
            
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
        /*  should be no else
        } else {  // midpoint lies between a node and the head
            newTreeRoot = tmpTree.insert(tmpTree.begin(), item);
            if (dToEnd1 == midpointDist) { // head is already the root; do nothing
                newTree = oldTree;
            } else {
                cursor2->distance -= item.distance;
                tmpTree.append_child(newTreeRoot, cursor2);
                tmpTree.erase(cursor2);
            }
            cursor2 = newTreeRoot;
            dToNewParent = cursor2->distance;
            cursor2->distance = 0.0;  
        }
        */
        
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
        } else if (nTopLevelNodes > 2) {  // an intermediate node will be inserted as a child of cursor2 below
            item = *cursor2;
            dToNewChild = cursor2->distance;
        }

        newTree.SeqTreeBase::operator=(tmpTree.reroot(newTreeRoot));

        //  Fix up the distance of internal node corresp. to head of old tree if needed.
        if (nTopLevelNodes > 2) {
                
            SeqItem nullItem = SeqItem();
            SeqTree::sibling_iterator sit;
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


bool GetMaxPath(const SeqTree& atree, double& dMax, SeqTree::iterator& end1, SeqTree::iterator& end2) {

    bool result = true;
    int foundCount = 0;
    double rootDMax = 0.0, d1 = 0.0, d2 = 0.0;

    if (atree.size() < 2) {  
        dMax = 0.0;
        end1 = atree.end();
        end2 = atree.end();
        return false;
    }

    SeqTree tmpTree;
    SeqTree::iterator tmpRoot = tmpTree.insert(tmpTree.begin(), SeqItem());
    SeqTree::iterator rootEnd1 = tmpTree.begin(), rootEnd2 = tmpTree.begin();
    SeqTree::iterator it = atree.begin();


    //  Since we cannot pass in the 'head' of the tree, make an dummy tree w/ a root node
    //  to which attach copy of the real tree.
    while (it.is_valid() && it != atree.end()) {
        tmpTree.append_child(tmpRoot, it);
        it = atree.next_sibling(it);
    }

    result = GetMaxPath(tmpRoot, rootDMax, d1, rootEnd1, d2, rootEnd2);

//    std::cout << "longest two branches:\n" << *rootEnd1 << " d1= " << d1 << std::endl;
//    std::cout << *rootEnd2 << " d2= " << d2 << std::endl;
//    std::cout << "in GetMaxPath:  max distance = " << rootDMax << std::endl;

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
//		dMax = 0.0;
//        end1 = atree.end();
//        end2 = atree.end();
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
bool GetMaxPath(const SeqTree::iterator& cursor, double& dMax, double& dBranch1, SeqTree::iterator& end1, double& dBranch2, SeqTree::iterator& end2) {

	bool result = true;
//    bool updatedPath = false;
	double localMax, localBranch1, localBranch2;
	SeqTree::iterator localEnd1;
	SeqTree::iterator localEnd2;

	assert(dBranch1 >= dBranch2);

	if (cursor.number_of_children() == 0)	{  // a leaf node
		dBranch1 = 0;
		dBranch2 = 0;
        dMax     = 0;
		end1     = cursor;
		end2     = SeqTree::iterator();
//        updatedPath = true;
	} else {

		double sibDist, testDist;
		SeqTree::sibling_iterator sibCursor = cursor.begin();

		//  Recusive call to GetMaxPath in loop
		while (result && sibCursor != cursor.end()) {
			sibDist = sibCursor->distance;
            localEnd1 = SeqTree::iterator();
            localEnd2 = SeqTree::iterator();
            localBranch1 = 0;
            localBranch2 = 0;
            localMax     = 0;
			result = GetMaxPath(sibCursor, localMax, localBranch1, localEnd1, localBranch2, localEnd2);
            if (result) {
                testDist = localBranch1 + sibDist + dBranch1;
                //  sibCursor contains both ends of the longest path yet seen
                if (localMax > dMax && localMax > testDist) {
//                    updatedPath = true;
                    dMax = localMax;
                    end1 = localEnd1;
                    end2 = localEnd2;
                    dBranch1 = localBranch1 + sibDist;
                    dBranch2 = localBranch2 + sibDist;
                //  sibCursor contains one end of the longest path yet seen
                } else if (testDist >= localMax && testDist > dMax) {
//                    updatedPath = true;
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
                    assert(result);
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

void TreeAlgorithm::midpointRootIfNeeded() {
    if (m_tree && useMidpointRooting()) {

		//  In case any nodes have zero distance (e.g., if there are identical sequences)
        //  reset distance to some small value. 
        //  (midpoint root can become confused in such cases...)
		for (SeqTree::iterator stit = m_tree->begin(); stit != m_tree->end(); ++stit) {
			if (stit->distance < RESET_WITH_TINY_DISTANCE && m_tree->parent(stit).is_valid()) {
				stit->distance = RESET_WITH_TINY_DISTANCE;
			}
		}

//		if (isMidpointRooted()) {
		SeqTree tmpTree = *m_tree;
//		    *m_tree = MidpointRootSeqTree(tmpTree, *m_tree);
		MidpointRootSeqTree(tmpTree, *m_tree);
		if (m_tree->size() == 0) {
		    *m_tree = tmpTree;
//			    if (m_tree) {
//				    delete m_tree;
//				    m_tree = 0;
//		    	}
		}
//    	}
    }
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
