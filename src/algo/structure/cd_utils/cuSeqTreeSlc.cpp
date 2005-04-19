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
#include <algo/structure/cd_utils/cuSeqTreeSlc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const int SLC_TreeAlgorithm::USED_ROW = -1;
const int SLC_TreeAlgorithm::BAD_INDEX = -1;
const double SLC_TreeAlgorithm::REPLACE_NEG_DIST = 0.001;
const ETreeMethod SLC_TreeAlgorithm::MY_TREE_METHOD = eSLC;
const TreeAlgorithm::Rootedness SLC_TreeAlgorithm::MY_ROOTEDNESS = TreeAlgorithm::eRooted;
const DistanceMatrix::TMatType SLC_TreeAlgorithm::INIT_MINIMA  = kMax_Double;

double myMin(double a, double b) {
    return a < b ? a : b;
}

SLC_TreeAlgorithm::SLC_TreeAlgorithm() 
    : TreeAlgorithm(MY_ROOTEDNESS, MY_TREE_METHOD) {

    m_dm = NULL;
    initializeNodes();
}

SLC_TreeAlgorithm::SLC_TreeAlgorithm(DistanceMatrix* dm) 
    : TreeAlgorithm(MY_ROOTEDNESS, MY_TREE_METHOD) {

    m_dm  = dm;
   // m_cdd = cdd;
    initializeNodes();
}

void SLC_TreeAlgorithm::SetDistMat(DistanceMatrix* dm) {
    m_dm = dm;
	/*
	if (dm->isSetCDD()) {
		m_cdd = const_cast<CCd*>(dm->GetCdd());    //  ATTN:  const_cast
	}*/
    initializeNodes();
}
/*
void SLC_TreeAlgorithm::SetCDD(CCd* cdd) {
    m_cdd = cdd;
    //fillSeqNames(m_tree, m_cdd);
}*/


//  Initialize a list of nodes that will be placed in a tree structure.
//  (For a rooted tree with N leafs, there are 2N-1 total nodes, of which 
//   N-2 are internal and one is the root.)
//  The node will be identified by its one-based row indices in the distance matrix
//  (assumed to correspond to rows an accompanying alignment).  Nodes with 'rowID'
//  > m_nseqs serve to hold composite nodes created during the SLC procedure.
//  
//  The root node will have length 0 and often called the 'hub' node.
//
//  NOTE:  If the distance matrix row does not map directly to the row indices of an 
//  alignment, the caller is responsible for re-mapping rowID to the correct
//  alignment row number.
// 
//  NOTE:  Names are initially assigned to be the row ID as a string.

void SLC_TreeAlgorithm::initializeNodes() {

    //  m_tree is initialized by base-class constructor

    int nnodes;
    SeqItem* sip;

    if (m_dm && m_nseqs>0) {
        for (int i=0; i<(2*m_nseqs - 1); ++i) {
            if (m_items[i]) {
                delete m_items[i];
            }
        }
    }

    m_seqiters.clear();
    m_items.clear();

    m_nseqs = 0;
    m_nextNode = 0;

    if (m_dm) {
        //  Allow square matrix only
        if (m_dm->GetNumRows() != m_dm->GetNumCols()) {
            m_dm = NULL;
            return;
        } else {
            m_nseqs = m_dm->GetNumRows();
            nnodes = 2*m_nseqs - 1;
            m_seqiters = TTreeIt(nnodes); 
            //cout << "m_seqiters:  " << m_seqiters.size() << endl;
            for (int i=0; i<nnodes; ++i) {
                sip = new SeqItem(i);
                sip->name = NStr::IntToString(i);
                if (sip) {
                    m_items.push_back(sip);
                } else {
                    m_items.push_back(NULL);
                }
            }
        }
    }
}

SLC_TreeAlgorithm::~SLC_TreeAlgorithm() {

    m_seqiters.clear();
    for (int i=0; i<(2*m_nseqs - 1); ++i) {
        delete m_items[i];
    }
}

