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

class CAuthListValidator
{
public:
    enum EOutcome {
        eNotSet = 0,
        eFailed_validation,
        eAccept_pubmed,
        eKeep_genbank
    };
    static void Configure(const CNcbiRegistry& cfg, const string& section);
    // If true, FixPubEquiv() will use this class to validate authors list
    static bool enabled;
    CAuthListValidator(IMessageListener* err_log);
    EOutcome validate(const CCit_art& gb_art, const CCit_art& pm_art);
    void DebugDump(CNcbiOstream& out) const;
    // utility method
    static void get_lastnames(const CAuth_list& authors, list<string>& lastnames);
    
    // public vars
    EOutcome outcome;
    int pub_year;
    int cnt_gb;
    int cnt_pm;
    int cnt_matched;
    int cnt_added;      // new from pubmed list
    int cnt_removed;    // not matched in genbank list
    int cnt_min;        // minimum # in GB/PM list, use as a base for ration
    list<string> matched;
    list<string> removed;
    list<string> added;
    string gb_type;
    string pm_type;
    // for DebugDump()
    string reported_limit;
    double actual_matched_to_min;
    double actual_removed_to_gb;

private:
    void compare_lastnames();
    void dumplist(const char* hdr, const list<string>& lst, CNcbiOstream& out) const;
    static void get_lastnames(const CAuth_list::C_Names::TStd& authors, list<string>& lastnames);
    static void get_lastnames(const CAuth_list::C_Names::TStr& authors, list<string>& lastnames);
    // vars
    IMessageListener* m_err_log;
    static bool configured;
    static double cfg_matched_to_min;
    static double cfg_removed_to_gb;
};

class CPubFixing
{
public:

    CPubFixing(bool always_lookup, bool replace_cit, bool merge_ids, IMessageListener* err_log) :
        m_always_lookup(always_lookup),
        m_replace_cit(replace_cit),
        m_merge_ids(merge_ids),
        m_err_log(err_log),
        m_authlist_validator(err_log)
    {
    }

    void FixPub(CPub& pub);
    void FixPubEquiv(CPub_equiv& pub_equiv);
    const CAuthListValidator& GetValidator() const { return m_authlist_validator; };

    static CRef<CCit_art> FetchPubPmId(TEntrezId pmid);
    static string GetErrorId(int code, int subcode);

private:
    bool m_always_lookup,
        m_replace_cit,
        m_merge_ids;

    IMessageListener* m_err_log;
    CAuthListValidator m_authlist_validator;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // MISC_FIX_PUB__HPP
