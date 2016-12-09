/*
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
*
* ===========================================================================
*/

#ifndef JOURNAL_HOOK_HPP
#define JOURNAL_HOOK_HPP

#include <string>
#include <map>
#include <memory>

#include <corelib/ncbistl.hpp>
#include <serial/objistr.hpp>

namespace ncbi
{
    class CEutilsClient;

    namespace objects
    {
        class CCit_jour;
    }
}

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace pub_report
{

class CJournalReport;

class CSkipPubJournalHook : public CSkipObjectHook
{
public:
    CSkipPubJournalHook(CJournalReport& report);

    virtual void SkipObject(CObjectIStream &in, const CObjectTypeInfo &info);

private:

    bool IsJournalMissing(const string& title);
    void ProcessJournal(const CCit_jour& journal);

    CEutilsClient& GetEUtils();

    CJournalReport& m_report;
    map<string, bool> m_term_cache;
    shared_ptr<CEutilsClient> m_eutils;
};

}

#endif //JOURNAL_HOOK_HPP