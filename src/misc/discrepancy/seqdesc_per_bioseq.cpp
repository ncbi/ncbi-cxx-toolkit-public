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

DISCREPANCY_CASE(RETROVIRIDAE_DNA, SEQUENCE, eOncaller, "Retroviridae DNA")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && bioseq.IsNa() && bioseq.IsSetInst() && bioseq.GetInst().IsSetMol() && bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
        const CBioSource& src = biosrc->GetSource();
        if (src.IsSetLineage() && context.HasLineage(src, src.GetLineage(), "Retroviridae")) {
            if (!src.IsSetGenome() || src.GetGenome() != CBioSource::eGenome_proviral) {
                m_Objs["[n] Retroviridae biosource[s] on DNA sequences [is] not proviral"].Add(*context.SeqdescObjRef(*biosrc, biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(RETROVIRIDAE_DNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(RETROVIRIDAE_DNA)
{
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
    CSeqdesc* desc_handle = const_cast<CSeqdesc*>(desc);
    desc_handle->SetSource().SetGenome(CBioSource::eGenome_proviral);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("RETROVIRIDAE_DNA: Genome set to proviral for [n] sequence[s]", 1));
}


// NON_RETROVIRIDAE_PROVIRAL

DISCREPANCY_CASE(NON_RETROVIRIDAE_PROVIRAL, SEQUENCE, eOncaller, "Non-Retroviridae biosources that are proviral")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && bioseq.IsNa() && bioseq.IsSetInst() && bioseq.GetInst().IsSetMol() && bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
        const CBioSource& src = biosrc->GetSource();
        if (src.IsSetLineage() && !context.HasLineage(src, src.GetLineage(), "Retroviridae")) {
            if (src.IsSetGenome() && src.GetGenome() == CBioSource::eGenome_proviral) {
                m_Objs["[n] non-Retroviridae biosource[s] [is] proviral"].Add(*context.SeqdescObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(NON_RETROVIRIDAE_PROVIRAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BAD_MRNA_QUAL

DISCREPANCY_CASE(BAD_MRNA_QUAL, SEQUENCE, eOncaller, "mRNA sequence contains rearranged or germline")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    const CSeqdesc* molinfo = context.GetMolinfo();
    if (biosrc && biosrc->GetSource().IsSetSubtype() && molinfo && molinfo->GetMolinfo().IsSetBiomol() && molinfo->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
        for (auto& sub : biosrc->GetSource().GetSubtype()) {
            if (sub->IsSetSubtype() && (sub->GetSubtype() == CSubSource::eSubtype_germline || sub->GetSubtype() == CSubSource::eSubtype_rearranged)) {
                m_Objs["[n] mRNA sequence[s] [has] germline or rearranged qualifier"].Add(*context.SeqdescObjRef(*biosrc));
                return;
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BAD_MRNA_QUAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ORGANELLE_NOT_GENOMIC

DISCREPANCY_CASE(ORGANELLE_NOT_GENOMIC, SEQUENCE, eDisc | eOncaller, "Organelle location should have genomic moltype")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    const CSeqdesc* biosrc = context.GetBiosource();
    const CSeqdesc* molinfo = context.GetMolinfo();
    if (biosrc && molinfo && !bioseq.IsAa()) {
        if ((!molinfo->GetMolinfo().IsSetBiomol() || molinfo->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_genomic) && bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
            return;
        }
        if (context.IsOrganelle(&biosrc->GetSource())) {
            m_Objs["[n] non-genomic sequence[s] [is] organelle[s]"].Add(*context.SeqdescObjRef(*biosrc));
        }
    }
}


DISCREPANCY_SUMMARIZE(ORGANELLE_NOT_GENOMIC)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SWITCH_STRUCTURED_COMMENT_PREFIX

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
        for (const auto& field : user.GetData()) {
            EPrefixOrSuffixType prefixOrSuffix = GetPrefixOrSuffixType(*field);
            if (prefixOrSuffix == eType_none) {
                CConstRef<CField_rule> field_rule = rule.FindFieldRuleRef(field->GetLabel().GetStr());
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
        for (const auto& rule : rules.Get()) {
            CComment_rule::TErrorList errors = rule->IsValid(user);
            if (errors.empty() && IsAppropriateRule(*rule, user)) {
                if (ret == nullptr) {
                    ret = rule;
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


DISCREPANCY_CASE(SWITCH_STRUCTURED_COMMENT_PREFIX, DESC, eOncaller, "Suspicious structured comment prefix")
{
    for (auto& desc : context.GetSeqdesc()) {
        if (desc.IsUser() && desc.GetUser().GetObjectType() == CUser_object::eObjectType_StructuredComment) {
            const CUser_object& user = desc.GetUser();
            string prefix = CComment_rule::GetStructuredCommentPrefix(user);
            if (!prefix.empty()) {
                CConstRef<CComment_set> comment_rules = CComment_set::GetCommentRules();
                if (comment_rules) {
                    CConstRef<CComment_rule> ruler = comment_rules->FindCommentRuleEx(prefix);
                    if (ruler) {
                        const CComment_rule& rule = *ruler;
                        CComment_rule::TErrorList errors = rule.IsValid(user);
                        if (!errors.empty()) {
                            const CComment_rule* new_rule = FindAppropriateRule(*comment_rules, user);
                            if (new_rule) {
                                m_Objs["[n] structured comment[s] [is] invalid but would be valid with a different prefix"].Add(*context.SeqdescObjRef(desc, &desc, const_cast<CComment_rule*>(new_rule)));
                            }
                        }
                    }
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
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
    const CComment_rule* new_rule = dynamic_cast<const CComment_rule*>(obj->GetMoreInfo().GetPointer());
    CSeqdesc* desc_handle = const_cast<CSeqdesc*>(desc);
    CUser_object& user = desc_handle->SetUser();
    static const string prefix_tail = "START##";
    static const string suffix_tail = "END##";
    string prefix = new_rule->GetPrefix();
    string suffix = prefix.substr(0, prefix.size() - prefix_tail.size()) + suffix_tail;
    for (auto& field : user.SetData()) {
        EPrefixOrSuffixType prefixOrSuffix = GetPrefixOrSuffixType(*field);
        if (prefixOrSuffix == eType_prefix) {
            field->SetData().SetStr(prefix);
        }
        else if (prefixOrSuffix == eType_suffix) {
            field->SetData().SetStr(suffix);
        }
    }
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("SWITCH_STRUCTURED_COMMENT_PREFIX: Prefix is changed in [n] structured comment[s]", 1));
}


// MISMATCHED_COMMENTS

DISCREPANCY_CASE(MISMATCHED_COMMENTS, SEQUENCE, eDisc, "Mismatched Comments")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.IsNa()) {
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsComment() && !desc.GetComment().empty()) {
                const string& comment = desc.GetComment();
                if (comment.find("Annotation was added by") == string::npos) {
                    m_Objs[comment].Add(*context.SeqdescObjRef(desc, &desc));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MISMATCHED_COMMENTS)
{
    CReportNode rep;
    auto& all = m_Objs.GetMap();
    if (all.size() > 1) {
        CRef<CSimpleTypeObject<string>> data;
        for (auto& it : all) {
            string subitem = "[n] comment[s] contain[S] " + it.first;
            for (auto obj : it.second->GetObjects()) {
                if (!data) {
                    data.Reset(new CSimpleTypeObject<string>(it.first));
                }
                else {
                    obj->SetMoreInfo(static_cast<CObject*>(&*data));
                }
                rep["Mismatched comments were found"][subitem].Ext().Add(*obj);
            }
        }
        m_ReportItems = rep.Export(*this)->GetSubitems();
    }
}


DISCREPANCY_AUTOFIX(MISMATCHED_COMMENTS)
{
    auto data = dynamic_cast<const CSimpleTypeObject<string>*>(obj->GetMoreInfo().GetPointer());
    if (data) {
        const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
        CSeqdesc* desc_handle = const_cast<CSeqdesc*>(desc);
        desc_handle->SetComment(data->Value);
        obj->SetFixed();
        return CRef<CAutofixReport>(new CAutofixReport("MISMATCHED_COMMENTS: Replaced [n] coment[s] with " + data->Value, 1));
    }
    return CRef<CAutofixReport>();
}


// GENOMIC_MRNA

DISCREPANCY_CASE(GENOMIC_MRNA, DESC, eOncaller | eSmart, "Genomic mRNA is legal, but not expected")
{
    for (auto& desc : context.GetSeqdesc()) {
        if (desc.IsMolinfo() && desc.GetMolinfo().IsSetBiomol() && desc.GetMolinfo().GetBiomol() == CMolInfo::eBiomol_genomic_mRNA) {
            m_Objs["[n] biololecule[s] [is] genomic mRNA"].Add(*context.SeqdescObjRef(desc));
        }
    }
}


DISCREPANCY_SUMMARIZE(GENOMIC_MRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// AUTODEF_USER_OBJECT

DISCREPANCY_CASE(AUTODEF_USER_OBJECT, SEQUENCE, eOncaller, "Nucleotide sequence must have an autodef user object")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    size_t count = 0;
    if (bioseq.IsNa()) {
        for (auto& desc : context.GetAllSeqdesc()) {
            if (desc.IsUser() && desc.GetUser().GetObjectType() == CUser_object::eObjectType_AutodefOptions) {
                count++;
            }
        }
        if (!count) {
            m_Objs["[n] nucleotide sequence[s] [has] no autodef user objects"].Add(*context.BioseqObjRef());
        }
        else if (count > 1) {
            m_Objs["[n] nucleotide sequence[s] [has] multiple autodef user objects"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(AUTODEF_USER_OBJECT)
{
    auto& M = m_Objs.GetMap();
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
