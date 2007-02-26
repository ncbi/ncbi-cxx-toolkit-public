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
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Class representing a text file containing sequences
/// in fasta format
///
class NCBI_XBLAST_EXPORT CBlastFastaInputSource : public CBlastInputSource
{
public:

    /// Constructor
    /// @param objmgr Object Manager instance
    /// @param infile The file to read
    /// @param strand All SeqLoc types will have this strand assigned [in]
    /// @param lowercase If true, lowercase mask locations are generated
    ///                 for all input sequences [in]
    /// @param believe_defline If true, all sequences ID's are parsed;
    ///                 otherwise all sequences receive a local ID set
    ///                 to a monotonically increasing count value [in]
    /// @param range Range restriction for all sequences (default means no
    //                  restriction) [in]
    CBlastFastaInputSource(objects::CObjectManager& objmgr,
                   CNcbiIstream& infile,
                   objects::ENa_strand strand = objects::eNa_strand_other,
                   bool lowercase = false,
                   bool believe_defline = false,
                   TSeqRange range = TSeqRange());

    /// Destructor
    ///
    virtual ~CBlastFastaInputSource() {}

    /// Retrieve a single sequence (in an SSeqLoc container)
    ///
    virtual SSeqLoc GetNextSSeqLoc();

    /// Retrieve a single sequence (in a CBlastSearchQuery container)
    ///
    virtual CRef<CBlastSearchQuery> GetNextSequence();

    /// Signal whether there are any unread sequences left
    /// @return true if no unread sequences remaining
    ///
    virtual bool End();

    /// Configuration for the sequences to be read
    CBlastInputConfig m_Config;

private:
    CNcbiIstream& m_InputFile;  ///< reference to input file
    int m_Counter;              ///< counter for local sequence IDs

    /// Read a single sequence from file and convert to
    /// a Seq_loc
    /// @param lcase_mask If not null, the first element of the 
    ///           vector is filled with a Seq_loc that describes the
    ///           lowercase-masked regions in the query that was read in.
    ///           If there are no such locations, the Seq_loc is of type
    ///           'null', otherwise it is of type 'packed_seqint'
    /// @param query_is_protein if NOT NULL, the value of this variable will
    /// be filled with a boolean value which is set to true if a protein query
    /// was read or false if a nucleotide query was read [out]
    /// @return The sequence in Seq_loc format
    ///
    CRef<objects::CSeq_loc> x_FastaToSeqLoc(
                          vector< CConstRef<objects::CSeq_loc> > *lcase_mask,
                          bool* query_is_protein = NULL);
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_FASTA_INPUT__HPP */

/*---------------------------------------------------------------------
 * $Log$
 * Revision 1.4  2006/10/03 19:42:14  ivanov
 * Added NCBI_XBLAST_EXPORT export specifier.
 * MSVC: blastinput added to ncbi_algo.dll.
 *
 * Revision 1.3  2006/09/26 21:45:38  papadopo
 * add to blast scope; add CVS log
 *
 *-------------------------------------------------------------------*/
