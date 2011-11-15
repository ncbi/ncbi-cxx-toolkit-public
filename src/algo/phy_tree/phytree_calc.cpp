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
 * Author:  Irena Zaretskaya, Greg Boratyn
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbifloat.h>    

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/align_ci.hpp>

#include <util/bitset/ncbi_bitset.hpp>

#include <algo/phy_tree/phytree_calc.hpp>

#include <math.h>

#ifdef NCBI_COMPILER_MSVC
#  define isfinite _finite
#endif

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CPhyTreeCalc::CDistMatrix::CDistMatrix(int num_elements)
    : m_NumElements(num_elements), m_Diagnol(0.0)
{
    if (num_elements > 0) {
        m_Distances.resize(num_elements * num_elements - num_elements, -1.0);
    }
}

void CPhyTreeCalc::CDistMatrix::Resize(int num_elements)
{
    m_NumElements = num_elements;
    if (num_elements > 0) {
        m_Distances.resize(num_elements * num_elements - num_elements);
    }
}

const double& CPhyTreeCalc::CDistMatrix::operator()(int i, int j) const
{
    if (i >= m_NumElements || j >= m_NumElements || i < 0 || j < 0) {
        NCBI_THROW(CPhyTreeCalcException, eDistMatrixError,
                   "Distance matrix index out of bounds");
    }

    if (i == j) {
        return m_Diagnol;
    }

    if (i < j) {
        swap(i, j);
    }

    int index = (i*i - i) / 2 + j;
    _ASSERT(index < (int)m_Distances.size());

    return m_Distances[index];
}

double& CPhyTreeCalc::CDistMatrix::operator()(int i, int j)
{
    if (i >= m_NumElements || j >= m_NumElements || i < 0 || j < 0) {
        NCBI_THROW(CPhyTreeCalcException, eDistMatrixError,
                   "Distance matrix index out of bounds");
    }

    if (i == j) {
        NCBI_THROW(CPhyTreeCalcException, eDistMatrixError,
                   "Distance matrix diagnol elements cannot be set");
    }

    if (i < j) {
        swap(i, j);
    }

    int index = (i*i - i) / 2 + j;
    _ASSERT(index < (int)m_Distances.size());

    return m_Distances[index];
}


CPhyTreeCalc::CPhyTreeCalc(const CSeq_align& seq_aln,
                               CRef<CScope> scope)
    : m_Scope(scope)
{
    x_Init();
    x_InitAlignDS(seq_aln);
}


CPhyTreeCalc::CPhyTreeCalc(const CSeq_align_set& seq_align_set,
                               CRef<CScope> scope)
    : m_Scope(scope)      
{
    x_Init();
    x_InitAlignDS(seq_align_set);
}

CRef<CBioTreeContainer> CPhyTreeCalc::GetSerialTree(void) const
{
    if (!m_Tree) {
        NCBI_THROW(CPhyTreeCalcException, eNoTree,
                   "Tree was not constructed");
    }

    CRef<CBioTreeContainer> btc = MakeBioTreeContainer(m_Tree);

    return btc;
}

CRef<CSeq_align> CPhyTreeCalc::GetSeqAlign(void) const
{
    CRef<CDense_seg> denseg(new CDense_seg());
    denseg->Assign(m_AlignDataSource->GetDenseg());

    CRef<CSeq_align> seqalign(new CSeq_align());
    seqalign->SetType(CSeq_align::eType_global);
    seqalign->SetSegs().SetDenseg(*denseg);
    seqalign->SetDim(denseg->GetDim());

    return seqalign;
}

const vector< CRef<CSeq_id> >& CPhyTreeCalc::GetSeqIds(void) const
{
    return m_AlignDataSource->GetDenseg().GetIds();
}

// Make sure that matrix has finite non-negative values
bool s_ValidateMatrix(const CPhyTreeCalc::CDistMatrix& mat)
{
    for (int i=0;i < mat.GetNumElements() - 1;i++) {
        for (int j=i+1;j < mat.GetNumElements();j++) {
            double val = mat(i, j);
            if (!finite(val) || val < 0.0) {
                return false;
            }
        }
    }
    return true;
}

