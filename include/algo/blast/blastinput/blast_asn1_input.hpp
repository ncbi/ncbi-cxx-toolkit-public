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
 * Interface for ASN1 files into blast sequence input
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_ASN1_INPUT__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_ASN1_INPUT__HPP

#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


/// Class representing a text or binary file containing sequences in ASN.1
/// format as a collection of Seq-entry objects
class NCBI_BLASTINPUT_EXPORT CASN1InputSourceOMF : public CBlastInputSourceOMF
{
public:

    /// Constructor
    /// @param infile Input stream for query sequences [in]
    /// @param is_bin Is input in binary ASN.1 format [in]
    /// @param is_paired Are queries paired [in]
    /// @param validate Should sequence validation be applied to each read
    CASN1InputSourceOMF(CNcbiIstream& infile, bool is_bin = false,
                        bool is_paired = false);

    /// Constructor for reading sequences from two files for paired short reads
    /// @param infile1 Input stream for query sequences [in]
    /// @param infile2 Input stream for query mates [in]
    /// @param is_bin Is input in binary ASN.1 format [in]
    /// @param validate Should sequence validation be applied to each read
    CASN1InputSourceOMF(CNcbiIstream& infile1, CNcbiIstream& infile2,
                        bool is_bin = false);

    virtual ~CASN1InputSourceOMF() {}

    virtual int GetNextSequence(CBioseq_set& bioseq_set);

    virtual bool End(void) {return m_InputStream->eof();}


private:
    CASN1InputSourceOMF(const CASN1InputSourceOMF&);
    CASN1InputSourceOMF& operator=(const CASN1InputSourceOMF&);

    /// Read one sequence from
    CRef<CSeq_entry> x_ReadOneSeq(CNcbiIstream& instream);

    /// Read sequences from one stream
    bool x_ReadFromSingleFile(CBioseq_set& bioseq_set);

    /// Read sequences from two streams
    bool x_ReadFromTwoFiles(CBioseq_set& bioseq_set);

    /// Number of bases added so far
    TSeqPos m_BasesAdded;
    CNcbiIstream* m_InputStream;
    // for reading paired reads from two FASTA files
    CNcbiIstream* m_SecondInputStream;
    /// Are queries paired
    bool m_IsPaired;
    /// Is input binary ASN1
    bool m_IsBinary;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_ASN1_INPUT__HPP */
