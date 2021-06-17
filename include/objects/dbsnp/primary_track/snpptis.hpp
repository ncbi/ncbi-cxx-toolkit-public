#ifndef SRA__READER__SRA__SNPPTIS__HPP
#define SRA__READER__SRA__SNPPTIS__HPP
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
 *   Access to primary SNP track service
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSeq_id;

class CSnpPtisClient : public CObject
{
public:
    CSnpPtisClient();
    virtual ~CSnpPtisClient();

    virtual string GetPrimarySnpTrackForGi(TGi gi) = 0;
    virtual string GetPrimarySnpTrackForAccVer(const string& acc_ver) = 0;
    
    string GetPrimarySnpTrackForId(const string& id);
    string GetPrimarySnpTrackForId(const CSeq_id& id);

    static bool IsEnabled();
    static CRef<CSnpPtisClient> CreateClient();
};

END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__SNPPTIS__HPP
