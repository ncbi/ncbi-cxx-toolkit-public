#ifndef GUIDE_TREE_CALC__HPP
#define GUIDE_TREE_CALC__HPP
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

/// @phylo_tree_calc.hpp
/// Phylogenetic tree calculation
///
/// This file provides various functions used for phylo tree calculation. 


#include <corelib/ncbiobj.hpp>

#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>
#include <algo/phy_tree/dist_methods.hpp>

#include <objects/biotree/BioTreeContainer.hpp>
#include <objects/biotree/NodeSet.hpp>

#include <objtools/alnmgr/alnmix.hpp>

#include <gui/widgets/aln_data/align_ds.hpp>

#include <hash_set.h>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


//static const string s_kPhyloTreeQuerySeqIndex = "0";


class CGuideTreeCalc : public CObject {
    
public:
    /// Possible inputs
    ///
    enum EInputFormat {
        eASN1, eAccession, eRID
    };


    /// Methods for computing evolutionary distance
    ///
    enum EDistMethod {
        eJukesCantor, ePoisson, eKimura, eGrishin, eGrishinGeneral
    };


    /// Algorithms for phylogenetic tree reconstruction
    ///
    enum ETreeMethod {
        eNeighborJoining, eFastME
    };


    /// Information shown as labels in the guide tree
    ///
    enum ELabelType {
        eTaxName, eSeqTitle, eBlastName, eSeqId, eSeqIdAndBlastName
    };

    
    typedef pair<string, string> TBlastNameToColor;
    typedef vector<TBlastNameToColor> TBlastNameColorMap;


public:

    //--- Constructors ---

    /// Create CGuideTreeCalc from Seq-annot
    /// @param annot Seq-annot [in]
    /// @param scope Scope [in]
    /// @param query_id Seqid of query sequence [in]
    /// @param format Input format [in]
    /// @param accession Accession
    ///
    CGuideTreeCalc(const CRef<CSeq_annot>& annot, const CRef<CScope>& scope,
                   const string &query_id, EInputFormat format,
                   const string& accession);

    /// Create CGuideTreeCalc from Seq-align
    /// @param seq_aln Seq-align [in]
    /// @param scope Scope [in]
    /// @param query_id Seqid of query sequence [in]
    ///
    CGuideTreeCalc(const CSeq_align& seq_aln, const CRef<CScope>& scope,
                   const string& query_id = "");

    ~CGuideTreeCalc() {}


    //--- Setters ---

    /// Set maximum allowed divergence between sequences included in tree
    //  reconstruction
    /// @param div [in]
    ///
    void SetMaxDivergence(double div) {m_MaxDivergence = div;}


    /// Set evolutionary correction method for computing distance between
    /// sequences
    /// @param method Distance method [in]
    ///
    void SetDistMethod(EDistMethod method) {m_DistMethod = method;}


    /// Set type of labels in guide tree
    /// @param label_type Label type
    ///
    void SetLabelType(ELabelType label_type) {m_LabelType = label_type;}


    /// Set marking of query node in the tree
    /// @param mark If true, query node will be marked
    ///
    void SetMarkQueryNode(bool mark) {m_MarkQueryNode = mark;}


    //--- Getters ---

    /// Get serial tree
    /// @return Tree
    ///
    const CBioTreeContainer& GetSerialTree(void) const {return *m_TreeContainer;}


    /// Get tree
    /// @return Tree
    ///
    auto_ptr<CBioTreeDynamic> GetTree(void);

    /// Get maximum allowed diveregence between sequences included in tree
    /// reconstruction
    /// @return Max divergence
    ///
    double GetMaxDivergence(void) const {return m_MaxDivergence;}


    /// Get evolutionary correction method for computing distance between
    /// sequences
    /// @return Distance method
    ///
    EDistMethod GetDistMethod(void) const {return m_DistMethod;}


    /// Get tree node id for the query sequence
    /// @return Query node id
    ///
    int GetQueryNodeId(void) const {return m_QueryNodeId;}


    /// Get type of tree labels
    /// @return Type of tree labels
    ///
    ELabelType GetLabelType(void) const {return m_LabelType;}


