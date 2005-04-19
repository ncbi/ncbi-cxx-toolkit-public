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
* File Description:  cdt_treealg.cpp
*
*   Implementation of the Neighbor Joining algorithm of Saitou and Nei.
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <algo/structure/cd_utils/cuSeqTreeNj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const int NJ_TreeAlgorithm::USED_ROW = -1;
const int NJ_TreeAlgorithm::BAD_INDEX = -1;
const double NJ_TreeAlgorithm::REPLACE_NEG_DIST = 0.001;
const ETreeMethod NJ_TreeAlgorithm::MY_TREE_METHOD = eNJ;
const TreeAlgorithm::Rootedness NJ_TreeAlgorithm::MY_ROOTEDNESS = TreeAlgorithm::eUnrooted;
const DistanceMatrix::TMatType NJ_TreeAlgorithm::INIT_MINIMA  = kMax_Double;

string GetTreeAlgorithmName(ETreeMethod algorithm)
{ 
	return TREE_ALGORITHM_NAMES[algorithm];
}

 NJ_TreeAlgorithm::NJ_TreeAlgorithm() 
    : TreeAlgorithm(MY_ROOTEDNESS, MY_TREE_METHOD) {

    m_dm  = NULL;
    initializeNodes();
}

NJ_TreeAlgorithm::NJ_TreeAlgorithm(DistanceMatrix* dm) 
    : TreeAlgorithm(MY_ROOTEDNESS, MY_TREE_METHOD) {

    m_dm  = dm;
    //m_cdd = cdd;
    initializeNodes();
}

void NJ_TreeAlgorithm::SetDistMat(DistanceMatrix* dm) {
    m_dm = dm;
	/*
	if (dm->isSetCDD()) {
		m_cdd = const_cast<CCd*>(dm->GetCdd());   //  ATTN:  const_cast
	}*/
    initializeNodes();
}
/*
void NJ_TreeAlgorithm::SetCDD(CCd* cdd) {
    m_cdd = cdd;
    //fillSeqNames(m_tree, m_cdd);
}*/


//  Initialize a list of nodes that will be placed in a tree structure.
//  (For a tree with N leafs, there are 2N-3 total nodes, of which N-3 are internal.)
//  The node will be identified by its one-based row indices in the distance matrix
//  (assumed to correspond to rows an accompanying alignment).  Nodes with 'rowID'
//  > m_nseqs serve to hold composite nodes created during the NJ procedure.
//  
//  Node zero serves as the "hub" internal node that acts as the central node of the
//  initial star configuration; it's value is 0 to set it apart from the other
//  internal nodes.  The final three nodes in the NJ algorithm get joined at the hub.
//
//  NOTE:  If the distance matrix row does not map directly to the row indices of an 
//  alignment, the caller is responsible for re-mapping rowID to the correct
//  alignment row number.
// 
//  NOTE:  Names are initially assigned to be the row ID as a string.

