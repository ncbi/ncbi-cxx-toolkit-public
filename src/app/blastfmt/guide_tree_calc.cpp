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

/// @guide_tree_calc.cpp
/// Phylogenetic tree calulation

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbifloat.h>    

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/align_ci.hpp>

#include <algo/cobalt/tree.hpp>

#include <gui/objutils/utils.hpp>

#include <math.h>

#include <guide_tree.hpp>
#include <guide_tree_calc.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


static const int    s_kLabelId = 0;
static const int    s_kSeqIdId = 2;
static const int    s_kOrganismId = 3;
static const int    s_kTitleId = 4;
static const int    s_kAccessionNbr = 5;
static const int    s_kBlastName = 6;
static const int    s_kAlignIndexId = 7;
static const int    s_kQueryNodeColorId = 8;
static const int    s_kQueryLabelColorId = 9;
static const int    s_kQueryLabelBgColorId = 10;
static const int    s_kQueryLabelTagColorId = 11;
static const int    s_kTreeSimplificationTagId = 12;
static const string s_kQueryNodeColor = "255 0 0";
static const string s_kQueryLabelColor = "176 0 0";
static const string s_kQueryNodeBgColor = "255 255 0";

static const string s_kUnknown = "unknown";
static const string s_kSubtreeDisplayed = "0";


CGuideTreeCalc::CGuideTreeCalc(const CRef<CSeq_annot>& annot,
                               const CRef<CScope>& scope,
                               const string& query_id,
                               EInputFormat format,
                               const string& accession)
    : m_Scope(scope), 
      m_QuerySeqId(query_id)
{
    x_Init();
    x_InitAlignDS(*annot, format, accession);
}


CGuideTreeCalc::CGuideTreeCalc(const CSeq_align& seq_aln,
                                    const CRef<CScope>& scope,
                                    const string& query_id)
    : m_Scope(scope),
      m_QuerySeqId(query_id)
{
    x_Init();
    x_InitAlignDS(seq_aln);
}


auto_ptr<CBioTreeDynamic> CGuideTreeCalc::GetTree(void)
{
    auto_ptr<CBioTreeDynamic> dyntree(new CBioTreeDynamic());

    BioTreeConvertContainer2Dynamic(*dyntree, *m_TreeContainer);

    return dyntree;
}


// Check if query in seq-align belongs to the query in m_QuerySeqID
static bool s_SeqAlignQueryMatch(CRef<CSeq_align>& seq_align,
                                 const string& query_seq_id)
{
    const CSeq_id& seq_id = seq_align->GetSeq_id(0);
    string query_id;
    seq_id.GetLabel(&query_id);    
    return (query_seq_id == query_id);
}



static const string& s_GetSequence(const CAlnVec& avec,
                                   vector<string> &seq_strings,
                                   unsigned int index)
{
    // The seq_strings array has to be pre-allocated
    _ASSERT(index < seq_strings.size());

    if (!seq_strings[index].empty()) {
        return seq_strings[index];
    }
    else {
        avec.GetWholeAlnSeqString(index, seq_strings[index]);
        return seq_strings[index];
    }
}


static bool IsIn(const hash_set<int>& set, int val)
{
    hash_set<int>::iterator it = set.find(val);
    return it != set.end();
}


// Calculate divergence matrix, considering max divergence constraint
bool CGuideTreeCalc::CalcDivergenceMatrix(CDistMethods::TMatrix& pmat,
                                          double max_divergence,
                                          hash_set<int>& removed) const
{
    const CAlnVec& alnvec = m_AlignDataSource->GetAlnMgr();
    int num_seqs = alnvec.GetNumRows();
    pmat.Resize(num_seqs, num_seqs);

    vector<string> sequences(num_seqs);

    const string& query_seq = s_GetSequence(alnvec, sequences, 0);

    // Compute distances to query
    // and remove sequences to distant from the query
    for (int i=1;i < num_seqs;i++) {
        // Divergence between same seqs is zero
        pmat(i, i) = 0;

        const string& seqi = s_GetSequence(alnvec, sequences, i);
        double dist = CDistMethods::Divergence(query_seq, seqi);
        _ASSERT(!finite(dist) || dist >= 0.0);

        pmat(i, 0) = pmat(0, i) = dist;

        if (!finite(dist) || dist > max_divergence) {
            removed.insert(i);
        }

    }

    // Same for all remaning pairs
    // for each sequence
    for (int i=1;i < num_seqs - 1;i++) { //skiping query

        // check if already removed
        if (IsIn(removed, i)) {
            continue;
        }
        const string& seqi = s_GetSequence(alnvec, sequences, i);

        // for each sequence
        for (int j=i+1;j < num_seqs;j++) {

            if (IsIn(removed, j)) {
                continue;
            }
            const string& seqj = s_GetSequence(alnvec, sequences, j);

            double dist = CDistMethods::Divergence(seqi, seqj);
            _ASSERT(!finite(dist) || dist >= 0.0);

            pmat(i, j) = pmat(j, i) = dist;

            if(finite(pmat(i, j)) && dist <= max_divergence) {
                continue;
            }
            
            // if disvergence too large compute scores for alignement
            // with query and remove lower scoring one
            int score_i = alnvec.CalculateScore(0,i);
            int score_j = alnvec.CalculateScore(0,j);
            
            removed.insert(score_i > score_j ? j : i);
            
        }
    }

    return (int)removed.size() < (num_seqs - 1);
}


