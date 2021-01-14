#ifndef MISC_FIX_PUB__HPP
#define MISC_FIX_PUB__HPP

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
 * Author:  Alexey Dobronadezhdin
 *
 * File Description:
 *   Code for fixing up publications.
 *
 * ===========================================================================
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class IMessageListener;

BEGIN_SCOPE(objects)

class CPub;
class CPub_equiv;
class CCit_art;

class CAuthorCompareSettings {
public:
    CAuthorCompareSettings() 
    : m_limit_list( {
            { 1984, 1995, 10, EAction::eKeepGB, EAction::eKeepGB },
            { 1996, 1999, 25, EAction::eKeepGB, EAction::eKeepGB },
            { 2000, 3000, -1, EAction::eKeepGB, EAction::eAccept }
        }
      ),
      percent_Pubmed_match(30), 
      percent_GB_match(30) {};
    enum EAction {
        eAccept,     // accept pubmed authors
        eKeepGB,     // keep Genbank authors
        eReject      // consider author comarison failed, cancel pub modification
    };
    struct SPubmedLimit {
        int year_from;  // for the firs period, specify 1900
        int year_to;    // for the last period specify 3000
        int limit;      // for no limit, specify -1; all actions ignored
        EAction on_below_limit; // what to do, if pubmed # is below limit
        EAction on_equal_to_limit;
    };
    vector<SPubmedLimit> m_limit_list;  // it's expected to be ordered by year range
    int percent_Pubmed_match;  // if Pubmed list is shorter than GB list
    int percent_GB_match;   // if GB list is shorter than Pubmed list, normally the same as above
};

/*-------------------------------------------------------------------------------
https://jira.ncbi.nlm.nih.gov/browse/ID-6514?focusedCommentId=6241819&page=com.atlassian.jira.plugin.system.issuetabpanels:comment-tabpanel#comment-6241819
As requested by Mark Cavanaugh:
So here's how I imagine things working Leonid:

1) PubMed Cit-art pub has a year value > 1999

Accept the Auth-list of the PubMed article, as-is

Consider generating a warning if the PubMed article author count is significantly less than the original author count.

"Significant" ? Hmmmmm..... Let's try: Auth-Count-Diff >= 1/3 * Orig-Auth-Count

2) PubMed Cit-art pub has a year value ranging from 1996 to 1999

If the original author count is > 25, preserve the Auth-list of the original article, discarding PubMed's author list

Log the author name counts : Original vs PubMed
Log the author lists: Original vs Pubmed

3) PubMed Cit-art pub has a year value < 1996

If the original author count is > 10, preserve the Auth-list of the original article, discarding PubMed's author list

Log the author name counts : Original vs PubMed
Log the author lists: Original vs Pubmed

We may have to tweak things a bit further, but this is a good start.
-------------------------------------------------------------------------------*/
class CAuthorsValidatorByPubYear {
public:
    enum EOutcome {
        eInvalid,
        eAccept_pubmed,
        eKeep_genbank
    };
    static void Configure(CNcbiRegistry& cfg, string& section);
    CAuthorsValidatorByPubYear(int pub_year, const vector<string>& gb_lastnames, const vector<string>& pm_lastnames);
    EOutcome validate();
    
    // public vars
    int pub_year;
    int cnt_gb;
    int cnt_pm;
    int cnt_matched;
    int cnt_added; // new from pubmed list
    int cnt_removed; // not matched in genbank list
    list<string> matched;
    list<string> removed;
    list<string> added;
private:
    static bool configured;
};

class CPubFixing
{
public:

    CPubFixing(bool always_lookup, bool replace_cit, bool merge_ids, IMessageListener* err_log) :
        m_always_lookup(always_lookup),
        m_replace_cit(replace_cit),
        m_merge_ids(merge_ids),
        m_err_log(err_log)
    {
    }
    void SetAuthorCompareSettings(CAuthorCompareSettings acs);

    void FixPub(CPub& pub);
    void FixPubEquiv(CPub_equiv& pub_equiv);

    static CRef<CCit_art> FetchPubPmId(TEntrezId pmid);
    static string GetErrorId(int code, int subcode);

private:
    bool m_always_lookup,
        m_replace_cit,
        m_merge_ids;

    IMessageListener* m_err_log;
    CAuthorCompareSettings m_author_settings;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // MISC_FIX_PUB__HPP
