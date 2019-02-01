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

#include <ncbi_pch.hpp>

#include <objects/general/Date.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>

#include "wgs_pubs.hpp"
#include "wgs_utils.hpp"
#include "wgs_med.hpp"

namespace wgsparse
{

static int GetPmid(const CPubdesc& pubdesc)
{
    int ret = 0;
    if (pubdesc.IsSetPub() && pubdesc.GetPub().IsSet()) {

        for (auto& pub : pubdesc.GetPub().Get()) {
            if (pub->IsPmid()) {
                ret = pub->GetPmid();
                break;
            }
        }
    }

    return ret;
}

string CPubCollection::AddPub(CPubdesc& pubdesc, bool medline_lookup)
{
    string pubdesc_str = GetPubdescKey(pubdesc, medline_lookup);
    auto it = m_pubs.find(pubdesc_str);

    if (it == m_pubs.end()) {
        
        it = m_pubs.insert({ pubdesc_str, CPubInfo()}).first;
        it->second.m_desc.Reset(&pubdesc);
        it->second.m_pmid = GetPmid(*it->second.m_desc);
    }

    return pubdesc_str;
}

CPubInfo& CPubCollection::GetPubInfo(const string& pubdesc_key)
{
    static CPubInfo empty_info;

    auto it = m_pubs.find(pubdesc_key);

    if (it != m_pubs.end()) {
        return it->second;
    }

    return empty_info;
}

string CPubCollection::GetPubdescKey(CPubdesc& pubdesc, bool medline_lookup)
{
    if (medline_lookup) {
        SinglePubLookup(pubdesc);
    }

    return ToStringKey(pubdesc);
}

string CPubCollection::GetPubdescKeyForCitSub(CPubdesc& pubdesc, const CDate_std* submission_date)
{
    CDate orig_date;
    CCit_sub* cit_sub = GetNonConstCitSub(pubdesc);

    string ret;
    if (cit_sub) {
        if (cit_sub->IsSetDate()) {
            orig_date.Assign(cit_sub->GetDate());

            if (submission_date) {
                cit_sub->SetDate().SetStd().Assign(*submission_date);
            }
            else {
                cit_sub->ResetDate();
            }
        }

        ret = ToStringKey(pubdesc);

        if (orig_date.Which() != CDate::e_not_set) {
            cit_sub->SetDate().Assign(orig_date);
        }
        else {
            cit_sub->ResetDate();
        }
    }

    return ret;
}

}
