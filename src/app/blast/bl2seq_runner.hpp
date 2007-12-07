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
 * Authors:  Vahram Avagyan
 *
 */

/** @file bl2seq_runner.hpp
 * Blast 2 sequences support for Blast command line applications
 */

#ifndef APP___BL2SEQ_RUNNER__HPP
#define APP___BL2SEQ_RUNNER__HPP

#include <corelib/ncbiapp.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_args.hpp>
#include "blast_app_util.hpp"
#include "blast_format.hpp"

BEGIN_NCBI_SCOPE

//--------------------------------------------------------------------------//

/// Blast 2 sequences command-line running and formatting support
class CBl2Seq_Runner
{
public:
    /// Constructor to create an uninitialized runner object
    CBl2Seq_Runner(bool is_query_protein,
                   bool is_subject_protein);

    /// Destructor
    virtual ~CBl2Seq_Runner();

    /// Initialize the runner object from the command line arguments
    void ProcessDatabaseArgs(CRef<blast::CBlastDatabaseArgs> db_args,
                             CRef<objects::CObjectManager> obj_mgr,
                             objects::CScope* scope_formatter);

    /// Is the object properly initialized to run Blast 2 sequences?
    bool IsBl2Seq() const;

    /// Use Blast API to run Blast 2 sequences and format the results
    void RunAndFormat(blast::CBlastFastaInputSource* fasta_query,
                      blast::CBlastInput* input_query,
                      blast::CBlastOptionsHandle* opts_handle,
                      CBlastFormat* formatter,
                      objects::CScope* scope_formatter);

private:
    bool m_is_bl2seq;                             ///< bl2seq can run
    bool m_is_query_protein;                      ///< query is protein
    bool m_is_subject_protein;                    ///< subject is protein

    CRef<blast::CBlastFastaInputSource>
        m_fasta_subject;                          ///< fasta subject source
    CRef<blast::CBlastInput> m_input_subject;     ///< subject sequence input
    blast::TSeqLocVector m_subject_seqs;          ///< subject sequences

    static const int m_subject_id_offset = 1<<16; ///< subject ids start here
};

//--------------------------------------------------------------------------//

END_NCBI_SCOPE

#endif /* APP___BL2SEQ_RUNNER__HPP */