void NJ_TreeAlgorithm::initializeNodes() {

    //  m_tree is initialized by base-class constructor

    int nnodes;
    SeqItem* sip;

    if (m_dm && m_nseqs>0) {
        for (int i=0; i<(2*m_nseqs - 2); ++i) {
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
            nnodes = 2*m_nseqs - 2;
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

NJ_TreeAlgorithm::~NJ_TreeAlgorithm() {

    m_seqiters.clear();
    for (int i=0; i<(2*m_nseqs - 2); ++i) {
        delete m_items[i];
    }
}

//  Disconnect composite nodes from hub if needed, and reconnect the indicated nodes
//  to a new composite node.  Connect the composite node to the hub.
void NJ_TreeAlgorithm::Join(int idmin, int jdmin, double ilen, double jlen) {

    double tmpd  = ilen;
    int maxIndex = 2*m_nseqs - 3;  //  tot. # nodes less hub 

    if (idmin == jdmin) {
        cerr << "Error:  You cannot join node " << idmin << " to itself.\n";
        return;
    }

    if (idmin < 0 || idmin > maxIndex || jdmin < 0 || jdmin > maxIndex || m_nextNode < 0 || m_nextNode > maxIndex) {
        cerr << "Error:  Out of range index in Join:  " << idmin << " " 
             << jdmin << " " << m_nextNode << "  Max allowed index:  " << maxIndex << endl;
        return;
    }
 
    TChildIt cit; 
    TSeqIt& hub = m_seqiters[0];

    //  New composite node
    m_seqiters[m_nextNode] = m_tree->append_child(hub, *m_items[m_nextNode]);

    int tmpi = idmin;
    while (tmpi == idmin || tmpi == jdmin) {
        cit = m_seqiters[tmpi];
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
        tmpd = jlen;
        tmpi = (tmpi==idmin) ? jdmin : -(idmin+jdmin);
    }
}


// return the number of loops in ComputeTree
long NJ_TreeAlgorithm::GetNumLoopsForTreeCalc() {
    int it, j, i;
    long count=0;
    int* indexMap = new int[m_nseqs];  
    for (i=0; i<m_nseqs; ++i) {
        indexMap[i] = i+1;
    }
    for (it=1; it<=m_nseqs-3; ++it) {
        for (j=1; j<m_nseqs; ++j) {  
//            if (indexMap[j] != USED_ROW) {
//                for (i=0; i<j; ++i) {
//                    if (indexMap[i] != USED_ROW) {
//                        count ++;
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


//  Main computational engine for the NJ method

void NJ_TreeAlgorithm::ComputeTree(SeqTree* atree, pProgressFunction pFunc) {

    int i, j, k;
    int imin, jmin, idmin, jdmin, tmp;
    double ilen, jlen; 
    double sum_d, tmp_d;

    double normalization;

/*#if _DEBUG
    ofstream ofs;
	ofs.open("nj_info.txt");
#endif
  */  
	DistanceMatrix::TMatType minval;
    DistanceMatrix::TMatType sum_ij   = 0.0;

    string newickStr = "";

    m_tree = atree;
    if (m_tree == NULL) {
        return;
    } else if (!m_dm) {
        m_tree->clear();
        m_tree = NULL;
        return;
    }

    m_seqiters[0] = m_tree->insert(m_tree->begin(), *m_items[0]);

    //  For composite nodes, save the mean dist. between the two component nodes.
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

    // pre-fetch all distances (for speed)
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

    // pre-calc sums (for speed)
    double* pSums;
    pSums = new double[m_nseqs];
    for (i=0; i<m_nseqs; i++) {
        sum_d = 0;
        for (k=0; k<m_nseqs; ++k) {
            sum_d += ppDists[i][k];
        }
        pSums[i] = sum_d;
    }

    //  Iterations loop
    long total = GetNumLoopsForTreeCalc();
    long count = 0;
    for (int it=1; it<=m_nseqs-3; ++it) {

        minval = INIT_MINIMA;
        normalization = 1.0/((double)(m_nseqs - it + 1) - 2.0);

        //  Compute/minimize sum of all branch lengths in tree
        for (j=1; j<m_nseqs; ++j) {  
            if (indexMap[j] != USED_ROW) {
                for (i=0; i<j; ++i) {
                    if (indexMap[i] != USED_ROW) {
//                        count++;
                        double Sum = pSums[i] + pSums[j];
                        sum_ij = ppDists[j][i]/normalization - Sum;
                        if (sum_ij < minval) {
                            minval = sum_ij;
                            imin = i;
                            jmin = j;
                        }
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

        //  Get length of new branches...

        //  l(inode->m_nextNode) = .5(d(inode,jnode) + sum_k(d(i,k) - d(j,k))/(m_nseqs-2))
        sum_d = 0.0;
        for (k=0; k<m_nseqs; ++k) {
            sum_d += ppDists[imin][k] - ppDists[jmin][k];
        }
        sum_d *= normalization;
        ilen = 0.5*(ppDists[imin][jmin] + sum_d) - internalDistCorrection[imin];
        jlen = 0.5*(ppDists[imin][jmin] - sum_d) - internalDistCorrection[jmin];
/*#if _DEBUG        
        if (1) {
            ofs << "\nIteration " << it << ":  min sum of lengths = " << minval << endl;
            ofs << "Rows :    " << imin << "(node " << idmin << "; length= " << ilen
                 << ") and " << jmin << "(node " << jdmin << "; length= " << jlen << ")." << endl;
            ofs << "Distance between these nodes:  " << (*m_dm)[imin][jmin] << endl;
            ofs << "norm:  " << sum_d << " dist corrs:  " << internalDistCorrection[imin] 
                 << " " << internalDistCorrection[jmin] << endl;
        }
#endif
*/
//  Detach any edges to the hub, and attach/create nodes as needed.

        ilen = (ilen >= 0) ? ilen : REPLACE_NEG_DIST;
        jlen = (jlen >= 0) ? jlen : REPLACE_NEG_DIST;

        Join(idmin, jdmin, ilen, jlen);

        internalDistCorrection[imin] = 0.5*ppDists[imin][jmin];
        indexMap[imin] = m_nextNode++;
        indexMap[jmin] = USED_ROW;

        //  Fix-up distance matrix:  row jmin is zeroed out, imin row becomes averaged of imin/jmin
        //  also fix up the pre-calculated partial sums
        for (k=0; k<m_nseqs; ++k) {
            if (indexMap[k] != USED_ROW) {
                tmp_d = 0.5*(ppDists[imin][k] + ppDists[jmin][k]);
                pSums[imin] -= ppDists[imin][k];
                pSums[imin] += tmp_d;
                ppDists[imin][k] = tmp_d;
                pSums[k] -= ppDists[k][imin];
                pSums[k] += tmp_d;
                ppDists[k][imin] = tmp_d;
            }
        }
        pSums[imin] -= ppDists[imin][imin];
        ppDists[imin][imin] = 0.0;
        for (k=0; k<m_nseqs; ++k) {
            pSums[jmin] -= ppDists[jmin][k];
            ppDists[jmin][k] = 0.0;
            pSums[k] -= ppDists[k][jmin];
            ppDists[k][jmin] = 0.0;
        }

//         cout << "Distance matrix at end of iteration " << it <<  endl;
//         m_dm->printMat();

    }  //  end iteration loop
    assert(count == total);

    //  Deal with final three nodes  (should all be hooked to the hub node)

    int finalNodes[3] = {USED_ROW, USED_ROW, USED_ROW};
    double finalLen[3] = {0.0, 0.0, 0.0};

    tmp = 0;
    i = USED_ROW;
    j = USED_ROW;
    k = USED_ROW;
    for (int l=0; l<m_nseqs; ++l) {
        if (indexMap[l] != USED_ROW) {
            if (tmp < 3) {
                finalNodes[tmp] = l;
            }
            tmp++;
        }
    }
    if (tmp == 3) {
        for (int l=0; l<3; ++l) {
            i = finalNodes[l];
            j = finalNodes[(l+1)%3];
            k = finalNodes[(l+2)%3];
            finalLen[l] = 0.5*(ppDists[i][j] + ppDists[i][k] - ppDists[j][k]) - internalDistCorrection[i];

            if (m_seqiters[indexMap[i]].is_valid()) {  //  existing internal node; set dist
                m_seqiters[indexMap[i]]->distance = finalLen[l];
            } else {
                m_items[indexMap[i]]->distance = finalLen[l];
                m_seqiters[indexMap[i]] = m_tree->append_child(m_seqiters[0], *m_items[indexMap[i]]);
            }
        }
/*#if _DEBUG
        ofs << "Final nodes (" << tmp << ") found -- expected 3.\n" << i << " " << j 
             << " " << k << endl;
        ofs << "Index Maps:  " << indexMap[i] << " " << indexMap[j] << " " << indexMap[k] << endl;
        ofs << "Lengths:     " << finalLen[0] << " " << finalLen[1] << " " << finalLen[2] << endl << endl; 
    } else {
        ofs << "Error:  the wrong number of 'final' nodes (" << tmp << ") found -- expected 3.\n" << i << " " << j << " " << k << endl << endl;
#endif    
		*/
    }    
	midpointRootIfNeeded();
    //fillSeqNames(m_tree, m_cdd);
/*
#if _DEBUG
	ofs.close();
#endif
*/
    delete [] indexMap;
    delete [] internalDistCorrection;

    // clean up temp storage of distance matrix
    for (i=0; i<m_nseqs; i++) {
        delete []ppDists[i];
    }
    delete []ppDists;
    delete []pSums;
}

int NJ_TreeAlgorithm::numChildren(const TSeqIt& sit) {

    int n = 0;
    if (sit.is_valid()) {
        n = m_tree->number_of_children(sit);
    }
    return n;

}

string NJ_TreeAlgorithm::toString() {
    return CdTreeStream::toNestedString(*m_tree);
}

// string NJ_TreeAlgorithm::toString(const TSeqIt& sit) {
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
