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
 * Authors:  Josh Cherry
 *
 * File Description:  Distance methods for phylogenic tree reconstruction
 *
 */


#include <ncbi_pch.hpp>
#include <algo/phy_tree/dist_methods.hpp>

#include <math.h>

#include "fastme/graph.h"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// d = -3/4 ln(1 - (4/3)p).
void CDistMethods::JukesCantorDist(const TMatrix& frac_diff,
                                   TMatrix& result)
{
    result.Resize(frac_diff.GetRows(), frac_diff.GetCols());
    for (size_t i = 0;  i < frac_diff.GetRows();  ++i) {
        for (size_t j = 0;  j < frac_diff.GetCols();  ++j) {
            result(i, j) = -0.75 * log(1 - (4.0 / 3.0) * frac_diff(i, j));
        }
    }
}


/// d = -ln(1 - p)
void CDistMethods::PoissonDist(const TMatrix& frac_diff,
                               TMatrix& result)
{
    result.Resize(frac_diff.GetRows(), frac_diff.GetCols());
    for (size_t i = 0;  i < frac_diff.GetRows();  ++i) {
        for (size_t j = 0;  j < frac_diff.GetCols();  ++j) {
            result(i, j) = -log(1 - frac_diff(i, j));
        }
    }
}


/// d = -ln(1 - p - 0.2p^2)
void CDistMethods::KimuraDist(const TMatrix& frac_diff,
                                     TMatrix& result)
{
    result.Resize(frac_diff.GetRows(), frac_diff.GetCols());
    for (size_t i = 0;  i < frac_diff.GetRows();  ++i) {
        for (size_t j = 0;  j < frac_diff.GetCols();  ++j) {
            result(i, j) = -log(1 - frac_diff(i, j)
                                - 0.2 * frac_diff(i, j) * frac_diff(i, j));
        }
    }
}


/// As per Hillis et al. (Ed.), Molecular Systematics, pg. 488-489
CDistMethods::TTree *CDistMethods::NjTree(const TMatrix& dist_mat,
                                          const vector<string>& labels)
{
    // prepare initial tree (a star phylogeny)
    TTree *tree = new TTree;
    for (unsigned int i = 0;  i < dist_mat.GetRows();  ++i) {
        TTree *new_node = tree->AddNode();
        new_node->GetValue().SetId(i);
        if (labels.empty()) {
            new_node->GetValue().SetLabel() = 'N' + NStr::IntToString(i);
        } else {
            new_node->GetValue().SetLabel() = labels[i];
        }
    }

    // now the real work; do N - 2 neighbor joinings
    int next_id = dist_mat.GetRows();
    TMatrix dmat = dist_mat;
    dmat.Resize(2 * dist_mat.GetRows() - 2, 2 * dist_mat.GetRows() - 2);
    vector<double> r(dmat.GetRows());
    for (unsigned int n = dist_mat.GetRows();  n > 2;  --n) {
        int i, j;
        double m;
        TTree::TNodeList_I neighbor1, neighbor2;
        // first compute r_i
        for (TTree::TNodeList_I it1 = tree->SubNodeBegin();
             it1 != tree->SubNodeEnd();  ++it1) {
            i = (*it1)->GetValue().GetId();
            double sum = 0;
            for (TTree::TNodeList_I it2 = tree->SubNodeBegin();
                 it2 != tree->SubNodeEnd();  ++it2) {
                if (it1 == it2) {
                    continue;
                }
                j = (*it2)->GetValue().GetId();
                sum += dmat(i, j);
            }
            r[i] = sum;
        }

        // find where M_{i, j} is minimal
        double min_m = 0;
        for (TTree::TNodeList_I it1 = tree->SubNodeBegin();
             it1 != tree->SubNodeEnd();  ++it1) {
            TTree::TNodeList_I it2 = it1;
            ++it2;
            for (;  it2 != tree->SubNodeEnd();  ++it2) {
                i = (*it1)->GetValue().GetId();
                j = (*it2)->GetValue().GetId();
                m = dmat(i, j) - (r[i] + r[j]) / (n - 2);
                if (m < min_m) {
                    neighbor1 = it1;
                    neighbor2 = it2;
                    min_m = m;
                }
            }
        }
        // join the neighbors
        TTree *new_node = new TTree;
        i = (*neighbor1)->GetValue().GetId();
        j = (*neighbor2)->GetValue().GetId();
        int u = next_id++;
        new_node->GetValue().SetId(u);
        double viu = dmat(i, j) / 2 + (r[i] - r[j]) / (2 * (n - 2));
        double vju = dmat(i, j) - viu;
        (*neighbor1)->GetValue().SetDist(viu);
        (*neighbor2)->GetValue().SetDist(vju);
        new_node->AddNode(tree->DetachNode(neighbor1));
        new_node->AddNode(tree->DetachNode(neighbor2));
        tree->AddNode(new_node);
        // add new distances to dmat
        for (TTree::TNodeList_CI it1 = tree->SubNodeBegin();
             it1 != tree->SubNodeEnd();  ++it1) {
            int k = (*it1)->GetValue().GetId();
            if (k == u) {
                continue;
            }
            dmat(k, u) = dmat(u, k)
                = (dmat(i, k) + dmat(j, k) - dmat(i, j)) / 2;
        }
    }

    // Now the root has just two children, whose distances
    // have not been set.  Could do different things here.
    // Let's make a trifurcation.
    TTree::TNodeList_I it1 = tree->SubNodeBegin();
    TTree::TNodeList_I it2 = it1;
    ++it2;
    if ((*it1)->IsLeaf()) {
        swap(*it1, *it2);
    }
    double d = dmat((*it1)->GetValue().GetId(), (*it2)->GetValue().GetId());
    (*it2)->GetValue().SetDist(d);
    (*it1)->AddNode(tree->DetachNode(it2));
    TTree *rv = tree->DetachNode(it1);
    delete tree;  // just the original root node
    return rv;
}