//  
void SLC_TreeAlgorithm::Join(int idmin, int jdmin, double ilen, double jlen) {

    double tmpd  = ilen;
    int maxIndex = 2*m_nseqs - 2;  //  tot. # nodes (less the hub)

    if (idmin == jdmin) {
        cerr << "Error:  You cannot join node " << idmin << " to itself.\n";
        return;
    }

    if (idmin < 0 || idmin > maxIndex || jdmin < 0 || jdmin > maxIndex || m_nextNode < 0 || m_nextNode > maxIndex+1) {
        cerr << "Error:  Out of range index in Join:  " << idmin << " " 
             << jdmin << " " << m_nextNode << "  Max allowed index:  " << maxIndex << endl;
        return;
    }
 
    TChildIt cit; 
    TSeqIt& hub = m_seqiters[0];

    //  New composite node (unless we're at the final two nodes)
    if (m_nextNode <= maxIndex) {
        m_seqiters[m_nextNode] = m_tree->append_child(hub, *m_items[m_nextNode]);
    }

    int tmpi = idmin;
    while (tmpi == idmin || tmpi == jdmin) {
        cit = m_seqiters[tmpi];
        if (m_nextNode <= maxIndex) {   //  unless the last two nodes...
            if (cit.is_valid()) {
                if (m_tree->parent(cit) == hub) {
                    cit->distance = tmpd;
                    TChildIt nexit=cit;++nexit;
                    m_tree->reparent(m_seqiters[m_nextNode], cit, nexit);  // don't use ++ as it modifies cit                   
                } else {   //  this case should not occur
                    tmpi = -(idmin+jdmin);
                    cerr << "Error:  iterator found (id= " << cit->rowID << ") not attached to hub.\n";
                }
            } else {
                m_items[tmpi]->distance = tmpd;
                m_seqiters[tmpi] = m_tree->append_child(m_seqiters[m_nextNode], *m_items[tmpi]);
            }
        } else {
            if (cit.is_valid()) {
                if (m_tree->parent(cit) == hub) {
                    cit->distance = tmpd;
                } else {   //  this case should not occur
                    tmpi = -(idmin+jdmin);
                    cerr << "Error:  iterator found (id= " << cit->rowID << ") not attached to hub for last nodes.\n";
                }
            } else {
                m_items[tmpi]->distance = tmpd;
                m_seqiters[tmpi] = m_tree->append_child(m_seqiters[0], *m_items[tmpi]);
            }
        }
        tmpd = jlen;
        tmpi = (tmpi==idmin) ? jdmin : -(idmin+jdmin);
    }
}


// return the number of loops in ComputeTree
long SLC_TreeAlgorithm::GetNumLoopsForTreeCalc() {
    int it, j, i;
    long count=0;
    int* indexMap = new int[m_nseqs];  
    for (i=0; i<m_nseqs; ++i) {
        indexMap[i] = i+1;
    }
    for (it=1; it<=m_nseqs-1; ++it) {
        for (j=1; j<m_nseqs; ++j) {  
//            if (indexMap[j] != USED_ROW) {
//                for (i=0; i<j; ++i) {
//                    if (indexMap[i] != USED_ROW) {
//                        count++;
//                    }
//                }
//            }
            count++;  // don't count in inner loop or can exceed int4 max
        }
        indexMap[it] = USED_ROW;
    }
    delete []indexMap;
    return(count);
}


//  Main computational engine for the SLC method (simplification to the NJ alg)