// Calculate divergence matrix, considering max divergence constraint
bool CPhyTreeCalc::x_CalcDivergenceMatrix(vector<int>& included)
{
    int num_seqs = m_AlignDataSource->GetNumRows();

    bm::bvector<> bitvector; // which seqeunces will be included in phylo tree
    list<SLink> links;       // for storing dissimilariues between sequences
    vector<string> sequences(num_seqs);  // buffer for AA sequences

    // query has always zero index and is always included
    const int query_idx = 0;

    m_AlignDataSource->GetWholeAlnSeqString(query_idx, sequences[query_idx]);
    const string& query_seq = sequences[query_idx];

    bitvector.set(query_idx);
    
    // Compute distances between query and each sequence
    // and include only sequences that are similar enough to the query

    // for each sequence except for query
    for (int i=1;i < num_seqs;i++) {

        // find divergence
        m_AlignDataSource->GetWholeAlnSeqString(i, sequences[i]);
        double dist = CDistMethods::Divergence(query_seq, sequences[i]);
        _ASSERT(!finite(dist) || dist >= 0.0);

        // if divergence is finite and smaller than cutoff save divergence
        // and mark sequence as included
        if (finite(dist) && dist <= m_MaxDivergence) {
            links.push_back(SLink(query_idx, i, dist));
            bitvector.set(i);
        }
        else {
            // otherwise purge the sequence
            sequences[i].erase();
        }
    }

    // Compute distances between incuded sequences
    list<SLink>::const_iterator it1 = links.begin();

    // for each included sequence 1
    for (; it1 != links.end() && it1->index1 == query_idx; ++it1) {

        // check if still included
        if (!bitvector.test(it1->index2)) {
            continue;
        }
        const string& seqi = sequences[it1->index2];
        _ASSERT(!seqi.empty());

        list<SLink>::const_iterator it2(it1);
        it2++;

        // for each sequence 2
        for (; it2 != links.end() && it2->index1 == query_idx
                 && bitvector.test(it1->index2); ++it2) {

            // check if still included
            if (!bitvector.test(it2->index2)) {
                continue;
            }
            const string& seqj = sequences[it2->index2];
            _ASSERT(!seqj.empty());

            // compute divergence
            double dist = CDistMethods::Divergence(seqi, seqj);
            _ASSERT(!finite(dist) || dist >= 0.0);

            // if divergence finite and smaller than cutoff save divergence
            if (finite(dist) && dist <= m_MaxDivergence) {
                links.push_back(SLink(it2->index2, it1->index2, dist));
            }
            else {
                 
                //TO DO: This should be changed to which divergence
                // otherwise exclude sequence less similar to the query
                int score_1 = m_AlignDataSource->CalculateScore(query_idx,
                                                                it1->index2);
                int score_2 = m_AlignDataSource->CalculateScore(query_idx,
                                                                it2->index2);
            
                bitvector[score_1 > score_2 ? it2->index2 : it1->index2]
                    = false;
            }
        }
    }

    unsigned int skip_array[bm::set_total_blocks] = {0,};
    bitvector.count_blocks(skip_array);

    // Get indices of included sequences
    bm::bvector<>::counted_enumerator en = bitvector.first();
    for (; en.valid(); ++en) {
        included.push_back((int)*en);
    }

    // Create divergence matrix and set values
    m_DivergenceMatrix.Resize(bitvector.count());
    // for each saved divergence
    ITERATE (list<SLink>, it, links) {
        // if both sequences included
        if (bitvector.test(it->index1) && bitvector.test(it->index2)) {

            // set divergence value using bit vector storage encoding

            int index1 = (it->index1 == 0 ? 0 : 
                    bitvector.count_range(0, it->index1 - 1, skip_array));

            int index2 = (it->index2 == 0 ? 0 : 
                    bitvector.count_range(0, it->index2 - 1, skip_array));

            m_DivergenceMatrix(index1, index2) = it->distance;
        }
    }
    _ASSERT(s_ValidateMatrix(m_DivergenceMatrix));

    return included.size() > 1;
}

static void s_RecordSeqId(int index,
                          const CAlnVec& align_data_source,
                          vector<string>& seqids)
{
    CSeq_id_Handle seq_id_handle 
        = sequence::GetId(align_data_source.GetBioseqHandle(index),
                          sequence::eGetId_Best);
                
    CConstRef<CSeq_id> seq_id = seq_id_handle.GetSeqId();
    string seq_id_str = "";
    (*seq_id).GetLabel(&seq_id_str);
            
    seqids.push_back(seq_id_str);
}

