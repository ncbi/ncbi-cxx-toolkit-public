#ifndef ALGO_BLAST_API__BLAST_PSI__HPP
#define ALGO_BLAST_API__BLAST_PSI__HPP

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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_psi.hpp
 * C++ API for the PSI-BLAST PSSM generation engine.
 *
 * Notes:
 * Variants of PSI-BLAST:
 *     PSI-BLAST: protein/pssm vs. protein db/sequence
 *     PSI-TBLASTN: protein/pssm vs. translated nucleotide db/sequence
 *
 * Binaries which run the variants mentioned above as implemented in C toolkit:
 *     blastpgp runs PSI-BLAST (-j 1 = blastp; -j n, n>1 = PSI-BLAST)
 *     blastall runs PSI-TBLASTN and it is only possible to run with a PSSM 
 *     from checkpoint file via the -R option and for only one iteration.
 *     PSI-TBLASTN cannot be run from just an iteration, it is always product
 *     of a restart with a checkpoint file.
 */
 
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimisc.hpp>
#include <algo/blast/core/blast_psi.h>
#include <algo/blast/api/blast_exception.hpp>

class CPssmCreate;   // forward declaration of unit test class

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_loc;
    class CScore;
    class CDense_seg;
    class CSeq_id;
    class CSeq_align_set;
END_SCOPE(objects)

// specialization of CDeleter template for PsiAlignmentData structure
// to be used when initializing an AutoPtr with the return value of
// PsiAlignmentDataNew
template <>
struct ncbi::CDeleter<PsiAlignmentData> {
    static void Delete(PsiAlignmentData* p)
    { PSIAlignmentDataFree(p); }
};

#ifndef GAP_IN_ALIGNMENT
#   define GAP_IN_ALIGNMENT     ((Uint4)-1)
#endif

BEGIN_SCOPE(blast)

class CScoreMatrixBuilder
{
public:
    // C-tor
    // @param query Query or Consensus sequence for the alignment. The data
    // query points to is *not* copied.
    CScoreMatrixBuilder(const Uint1* query,
                        TSeqPos query_sz,
                        CRef<objects::CSeq_align_set> sset,
                        CRef<objects::CScope> scope,
                        const PSIBlastOptions& opts);

    // N.B.:
    // Only two types of Seq-aligns are supported: those for protein-protein
    // comparison (Dense-seg) and those for protein-translated nucleotide
    // comparison (Std-seg).
    // Dense-diag?
    // FIXME: need to return Score-matrix ASN.1 structure with PSSM
    void Run();

    unsigned int GetNumAlignedSequences() const;
    unsigned int GetQuerySeqLen() const;

protected:
    // Convenience for debugging
    friend ostream& operator<<(ostream& out, const CScoreMatrixBuilder& smb);

    void SelectSequencesForProcessing();
    void ExtractAlignmentData();

    /// Process each segment in the alignment to extract information about
    /// matching letters, evalues, and extents of the matching regions
    /// @param s abstraction of an aligned segment [in]
    /// @param seq_index index of the sequence aligned with the query in the
    ///        desc_matrix field of the m_AlignmentData data member [in]
    /// @param e_value E-value for this subject alignment.
    void x_ProcessDenseg(const objects::CDense_seg& denseg, TSeqPos seq_index,
                         double e_value);

    void x_Init(const Uint1* query, TSeqPos query_size, 
                CRef<objects::CSeq_align_set> sset);

    /// Member function that selects those sequences in the alignment which
    /// have an evalue lower than the threshold specified in PsiInfo structure
    void x_SelectByEvalue();

    /// Allow specification of seq-ids for considering sequences to process
    void x_SelectBySeqId(const std::vector< CRef<objects::CSeq_id> >& ids);

private:
    // Convenience typedefs
    typedef AutoPtr<PsiAlignmentData, CDeleter<PsiAlignmentData> > 
        TPsiAlignDataSmartPtr;
    typedef pair<AutoPtr<Uint1, ArrayDeleter<Uint1> >, TSeqPos> TSeqPair;

    CRef<objects::CScope>           m_Scope;
    TPsiAlignDataSmartPtr           m_AlignmentData;
    PSIBlastOptions                 m_Opts;
    CRef<objects::CSeq_align_set>   m_SeqAlignSet;
    PsiInfo                         m_PssmDimensions;

    /// Prohibit copy constructor
    CScoreMatrixBuilder(const CScoreMatrixBuilder& rhs);
    /// Prohibit assignment operator
    CScoreMatrixBuilder& operator=(const CScoreMatrixBuilder& rhs);

    friend class ::CPssmCreate; // unit test class

    // Auxiliary functions
    static TSeqPair 
    x_GetSubjectSequence(const objects::CDense_seg& ds, objects::CScope& scope);

};

inline unsigned int
CScoreMatrixBuilder::GetNumAlignedSequences() const
{
    return m_PssmDimensions.num_seqs;
}

inline unsigned int
CScoreMatrixBuilder::GetQuerySeqLen() const
{
    return m_PssmDimensions.query_sz;
}

/// Used for debugging purposes
std::string
PsiAlignmentData2String(const PsiAlignmentData* alignment);

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2004/05/28 16:39:42  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API__BLAST_PSI__HPP */
