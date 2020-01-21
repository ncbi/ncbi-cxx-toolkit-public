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
 *    Utils for MagicBlast applications
 *
 */


#ifndef APP_MAGICBLAST___MAGICBLAST_UTILS__HPP
#define APP_MAGICBLAST___MAGICBLAST_UTILS__HPP

#include <objects/seqset/Seq_entry.hpp>
#include <algo/blast/api/magicblast.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/blastinput/blast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


enum E_StrandSpecificity {
    eNonSpecific, eFwdRev, eRevFwd
};

CNcbiOstream& PrintSAMHeader(CNcbiOstream& ostr,
                             CRef<CLocalDbAdapter> db_adapter,
                             const string& cmd_line_args);

CNcbiOstream& PrintSAM(CNcbiOstream& ostr,
                       CNcbiOstream& unaligned_ostr,
                       CFormattingArgs::EOutputFormat fmt,
                       const CMagicBlastResultSet& results,
                       const CBioseq_set& query_batch,
                       const BlastQueryInfo* query_info,
                       bool is_spliced,
                       int batch_number,
                       bool trim_read_id,
                       bool print_unaligned,
                       bool no_discordant,
                       E_StrandSpecificity strand_specific,
                       bool only_specific,
                       bool print_md_tag);

CNcbiOstream& PrintTabularHeader(CNcbiOstream& ostr, const string& version,
                                 const string& cmd_line_args);

CNcbiOstream& PrintTabular(CNcbiOstream& ostr,
                           CNcbiOstream& unaligned_ostr,
                           CFormattingArgs::EOutputFormat unaligned_fmt,
                           const CMagicBlastResultSet& results,
                           const CBioseq_set& query_batch,
                           bool is_paired, int batch_number,
                           bool trim_read_id,
                           bool print_unaligned,
                           bool no_discordant);

CNcbiOstream& PrintASN1(CNcbiOstream& ostr, const CBioseq_set& query_batch,
                        CSeq_align_set& aligns);



END_SCOPE(blast)
END_NCBI_SCOPE


#endif  /* APP_MAGICBLAST___MAGICBLAST_UTILS__HPP */
