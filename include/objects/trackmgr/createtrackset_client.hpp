#ifndef OBJECTS_TRACKMGR_GRIDCLI__CREATE_TRACKSET_CLIENT_HPP
#define OBJECTS_TRACKMGR_GRIDCLI__CREATE_TRACKSET_CLIENT_HPP

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
 * Authors: Peter Meric
 *
 * File Description:  NetSchedule grid client for TrackManager trackset request/reply
 *
 */

/// @file tmgr_displaytrack_client.hpp
/// NetSchedule grid client for TrackManager trackset request/reply

#include <objects/trackmgr/gridrpcclient.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
class CTMgr_CreateTracksetRequest;
class CTMgr_CreateTracksetReply;
END_SCOPE(objects)


class CTMS_CreateTrackSet_Client : private CGridRPCBaseClient<CAsnBinCompressed>
{
private:
    typedef CGridRPCBaseClient<ncbi::CAsnBinCompressed> TBaseClient;

public:
    typedef objects::CTMgr_CreateTracksetRequest TRequest;
    typedef objects::CTMgr_CreateTracksetReply TReply;
    typedef CConstRef<TRequest> TRequestCRef;
    typedef CRef<TReply> TReplyRef;

public:
    CTMS_CreateTrackSet_Client(const string& NS_service,
                         const string& NS_queue,
                         const string& client_name,
                         const string& NC_registry_section
                        );

    CTMS_CreateTrackSet_Client(const string& NS_registry_section = "netschedule_api",
                         const string& NC_registry_section = kEmptyStr
                        );

    virtual ~CTMS_CreateTrackSet_Client();

    TReplyRef Fetch(const TRequest& request) const;
};


END_NCBI_SCOPE

#endif // OBJECTS_TRACKMGR_GRIDCLI__CREATE_TRACKSET_CLIENT_HPP