CRef<CAlnVec> CGuideTreeCalc::CreateValidAlign(
                                         const hash_set<int>& removed_inds,
                                         vector<int>& new_align_index)
{
    CRef<CAlnVec> alnvec;
    alnvec.Reset(new CAlnVec(m_AlignDataSource->GetAlnMgr().GetDenseg(),
                             *m_Scope));
    alnvec->SetGapChar('-');
    alnvec->SetEndChar('-');


    if(!removed_inds.empty()) {
    

        //remove invalid for phylotree gi from alignment
        int num_seqs = alnvec->GetNumRows();

        new_align_index.clear();

        for (int i=0; i < num_seqs;i++) {
            if(!IsIn(removed_inds, i)) {
                new_align_index.push_back(i);
            }
            else //record removed gis
            {
                CSeq_id_Handle seq_id_handle 
                    = sequence::GetId(alnvec->GetBioseqHandle(i),
                                      sequence:: eGetId_Best);

                CConstRef<CSeq_id> seq_id = seq_id_handle.GetSeqId();
                string seq_id_str = "";
                (*seq_id).GetLabel(&seq_id_str);

                m_RemovedSeqIds.push_back(seq_id_str);
            }

        }
        //newindex contains valid for phylo_tree gis

        alnvec = x_CreateSubsetAlign(alnvec, new_align_index, false);

    }        
    return alnvec;

}


void CGuideTreeCalc::TrimMatrix(CDistMethods::TMatrix& pmat,
                                const vector<int>& used_inds)
{
    _ASSERT(used_inds.size() <= pmat.GetRows());

    if (used_inds.size() == pmat.GetRows()) {
        return;
    }

    int new_size = used_inds.size();
    CDistMethods::TMatrix new_mat(new_size, new_size, 0.0);
    for (int i=0;i < new_size - 1;i++) {
        for (int j=i+1;j < new_size;j++) {
            new_mat(i, j) = new_mat(j, i) = pmat(used_inds[i], used_inds[j]);
        }
    }
    pmat.Swap(new_mat);
}


// Calculate pairwise distances for sequeneces in m_AlignDataSource->GetAlnMgr
void CGuideTreeCalc::CalcDistMatrix(const CDistMethods::TMatrix& pmat,
                                    CDistMethods::TMatrix& result,
                                    EDistMethod method)
{
    switch (method) {
    case eJukesCantor : 
        CDistMethods::JukesCantorDist(pmat, result);
        break;

    case ePoisson :
        CDistMethods::PoissonDist(pmat, result);
        break;

    case eKimura :
        CDistMethods::KimuraDist(pmat, result);
        break;

    case eGrishin :
        CDistMethods::GrishinDist(pmat, result);
        break;

    case eGrishinGeneral :
        CDistMethods::GrishinGeneralDist(pmat, result);
        break;

    default:
        NCBI_THROW(CException, eUnknown, "Invalid distance calculation method");
    }
}

