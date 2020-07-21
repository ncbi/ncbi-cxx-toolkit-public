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
 * Authors: Eugene Vasilchenko
 *
 * File Description: processor for data from OSG
 *
 */

#include <ncbi_pch.hpp>

#include "osg_getblob_base.hpp"

#include <objects/id2/ID2_Blob_Id.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

CPSGS_OSGGetBlobBase::CPSGS_OSGGetBlobBase(const CRef<COSGConnectionPool>& pool,
                                           const shared_ptr<CPSGS_Request>& request,
                                           const shared_ptr<CPSGS_Reply>& reply)
    : CPSGS_OSGProcessorBase(pool, request, reply)
{
}


static const char kOSG_IdPrefix[] = "OSG:";
static const int kOSG_Sat_WGS_min = 1000;
static const int kOSG_Sat_WGS_max = 1130;
static const int kOSG_Sat_SNP_min = 2001;
static const int kOSG_Sat_SNP_max = 2099;
static const int kOSG_Sat_CDD_min = 8087;
static const int kOSG_Sat_CDD_max = 8087;
//static const int kOSG_Sat_NAGraph_min = 8000;
//static const int kOSG_Sat_NAGraph_max = 8000;


static bool s_ParseOSGBlob(const SPSGS_BlobId& blob_id,
                           Int4& sat, Int4& subsat, Int4& satkey)
{
    CTempString id = blob_id.GetId();
    if ( !NStr::StartsWith(id, kOSG_IdPrefix) ) {
        return false;
    }
    id = id.substr(strlen(kOSG_IdPrefix));
    vector<CTempString> parts;
    NStr::Split(id, ".", parts);
    if ( parts.size() == 2 ) {
        // sat.satkey
        sat = NStr::StringToNumeric<typeof(sat)>(parts[0]);
        subsat = 0;
        satkey = NStr::StringToNumeric<typeof(satkey)>(parts[1]);
    }
    else if ( parts.size() == 3 ) {
        // sat.subsat.satkey
        sat = NStr::StringToNumeric<typeof(sat)>(parts[0]);
        subsat = NStr::StringToNumeric<typeof(sat)>(parts[1]);
        satkey = NStr::StringToNumeric<typeof(satkey)>(parts[2]);
    }
    else {
        return false;
    }
    return true;
}


bool CPSGS_OSGGetBlobBase::IsOSGBlob(const SPSGS_BlobId& blob_id)
{
    Int4 sat;
    Int4 subsat;
    Int4 satkey;
    if ( !s_ParseOSGBlob(blob_id, sat, subsat, satkey) ) {
        return false;
    }
    if ( sat >= kOSG_Sat_WGS_min &&
         sat <= kOSG_Sat_WGS_max ) {
        return true;
    }
    if ( sat >= kOSG_Sat_SNP_min &&
         sat <= kOSG_Sat_SNP_max ) {
        return true;
    }
    if ( sat >= kOSG_Sat_CDD_min &&
         sat <= kOSG_Sat_CDD_max ) {
        return true;
    }
    /*
    if ( sat >= kOSG_Sat_NAGraph_min &&
         sat <= kOSG_Sat_NAGraph_max ) {
        return true;
    }
    */
    return false;
}


CRef<CID2_Blob_Id> CPSGS_OSGGetBlobBase::GetOSGBlobId(const SPSGS_BlobId& blob_id)
{
    Int4 sat;
    Int4 subsat;
    Int4 satkey;
    if ( !s_ParseOSGBlob(blob_id, sat, subsat, satkey) ) {
        return null;
    }
    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    id->SetSat(sat);
    id->SetSub_sat(subsat);
    id->SetSat_key(satkey);
    return id;
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
