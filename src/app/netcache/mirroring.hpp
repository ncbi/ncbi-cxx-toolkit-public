#ifndef NETCACHE__MIRRORING__HPP
#define NETCACHE__MIRRORING__HPP

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
 * Authors: Denis Vakatov, Pavel Ivanov, Sergey Satskiy
 *
 * File Description: Data structures and API to support blobs mirroring.
 *
 */


#include <map>
#include <deque>
#include <string>

// For Uint2, Uint8, NCBI_CONST_UINT8
#include <corelib/ncbitype.h>

// For CFastMutex
#include <corelib/ncbimtx.hpp>

// For BEGIN_NCBI_SCOPE
#include <corelib/ncbistl.hpp>

#include <corelib/ncbithr.hpp>


BEGIN_NCBI_SCOPE

// pairs of (blob key, local_rec_no)
typedef deque< pair< string, Uint8 > >     TPropagateWrites;
struct SDistribution;

enum ENCMirroringType {
    eMirrorSmallPrefered,
    eMirrorSmallExclusive,
    eMirrorLargePrefered
};

class CNCMirroringThread : public CThread
{
public:
    CNCMirroringThread(Uint8            server_id,
                       SDistribution*   distr,
                       ENCMirroringType mirror_type);

    void WakeUp( void )
    {
        try {
            notifier.Post();
        }
        catch (CCoreException&) {
            //
        }
    }

    void RequestFinish( void )
    {
        finish_flag = true;
        WakeUp();
    }

private:
    virtual void *  Main( void );

    Uint8           server_id;
    SDistribution*  distr;
    ENCMirroringType mirror_type;
    bool            finish_flag;
    CSemaphore      notifier;
};






class CNCMirroring
{
public:
    // Reads settings from an ini file,
    // Creates command queues,
    // Creates service threads
    static void Initialize( void );

    // Stops the threads
    // Does not clean the memory because it is guaranteed that it is called
    // only when NC is shut down.
    static void Finalize( void );

    // Generates 1 or more write commands for the corresponding servers
    // or overwrites previous write commands if any
    static void BlobWriteEvent( const string &  key,
                                Uint2           slot,
                                Uint8           local_rec_no,
                                Uint8           size);

    static Uint8 GetQueueSize(void);
    static Uint8 GetQueueSize(Uint8 server_id);


    static CAtomicCounter   sm_TotalCopyRequests;
    static CAtomicCounter   sm_CopyReqsRejected;
};


END_NCBI_SCOPE


#endif /* NETCACHE__MIRRORING__HPP */