// Compute phylogenetic tree
void CGuideTreeCalc::ComputeTree(const CAlnVec& alnvec,
                                 const CDistMethods::TMatrix& dmat,
                                 ETreeMethod method,
                                 bool correct)
{
    _ASSERT((size_t)alnvec.GetNumRows() == dmat.GetRows());

    // Build a tree based on distances
    // Use number as labels, and sort it out later
    // Number corresponds to the position in  alnVec
    vector<string> labels(alnvec.GetNumRows());
    for (unsigned int i = 0;i < labels.size();i++) {
        labels[i] = NStr::IntToString(i);
    }
    
    // TODO: Re-rooting now handled through CTree class needs to be implemented
    // in CGuideTreeCalc
    cobalt::CTree tree;

    switch (method) {
    case eNeighborJoining :
        tree.ComputeTree(dmat, false);
        break;

    case eFastME :
        tree.ComputeTree(dmat, true);
        break;

    default:
        NCBI_THROW(CException, eUnknown, "Invalid tree reconstruction method");
    };


    if (!tree.GetTree()) {
        NCBI_THROW(CException, eUnknown, "Tree was not created");
    }

    TPhyTreeNode* ptree = (TPhyTreeNode*)tree.GetTree();

    if (correct) {
        CDistMethods::ZeroNegativeBranches(ptree);
    }

    m_TreeContainer = MakeBioTreeContainer(ptree);

    x_InitTreeFeatures(alnvec);
}


bool CGuideTreeCalc::CalcBioTree(void)
{

    CDistMethods::TMatrix pmat;
    CDistMethods::TMatrix dmat;
    hash_set<int> removed_inds;
    vector<int> used_inds;

    bool valid;
    valid = CalcDivergenceMatrix(pmat, m_MaxDivergence, removed_inds);

    if (valid) {
        CRef<CAlnVec> alnvec;

        if (!removed_inds.empty()) {
            alnvec = CreateValidAlign(removed_inds, used_inds);
            TrimMatrix(pmat, used_inds);
        }
        else {
            alnvec.Reset(new CAlnVec(m_AlignDataSource->GetAlnMgr().GetDenseg(),
                                     *m_Scope));
        }

        CalcDistMatrix(pmat, dmat, m_DistMethod);
        ComputeTree(*alnvec, dmat, m_TreeMethod);

    }

    return valid;
}


/*
// Needed for displaying alignment
bool CGuideTreeCalc::CalcSelectedAligns(int selected_node_id)
{
    CRef<CAlnVec> alignvec;
    bool success = true;

    if(m_AlignDataSource.Empty()) {   
        // Create m_SeqAlign and initialize m_AlignDataSource
        success = x_InitAlignDS();
        if(success) {
            // Validate distances, if invalid seqs are found
            // initialize m_AlignDataSource with m_SeqAlign 
//            x_CreatePhyloValidAlign();
            CreateValidAlign();
        }
    }
    if(success) {        
        const CAlnVec& alnvec = m_AlignDataSource->GetAlnMgr();
        alignvec.Reset(const_cast<CAlnVec*>(&alnVec));        
    }
    return success;
}
*/


// Init parameters to default values
void CGuideTreeCalc::x_Init(void)
{
    m_DistMethod = eGrishin;
    m_TreeMethod = eFastME;
    m_LabelType = eSeqTitle;
    m_MarkQueryNode = true;
    m_QueryNodeId = -1;
    m_MaxDivergence = 0.85;
}


bool CGuideTreeCalc::x_CreateMixForASN1(const CSeq_annot& annot, CAlnMix& mix)
{
    bool success = false;

    //    bool query_seq_align_found = false;        
    ITERATE (CSeq_annot::TData::TAlign, iter, 
             annot.GetData().GetAlign()) {

        CRef<CSeq_align> seq_align = *iter;
        
        if (s_SeqAlignQueryMatch(seq_align, m_QuerySeqId)) {
            success = true;
            mix.Add(**iter);
        }
    }   

    return success;
}


// This function creates CAlnMix for alignments in accession
// located in m_inputParamVal
// (m_annot is set from RID)
bool CGuideTreeCalc::x_CreateMixForAccessNum(const string& accession,
                                             CAlnMix& mix)
{
    bool success = true;

    CSeq_id id(accession);
    if (id.Which() == CSeq_id::e_not_set) {

        NCBI_THROW(CException, eUnknown, (string)"Accession " + accession
                   + "not recognized as a valid accession");
        success = false;
    }
    else {
        
        // Retrieve our sequence
        CBioseq_Handle handle = m_Scope->GetBioseqHandle(id);
        if (handle) {
            SAnnotSelector sel =
                CSeqUtils::GetAnnotSelector(CSeq_annot::TData::e_Align);
            CAlign_CI iter(handle, sel);
    
            for (; iter; ++iter) {
                mix.Add(*iter);
            }
        }
        else {
            NCBI_THROW(CException, eUnknown,
                       (string)"Can't find sequence for accession " + accession);
            success = false;
        }
    }
    return success;
}


