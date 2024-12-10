/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include "utils.hpp"

#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

// DEPRECATED

namespace
{

class xDeprecatedQualsMoreData: public CObject
{
public:
    std::string m_qual;
};

void xRegisterBioSrc(CDiscrepancyContext& context, const CBioSource* biosrc, CReportNode& report_node, const std::string& qual)
{
    CRef<xDeprecatedQualsMoreData> more_obj;
    // no need to provide 'more' data in command line mode
    if (CDiscrepancySet::IsGui()) {
        more_obj.Reset(new xDeprecatedQualsMoreData);
        more_obj->m_qual = qual;
    }
    auto disc_obj = context.BiosourceObjRef(*biosrc, true, more_obj);
    auto key = "[n] bioseq[s] [has] '" + qual + "' qualifier";
    auto& rec = report_node["Deprecated Qualifiers"][key];
    rec.Add(*disc_obj);
}

template<typename _Container>
void xRegisterBioSrc(_Container& vec, CDiscrepancyContext& context, const CBioSource* biosrc, CReportNode& report_node)
{
    for (auto& it : vec) {
        auto& rec = *it;
        if (rec.IsDeprecated()) {
            auto qual = rec.GetSubtypeName(rec.GetSubtype(), std::remove_cvref_t<decltype(rec)>::eVocabulary_raw);
            xRegisterBioSrc(context, biosrc, report_node, qual);
        }
    }

}

template<typename _Container, typename _Removed>
unsigned xRemoveDeprecated(_Removed& removed_quals, _Container& vec, const xDeprecatedQualsMoreData* qual_to_remove)
{
    unsigned counter = 0;
    for (auto it = vec.begin(); it != vec.end(); ) {
        auto& rec = **it;
        if (rec.IsDeprecated()) {
            auto qual = rec.GetSubtypeName(rec.GetSubtype(), std::remove_cvref_t<decltype(rec)>::eVocabulary_raw);
            if (qual_to_remove == nullptr || (qual == qual_to_remove->m_qual)) {
                removed_quals.insert(qual);
                it = vec.erase(it);
                counter++;
                continue;
            }
        }
        it++;
    }
    return counter;
}

}

DISCREPANCY_CASE(DEPRECATED, BIOSRC, eDisc | eOncaller | eSubmitter | eSmart | eBig | eFatal, "Deprecated qualifiers")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->CanGetSubtype()) {
            auto& subtype = biosrc->GetSubtype();
            xRegisterBioSrc(subtype, context, biosrc, m_Objs);
        }
        if (biosrc->IsSetOrgMod()) {
            auto& mods = biosrc->GetOrg().GetOrgname().GetMod();
            xRegisterBioSrc(mods, context, biosrc, m_Objs);
        }
    }
}

DISCREPANCY_AUTOFIX(DEPRECATED)
{
    unsigned removed = 0;

    std::set<string> removed_quals;

    CBioSource* biosrc = GetBioSourceFromContext(obj, context);

    auto more_data = static_cast<const xDeprecatedQualsMoreData*>(obj->GetMoreInfo().GetPointerOrNull());

    if (biosrc) {

        if (biosrc->CanGetSubtype()) {
            auto& subtype = biosrc->SetSubtype();
            removed += xRemoveDeprecated(removed_quals, subtype, more_data);
        }
        if (biosrc->IsSetOrgMod()) {
            auto& mods = biosrc->SetOrg().SetOrgname().SetMod();
            removed += xRemoveDeprecated(removed_quals, mods, more_data);
        }
    }

    if (removed && !removed_quals.empty() ) {
        obj->SetFixed();
        auto s = NStr::Join(removed_quals, ", ");
        return Ref(new CAutofixReport("DEPRECATED: [n] removed qualifier[s] (" + s + ")", removed));
    } else {
        return {};
    }
}

DISCREPANCY_SUMMARIZE(DEPRECATED)
{
    xSummarize();
    if (CDiscrepancySet::IsGui()) {
        for (auto& rec: m_ReportItems) {
            CDiscrepancyItem* item = (CDiscrepancyItem*)rec.GetPointer();
            item->ClearTitle();
        }
    }
}

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
