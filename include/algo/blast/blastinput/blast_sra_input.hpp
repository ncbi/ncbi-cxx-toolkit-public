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
 * Author:  Greg Boratyn
 *
 */

/** @file blast_fasta_input.hpp
 * Interface for reading SRA sequences into blast input
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_SRA_INPUT__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_SRA_INPUT__HPP

#include <algo/blast/blastinput/blast_input.hpp>
//#include <algo/blast/blastinput/blast_scope_src.hpp>

#include <sra/readers/sra/csraread.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


/// Class for reading sequences from SRA respository or SRA file
class CSraInputSource : public CBlastInputSourceOMF
{
public:

    /// Constructor
    /// @param accessions SRA accessions or files [in]
    /// @param num_seqs_in_batch Number of sequences to read in a single batch
    /// [in]
    /// @param check_for_pairs If true, determine if reads are paired based on
    /// information in SRA [in]
    /// sequence; if true sequences that do not pass validation will be
    /// rejected [in]
    CSraInputSource(const vector<string>& accessions,
                    TSeqPos num_seqs_in_bacth, bool check_for_paires = true,
                    bool validate = true);

    virtual ~CSraInputSource() {}

    virtual void GetNextNumSequences(CBioseq_set& bioseq_set, TSeqPos num_seqs);
    virtual bool End(void) {return !m_It.get() || !(*m_It);}


private:
    CSraInputSource(const CSraInputSource&);
    CSraInputSource& operator=(const CSraInputSource&);

    /// Read one sequence pointed by the iterator
    int x_ReadOneSeq(CBioseq_set& bioseq_set);

    /// Read one batch of sequences and mark pairs
    void x_ReadPairs(CBioseq_set& bioseq_set);

    /// Validate sequence base distribution
    bool x_ValidateSequence(const char* sequence, int length);


    auto_ptr<CCSraDb> m_SraDb;
    auto_ptr<CCSraShortReadIterator> m_It;
    
    TSeqPos m_NumSeqsInBatch;

    /// Are queries paired
    bool m_IsPaired;
    /// Validate quereis and reject those that do not pass
    bool m_Validate;

    /// Used for indexing Seq-entries when reading from two files
    int m_Index;

    vector< CRef<CSeq_entry> > m_Entries;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_SRA_INPUT__HPP */