// Create m_SeqAlign and initialize m_AlignDataSource
bool CGuideTreeCalc::x_InitAlignDS(const CSeq_annot& annot, EInputFormat format,
                                   const string& accession)
{
    bool success = false;
    CAlnMix mix;
    
    switch (format) {
    case eAccession:
        _ASSERT(!accession.empty());
        success = x_CreateMixForAccessNum(accession, mix);
        break;

    case eRID:
    case eASN1:
        success = x_CreateMixForASN1(annot, mix);
        break;
    }

    if (success ) {
        
        CAlnMix::TMergeFlags merge_flags;
        merge_flags = CAlnMix::fGapJoin | CAlnMix::fTruncateOverlaps;
        mix.Merge(merge_flags);
        x_InitAlignDS(mix.GetSeqAlign());
    }
    return success;
}



// Initialize m_AlignDataSource with m_SeqAlign 
void CGuideTreeCalc::x_InitAlignDS(const CSeq_align& seq_aln)
{
    m_AlignDataSource.Reset(new CAlignDataSource());  
    CRef<CAlnVec> alnvec;
    alnvec.Reset(new CAlnVec(seq_aln.GetSegs().GetDenseg(), *m_Scope));       
    alnvec->SetGapChar('-');   
    alnvec->SetEndChar('-');     
    m_AlignDataSource->Init(*alnvec);        
}


// Find the first segment that does not have all gaps
static void s_GetNonGapSegmentRange(const CAlnVec& avec,
                                    const vector<int>& align_index,
                                    int* seg_start, int* seg_stop)
{
    const size_t& num_segs = avec.GetNumSegs(); 
    const size_t& num_rows = avec.GetNumRows(); 
    const CDense_seg& denseSeq = avec.GetDenseg();
    const CDense_seg::TStarts&  starts  = denseSeq.GetStarts();
    
    bool segment_start = false;
    bool segment_stop = false;
    size_t seg;
    for (seg=0;seg < num_segs;seg++) {
        
        for(size_t i=1;i < align_index.size();i++)
        {
            int sel_aln_row = align_index[i];
            int curr_start = starts[seg*num_rows + sel_aln_row];
            if(curr_start != -1) {
                segment_start = true;
                break;
            }            
        }
        if(segment_start) {
            break;
        }
    }
    *seg_start = (seg < num_segs) ? seg : 0;
    for (seg=num_segs - 1;seg >= 0;seg--) {
        
        for(size_t i=1;i < align_index.size();i++)
        {
            int sel_aln_row = align_index[i];
            
            if(starts[seg * num_rows + sel_aln_row] != -1) {
                segment_stop = true;                
                break;
             }            
        }
        if(segment_stop) {
            break;
        }
    }
    *seg_stop = (seg > 0) ? seg + 1 : num_segs;
}


