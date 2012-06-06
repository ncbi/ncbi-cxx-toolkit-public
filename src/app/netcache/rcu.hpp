#ifndef NETCACHE__RCU__HPP
#define NETCACHE__RCU__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 */


BEGIN_NCBI_SCOPE


struct SRCUInfo;


void RCUInitNewThread(SSrvThread* thr);
void RCUFinalizeThread(SSrvThread* thr);
void RCUPassQS(SRCUInfo* rcu);
bool RCUHasCalls(SRCUInfo* rcu);



struct SRCUInfo
{
    TSrvRCUList calls;
    CSrvRCUUser* gp_marker_cur;
    CSrvRCUUser* gp_marker_next;
    Uint1 seen_gp;
};


class CFakeRCUUser : public CSrvRCUUser
{
public:
    CFakeRCUUser(void);
    virtual ~CFakeRCUUser(void);

    virtual void ExecuteRCU(void);
};

END_NCBI_SCOPE

#endif /* NETCACHE__RCU__HPP */
