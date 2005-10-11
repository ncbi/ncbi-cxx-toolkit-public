#ifndef ALGO_BLAST_API___SEQSRC_QUERY_FACTORY__HPP
#define ALGO_BLAST_API___SEQSRC_QUERY_FACTORY__HPP

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
 * Author:  Christiam Camacho
 *
 */

/// @file seqsrc_query_factory.hpp
/// Implementation of the BlastSeqSrc interface for a query factory

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/api/query_data.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/** Initialize the sequence source structure from a query factory.
 * @param query_factory Factory from which the queries will be manufactured [in]
 * @param program Type of BLAST to be performed [in]
 */
NCBI_XBLAST_EXPORT BlastSeqSrc* 
QueryFactoryBlastSeqSrcInit(CRef<IQueryFactory> query_factory, 
                            EBlastProgramType program);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___SEQSRC_QUERY_FACTORY__HPP */
