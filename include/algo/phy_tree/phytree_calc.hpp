#ifndef PHYTREE_CALC__HPP
#define PHYTREE_CALC__HPP
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

#include <corelib/ncbiobj.hpp>

#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>
#include <algo/phy_tree/dist_methods.hpp>

#include <objects/biotree/BioTreeContainer.hpp>
#include <objtools/alnmgr/alnmix.hpp>

/// Class used in unit tests
class CTestPhyTreeCalc;

BEGIN_NCBI_SCOPE;
USING_SCOPE(objects);


/// Computaion of distance-based phylognetic tree
class NCBI_XALGOPHYTREE_EXPORT CPhyTreeCalc : public CObject {
    
public:

    /// Methods for computing evolutionary distance
    ///
    enum EDistMethod {
        eJukesCantor, ePoisson, eKimura, eGrishin, eGrishinGeneral
    };


    /// Algorithms for phylogenetic tree reconstruction
    ///
    enum ETreeMethod {
        eNJ,      ///< Neighbor Joining
        eFastME,  ///< Fast Minumum Evolution
    };


    /// Distance matrix (square, symmetric with zeros on diagnol)
    class NCBI_XALGOPHYTREE_EXPORT CDistMatrix
    {
    public:

        /// Constructor
        CDistMatrix(int num_elements = 0);

        /// Get number of rows/columns
        /// @return Number of rows/columns
        int GetNumElements(void) const {return m_NumElements;}

        /// Is matrix size zero
        /// @return true if the matrx has zero elements and false otherwise
        bool Empty(void) const {return GetNumElements() <= 0;}

        /// Resize matrix
        /// @param num_elements New number of rows/columns [in]
        void Resize(int num_elements);

        /// Get distance value
        /// @param i Row index [in]
        /// @param j Column index [in]
        /// @return Distance between i-th and j-th element
        const double& operator()(int i, int j) const;

        /// Get distance value
        /// @param i Row index [in]
        /// @param j Column index [in]
        /// @return Distance between i-th and j-th element
        double& operator()(int i, int j);

    private:
        /// Number of rows/columns
        int m_NumElements;

        /// Storage for distance values
        vector<double> m_Distances;

        /// Storage for matrix diagnol value
        const double m_Diagnol;
    };

public:

    //--- Constructors ---    

    /// Create CPhyTreeCalc from Seq-align
    /// @param seq_aln Seq-align [in]
    /// @param scope Scope [in]
    ///
    CPhyTreeCalc(const CSeq_align& seq_aln, CRef<CScope> scope);

    /// Create CPhyTreeCalc from CSeq_align_set
    /// @param seq_align_set SeqAlignSet [in]
    /// @param scope Scope [in]    
    ///
    CPhyTreeCalc(const CSeq_align_set& seq_align_set, CRef<CScope> scope);

    ~CPhyTreeCalc() {delete m_Tree;}


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


    /// Set algorithm for phylogenetic tree computation
    /// @param method Tree compuutation method [in]
    ///
    void SetTreeMethod(ETreeMethod method) {m_TreeMethod = method;}


    /// Set labels for tree leaves
    /// @return Labels
    ///
    vector<string>& SetLabels(void) {return m_Labels;}


    //--- Getters ---

    /// Get computed tree
    /// @return Tree
    ///
    TPhyTreeNode* GetTree(void) {return m_Tree;}

    /// Get computed tree
    /// @return Tree
    ///
    const TPhyTreeNode* GetTree(void) const {return m_Tree;}

    /// Get serial tree
    /// @return Tree
    ///
    CRef<CBioTreeContainer> GetSerialTree(void) const;

    /// Get seq_align that corresponds to current tree
    /// @return Seq_align
    ///
    CRef<CSeq_align> GetSeqAlign(void) const;

    /// Get seq-ids of sequences used in tree construction
    /// @return Seq-ids
    ///
    const vector< CRef<CSeq_id> >& GetSeqIds(void) const;

    /// Get scope
    /// @return Scope
    ///
    CRef<CScope> GetScope(void) {return m_Scope;}

    /// Get divergence matrix
    /// @return Divergence matrix
    const CDistMatrix& GetDivergenceMatrix(void) const
    {return m_DivergenceMatrix;}

    
    /// Get distance matrix
    /// @return Distance matrix
    const CDistMatrix& GetDistanceMatrix(void) const
    {return m_DistMatrix;}


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


