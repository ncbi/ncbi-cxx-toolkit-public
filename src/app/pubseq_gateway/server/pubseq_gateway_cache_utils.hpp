#ifndef PUBSEQ_GATEWAY_CACHE_UTILS__HPP
#define PUBSEQ_GATEWAY_CACHE_UTILS__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include "pubseq_gateway_types.hpp"
#include "pubseq_gateway_utils.hpp"

#include <string>
using namespace std;

USING_IDBLOB_SCOPE;


class CPSGCache
{
public:
    CPSGCache(bool  allowed) :
        x_Allowed(allowed)
    {}

    ECacheLookupResult  LookupBioseqInfo(SBioseqResolution &  bioseq_resolution)
    {
        if (x_Allowed)
            return s_LookupBioseqInfo(bioseq_resolution);
        return eNotFound;
    }

    ECacheLookupResult  LookupSi2csi(SBioseqResolution &  bioseq_resolution)
    {
        if (x_Allowed)
            return s_LookupSi2csi(bioseq_resolution);
        return eNotFound;
    }

    ECacheLookupResult  LookupBlobProp(int  sat,
                                       int  sat_key,
                                       int64_t &  last_modified,
                                       CBlobRecord &  blob_record)
    {
        if (x_Allowed)
            return s_LookupBlobProp(sat, sat_key, last_modified, blob_record);
        return eNotFound;
    }

public:
    static
    ECacheLookupResult  s_LookupBioseqInfo(SBioseqResolution &  bioseq_resolution);

    static
    ECacheLookupResult  s_LookupSi2csi(SBioseqResolution &  bioseq_resolution);

    static
    ECacheLookupResult  s_LookupBlobProp(int  sat,
                                         int  sat_key,
                                         int64_t &  last_modified,
                                         CBlobRecord &  blob_record);

private:
    bool        x_Allowed;
};

#endif
