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

/** @file blast_aux_priv.hpp
 * Auxiliary functions for BLAST
 */

#ifndef ALGO_BLAST_API___BLAST_AUX_PRIV__HPP
#define ALGO_BLAST_API___BLAST_AUX_PRIV__HPP

#include <corelib/ncbiobj.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

struct BlastSeqSrc;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class IBlastSeqInfoSrc;

/** Initializes the IBlastSeqInfoSrc from data obtained from the BlastSeqSrc
 * (for database searches only, uses CSeqDB)
 * @param seqsrc BlastSeqSrc from which to obtain the database information
 */
IBlastSeqInfoSrc* InitSeqInfoSrc(const BlastSeqSrc* seqsrc);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_AUX_PRIV__HPP */
