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
class NCBI_XBLAST_EXPORT CBlastFastaInputSource : public CBlastInputSource
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
    virtual SSeqLoc GetNextSSeqLoc(CScope& scope);

    /// Retrieve a single sequence (in a CBlastSearchQuery container)
    /// @param scope CScope object to use in CBlastSearchQuery returned [in]
    /// @throws CObjReaderParseException if input file is empty of the end of
    /// file is reached unexpectedly
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

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_FASTA_INPUT__HPP */