void CPhyTreeCalc::x_CreateValidAlign(const vector<int>& used_indices)
{
    if ((int)used_indices.size() < m_AlignDataSource->GetNumRows()) {

        // collect ids of removed sequences
        int index = 0;
        ITERATE (vector<int>, it, used_indices) {
            for (; index < *it; index++) {
                s_RecordSeqId(index, *m_AlignDataSource, m_RemovedSeqIds);
            }
            index++;
        }
        for (; index < m_AlignDataSource->GetNumRows();index++) {
            s_RecordSeqId(index, *m_AlignDataSource, m_RemovedSeqIds);
        }
        _ASSERT(used_indices.size() + m_RemovedSeqIds.size()
                == (size_t)m_AlignDataSource->GetNumRows());

        CRef<CDense_seg> new_denseg
            = m_AlignDataSource->GetDenseg().ExtractRows(used_indices);

        // if sequence labels are specified, remove labels correponding to
        // unused sequences
        if (!m_Labels.empty()) {
            vector<string> new_labels;
            ITERATE (vector<int>, it, used_indices) {
                new_labels.push_back(m_Labels[*it]);
            }
            m_Labels.swap(new_labels);
            _ASSERT((int)m_Labels.size() == m_AlignDataSource->GetNumRows());
        }

        m_AlignDataSource.Reset(new CAlnVec(*new_denseg, *m_Scope));
    }
}

void CPhyTreeCalc::x_CalcDistMatrix(void)
{
    _ASSERT(!m_DivergenceMatrix.Empty());

    // create dist matrix data structure required by CDistMethos
    CDistMethods::TMatrix pmat(m_DivergenceMatrix.GetNumElements(),
                               m_DivergenceMatrix.GetNumElements(),
                               0.0);

    for (int i=0;i < m_DivergenceMatrix.GetNumElements() - 1;i++) {
        for (int j=i+1; j < m_DivergenceMatrix.GetNumElements();j++) {
            pmat(i, j) = pmat(j, i) = m_DivergenceMatrix(i, j);
        }
    }

    // prepare structure for string results of distance computation
    m_FullDistMatrix.Resize(m_DivergenceMatrix.GetNumElements(),
                            m_DivergenceMatrix.GetNumElements(),
                            0.0);

    // compute distances
    switch (m_DistMethod) {
    case eJukesCantor : 
        CDistMethods::JukesCantorDist(pmat, m_FullDistMatrix);
        break;

    case ePoisson :
        CDistMethods::PoissonDist(pmat, m_FullDistMatrix);
        break;

    case eKimura :
        CDistMethods::KimuraDist(pmat, m_FullDistMatrix);
        break;

    case eGrishin :
        CDistMethods::GrishinDist(pmat, m_FullDistMatrix);
        break;

    case eGrishinGeneral :
        CDistMethods::GrishinGeneralDist(pmat, m_FullDistMatrix);
        break;

    default:
        NCBI_THROW(CPhyTreeCalcException, eInvalidOptions,
                   "Invalid distance calculation method");
    }

    // store distances in memory efficient data structure
    m_DistMatrix.Resize(m_FullDistMatrix.GetRows());
    for (int i=0;i < m_DistMatrix.GetNumElements() - 1;i++) {
        for (int j=i+1;j < m_DistMatrix.GetNumElements();j++) {
            m_DistMatrix(i, j) = m_FullDistMatrix(i, j);
        }
    }
    _ASSERT(s_ValidateMatrix(m_DistMatrix));
}

// Compute phylogenetic tree
void CPhyTreeCalc::x_ComputeTree(bool correct)
{
    _ASSERT((size_t)m_AlignDataSource->GetNumRows()
            == m_FullDistMatrix.GetRows());

    // if labels not provided, use sequence indeces as labels
    if (m_Labels.empty()) {
        for (int i=0;i < m_AlignDataSource->GetNumRows();i++) {
            m_Labels.push_back(NStr::IntToString(i));
        }
    }
    _ASSERT((int)m_Labels.size() == m_AlignDataSource->GetNumRows());
 
    m_Tree = NULL;
    switch (m_TreeMethod) {
    case eNJ :
        m_Tree = CDistMethods::NjTree(m_FullDistMatrix, m_Labels);
        break;

    case eFastME :
        m_Tree = CDistMethods::FastMeTree(m_FullDistMatrix, m_Labels);
        break;

    default:
        NCBI_THROW(CPhyTreeCalcException, eInvalidOptions,
                   "Invalid tree reconstruction method");
    };

    if (!m_Tree) {
        NCBI_THROW(CPhyTreeCalcException, eTreeComputationProblem,
                   "Tree was not created");
    }

    m_Tree->GetValue().SetDist(0.0);
    m_Tree = CDistMethods::RerootTree(m_Tree);

    // release memory used by full distance matrix
    m_FullDistMatrix.Resize(1, 1);

    if (correct) {
        x_CorrectBranchLengths(m_Tree);
    }
}

