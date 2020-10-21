#ifndef PUBSEQ_GATEWAY_ID2INFO__HPP
#define PUBSEQ_GATEWAY_ID2INFO__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   PSG ID2 info wrapper
 *
 */

#include "cass_blob_id.hpp"

#include <string>
using namespace std;


const int64_t   kSplitInfoChunk = 999999999;


// There could be many ID2INFO flavors
enum EPSGSId2InfoFlavor {
    ePSGS_SatInfoChunksVerId2InfoFlavor,
    ePSGS_IdModifiedVerId2InfoFlavor,
    ePSGS_UnknownId2InfoFlavor
};


// May be found in cassandra
class CPSGS_SatInfoChunksVerFlavorId2Info
{
    public:
        using TSat = int16_t;
        using TInfo = int32_t;
        using TChunks = int32_t;
        using TSplitVersion = int32_t;

    public:
        CPSGS_SatInfoChunksVerFlavorId2Info(const string &  id2_info,
                                            bool  count_errors = true);
        CPSGS_SatInfoChunksVerFlavorId2Info() :
            m_Sat(-1), m_Info(-1), m_Chunks(-1),
            m_SplitVersion(-1), m_SplitVersionPresent(false)
        {}

    public:
        TSat GetSat(void) const
        { return m_Sat; }

        TInfo GetInfo(void) const
        { return m_Info; }

        TChunks GetChunks(void) const
        { return m_Chunks; }

        TSplitVersion GetSplitVersion(void) const
        { return m_SplitVersion; }

        string Serialize(void) const;

    private:
        TSat            m_Sat;
        TInfo           m_Info;
        TChunks         m_Chunks;
        TSplitVersion   m_SplitVersion;
        bool            m_SplitVersionPresent;
};


// May come in the URL
class CPSGS_IdModifiedVerFlavorId2Info
{
    public:
        using TLastModified = int64_t;
        using TSplitVersion = int32_t;

    public:
        CPSGS_IdModifiedVerFlavorId2Info(const string &  id2_info);
        CPSGS_IdModifiedVerFlavorId2Info() :
            m_LastModified(-1), m_SplitVersion(-1),
            m_LastModifiedPresent(false),
            m_SplitVersionPresent(false)
        {}

    public:
        SCass_BlobId &  GetTSEId(void)
        { return m_TSEId; }

        TLastModified GetLastModified(void) const
        { return m_LastModified; }

        TSplitVersion GetSplitVersion(void) const
        { return m_SplitVersion; }

        string Serialize(void) const;

    private:
        SCass_BlobId    m_TSEId;
        TLastModified   m_LastModified;
        TSplitVersion   m_SplitVersion;
        bool            m_LastModifiedPresent;
        bool            m_SplitVersionPresent;
};

#endif /* PUBSEQ_GATEWAY_ID2INFO__HPP */

