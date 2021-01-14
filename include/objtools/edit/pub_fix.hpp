#ifndef _PUB_FIX_HPP_
#define _PUB_FIX_HPP_

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

BEGIN_SCOPE(edit)

class CPubFix
{
public:

    CPubFix(bool always_lookup, bool replace_cit, bool merge_ids, IMessageListener* err_log) :
        m_always_lookup(always_lookup),
        m_replace_cit(replace_cit),
        m_merge_ids(merge_ids),
        m_err_log(err_log)
    {
    }

    void FixPub(CPub& pub);
    void FixPubEquiv(CPub_equiv& pub_equiv);

    static CRef<CCit_art> FetchPubPmId(TEntrezId pmid);
    static string GetErrorId(int code, int subcode);

private:
    bool m_always_lookup,
        m_replace_cit,
        m_merge_ids;

    IMessageListener* m_err_log;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // MISC_FIX_PUB__HPP
