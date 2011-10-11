#ifndef NETSCHEDULE_ACCESS__HPP
#define NETSCHEDULE_ACCESS__HPP

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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Access bits to certain commands
 *
 */


BEGIN_NCBI_SCOPE


enum ENSAccess {
    eNSAC_Queue         = 1 << 0,
    eNSAC_Worker        = 1 << 1,
    eNSAC_Submitter     = 1 << 2,
    eNSAC_Admin         = 1 << 3,
    eNSAC_QueueAdmin    = 1 << 4,
    eNSAC_Test          = 1 << 5,
    eNSAC_DynQueueAdmin = 1 << 6,
    eNSAC_DynClassAdmin = 1 << 7,
    eNSAC_AnyAdminMask  = eNSAC_Admin | eNSAC_QueueAdmin |
                          eNSAC_DynQueueAdmin | eNSAC_DynClassAdmin,

    // Combination of flags for client roles
    eNSCR_Any           = 0,
    eNSCR_Queue         = eNSAC_Queue,
    eNSCR_Worker        = eNSAC_Worker | eNSAC_Queue,
    eNSCR_Submitter     = eNSAC_Submitter | eNSAC_Queue,
    eNSCR_Admin         = eNSAC_Admin,
    eNSCR_QueueAdmin    = eNSAC_QueueAdmin | eNSAC_Queue
};


typedef unsigned int        TNSClientRole;



END_NCBI_SCOPE

#endif  // NETSCHEDULE_ACCESS__HPP

