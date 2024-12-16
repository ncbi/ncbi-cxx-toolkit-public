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
 * Author: Vitaly Stakhovsky, NCBI
 *
 * File Description:
 *   Pubmed Updater based on EUtils
 *
 * ===========================================================================
 */

#ifndef _EUTILS_UPDATER_HPP_
#define _EUTILS_UPDATER_HPP_

#include <objtools/eutils/api/eutils.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
class CPub;
class CCit_art;
class CPubmed_entry;
BEGIN_SCOPE(edit)

enum class EPubmedError {
    ok,
    not_found,
    operational_error,
    citation_not_found,
    citation_ambiguous,
    cannot_connect_pmdb,
    cannot_connect_searchbackend_pmdb,
};

NCBI_XOBJEDIT_EXPORT
CNcbiOstream& operator<<(CNcbiOstream&, EPubmedError);

struct NCBI_XOBJEDIT_EXPORT SCitMatch {
    string Journal;
    string Volume;
    string Page;
    string Year;
    string Author;
    string Issue;
    string Title;
    bool   InPress = false;
    bool   Option1 = false;

    void FillFromArticle(const CCit_art&);
};

using TPubInterceptor = std::function<void(CRef<CPub>&)>;

class NCBI_XOBJEDIT_EXPORT CEUtilsUpdater
{
public:
    enum class ENormalize { Off, On };
    enum class EPubmedSource { EUtils, PubOne };

public:
    CEUtilsUpdater(ENormalize = ENormalize::Off, EPubmedSource = EPubmedSource::EUtils);
    virtual ~CEUtilsUpdater() {}

    virtual bool       Init() { return true; }
    virtual void       Fini() {}
    virtual TEntrezId  CitMatch(const CPub&, EPubmedError* = nullptr);
    virtual TEntrezId  CitMatch(const SCitMatch&, EPubmedError* = nullptr);
    virtual CRef<CPub> GetPubmedEntry(TEntrezId pmid, EPubmedError* = nullptr);
    CRef<CPub>         GetPub(TEntrezId pmid, EPubmedError* = nullptr);
    void               SetBaseURL(string);

    TPubInterceptor SetPubInterceptor(TPubInterceptor f)
    {
        TPubInterceptor old = m_pub_interceptor;
        m_pub_interceptor = f;
        return old;
    }

    // Hydra replacement using citmatch api; RW-1918,RW-1999
    static bool DoPubSearch(const std::vector<string>& query, std::vector<TEntrezId>& pmids);

protected:
    CRef<CPubmed_entry> x_GetPubmedEntry(TEntrezId pmid, EPubmedError*);
    CRef<CPub>          x_GetPub(TEntrezId pmid, EPubmedError*);

protected:
    CRef<CEUtils_ConnContext> m_Ctx;
    ENormalize                m_Norm;
    EPubmedSource             m_pubmed_src;
    TPubInterceptor           m_pub_interceptor = nullptr;
};

using IPubmedUpdater = CEUtilsUpdater;

class NCBI_XOBJEDIT_EXPORT CEUtilsUpdaterWithCache : public CEUtilsUpdater
{
public:
    CEUtilsUpdaterWithCache(ENormalize norm = ENormalize::Off) :
        CEUtilsUpdater(norm) {}

    CRef<CPub> GetPubmedEntry(TEntrezId pmid, EPubmedError* = nullptr) override;

    void ReportStats(std::ostream&);
    void ClearCache();

private:
    map<TEntrezId, CConstRef<CPub>> m_cache;

    size_t m_num_requests = 0;
    size_t m_cache_hits   = 0;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // _EUTILS_UPDATER_HPP_
