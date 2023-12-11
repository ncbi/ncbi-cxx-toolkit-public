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
 * Author:
 *
 * File Description:
 *
 * ===========================================================================
 */

#ifndef _PUBMED_UPDATER_HPP_
#define _PUBMED_UPDATER_HPP_

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CPub;
class CCit_art;

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
CNcbiOstream& operator<<(CNcbiOstream& os, EPubmedError err);

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

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _PUBMED_UPDATER_HPP_