void CPhyTreeCalc::x_CorrectBranchLengths(TPhyTreeNode* node)
{
    _ASSERT(node);
    if (!node->IsLeaf()) {
        for (TPhyTreeNode::TNodeList_CI it = node->SubNodeBegin();
            it != node->SubNodeEnd(); ++it) {
            x_CorrectBranchLengths(*it);
        }
    }

    if (node->GetValue().IsSetDist()) {
        double dist = node->GetValue().GetDist();
        if (!isfinite(dist) || dist < 0.0) {
            node->GetValue().SetDist(0.0);
        }
    }
}

bool CPhyTreeCalc::CalcBioTree(void)
{
    if (!m_Labels.empty()
        && (int)m_Labels.size() != m_AlignDataSource->GetNumRows()) {

        NCBI_THROW(CPhyTreeCalcException, eInvalidOptions, "Number of labels"
                   " is not the same as number of sequences");
    }

    if (m_MaxDivergence < 0.0) {
        NCBI_THROW(CPhyTreeCalcException, eInvalidOptions, "Maximum "
                   "divergence must be positive");
    }


    if (m_DistMethod == eKimura && m_MaxDivergence > 0.85) {
        NCBI_THROW(CPhyTreeCalcException, eInvalidOptions, "Maximum "
                   "divergence must be smaller than 0.85 if Kimura distance is"
                   " selected");
    }

    vector<int> used_inds;

    bool valid;
    valid = x_CalcDivergenceMatrix(used_inds);

    if (valid) {

        if ((int)used_inds.size() < m_AlignDataSource->GetNumRows()) {
            int initial_num_seqs = m_AlignDataSource->GetNumRows();
            x_CreateValidAlign(used_inds);
            m_Messages.push_back(NStr::IntToString(initial_num_seqs
                                                   - used_inds.size())
                                 + " sequences were discarded due to"
                                 " divergence that exceeds maximum allowed.");
        }
        x_CalcDistMatrix();
        x_ComputeTree();

    }
    else {
        m_Messages.push_back("Sequence dissimilarity exceeds maximum"
                             " divergence.");
    }

    return valid;
}

// Init parameters to default values
void CPhyTreeCalc::x_Init(void)
{
    m_DistMethod = eGrishin;
    m_TreeMethod = eFastME;
    m_MaxDivergence = 0.85;
    m_Tree = NULL;
}



bool CPhyTreeCalc::x_InitAlignDS(const CSeq_align_set& seq_align_set)
{
    bool success = true;

    if(seq_align_set.Get().size() == 1) {   
        x_InitAlignDS(**(seq_align_set.Get().begin()));
    }
    else if(seq_align_set.Get().size() > 1) {   
        CAlnMix mix;
        ITERATE (CSeq_annot::TData::TAlign, iter, seq_align_set.Get()) {

            CRef<CSeq_align> seq_align = *iter;            
            mix.Add(**iter);        
        }

        CAlnMix::TMergeFlags merge_flags;
        merge_flags = CAlnMix::fGapJoin | CAlnMix::fTruncateOverlaps;
        mix.Merge(merge_flags);

        x_InitAlignDS(mix.GetSeqAlign());        
    }
    else {        
        success = false;
        NCBI_THROW(CPhyTreeCalcException, eInvalidOptions,
                   "Empty seqalign provided");
    }
    return success;
}



void CPhyTreeCalc::x_InitAlignDS(const CSeq_align& seq_aln)
{
    m_AlignDataSource.Reset(new CAlnVec(seq_aln.GetSegs().GetDenseg(),
                                        *m_Scope));
    m_AlignDataSource->SetGapChar('-');
    m_AlignDataSource->SetEndChar('-');
}

END_NCBI_SCOPE