    /// Is query node going to be marked
    /// @return Query node marked if true, not marked otherwise
    ///
    bool GetMarkQueryNode(void) const {return m_MarkQueryNode;}


    /// Get ids of sequences excluded from tree computation
    /// @return Ids of excluded sequences
    ///
    const vector<string>& GetRemovedSeqIds(void) const {return m_RemovedSeqIds;}

    /// Get BlastName-to-Color map
    /// @return BlastName-to-color map
    ///
    const TBlastNameColorMap& GetBlastNameColorMap(void) const
    {return m_BlastNameColorMap;}



    //--- Tree computation ---

    /// Compute divergence matrix and find sequences to exclude from tree
    /// reconstruction
    /// @param pmat Divergence matrix [out]
    /// @param max_divergence Maximum allowed divergence [in]
    /// @param removed Set of indices of excluded sequences [out]
    /// @return True if at least two sequences are left for tree computation,
    /// false otherwise
    ///
    bool CalcDivergenceMatrix(CDistMethods::TMatrix& pmat,
                              double max_divergence,
                              hash_set<int>& removed) const;


    /// Create alignment only for sequences included in tree computation
    /// @param removed_inds Indices of excluded sequences [in]
    /// @param new_align_index Indices of included sequences [out]
    /// @return New alignment vector
    ///
    CRef<CAlnVec> CreateValidAlign(const hash_set<int>& removed_inds,
                                   vector<int>& new_align_index);

    /// Remove entries coresponding to excluded sequences from divergence
    /// matrix
    /// @param pmat Divergence matrix [in|out]
    /// @param used_inds Indices of sequences included in tree computation [in]
    ///
    static void TrimMatrix(CDistMethods::TMatrix& pmat,
                           const vector<int>& used_inds);


    /// Compute phylogenetic tree
    /// @param alnvec Alignment vector [in]
    /// @param dmat Distance matrix [in]
    /// @param method Tree reconstruction method [in]
    /// @param correct Whether negative tree egde lengths should be set to zero
    /// [in]
    ///
    void ComputeTree(const CAlnVec& alnvec, const CDistMethods::TMatrix& dmat,
                     ETreeMethod method, bool correct = true);       


    /// Compute distance matrix -- apply evolutionary correction to divergence
    /// @param pmat Divergence matrix [in]
    /// @param result Distance matrix [out]
    /// @param method Distance computation method [in]
    ///
    static void CalcDistMatrix(const CDistMethods::TMatrix& pmat,
                               CDistMethods::TMatrix& result,
                               EDistMethod method);

    /// Compute bio tree for the current alignment in a black box manner
    /// @return True is computation successful, false otherwise
    ///
    bool CalcBioTree(void);


protected:

    void x_Init(void);

    bool x_InitAlignDS(const CSeq_annot& annot, EInputFormat format,
                       const string& accession = "");

    void x_InitAlignDS(const CSeq_align& seq_aln);
    bool x_CreateMixForASN1(const CSeq_annot& annot, CAlnMix& mix);
    bool x_CreateMixForAccessNum(const string& accession, CAlnMix& mix);

    CRef<CAlnVec> x_CreateSubsetAlign(const CRef<CAlnVec>& alnvec,
                                      const vector<int>& align_index,
                                      bool create_align_set);


    void x_InitTreeFeatures(const CAlnVec& alnvec);


    static void x_AddFeatureDesc(int id, const string& desc,
                                 CRef<CBioTreeContainer>& btc); 

    static void x_AddFeature(int id, const string& value,
                             CNodeSet::Tdata::iterator iter);    


protected:

    CRef<CScope> m_Scope;

    CRef<CAlignDataSource> m_AlignDataSource;

    string m_QuerySeqId;

    double m_MaxDivergence;
    EDistMethod m_DistMethod;
    ETreeMethod m_TreeMethod;
    ELabelType m_LabelType;
    bool m_MarkQueryNode;

    vector<string> m_RemovedSeqIds;

    CRef<CBioTreeContainer> m_TreeContainer;

    TBlastNameColorMap m_BlastNameColorMap;
    int m_QueryNodeId;

};



#endif // GUIDE_TREE_CALC__HPP
