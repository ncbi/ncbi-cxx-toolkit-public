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

/** @file blast_mtlock.hpp
 * C++ version of the initialization for the mutex locking interface.
 */

#ifndef ALGO_BLAST_API___BLAST_MTLOCK_HPP
#define ALGO_BLAST_API___BLAST_MTLOCK_HPP

#include <connect/ncbi_core.h>
#include <algo/blast/core/blast_export.h>

BEGIN_NCBI_SCOPE

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_SCOPE(blast)

/** Initialize a mutex locking mechanism for BLAST. */
NCBI_XBLAST_EXPORT MT_LOCK Blast_CMT_LOCKInit(void);

END_SCOPE(blast)
END_NCBI_SCOPE
/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2005/03/30 16:21:27  jcherry
* Added export specifier
*
* Revision 1.3  2005/03/29 15:58:54  dondosha
* Added blast scope
*
* Revision 1.2  2004/07/06 15:43:59  dondosha
* Added doxygen comments
*
* ===========================================================================
*/


#endif /* ALGO_BLAST_API___BLAST_MTLOCK_HPP */
