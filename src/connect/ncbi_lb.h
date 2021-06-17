#ifndef CONNECT___NCBI_LB__H
#define CONNECT___NCBI_LB__H

/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *    Generic load-balancing API
 */

#include "ncbi_servicep.h"

#if 0/*defined(_DEBUG) && !defined(NDEBUG)*/
#  define NCBI_LB_DEBUG 1
#endif /*_DEBUG && !NDEBUG*/


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    const SSERV_Info* info;
    double            status;
} SLB_Candidate;


typedef SLB_Candidate* (*FGetCandidate)(void* data, size_t n);


extern size_t LB_Select(SERV_ITER     iter,
                        void*         data,
                        FGetCandidate getter,
                        double        bonus);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONNECT___NCBI_LB__H */
