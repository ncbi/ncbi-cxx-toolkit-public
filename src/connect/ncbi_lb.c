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
    double total = 0.0, access = 0.0, point = 0.0, p = 0.0, status = 0.0;
    int/*bool*/ best_match;
    const SSERV_Info* info;
    SLB_Candidate* cand;
    size_t i = 0, n;

    assert(bonus >= 1.0);
    assert(iter  &&  get_candidate);
    if (iter->ismask  ||  iter->ok_down  ||  iter->ok_suppressed)
        return 0/*first entry (DISPD: probably) fits*/;
    best_match = 0/*false*/;
    for (n = 0; ; n++) {
        int/*bool*/ match;
        if (!(cand = get_candidate(data, n)))
            break;
        info   = cand->info;
        status = cand->status;
        match  = iter->host  &&  iter->host == info->host
            && (!iter->port  ||  iter->port == info->port);
        if (match  ||  (!best_match  &&  !iter->host  &&
                        info->locl  &&  info->coef < 0.0)) {
            if (best_match < match) {
                best_match = match;
                access = point = 0.0;
            }
            if (iter->pref  ||  info->coef <= 0.0) {
                status *= bonus;
                if (access < status  &&  (iter->pref  ||  info->coef < 0.0)) {
                    access =  status;         /* always take the largest */
                    point  =  total + status; /* latch this local server */
                    p      = -info->coef;     /* NB: may end up negative */
                    i      = n;
                }
            } else /* assert(match); */
                status *= info->coef;
        }
        total       += status;
        cand->status = total;
    }
    assert(n > 0);

    if (best_match  &&  iter->pref < 0.0) {
        /* Latched preference */
        cand = get_candidate(data, i);
        status = access;
    } else {
        if (iter->pref > 0.0) {
            if (point > 0.0  &&  access > 0.0  &&  total != access) {
                p = SERV_Preference(iter->pref, access/total, n);
#ifdef NCBI_LB_DEBUG
                CORE_LOGF(eLOG_Note,
                          ("(P = %lf, A = %lf, T = %lf, A/T = %lf, N = %d)"
                           " -> Pref = %lf", iter->pref, access, total,
                           access/total, (int) n, p));
#endif /*NCBI_LB_DEBUG*/
                status = total * p;
                p = total * (1.0 - p) / (total - access);
                for (i = 0; i < n; i++) {
                    cand = get_candidate(data, i);
                    if (point <= cand->status)
                        cand->status  = p * (cand->status - access) + status;
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
        if (point <= 0.0  ||  access * (n - 1) < p * 0.01 * (total - access)) {
            point = (total * rand()) / (double) RAND_MAX;
        }

        total = 0.0;
        for (i = 0; i < n; i++) {
            cand = get_candidate(data, i);
            assert(cand);
            if (point <= cand->status) {
                status = cand->status - total;
                break;
            }
            total = cand->status;
        }
    }

    assert(cand  &&  i < n);
    cand->status = status;

    return i;
}
