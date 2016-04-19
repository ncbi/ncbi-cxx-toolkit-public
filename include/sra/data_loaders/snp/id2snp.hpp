#ifndef SRA__LOADER__SNP__ID2SNP__HPP
#define SRA__LOADER__SNP__ID2SNP__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Processor of ID2 requests for SNP data
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_config.hpp>
#include <objects/id2/id2processor.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CID2SNPProcessor_Impl;

class NCBI_ID2PROC_SNP_EXPORT CID2SNPProcessor : public CID2Processor
{
public:
    CID2SNPProcessor(void);
    CID2SNPProcessor(const CConfig::TParamTree* params,
                     const string& driver_name);
    virtual ~CID2SNPProcessor(void);

    virtual TReplies ProcessSomeRequests(CID2_Request_Packet& packet,
                                         CID2ProcessorResolver* resolver);

    virtual bool ProcessRequest(TReplies& replies,
                                CID2_Request& request,
                                CID2ProcessorResolver *resolver);

private:
    CRef<CID2SNPProcessor_Impl> m_Impl;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__LOADER__SNP__ID2SNP__HPP
