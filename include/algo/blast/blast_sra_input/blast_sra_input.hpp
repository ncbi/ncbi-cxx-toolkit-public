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
class CSraInputSource : public CBlastInputSourceOMF, public CBlastInputSource
{
public:

    /// Constructor
    /// @param accessions SRA accessions or files [in]
    /// @param check_for_pairs If true, determine if reads are paired based on
    /// information in SRA [in]
    /// @param cache_enabled Enable caching SRA data in local files (see
    /// File Caching at
    /// https://github.com/ncbi/sra-tools/wiki/Toolkit-Configuration) [in]
    CSraInputSource(const vector<string>& accessions,
                    bool check_for_paires = true,
                    bool cache_enabled = false);

    virtual ~CSraInputSource() {}

    virtual int GetNextSequence(CBioseq_set& bioseq_set);

    virtual bool End(void);

    virtual SSeqLoc GetNextSSeqLoc(CScope& scope);

    virtual CRef<CBlastSearchQuery> GetNextSequence(CScope& scope);


private:
    CSraInputSource(const CSraInputSource&);
    CSraInputSource& operator=(const CSraInputSource&);

    /// Read one sequence pointed by the iterator
    CRef<CSeq_entry> x_ReadOneSeq(void);

    /// Read one sequence pointed by the iterator and add it to the bioseq_set
    /// object
    CSeq_entry* x_ReadOneSeq(CBioseq_set& bioseq_set);

    /// Read one batch of sequences and mark pairs
    void x_ReadPairs(CBioseq_set& bioseq_set);

    /// Advance to the next SRA accession
    void x_NextAccession(void);

    /// Read the next sequence, add it to scope and return Seq-loc object
    CRef<CSeq_loc> x_GetNextSeq_loc(CScope& scope);

    unique_ptr<CCSraDb> m_SraDb;
    unique_ptr<CCSraShortReadIterator> m_It;

    vector<string> m_Accessions;
    vector<string>::iterator m_ItAcc;
    
    /// Number of bases added so far
    TSeqPos m_BasesAdded;

    /// Are queries paired
    bool m_IsPaired;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_SRA_INPUT__HPP */