// The following is a wrapper for Richard Desper's fast minimume evolution
// code.  The user passes in a CDistMethods::TMatrix, and gets back
// a CDistMethods::TTree.  Behind the scenes, the code converts the
// TTMatrix to the representation used by fastme, runs the fastme
// algorithm using fastme_run, and converts the resulting tree
// to the CDistMethods::TTree representation.

BEGIN_SCOPE(fastme)

typedef char boolean;
boolean leaf(meNode *v);
meTree* fastme_run(double** D_in, int numSpecies_in, char **labels,
                   int btype_in, int wtype_in, int ntype_in);
double **initDoubleMatrix(int d);
void freeMatrix(double **D, int size);
void freeTree(meTree *T);

END_SCOPE(fastme)

// Functions to convert from tree representation used by fastme code

static void s_AddFastMeSubtree(fastme::meNode *me_node,
                               CDistMethods::TTree* node,
                               const vector<string>& labels)
{
    if (fastme::leaf(me_node)) {
        int id = NStr::StringToInt(me_node->label);
        node->GetValue().SetId(id);
        if (!labels.empty()) {
            node->GetValue().SetLabel(labels[id]);
        } else {
            node->GetValue().SetLabel(me_node->label);
        }
        return;
    }

    // not a leaf; add two subtrees
    // first left
    CDistMethods::TTree* child_node = node->AddNode();
    child_node->GetValue().SetDist(me_node->leftEdge->distance);
    s_AddFastMeSubtree(me_node->leftEdge->head, child_node, labels);
    // then right
    child_node = node->AddNode();
    child_node->GetValue().SetDist(me_node->rightEdge->distance);
    s_AddFastMeSubtree(me_node->rightEdge->head, child_node, labels);
}


static CDistMethods::TTree* s_ConvertFastMeTree(fastme::meTree *me_tree,
                                                const vector<string>& labels) {

    CDistMethods::TTree *tree = new CDistMethods::TTree;
    fastme::meEdge *edge;
    edge = me_tree->root->leftEdge;

    CDistMethods::TTree *node = tree->AddNode();
    node->GetValue().SetDist(edge->distance);

    int id = NStr::StringToInt(me_tree->root->label);
    node->GetValue().SetId(id);
    if (!labels.empty()) {
        node->GetValue().SetLabel(labels[id]);
    } else {
        node->GetValue().SetLabel(me_tree->root->label);
    }

    s_AddFastMeSubtree(edge->head, tree, labels);

    return tree;
}


