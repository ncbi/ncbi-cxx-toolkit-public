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
#include "utils.hpp"
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/validator/utilities.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(seqdesc_per_bioseq);

// RETROVIRIDAE_DNA
static const std::string kGenomeNotProviral = "[n] Retroviridae biosource[s] on DNA sequences [is] not proviral";

DISCREPANCY_CASE(RETROVIRIDAE_DNA, CSeqdesc_BY_BIOSEQ, eOncaller, "Retroviridae DNA")
{
    if (!obj.IsSource() || !context.IsDNA()) {
        return;
    }
    const CBioSource& src = obj.GetSource();
    if (src.IsSetLineage() && context.HasLineage(src, src.GetLineage(), "Retroviridae")) {
        if (!src.IsSetGenome() || src.GetGenome() != CBioSource::eGenome_proviral) {
            m_Objs[kGenomeNotProviral].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel(), eKeepRef));
        }
    }
}


DISCREPANCY_SUMMARIZE(RETROVIRIDAE_DNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(RETROVIRIDAE_DNA)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (desc) {
            CSeqdesc* desc_handle = const_cast<CSeqdesc*>(desc); // Is there a way to do this without const_cast???
            desc_handle->SetSource().SetGenome(CBioSource::eGenome_proviral);
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("RETROVIRIDAE_DNA: Genome set to proviral for [n] sequence[s]", n) : 0);
}


// NON_RETROVIRIDAE_PROVIRAL
static const std::string kGenomeProviral = "[n] non-Retroviridae biosource[s] [is] proviral";

DISCREPANCY_CASE(NON_RETROVIRIDAE_PROVIRAL, CSeqdesc_BY_BIOSEQ, eOncaller, "Non-Retroviridae biosources that are proviral")
{
    if (!obj.IsSource() || !context.IsDNA()) {
        return;
    }

    const CBioSource& src = obj.GetSource();
    if (src.IsSetLineage() && !context.HasLineage(src, src.GetLineage(), "Retroviridae")) {
        if (src.IsSetGenome() && src.GetGenome() == CBioSource::eGenome_proviral) {

            m_Objs[kGenomeProviral].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel(), eKeepRef));
        }
    }
}


DISCREPANCY_SUMMARIZE(NON_RETROVIRIDAE_PROVIRAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BAD_MRNA_QUAL

DISCREPANCY_CASE(BAD_MRNA_QUAL, CSeqdesc_BY_BIOSEQ, eOncaller, "mRNA sequence contains rearranged or germline")
{
    if (!obj.IsSource() || !context.IsCurrentSequenceMrna()) {
        return;
    }
    const CBioSource& src = obj.GetSource();
    if (!src.IsSetSubtype()) {
        return;
    }
    ITERATE(CBioSource::TSubtype, s, src.GetSubtype()) {
        if ((*s)->IsSetSubtype() && ((*s)->GetSubtype() == CSubSource::eSubtype_germline || (*s)->GetSubtype() == CSubSource::eSubtype_rearranged)) {
            m_Objs["[n] mRNA sequence[s] [has] germline or rearranged qualifier"].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel()));
            return;
        }
    }
}


DISCREPANCY_SUMMARIZE(BAD_MRNA_QUAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ORGANELLE_NOT_GENOMIC

static const std::string kOrganelleNotGenomic = "[n] non-genomic sequence[s] [is] organelle[s]";

DISCREPANCY_CASE(ORGANELLE_NOT_GENOMIC, CSeqdesc_BY_BIOSEQ, eDisc | eOncaller, "Organelle location should have genomic moltype")
{
    if (!obj.IsSource() || context.GetCurrentBioseq()->IsAa()) {
        return;
    }
    const CMolInfo* molinfo = context.GetCurrentMolInfo();
    if (molinfo == nullptr ||
        (!molinfo->IsSetBiomol() || molinfo->GetBiomol() == CMolInfo::eBiomol_genomic) && context.IsDNA()) {
        return;
    }
    if (context.IsOrganelle()) {
        m_Objs[kOrganelleNotGenomic].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel()));
    }
}


DISCREPANCY_SUMMARIZE(ORGANELLE_NOT_GENOMIC)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SWITCH_STRUCTURED_COMMENT_PREFIX

static const std::string kBadStructCommentPrefix = "[n] structured comment[s] [is] invalid but would be valid with a different prefix";

static const char* kStructuredCommentPrefix = "StructuredCommentPrefix";
static const char* kStructuredCommentSuffix = "StructuredCommentSuffix";

enum EPrefixOrSuffixType
{
    eType_none,
    eType_prefix,
    eType_suffix
};

static EPrefixOrSuffixType GetPrefixOrSuffixType(const CUser_field& field)
{
    EPrefixOrSuffixType type = eType_none;

    if (field.IsSetData() && field.GetData().IsStr() && field.IsSetLabel() && field.GetLabel().IsStr()) {
        if (NStr::Equal(field.GetLabel().GetStr(), kStructuredCommentPrefix)) {
            type = eType_prefix;
        }
        else if (NStr::Equal(field.GetLabel().GetStr(), kStructuredCommentSuffix)) {
            type = eType_suffix;
        }
    }

    return type;
}

static bool IsAppropriateRule(const CComment_rule& rule, const CUser_object& user)
{
    bool ret = true;
    if (user.IsSetData() && !user.GetData().empty()) {
        ITERATE(CUser_object::TData, field, user.GetData()) {
            EPrefixOrSuffixType prefixOrSuffix = GetPrefixOrSuffixType(**field);
            if (prefixOrSuffix == eType_none) {
                CConstRef<CField_rule> field_rule = rule.FindFieldRuleRef((*field)->GetLabel().GetStr());
                if (field_rule.Empty()) {
                    ret = false;
                    break;
                }
            }
        }
    }

    return ret;
}

