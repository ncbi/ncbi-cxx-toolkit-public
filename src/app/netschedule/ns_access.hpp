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


enum ENSCommandChecks {
    eNS_NoChecks  = 0,

    eNS_Queue     = 1 << 0,     // Check that a queue is set
    eNS_Admin     = 1 << 1,     // Check for admin permissions
    eNS_Submitter = 1 << 2,     // Check for the submitter hosts
    eNS_Worker    = 1 << 3,     // Check for wnode hosts
    eNS_Program   = 1 << 4,     // Check for program [and version]
    eNS_Reader    = 1 << 5      // Check for the reader hosts
};

typedef unsigned int    TNSCommandChecks;


END_NCBI_SCOPE

#endif  // NETSCHEDULE_ACCESS__HPP

