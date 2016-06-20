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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_fasta_input.hpp
 * Interface for FASTA files into blast sequence input
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_FASTA_INPUT__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_FASTA_INPUT__HPP

#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <objtools/readers/fasta.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Class representing a text file containing sequences
/// in fasta format
///
class NCBI_BLASTINPUT_EXPORT CBlastFastaInputSource : public CBlastInputSource
{
public:

    /// Constructor
    /// @param infile The file to read [in]
    /// @param iconfig Input configuration object, this options apply to all
    /// input read [in]
    CBlastFastaInputSource(CNcbiIstream& infile,
                   const CBlastInputSourceConfig& iconfig);

    /// Constructor
    /// @param objmgr Object Manager instance [in]
    /// @param user_input User provided input in a string [in]
    /// @param iconfig Input configuration object, this options apply to all
    /// input read [in]
    CBlastFastaInputSource(const string& user_input,
                   const CBlastInputSourceConfig& iconfig);

    /// Destructor
    virtual ~CBlastFastaInputSource() {}

protected:
    /// Retrieve a single sequence (in an SSeqLoc container)
    /// @param scope CScope object to use in SSeqLoc returned [in]
    /// @throws CObjReaderParseException if input file is empty or the end of
    /// file is reached unexpectedly
    /// @note all masks are returned in either the plus strand (for
    /// nucleotides) or unknown (for proteins)
    virtual SSeqLoc GetNextSSeqLoc(CScope& scope);

    /// Retrieve a single sequence (in a CBlastSearchQuery container)
    /// @param scope CScope object to use in CBlastSearchQuery returned [in]
    /// @throws CObjReaderParseException if input file is empty of the end of
    /// file is reached unexpectedly
    /// @note all masks are returned in either both strands (for
    /// nucleotides) or unknown (for proteins)
    virtual CRef<CBlastSearchQuery> GetNextSequence(CScope& scope);

    /// Signal whether there are any unread sequences left
    /// @return true if no unread sequences remaining
    virtual bool End();

private:
    CBlastInputSourceConfig m_Config; ///< Configuration for the sequences to be read
    CRef<ILineReader> m_LineReader; ///< interface to read lines
    /// Reader of FASTA sequences or identifiers
    AutoPtr<CFastaReader> m_InputReader; 
    bool m_ReadProteins;        ///< read protein sequences?

    /// Read a single sequence from file and convert to a Seq_loc
    /// @param lcase_mask A Seq_loc that describes the
    ///           lowercase-masked regions in the query that was read in.
    ///           If there are no such locations, the Seq_loc is of type
    ///           'null', otherwise it is of type 'packed_seqint' [out]
    /// @param scope CScope object to which the read sequence is added [in]
    /// @return The sequence in Seq_loc format
    ///
    CRef<objects::CSeq_loc> 
    x_FastaToSeqLoc(CRef<objects::CSeq_loc>& lcase_mask, CScope& scope);

    /// Initialization method for the input reader
    void x_InitInputReader();
};


class NCBI_BLASTINPUT_EXPORT CShortReadFastaInputSource
    : public CBlastInputSourceOMF
{
public:
    /// Input formats
    enum EInputFormat {
        eFasta = 0,
        eFastc,
        eFastq
    };


    CShortReadFastaInputSource(CNcbiIstream& infile, TSeqPos num_seqs_in_bacth,
                               EInputFormat format = eFasta,
                               bool paired = false, bool validate = true);

    CShortReadFastaInputSource(CNcbiIstream& infile1, CNcbiIstream& infile2,
                               TSeqPos num_seqs_in_bacth,
                               EInputFormat format = eFasta,
                               bool validate = true);

    virtual ~CShortReadFastaInputSource() {}

    virtual void GetNextNumSequences(CBioseq_set& bioseq_set, TSeqPos num_seqs);
    virtual bool End(void) {return m_LineReader->AtEOF();}

    /// Get number of rejected queries
    Int4 GetNumRejected(void) const {return m_NumRejected;}

private:
    CShortReadFastaInputSource(const CShortReadFastaInputSource&);
    CShortReadFastaInputSource& operator=(const CShortReadFastaInputSource&);

    CTempString x_ParseDefline(CTempString& line);
    bool x_ValidateSequence(const char* sequence, int length);

    /// Read sequences in FASTA format
    void x_ReadFasta(CBioseq_set& bioseq_set);

    /// Read sequences in FASTQ format
    void x_ReadFastq(CBioseq_set& bioseq_set);

    /// Read one sequence from a FASTA file
    int x_ReadFastaOneSeq(CRef<ILineReader> line_reader);

    /// Read one sequence from a FASTQ file
    int x_ReadFastqOneSeq(CRef<ILineReader> line_reader);

    /// Read sequences from two FASTA or FASTQ files (for paired reads)
    bool x_ReadFromTwoFiles(CBioseq_set& bioseq_set, EInputFormat format);

    /// Read sequences in FASTC format: defline, new line, a pair of sequences
    /// on a single line separated by '><'
    void x_ReadFastc(CBioseq_set& bioseq_set);

    TSeqPos m_NumSeqsInBatch;
    /// string::capacity() can be used instead
    TSeqPos m_SeqBuffLen;
    CRef<ILineReader> m_LineReader;
    // for reading paired reads from two FASTA files
    CRef<ILineReader> m_SecondLineReader;
    string m_Sequence;
    CTempString m_Line;
    /// Are paired sequences in the input
    bool m_IsPaired;
    /// Validate quereis and reject those that do not pass
    bool m_Validate;
    /// Number of queries that did not pass validation and were rejected
    Int4 m_NumRejected;
    /// Input format: FASTA, FASTQ, FASTC
    EInputFormat m_Format;
    /// Used for indexing Seq-entries when reading from two files
    int m_Index;

    vector< CRef<CSeq_id> > m_SeqIds;
    vector< CRef<CSeq_entry> > m_Entries;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_FASTA_INPUT__HPP */
