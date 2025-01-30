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
 * Authors:  Sergiy Gotvyanskyy, NCBI
 *           Colleen Bolin, NCBI
 *
 * File Description:
 *   Front-end class for making remote request to MLA and taxon
 *
 * ===========================================================================
 */

#ifndef __REMOTE_UPDATER_HPP_INCLUDED__
#define __REMOTE_UPDATER_HPP_INCLUDED__

#include <corelib/ncbimisc.hpp>
#include <functional>
#include <objects/pub/Pub.hpp>
#include <objtools/edit/edit_error.hpp>
#include <objtools/edit/eutils_updater.hpp>
#include <objects/taxon3/itaxon3.hpp>
#include <mutex>

BEGIN_NCBI_SCOPE

class CSerialObject;

BEGIN_SCOPE(objects)

class CSeq_entry_EditHandle;
class CSeq_entry;
class CSeqdesc;
class CSeq_descr;
class COrg_ref;
class CAuth_list;
class IObjtoolsListener;
class CPubdesc;
class CTaxon3_reply;

BEGIN_SCOPE(edit)

class CCachedTaxon3_impl;

class NCBI_XOBJEDIT_EXPORT CRemoteUpdaterMessage: public CObjEditMessage
{
public:
    CRemoteUpdaterMessage(const string& msg, EPubmedError error)
    : CObjEditMessage(msg, eDiag_Warning), m_error(error)
    {
    }
    virtual CRemoteUpdaterMessage *Clone(void) const {
        return new CRemoteUpdaterMessage(GetText(), m_error);
    }

    EPubmedError m_error;
};

class NCBI_XOBJEDIT_EXPORT CRemoteUpdaterException: public CException
{
public:
    using CException::CException;
};

class NCBI_XOBJEDIT_EXPORT CRemoteUpdater
{
public:

    using FLogger = function<void(const string&)>;

    // With this constructor, failure to retrieve
    // a publication for a PMID is logged with the supplied message listener.
    // If no message listener is supplied, an exception is thrown.
    CRemoteUpdater(IObjtoolsListener* pMessageListener, CEUtilsUpdater::ENormalize = CEUtilsUpdater::ENormalize::Off);
    // NCBI_DEPRECATED
    CRemoteUpdater(FLogger logger, CEUtilsUpdater::ENormalize norm = CEUtilsUpdater::ENormalize::Off);
    ~CRemoteUpdater();

    void UpdatePubReferences(CSerialObject& obj);
    void UpdatePubReferences(CSeq_entry_EditHandle& obj);
    void SetMaxMlaAttempts(int max);
    // Specify flavor of updated CPub: CMedline_entry or CCit_art
    void SetPubReturnType(CPub::E_Choice t)
    {
        if (t == CPub::e_Medline || t == CPub::e_Article) {
            m_pm_pub_type = t;
        } else {
            throw std::invalid_argument("invalid CPub choice");
        }
    }

    CConstRef<CTaxon3_reply> SendOrgRefList(const vector<CRef<COrg_ref>>& list);

    void UpdateOrgFromTaxon(CSeq_entry& entry);
    void UpdateOrgFromTaxon(CSeqdesc& desc);
    void SetTaxonTimeout(unsigned seconds, unsigned retries, bool exponential);

    void ClearCache();
    static void ConvertToStandardAuthors(CAuth_list& auth_list);
    static void PostProcessPubs(CSeq_entry_EditHandle& obj);
    static void PostProcessPubs(CSeq_entry& obj);
    static void PostProcessPubs(CPubdesc& pubdesc);

    void SetPubmedClient(CEUtilsUpdater*);
    // Use either shared singleton or individual instances
    NCBI_DEPRECATED static CRemoteUpdater& GetInstance();
    void ReportStats(std::ostream& str);
    taxupdate_func_t GetUpdateFunc() const { return m_taxon_update; }
    TPubInterceptor SetPubmedInterceptor(TPubInterceptor f)
    {
        TPubInterceptor old = m_pm_interceptor;
        m_pm_interceptor = f;
        return old;
    }
    void SetBaseURL(string url)
    {
        m_pm_url = url;
    }

private:
    void xUpdatePubReferences(CSeq_entry& entry);
    void xUpdatePubReferences(CSeq_descr& descr);
    void xUpdateOrgTaxname(COrg_ref& org);
    bool xUpdatePubPMID(list<CRef<CPub>>& pubs, TEntrezId id);
    bool xSetFromConfig();
    void xInitTaxCache();

    IObjtoolsListener* m_pMessageListener = nullptr;
    FLogger m_logger = nullptr; // wrapper for compatibility between IObjtoolsListener and old FLogger

    string                     m_pm_url;
    unique_ptr<CEUtilsUpdater> m_pubmed;
    bool                       m_pm_use_cache = true;
    CEUtilsUpdater::ENormalize m_pm_normalize = CEUtilsUpdater::ENormalize::Off;
    TPubInterceptor            m_pm_interceptor = nullptr;
    CPub::E_Choice             m_pm_pub_type = CPub::e_Article;

    unique_ptr<CCachedTaxon3_impl> m_taxClient;
    taxupdate_func_t m_taxon_update;

    std::mutex m_Mutex;
    int m_MaxMlaAttempts = 3;

    bool m_TaxonTimeoutSet = false;
    unsigned m_TaxonTimeout = 20;   // in seconds
    unsigned m_TaxonAttempts = 5;
    bool m_TaxonExponential = false;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