    /// Get ids of sequences excluded from tree computation
    /// @return Ids of excluded sequences
    ///
    const vector<string>& GetRemovedSeqIds(void) const {return m_RemovedSeqIds;}

    /// Get error/warning messages
    /// @return List of messages
    ///
    const vector<string>& GetMessages(void) const {return m_Messages;}

    /// Check whether there are any messages
    /// @return True if there are messages, false otherwise
    ///
    bool IsMessage(void) const {return m_Messages.size() > 0;}


    //--- Tree computation ---

    /// Compute bio tree for the current alignment in a black box manner
    /// @return True is computation successful, false otherwise
    ///
    bool CalcBioTree(void);


protected:

    /// Initialize class attributes
    void x_Init(void);

    /// Initialize alignment data source
    /// @param seq_aln Seq-align [in]
    /// @return True if success, false otherwise
    void x_InitAlignDS(const CSeq_align& seq_aln);

    /// Initialize alignment data source
    /// @param seq_align_set CSeq_align_set [in]
    /// @return True if success, false otherwise
    bool x_InitAlignDS(const CSeq_align_set& seq_align_set);

    /// Compute divergence matrix and find sequences to exclude from tree
    /// reconstruction
    ///
    /// Divergence between all pairs of sequences used for phylogenetic tree
    /// reconstruction must be smaller than a cutoff (m_Divergence). Hence 
    /// some sequences may be excluded from tree computation. The first 
    /// sequence in alignmnet is considered query/master. All sequences similar
    /// enough to the query and each other are included in tree computation.
    ///
    /// @param used_indices Vector of sequence indices included in phylogenetic
    /// tree computation [out]
    /// @return True if tree can be computed (ie at least two sequences are
    /// included for tree computation), false otherwise
    ///
    bool x_CalcDivergenceMatrix(vector<int>& used_indices);

    /// Compute distance as evolutionary corrected dissimilarity
    void x_CalcDistMatrix(void);

    /// Create alignment only for sequences included in tree computation
    /// @param included_indices Indices of included sequences [in]
    ///
    void x_CreateValidAlign(const vector<int>& used_indices);
    
    /// Compute phylogenetic tree
    /// @param correct Whether negative tree egde lengths should be set to zero
    /// [in]
    ///
    void x_ComputeTree(bool correct = true);

    /// Change non-finite and negative tree branch lengths to zeros
    /// @param node Tree root [in]
    ///
    void x_CorrectBranchLengths(TPhyTreeNode* node);


protected:

    /// Structure for storing divergences between sequences as list of links
    struct SLink {
        int index1;       //< index of sequence 1
        int index2;       //< index of sequence 2
        double distance;  //< distance between sequences

        /// Constructor
        SLink(int ind1, int ind2, double dist)
            : index1(ind1), index2(ind2), distance(dist) {}
    };


protected:

    /// Scope
    CRef<CScope> m_Scope;

    /// Alignment data source
    CRef<CAlnVec> m_AlignDataSource;

    /// Maximum allowed divergence between sequences
    double m_MaxDivergence;

    /// Method of calculating evolutionary distance correction
    EDistMethod m_DistMethod;

    /// Method of calculating tree
    ETreeMethod m_TreeMethod;

    /// Matrix of percent identities based distances
    CDistMatrix m_DivergenceMatrix;

    /// Matrix of evolutionary distance
    CDistMatrix m_DistMatrix;

    /// Full distance matrix, this data structure is required by CDistMethods
    /// functions
    CDistMethods::TMatrix m_FullDistMatrix;

    /// Sequences that are not included in the tree
    vector<string> m_RemovedSeqIds;

    /// Computed tree
    TPhyTreeNode* m_Tree;

    /// Labels for tree leaves
    vector<string> m_Labels;

    /// Error/warning messages
    vector<string> m_Messages;    

    friend class ::CTestPhyTreeCalc;
};


/// Guide tree calc exceptions
class NCBI_XALGOPHYTREE_EXPORT CPhyTreeCalcException : public CException
{
public:
    enum EErrCode {
        eInvalidOptions,
        eTreeComputationProblem,
        eNoTree,
        eDistMatrixError
    };

    NCBI_EXCEPTION_DEFAULT(CPhyTreeCalcException, CException);
};

END_NCBI_SCOPE

#endif // PHYTREE_CALC__HPP
