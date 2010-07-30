#ifndef ALGO_BLAST_API___SEQSRC_SEQDB__HPP
#define ALGO_BLAST_API___SEQSRC_SEQDB__HPP

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
 * Author:  Ilya Dondoshansky
 *
 */

/// @file seqsrc_seqdb.hpp
/// Implementation of the BlastSeqSrc interface using the C++ BLAST databases
/// API

#include <objtools/blast/seqdb_reader/seqdb.hpp>

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/api/blast_types.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/** Initialize the sequence source structure.
 * @param dbname BLAST database name [in]
 * @param is_prot Is this a protein or nucleotide database? [in]
 * @param first_seq First ordinal id in the database to search [in]
 * @param last_seq Last ordinal id in the database to search 
 *                 (full database if 0) [in]
 * @param mask_algo_id Database masking algorithm ID (-1 if not applicable) [in]
 * @param mask_type Type of database masking [in]
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* 
SeqDbBlastSeqSrcInit(const string& dbname, bool is_prot, 
                     Uint4 first_seq = 0, Uint4 last_seq = 0,
                     Int4 mask_algo_id  = -1,
                     Int4 mask_type = DB_MASK_NONE);

/** Initialize the sequence source structure using an existing SeqDB object.
 * @param seqdb CSeqDB object [in]
 * @param mask_algo_id Database masking algorithm ID (-1 if not applicable) [in]
 * @param mask_type Type of database masking [in]
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc*
SeqDbBlastSeqSrcInit(CSeqDB * seqdb,
                     Int4 mask_algo_id  = -1,
                     Int4 mask_type = DB_MASK_NONE);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___SEQSRC_SEQDB__HPP */
