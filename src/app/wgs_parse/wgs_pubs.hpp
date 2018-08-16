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

#ifndef WGS_PUBS_HPP
#define WGS_PUBS_HPP

#include <unordered_map>
#include <unordered_set>

#include <objects/seq/Pubdesc.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


namespace wgsparse
{

struct CPubInfo
{
    CRef<CPubdesc> m_desc;
    int m_pmid;
    string m_pubdesc_key;

    CPubInfo() :
        m_pmid(0) {};
};

class CPubCollection
{
public:
	CPubCollection() {};

    string AddPub(CPubdesc& pubdesc, bool medline_lookup);
    CPubInfo& GetPubInfo(const string& pubdesc_key);

    static string GetPubdescKeyForCitSub(CPubdesc& pubdesc, const CDate_std* submission_date);

private:
	unordered_map<string, CPubInfo> m_pubs;
};

}

#endif // WGS_PUBS_HPP
