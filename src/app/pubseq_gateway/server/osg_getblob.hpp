#ifndef PSGS_OSGGETBLOB__HPP
#define PSGS_OSGGETBLOB__HPP

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
 * File Description: processor to get blob/chunk from OSG
 *
 */

#include "osg_getblob_base.hpp"


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

class CPSGS_OSGGetBlob : public CPSGS_OSGGetBlobBase
{
public:
    CPSGS_OSGGetBlob(const CRef<COSGConnectionPool>& pool,
                     const shared_ptr<CPSGS_Request>& request,
                     const shared_ptr<CPSGS_Reply>& reply,
                     TProcessorPriority priority);
    virtual ~CPSGS_OSGGetBlob();

    virtual string GetName() const override;

    static bool CanProcess(SPSGS_BlobBySatSatKeyRequest& request);
    
    void CreateRequests() override;
    virtual void ProcessReplies() override;
};


class CPSGS_OSGGetChunks : public CPSGS_OSGGetBlobBase
{
public:
    CPSGS_OSGGetChunks(const CRef<COSGConnectionPool>& pool,
                       const shared_ptr<CPSGS_Request>& request,
                       const shared_ptr<CPSGS_Reply>& reply,
                       TProcessorPriority priority);
    virtual ~CPSGS_OSGGetChunks();
    
    virtual string GetName() const override;

    static bool CanProcess(SPSGS_TSEChunkRequest& request);
    
    void CreateRequests() override;
    virtual void ProcessReplies() override;
};


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_OSGGETBLOB__HPP