// Create seq_align from a subset of given alnvec
CRef<CAlnVec> CGuideTreeCalc::x_CreateSubsetAlign(const CRef<CAlnVec>& alnvec,
                                        const vector<int>& align_index,
                                        bool create_align_set) 
{
    CAlnVec avec(alnvec->GetDenseg(), alnvec->GetScope());
    avec.SetGapChar('-');  
    avec.SetEndChar('-');
   
    // Create dense-seg from curent avec
    const CDense_seg& dense_seq = avec.GetDenseg();
    const size_t& num_segs = avec.GetNumSegs(); 
    const size_t& num_rows = avec.GetNumRows(); 
    const CDense_seg::TStarts& starts  = dense_seq.GetStarts();
    const CDense_seg::TStrands& strands = dense_seq.GetStrands();
    const CDense_seg::TLens& lens = dense_seq.GetLens();

    // Create new dense-seg
    CRef<CDense_seg> new_dseg(new CDense_seg);
    CDense_seg::TStarts& new_starts = new_dseg->SetStarts();
    CDense_seg::TStrands& new_strands = new_dseg->SetStrands();
    CDense_seg::TLens& new_lens = new_dseg->SetLens();
    CDense_seg::TIds&  new_ids = new_dseg->SetIds();

    // Compute dimensions
    int new_num_rows = align_index.size();
    new_dseg->SetDim(new_num_rows);
    CDense_seg::TNumseg& new_num_segs = new_dseg->SetNumseg();
    new_num_segs = num_segs; // initialize

    // Assign ids
    new_ids.resize(new_num_rows);
    for (int i=0;i < new_num_rows;i++) {
        int sel_aln_row = align_index[i];
        const CSeq_id& seq_id = avec.GetSeqId(sel_aln_row);
        CRef<CSeq_id> selected_seq_id(new CSeq_id);
        SerialAssign(*selected_seq_id, seq_id);
        new_ids[i] = selected_seq_id;
    }
    new_lens.resize(new_num_segs);
    new_starts.resize(new_num_rows * new_num_segs, -1);
    if (!strands.empty()) {
        new_strands.resize(new_num_rows * new_num_segs, eNa_strand_minus);
    }
    
    // main loop through segments
    int new_seg = 0;

    // Delete the whole segment if all starts = -1 
    int seg_start = 0;
    int seg_stop = new_num_segs;
    if(align_index.size() > 1) {
        s_GetNonGapSegmentRange(avec, align_index, &seg_start, &seg_stop);
    }

    for (int seg=seg_start;seg < seg_stop;seg++) {

        new_lens[new_seg] = lens[seg];
        bool delete_gap_seg = true;
        bool seg_cont_query_ins = false;
        for (int row=0;row < new_num_rows;row++) {            
            int sel_aln_row = align_index[row];

            delete_gap_seg = (starts[seg*num_rows + sel_aln_row] == -1);


            new_starts[new_seg*new_num_rows + row] = starts[seg*num_rows
                                                            + sel_aln_row];
            if (!strands.empty()) {
                new_strands[new_seg*new_num_rows + row] = strands[seg*num_rows
                                                                  + sel_aln_row];
            }
            
        }        
        if(!delete_gap_seg) {
            new_seg++;
        }
        // If extra segment was inserted to keep query continious
        if(seg_cont_query_ins) {
           seg--;            // don't increment current seg
        }
    }
    new_num_segs = new_seg;
    
    new_lens.resize(new_num_segs);
    new_starts.resize(new_num_rows * new_num_segs, -1);
    if (!strands.empty()) {
        new_strands.resize(new_num_rows * new_num_segs);
    }
           
    new_dseg->Validate(true);

    CRef<CAlnVec> new_alnvec(new CAlnVec(*new_dseg, *m_Scope));
    return new_alnvec;
}


#define MAX_NODES_TO_COLOR 24
static string s_GetBlastNameColor(
                     CGuideTreeCalc::TBlastNameColorMap& blast_name_color_map,
                     string blast_tax_name)
{
    string color = "";

    //number of blast taxgroups ???
    //This should be rewritten in more elegant way
    string colors[MAX_NODES_TO_COLOR] 
                   = {"0 0 255", "0 255 0", "191 159 0", "30 144 255",
                      "255 0 255", "223 11 95", "95 79 95", "143 143 47",
                      "0 100 0", "128 0 0", "175 127 255", "119 136 153",
                      "255 69 0", "205 102 0", "0 250 154", "173 255 47",
                      "139 0 0", "255 131 250", "155 48 255", "205 133 0",
                      "127 255 212", "255 222 173", "221 160 221", "200 100 0"};


    unsigned int i = 0;
    for(;i < blast_name_color_map.size();i++) {
        pair<string, string>& map_item = blast_name_color_map[i];

        if(map_item.first == blast_tax_name) {
            color = map_item.second;
            break;
        }
    }
    
    if(color == "") { //blast name not in the map
        if(blast_name_color_map.size() >= MAX_NODES_TO_COLOR) {
            i = MAX_NODES_TO_COLOR - 1;
        }
        color = colors[i];
        blast_name_color_map.push_back(make_pair(blast_tax_name, color));
    }
    return color;
}    


