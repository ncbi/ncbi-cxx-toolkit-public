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
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Generic LB API
 *
 */

#include "ncbi_lb.h"
#include "ncbi_priv.h"
#include <assert.h>
#include <stdlib.h>


size_t LB_Select(SERV_ITER     iter,          void*  data,
                 FGetCandidate get_candidate, double bonus)
{
    double total = 0.0, access = 0.0, point = 0.0, p = 0.0, status;
    const SSERV_Info* info;
    SLB_Candidate* cand;
    size_t i, n;

    assert(iter);
    for (n = 0; ; n++) {
        if (!(cand = get_candidate(data, n))) {
            assert(n);
            break;
        }
        if ((iter->type & fSERV_Promiscuous)  ||  iter->mask)
            return 0;
        info   = cand->info;
        status = cand->status;
        if ((iter->host == info->host  ||
             (!iter->host  &&  info->locl  &&  info->coef < 0.0))) {
            if (iter->pref  ||  info->coef <= 0.0) {
                status *= bonus;
                if (access < status  &&  (iter->pref  ||  info->coef < 0.0)) {
                    access =  status;
                    point  =  total + status; /* Latch this local server */
                    p      = -info->coef;
                    assert(point > 0.0);
                }
            } else
                status *= info->coef;
        }
        total       += status;
        cand->status = total;
    }

    if (point <= 0.0  ||  iter->pref >= 0.0) {
        if (iter->pref > 0.0) {
            if (point > 0.0  &&  total != access) {
                p = SERV_Preference(iter->pref, access/total, n);
#ifdef NCBI_LB_DEBUG
                CORE_LOGF(eLOG_Note,
                          ("(P = %lf, A = %lf, T = %lf, A/T = %lf, N = %d)"
                           " -> Pref = %lf", iter->pref, access, total,
                           access/total, (int) n, p));
#endif /*NCBI_LB_DEBUG*/
                status = total*p;
                p = total*(1.0 - p)/(total - access);
                for (i = 0; i < n; i++) {
                    cand = get_candidate(data, n);
                    assert(cand);
                    if (point <= cand->status)
                        cand->status  = p*(cand->status - access) + status;
                    else
                        cand->status *= p;
                }
#ifdef NCBI_LB_DEBUG
                status = 0.0;
                for (i = 0; i < n; i++) {
                    char addr[16];
                    cand = get_candidate(data, i);
                    info = cand->info;
                    p    = cand->status - status;
                    status = cand->status;
                    SOCK_ntoa(info->host, addr, sizeof(addr));
                    CORE_LOGF(eLOG_Note, ("%s %lf %.2lf%%",
                                          addr, p, p / total * 100.0));
                }
#endif /*NCBI_LB_DEBUG*/
            }
            point = -1.0;
        }

        /* We take pre-chosen local server only if its status is not less
           than p% of the average remaining status; otherwise, we ignore
           the server, and apply the generic procedure by random seeding.*/
        if (point <= 0.0  ||  access*(n - 1) < p*0.01*(total - access))
            point = (total * rand()) / (double) RAND_MAX;
    }

    status = 0.0;
    for (i = 0; i < n; i++) {
        cand = get_candidate(data, i);
        assert(cand);
        if (point <= cand->status)
            break;
        status = cand->status;
    }
    assert(cand  &&  i < n);
    cand->status -= status;

    return i;
}
