#ifndef NCBI_CACHED_TAXON3_HPP
#define NCBI_CACHED_TAXON3_HPP

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
 * Author:  Brad Holmes
 *
 * File Description:
 *     Taxon service that caches a parameterized number of replies
 *      for queries by list of org-ref.  It *DOES NOT* cache replies
 *      for request objects.
 *
 */


#include <objects/taxon3/taxon3__.hpp>
#include <objects/taxon3/itaxon3.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <serial/serialdef.hpp>
#include <connect/ncbi_types.h>
#include <corelib/ncbi_limits.hpp>
#include <util/ncbi_cache.hpp>

#include <list>
#include <vector>
#include <map>


BEGIN_NCBI_SCOPE

class CObjectOStream;
class CConn_ServiceStream;


BEGIN_objects_SCOPE

class NCBI_TAXON3_EXPORT CCachedTaxon3 : public ITaxon3,
    protected CCache<string, CRef<CTaxon3_reply> > 
{

private:
    CCachedTaxon3(AutoPtr<ITaxon3> taxon, TSizeType capacity);

public:
    virtual ~CCachedTaxon3() {};

    typedef string TCacheKey;

    // The method to create the cache one-off
    static AutoPtr<CCachedTaxon3> Create(
        AutoPtr<ITaxon3> taxon, TSizeType capacity = 100000);

    // This should only be used if it will be
    // immediately wrapped into a safe pointer
    static CCachedTaxon3* CreateUnSafe(
        AutoPtr<ITaxon3> taxon, TSizeType capacity = 100000);

    //---------------------------------------------
    // Taxon1 server init
    // Returns: TRUE - OK
    //          FALSE - Can't open connection to taxonomy service
    ///
    virtual void Init(void);  // default:  120 sec timeout, 5 reconnect attempts,
                     
    virtual void Init(const STimeout* timeout, unsigned reconnect_attempts=5);

    // submit a list of org_refs
    virtual CRef<CTaxon3_reply>    SendOrgRefList(const vector<CRef< COrg_ref> >& list);
    virtual CRef< CTaxon3_reply >  SendRequest(const CTaxon3_request& request);

    //--------------------------------------------------
    // Get error message after latest erroneous operation
    // Returns: error message, or empty string if no error occurred
    // FIXME: Not implemented properly at this time.
    virtual const string& GetLastError() const { NCBI_USER_THROW("LastError state is not properly implemented");  return m_sLastError; }


private:

    CRef<CTaxon3_reply> x_AddReplyToCache(const TCacheKey& key, const COrg_ref& org_ref);
    CRef<CTaxon3_reply> x_GetReplyForOrgRef(const COrg_ref& org_ref);



    ESerialDataFormat        m_eDataFormat;
    const char*              m_pchService;
    STimeout*                m_timeout;  // NULL, or points to "m_timeout_value"
    STimeout                 m_timeout_value;

    unsigned                 m_nReconnectAttempts;

    string                   m_sLastError;

    /// The cached taxon does not own the taxon.
    AutoPtr<ITaxon3>         m_taxon;

    void             SetLastError(const char* err_msg);

};


END_objects_SCOPE
END_NCBI_SCOPE

#endif //NCBI_CACHED_TAXON3_HPP