// Get SeqID string from CBioseq_Handle such as gi|36537373
// If getGIFirst tries to get gi. If gi does not exist tries to get be 'Best ID'
static string s_GetSeqIDString(CBioseq_Handle& handle, bool get_gi_first)
{
    CSeq_id_Handle seq_id_handle;
    bool get_best_id = true;

    if(get_gi_first) {        
        try {
            seq_id_handle = sequence::GetId(handle, sequence::eGetId_ForceGi);
            get_best_id = false;
        }
        catch(CException& e) {
            //x_TraceLog("sequence::GetId-ForceGi error"  + (string)e.what());
            //seq_id_handle = sequence::GetId(handle,sequence::eGetId_Best);                 
        }
    }

    if(get_best_id) {
        seq_id_handle = sequence::GetId(handle, sequence::eGetId_Best);                 
    }
    CConstRef<CSeq_id> seq_id = seq_id_handle.GetSeqId();

    string id_string;
    (*seq_id).GetLabel(&id_string);

    return id_string;
}


//Init lables for the tree nodes
void CGuideTreeCalc::x_InitTreeFeatures(const CAlnVec& alnvec)
{
    _ASSERT(m_TreeContainer.NotEmpty());

    CTaxon1 tax;

    bool success = tax.Init();
//    if (!success) {
//        NCBI_THROW();
//    }
    
    // Come up with some labels for the terminal nodes
    int num_rows = alnvec.GetNumRows();
    vector<string> labels(num_rows);
    vector<string> organisms(num_rows);
    vector<string> accession_nbrs(num_rows);    
    vector<string> titles(num_rows);
    vector<string> blast_names(num_rows);    
    vector<string> tax_node_colors(num_rows);
    vector<CBioseq_Handle> bio_seq_handles(num_rows);

    for (int i=0;i < num_rows;i++) {
        bio_seq_handles[i] = alnvec.GetBioseqHandle(i);

        int tax_id = 0;
        try{
            const COrg_ref& org_ref = sequence::GetOrg_ref(bio_seq_handles[i]);                                
            organisms[i] = org_ref.GetTaxname();
            tax_id = org_ref.GetTaxId();
            tax.GetBlastName(tax_id, blast_names[i]);
        }
        catch(CException& e) {            
            organisms[i] = s_kUnknown;
            blast_names[i]= s_kUnknown;
        }

        try{
            titles[i] = sequence::GetTitle(bio_seq_handles[i]);
        }
        catch(CException& e) {
            titles[i] = s_kUnknown;
        }
                   
        //May be we need here eForceAccession here ???
        CSeq_id_Handle accession_handle = sequence::GetId(bio_seq_handles[i],
                                                        sequence::eGetId_Best);

        CConstRef<CSeq_id> accession = accession_handle.GetSeqId();
        (*accession).GetLabel(&accession_nbrs[i]);

        tax_node_colors[i] = s_GetBlastNameColor(m_BlastNameColorMap,
                                                 blast_names[i]);

        switch (m_LabelType) {
        case eTaxName:
            labels[i] = organisms[i];
            break;

        case eSeqTitle:
            labels[i] = titles[i];
            break;

        case eBlastName:
            labels[i] = blast_names[i];
            break;

        case eSeqId:
            labels[i] = accession_nbrs[i];
            break;

        case eSeqIdAndBlastName:
            labels[i] = accession_nbrs[i] + "(" + blast_names[i] + ")";
            break;
        }


        if (labels[i].empty()) {
            CSeq_id_Handle best_id_handle = sequence::GetId(bio_seq_handles[i],
                                                       sequence::eGetId_Best);

            CConstRef<CSeq_id> best_id = best_id_handle.GetSeqId();
            (*best_id).GetLabel(&labels[i]);            
        }

    }
    
    // Add attributes to terminal nodes
    x_AddFeatureDesc(s_kSeqIdId, CGuideTree::kSeqIDTag, m_TreeContainer);
    x_AddFeatureDesc(s_kOrganismId, CGuideTree::kOrganismTag, m_TreeContainer);
    x_AddFeatureDesc(s_kTitleId, CGuideTree::kSeqTitleTag, m_TreeContainer);
    x_AddFeatureDesc(s_kAccessionNbr, CGuideTree::kAccessionNbrTag,
                     m_TreeContainer);

    x_AddFeatureDesc(s_kBlastName, CGuideTree::kBlastNameTag, m_TreeContainer);
    x_AddFeatureDesc(s_kAlignIndexId, CGuideTree::kAlignIndexIdTag,
                     m_TreeContainer);

    x_AddFeatureDesc(s_kQueryNodeColorId, CGuideTree::kNodeColorTag,
                     m_TreeContainer);

    x_AddFeatureDesc(s_kQueryLabelColorId, CGuideTree::kLabelColorTag,
                     m_TreeContainer);

    x_AddFeatureDesc(s_kQueryLabelBgColorId, CGuideTree::kLabelBgColorTag,
                     m_TreeContainer);

    x_AddFeatureDesc(s_kQueryLabelTagColorId, CGuideTree::kLabelTagColor,
                     m_TreeContainer);

    x_AddFeatureDesc(s_kTreeSimplificationTagId, CGuideTree::kCollapseTag,
                     m_TreeContainer);

    
    NON_CONST_ITERATE (CNodeSet::Tdata, node,
                       m_TreeContainer->SetNodes().Set()) {
        if ((*node)->CanGetFeatures()) {
            NON_CONST_ITERATE (CNodeFeatureSet::Tdata, node_feature,
                               (*node)->SetFeatures().Set()) {
                if ((*node_feature)->GetFeatureid() == s_kLabelId) {
                    // a terminal node
                    // figure out which sequence this corresponds to
                    // from the numerical id we stuck in as a label
                    
                    string label_id = (*node_feature)->GetValue();
                    unsigned int seq_number;
                    if(!isdigit((unsigned char) label_id[0])) {
                        const char* ptr = label_id.c_str();
                        // For some reason there is "N<number>" now, 
                        // not numerical any more. Need to skip "N" 
                        seq_number = NStr::StringToInt((string)++ptr);
                    }
                    else {
                        seq_number = NStr::StringToInt(
                                                 (*node_feature)->GetValue());                    
                    }
                    // Replace numeric label with real label
                    (*node_feature)->SetValue(labels[seq_number]);

                    //Gets gi, if cnnot gets best id
                    string id_string 
                        = s_GetSeqIDString(bio_seq_handles[seq_number], true);

                    x_AddFeature(s_kSeqIdId, id_string, node); 

                    // add organism attribute if possible
                    if (!organisms[seq_number].empty()) {
                        x_AddFeature(s_kOrganismId, organisms[seq_number], node);
                    }

                    // add seq-title attribute if possible
                    if (!titles[seq_number].empty()) {
                        x_AddFeature(s_kTitleId, titles[seq_number], node); 
                    }
                    // add blast-name attribute if possible
                    if (!accession_nbrs[seq_number].empty()) {
                        x_AddFeature(s_kAccessionNbr, accession_nbrs[seq_number],
                                     node);
                    }

                    // add blast-name attribute if possible
                    if (!blast_names[seq_number].empty()) {
                        x_AddFeature(s_kBlastName, blast_names[seq_number],
                                     node);
                    }                   

                    x_AddFeature(s_kAlignIndexId, NStr::IntToString(seq_number),
                                 node); 

                    x_AddFeature(s_kQueryNodeColorId,
                                 tax_node_colors[seq_number], node);                         

                    if(seq_number == 0 && m_MarkQueryNode) { 
                        // color for query node
                        x_AddFeature(s_kQueryLabelBgColorId,
                                     s_kQueryNodeBgColor, node); 

                        x_AddFeature(s_kQueryLabelTagColorId,
                                     s_kQueryNodeColor, node); 

                        //Not sure if needed
                        //m_QueryAccessionNbr = accession_nbrs[seq_number];

                        m_QueryNodeId = (*node).GetObject().GetId();
                    }
                    
                    // done with this node
                    break;
                }
            }
            x_AddFeature(s_kTreeSimplificationTagId, s_kSubtreeDisplayed, node);
        }
    }      
}

// Add feature descriptor in bio tree
void CGuideTreeCalc::x_AddFeatureDesc(int id, const string& desc, 
                                      CRef<CBioTreeContainer>& btc) 
{
    CRef<CFeatureDescr> feat_descr(new CFeatureDescr);
    feat_descr->SetId(id);
    feat_descr->SetName(desc);
    btc->SetFdict().Set().push_back(feat_descr);
}   


// Add feature to a node in bio tree
void CGuideTreeCalc::x_AddFeature(int id, const string& value,
                                  CNodeSet::Tdata::iterator iter) 
{
    CRef<CNodeFeature> node_feature(new CNodeFeature);
    node_feature->SetFeatureid(id);
    node_feature->SetValue(value);
    (*iter)->SetFeatures().Set().push_back(node_feature);
}