void SLC_TreeAlgorithm::ComputeTree(SeqTree* atree, pProgressFunction pFunc) {

    int i, j, k;
    int imin, jmin, idmin, jdmin;
    double ilen, jlen; 

    DistanceMatrix::TMatType minval;

    string newickStr = "";
/*
#if _DEBUG
    ofstream ofs;
	ofs.open("slc_info.txt");
#endif
  */  
    m_tree = atree;
    if (m_tree == NULL) {
        return;
    } else if (!m_dm) {
        m_tree->clear();
        m_tree = NULL;
        return;
    }

    m_seqiters[0] = m_tree->insert(m_tree->begin(), *m_items[0]);

    //  For composite nodes, save the 1/2 dist. between the two component nodes.
    //  When this node becomes part of a composite node in a later iteration, that
    //  distance is subtracted from the orig-composite-node to hub distance.

    double* internalDistCorrection = new double[m_nseqs];  

    //  Map rows in distance matrix to index of corresponding node in m_seqiters;
    //  when there is no corresponding distance matrix row, enter USED_ROW.

    int* indexMap = new int[m_nseqs];  

    m_nextNode = m_nseqs + 1;

    if (indexMap == 0 || internalDistCorrection == 0) {
        m_tree->clear();
        m_tree = NULL;
        return;
    } else {
        for (i=0; i<m_nseqs; ++i) {
            indexMap[i] = i+1;
            internalDistCorrection[i] = 0.0;
        }
    }

    // adjust distance matrix (do this before pre-fetching)
    m_dm->EnforceSymmetry();

    // speeding up tree-calc, pre-fetch all distances
    double** ppDists;
    ppDists = new double*[m_nseqs];
    for (i=0; i<m_nseqs; i++) {
        ppDists[i] = new double[m_nseqs];
    }
    for (i=0; i<m_nseqs; i++) {
        for (j=0; j<m_nseqs; j++) {
            ppDists[i][j] = m_dm->FastGet(i, j);
        }
    }

    //  Iterations loop
    long count = 0;
    long total = GetNumLoopsForTreeCalc();
    for (int it=1; it<=m_nseqs-1; ++it) {

        minval = INIT_MINIMA;

        //  Find minimum distance in active part of the matrix
        for (j=1; j<m_nseqs; ++j) {  
            if (indexMap[j] != USED_ROW) {
                for (i=0; i<j; ++i) {
                    if (indexMap[i] != USED_ROW) {
                        if (ppDists[j][i] < minval) {
                            minval = ppDists[j][i];
                            imin = i;
                            jmin = j;
                        }
//                        count++;
                    }
                }
            }
            count++;
        }
        pFunc(count, total);
        idmin = indexMap[imin];
        jdmin = indexMap[jmin];

//         if (idmin == USED_ROW || jdmin == USED_ROW) {
//             cerr << "Indexing Error:  " << idmin << " or " << jdmin << " is not a valid row index "
//                  << " for DM indices " << imin << " and " << jmin << ", respectively.\n\n";
//             break;
//         }

        //  Assign length of new branches to be 1/2 m_dm(imin, jmin)

        ilen = 0.5*ppDists[imin][jmin] - internalDistCorrection[imin];
        jlen = 0.5*ppDists[imin][jmin] - internalDistCorrection[jmin];
/*        
#if _DEBUG
        if (1) {
            ofs << "\nIteration " << it << ":  min distance = " << minval << endl;
            ofs << "Rows :    " << imin << "(node " << idmin << "; length= " << ilen
                 << ") and " << jmin << "(node " << jdmin << "; length= " << jlen << ")." << endl;
            ofs << " dist corrs:  " << internalDistCorrection[imin] 
                 << " " << internalDistCorrection[jmin] << endl;
        }
#endif
*/
//  Connect nodes, creating them as needed.

        ilen = (ilen > 0) ? ilen : REPLACE_NEG_DIST;
        jlen = (jlen > 0) ? jlen : REPLACE_NEG_DIST;

        Join(idmin, jdmin, ilen, jlen);

        internalDistCorrection[imin] = 0.5*ppDists[imin][jmin];
        indexMap[imin] = m_nextNode++;
        indexMap[jmin] = USED_ROW;

        //  Fix-up distance matrix:  row jmin is zeroed out, imin row becomes m_dm(imin, jmin)
        for (k=0; k<m_nseqs; ++k) {
            if (indexMap[k] != USED_ROW) {
                ppDists[k][imin] = myMin(ppDists[k][imin], ppDists[k][jmin]);
                ppDists[k][jmin] = 0.0;
                ppDists[jmin][k] = 0.0;
                ppDists[imin][k] = ppDists[k][imin];
            }
        }
        ppDists[imin][imin] = 0.0;

    }  //  end iteration loop
    assert(count == total);

    //  Need to deal with final node?  (should all be hooked to the hub node)
/*#if _DEBUG
	ofs.close();
#endif
*/
	midpointRootIfNeeded();
    //fillSeqNames(m_tree, m_cdd);
	
    delete [] indexMap;
    delete [] internalDistCorrection;

}

int SLC_TreeAlgorithm::numChildren(const TSeqIt& sit) {

    int n = 0;
    if (sit.is_valid()) {
        n = m_tree->number_of_children(sit);
    }
    return n;

}

string SLC_TreeAlgorithm::toString() {
    return CdTreeStream::toNestedString(*m_tree);
}

// string SLC_TreeAlgorithm::toString(const TSeqIt& sit) {
//     return "a string from an iterator\n";
// }

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