// The publically visible interface
CDistMethods::TTree *CDistMethods::FastMeTree(const TMatrix& dist_mat,
                                              const vector<string>& labels,
                                              EFastMePar btype,
                                              EFastMePar wtype,
                                              EFastMePar ntype)
{

    double **dfme = fastme::initDoubleMatrix(dist_mat.GetRows());
    for (unsigned int i = 0;  i < dist_mat.GetRows();  ++i) {
        for (unsigned int j = 0;  j < dist_mat.GetRows();  ++j) {
            dfme[i][j] = dist_mat(i, j);
        }
    }

    // leaf labels for fastme code; just pass in
    // "0", "1", etc., and fill in real labels later
    char **clabels = new char*[dist_mat.GetRows()];
    vector<string> dummy_labels;
    dummy_labels.resize(dist_mat.GetRows());
    for (unsigned int i = 0;  i < dist_mat.GetRows();  ++i) {
        dummy_labels[i] = NStr::IntToString(i);
        clabels[i] = const_cast<char *>(dummy_labels[i].c_str());
    }

    fastme::meTree *me_out =
        fastme::fastme_run(dfme, dist_mat.GetRows(), clabels,
                           btype, wtype, ntype);
    fastme::freeMatrix(dfme, dist_mat.GetRows());
    delete [] clabels;
    TTree *me_tree = s_ConvertFastMeTree(me_out, labels);
    fastme::freeTree(me_out);
    return me_tree;
}


// Calculate pairwise fractional differences

double CDistMethods::Divergence(const string& seq1, const string& seq2)
{
    int diff_count = 0;
    int pos_count = 0;

    for (unsigned int pos = 0;  pos < seq1.size();  ++pos) {
        if (seq1[pos] == '-' || seq2[pos] == '-') {
            continue;  // ignore this position if a seq has a gap
        }
        pos_count++;
        if (seq1[pos] != seq2[pos]) {
            diff_count++;
        }
    }

    return diff_count / (double) pos_count;
}


void CDistMethods::Divergence(const CAlnVec& avec_in, TMatrix& result)
{
    // want to change gap char of CAlnVec, but no copy constructor,
    // so effectively copy in a round-about way
    CAlnVec avec(avec_in.GetDenseg(), avec_in.GetScope());
    avec.SetGapChar('-');

    int nseqs = avec.GetNumRows();
    result.Resize(nseqs, nseqs);

    string seqi, seqj;

    for (int i = 0;  i < nseqs;  ++i) {
        result(i, i) = 0;  // 0 difference from itself
        avec.GetWholeAlnSeqString(i, seqi);
        for (int j = i + 1;  j < nseqs;  ++j) {
            avec.GetWholeAlnSeqString(j, seqj);
            result(i, j) = result(j, i) = CDistMethods::Divergence(seqi, seqj);
        }
    }
}


#if 0
void CDistMethods::Divergence(const CAlignment& aln, TMatrix& result)
{
    int nseqs = aln.GetSeqs().size();
    result.Resize(nseqs, nseqs);

    for (int i = 0;  i < nseqs;  ++i) {
        result(i, i) = 0;  // 0 difference from itself
        for (int j = i + 1;  j < nseqs;  ++j) {
            result(i, j) = result(j, i) = 
                CDistMethods::Divergence(aln.GetSeqs()[i], aln.GetSeqs()[j]);
        }
    }
}
#endif

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.7  2004/03/17 17:57:48  jcherry
 * Made fastme set the node ids of leaf nodes
 *
 * Revision 1.6  2004/02/19 16:43:45  jcherry
 * Temporarily disable one form of Divergence() method
 *
 * Revision 1.5  2004/02/19 13:20:04  dicuccio
 * Roll back to version 1.3
 *
 * Revision 1.3  2004/02/10 23:07:46  ucko
 * +<math.h> for log()
 *
 * Revision 1.2  2004/02/10 17:01:19  dicuccio
 * Use swap() instead of iter_swap(), as the latter isn't found on MSVC
 *
 * Revision 1.1  2004/02/10 15:15:58  jcherry
 * Initial version
 *
 * ===========================================================================
 */

