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
#include <sra/readers/sra/csraread.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


/// Class for reading sequences from SRA respository or SRA file
class CSraInputSource : public CBlastInputSourceOMF
{
public:

    /// Constructor
    /// @param accessions SRA accessions or files [in]
    /// @param check_for_pairs If true, determine if reads are paired based on
    /// information in SRA [in]
    /// sequence; if true sequences that do not pass validation will be
    /// rejected [in]
    CSraInputSource(const vector<string>& accessions,
                    bool check_for_paires = true,
                    bool validate = true);

    virtual ~CSraInputSource() {}

    virtual void GetNextSequenceBatch(CBioseq_set& bioseq_set,
                                      TSeqPos batch_size);

    virtual bool End(void) {return m_ItAcc == m_Accessions.end();}


private:
    CSraInputSource(const CSraInputSource&);
    CSraInputSource& operator=(const CSraInputSource&);

    /// Read one sequence pointed by the iterator
    CSeq_entry* x_ReadOneSeq(CBioseq_set& bioseq_set);

    /// Read one batch of sequences and mark pairs
    void x_ReadPairs(CBioseq_set& bioseq_set, TSeqPos batch_size);

    /// Validate sequence base distribution
    bool x_ValidateSequence(const char* sequence, int length);

    /// Advance to the next SRA accession
    void x_NextAccession(void);

    auto_ptr<CCSraDb> m_SraDb;
    auto_ptr<CCSraShortReadIterator> m_It;

    vector<string> m_Accessions;
    vector<string>::iterator m_ItAcc;
    
    /// Number of bases added so far
    TSeqPos m_BasesAdded;

    /// Are queries paired
    bool m_IsPaired;
    /// Validate quereis and reject those that do not pass
    bool m_Validate;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_SRA_INPUT__HPP */