const CComment_rule* FindAppropriateRule(const CComment_set& rules, const CUser_object& user)
{
    const CComment_rule* ret = nullptr;
    if (rules.IsSet()) {
        ITERATE(CComment_set::Tdata, rule, rules.Get()) {
            CComment_rule::TErrorList errors = (*rule)->IsValid(user);
            if (errors.empty() && IsAppropriateRule(**rule, user)) {
                if (ret == nullptr) {
                    ret = *rule;
                }
                else {
                    ret = nullptr;
                    break;
                }
            }
        }
    }

    return ret;
}

DISCREPANCY_CASE(SWITCH_STRUCTURED_COMMENT_PREFIX, CSeqdesc, eOncaller, "Suspicious structured comment prefix")
{
    if (obj.IsUser() && obj.GetUser().GetObjectType() == CUser_object::eObjectType_StructuredComment) {
        const CUser_object& user = obj.GetUser();
        string prefix = CComment_rule::GetStructuredCommentPrefix(user);
        if (!prefix.empty()) {
            CConstRef<CComment_set> comment_rules = CComment_set::GetCommentRules();
            const CComment_rule* rule;
            try {
                rule = &comment_rules->FindCommentRule(prefix);
            }
            catch(...) {
                return;
            }
            CComment_rule::TErrorList errors = rule->IsValid(user);
            if (!errors.empty()) {
                const CComment_rule* new_rule = FindAppropriateRule(*comment_rules, user);
                if (new_rule) {
                    m_Objs[kBadStructCommentPrefix].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel(), eKeepRef, true, const_cast<CComment_rule*>(new_rule)));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(SWITCH_STRUCTURED_COMMENT_PREFIX)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(SWITCH_STRUCTURED_COMMENT_PREFIX)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;

    CConstRef<CComment_set> comment_rules = CComment_set::GetCommentRules();

    NON_CONST_ITERATE(TReportObjectList, it, list) {
        CDiscrepancyObject* dobj = dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer());

        const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(dobj->GetObject().GetPointer());
        const CComment_rule* new_rule = dynamic_cast<const CComment_rule*>(dobj->GetMoreInfo().GetPointer());
        if (desc && new_rule) {

            CSeqdesc* desc_handle = const_cast<CSeqdesc*>(desc);
            CUser_object& user = desc_handle->SetUser();

            static const string prefix_tail = "START##";
            static const string suffix_tail = "END##";

            string prefix = new_rule->GetPrefix();
            string suffix = prefix.substr(0, prefix.size() - prefix_tail.size()) + suffix_tail;

            NON_CONST_ITERATE(CUser_object::TData, field, user.SetData()) {

                EPrefixOrSuffixType prefixOrSuffix = GetPrefixOrSuffixType(**field);
                if (prefixOrSuffix == eType_prefix) {

                    (*field)->SetData().SetStr(prefix);
                    n++;
                }
                else if (prefixOrSuffix == eType_suffix) {
                    (*field)->SetData().SetStr(suffix);
                    ++n;
                }
            }
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("SWITCH_STRUCTURED_COMMENT_PREFIX: Prefix is changed in [n] structured comment[s]", n) : 0);
}


// MISMATCHED_COMMENTS

static const string kComments = "Comments";
static const string kMismatchedComments = "Mismatched comments were found";


DISCREPANCY_CASE(MISMATCHED_COMMENTS, CSeqdesc, eDisc, "Mismatched Comments")
{
    if (obj.IsComment() && !obj.GetComment().empty()) {
        m_Objs[kComments].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel(), eKeepRef));
    }
}


DISCREPANCY_SUMMARIZE(MISMATCHED_COMMENTS)
{
    TReportObjectList report;
    const string* cur_comment = nullptr;
    bool need_report = false;
    NON_CONST_ITERATE(TReportObjectList, obj, m_Objs[kComments].GetObjects()) {
        const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(dynamic_cast<CDiscrepancyObject*>((*obj).GetNCPointer())->GetObject().GetPointer());
        if (desc) {
            if (cur_comment == nullptr) {
                cur_comment = &desc->GetComment();
            } else if (desc->GetComment() != *cur_comment) {
                need_report = true;
                break;
            }
        }
    }
    NON_CONST_ITERATE(TReportObjectList, obj, m_Objs[kComments].GetObjects()) {
        const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(dynamic_cast<CDiscrepancyObject*>((*obj).GetNCPointer())->GetObject().GetPointer());
        if (desc) {
            string subitem = "[n] comment[s] contain[S] " + desc->GetComment();
            m_Objs[kMismatchedComments][subitem].Ext().Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(desc), context.GetCurrentBioseqLabel(), eKeepRef, true));
        }
    }
    m_Objs.GetMap().erase(kComments);
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

DISCREPANCY_AUTOFIX(MISMATCHED_COMMENTS)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;

    string comment;

    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (desc) {

            if (comment.empty()) {
                comment = desc->GetComment();
            }
            else {
                CSeqdesc* desc_handle = const_cast<CSeqdesc*>(desc); // Is there a way to do this without const_cast???
                desc_handle->SetComment(comment);

                n++;
            }
        }
    }

    return CRef<CAutofixReport>(n ? new CAutofixReport("MISMATCHED_COMMENTS: Replaced [n] coment[s] with " + comment, n) : 0);
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
