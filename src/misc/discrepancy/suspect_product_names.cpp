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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/macro/Suspect_rule.hpp>

#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/macro/Search_func.hpp>
#include <objects/macro/Replace_func.hpp>
#include <objects/macro/Simple_replace.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/misc/sequence_util_macros.hpp>

#include <serial/objistrasn.hpp>

#include "discrepancy_core.hpp"
#include "utils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);


string GetTwoFieldSubfield(const string& str, unsigned subfield)
{
    string strtmp;
    if (str.empty() || subfield > 2)  return "";
    if (!subfield) return str;
    else {
        size_t pos = str.find(':');
        if (pos == string::npos) {
            if (subfield == 1) return str;
            else return kEmptyStr;
        }
        else {
            if (subfield == 1) return str.substr(0, pos);
            else {
                strtmp = CTempString(str).substr(pos + 1).empty();
                if (!strtmp.empty()) return strtmp;
                else return "";
            }
        }
    }
}


static string GetFirstGBQualMatch (const vector <CRef <CGb_qual> >& quals, const string& qual_name, unsigned subfield = 0, const CString_constraint* str_cons = nullptr)
{
    string str;
    for (auto it : quals) {
        if (NStr::EqualNocase(it->GetQual(), qual_name)) {
            str = it->GetVal();
            str = GetTwoFieldSubfield(str, subfield);
            if ( str.empty() || (str_cons && !str_cons->Empty() && !(str_cons->Match(str))) ) {
                str.clear();
            }
            else break;
        }
    }
    return str;
}


static string GetRNAProductString(const CSeq_feat& seq_feat)
{
    const CRNA_ref& rna = seq_feat.GetData().GetRna();
    string rna_str;
    if (!rna.CanGetExt()) {
        rna_str = seq_feat.GetNamedQual("product");
    }
    else {
        const CRNA_ref::C_Ext& ext = rna.GetExt();
        switch (ext.Which()) {
            case CRNA_ref::C_Ext::e_Name:
                    rna_str = ext.GetName();
                    if (seq_feat.CanGetQual() && (rna_str.empty() || rna_str== "ncRNA" || rna_str== "tmRNA" || rna_str== "misc_RNA")) {
                        rna_str = GetFirstGBQualMatch(seq_feat.GetQual(), (string)"product");
                    }
                    break;
            case CRNA_ref::C_Ext::e_TRNA:
                    GetLabel(seq_feat, &rna_str, feature::fFGL_Content);
                    rna_str = "tRNA-" + rna_str;
                    break;
            case CRNA_ref::C_Ext::e_Gen:
                    if (ext.GetGen().CanGetProduct()) {
                        rna_str = ext.GetGen().GetProduct();
                    }
            default: break;
        }
    }
    return rna_str;
}


static string GetRuleText(const CSuspect_rule& rule)
{
    static const char* rule_type[] = {
        "None",
        "Typo",
        "Putative Typo",
        "Quick fix",
        "Organelles not appropriate in prokaryote",
        "Suspicious phrase; should this be nonfunctional?",
        "May contain database identifier more appropriate in note; remove from product name",
        "Remove organism from product name",
        "Possible parsing error or incorrect formatting; remove inappropriate symbols",
        "Implies evolutionary relationship; change to -like protein",
        "Consider adding 'protein' to the end of the product name",
        "Correct the name or use 'hypothetical protein'",
        "Use American spelling",
        "Use short product name instead of descriptive phrase",
        "use protein instead of gene as appropriate"
    };
    return rule_type[rule.GetRule_type()];
}


static string GetRuleMatch(const CSuspect_rule& rule)
{
    if (rule.IsSetDescription()) {
        string desc = rule.GetDescription();
        NStr::ReplaceInPlace(desc, "contains", "contain[s]");
        return "[n] feature[s] " + desc;
    }
    if (rule.CanGetFind()) {
        const CSearch_func& find = rule.GetFind();
        switch (find.Which()) {
            case CSearch_func::e_String_constraint:
            {   string s = "[n] feature[s] ";
                switch (find.GetString_constraint().GetMatch_location()) {
                    case eString_location_starts:
                        s += "start[S] with";
                        break;
                    case eString_location_ends:
                        s += "end[S] with";
                        break;
                    case eString_location_equals:
                        s += "equal[S]";
                        break;
                    default:
                        s += "contain[S]";
                }
                return s+ " [*(*]\'" + find.GetString_constraint().GetMatch_text()
                    + (rule.CanGetRule_type() && (rule.GetRule_type() == eFix_type_typo || rule.GetRule_type() == eFix_type_quickfix) &&
                        rule.CanGetReplace() && rule.GetReplace().GetReplace_func().IsSimple_replace() && rule.GetReplace().GetReplace_func().GetSimple_replace().CanGetReplace()
                        ? "\'[*)*], Replace with [*(*]\'" + rule.GetReplace().GetReplace_func().GetSimple_replace().GetReplace() : "")
                    + "\'[*)*]";
            }
            case CSearch_func::e_Contains_plural:
                return "[n] feature[s] May contain plural";
            case CSearch_func::e_N_or_more_brackets_or_parentheses:
                return "[n] feature[s] violate[S] e_N_or_more_brackets_or_parentheses !!!";
            case CSearch_func::e_Three_numbers:
                //return "[n] feature[s] contain[S] three or more numbers together, but not contain[S] \'methyltransferas\'";
                return "[n] feature[s] Three or more numbers together but not contain[S] \'methyltransferas\'"; // from C Toolkit
            case CSearch_func::e_Underscore:
                return "[n] feature[s] contain[S] underscore";
            case CSearch_func::e_Prefix_and_numbers:
                return "[n] feature[s] violate[S] e_Prefix_and_numbers !!!";
            case CSearch_func::e_All_caps:
                return "[n] feature[s] [is] all capital letters";
            case CSearch_func::e_Unbalanced_paren:
                return "[n] feature[s] contain[S] unbalanced brackets or parentheses";
            case CSearch_func::e_Too_long:
                return "[n] feature[s] violate[S] e_Too_long !!!";
            case CSearch_func::e_Has_term:
                return "[n] feature[s] violate[S] e_Has_term !!!";
            default:
                break;
        }
    }
    return "[n] feature[s] violate[S] some other mysterious rule!";
}

///////////////////////////////////// SUSPECT_PRODUCT_NAMES

static const string kSuspectProductNames = "[n] product_name[s] contain[S] suspect phrase[s] or character[s]";

static bool ContainsLetters(const string& prod_name)
{
    for (auto& symbol : prod_name) {
        if (isalpha(symbol)) {
            return true;
        }
    }
    return false;
}


DISCREPANCY_CASE(SUSPECT_PRODUCT_NAMES, FEAT, eDisc | eOncaller | eSubmitter | eSmart | eTSA | eFatal, "Suspect Product Name")
{
    for (auto& feat : context.GetFeat()) {
        if (! feat.IsSetData()) {
            continue;
        }
        if (context.IsPseudo(feat)) {
            continue;
        }
        const CSeqFeatData &data = feat.GetData();
        CSeqFeatData::ESubtype subtype = data.GetSubtype();
        if (subtype == CSeqFeatData::eSubtype_prot && data.GetProt().IsSetName() && !data.GetProt().GetName().empty()) {
            CConstRef<CSuspect_rule_set> rules = context.GetProductRules();
            string prot_name = *data.GetProt().GetName().begin();
            vector<char> Hits(rules->Get().size());
            std::fill(Hits.begin(), Hits.end(), 0);
            rules->Screen(prot_name, Hits.data());
            if (!ContainsLetters(prot_name)) {
                const CSeq_feat* cds = sequence::GetCDSForProduct(context.CurrentBioseq(), &(context.GetScope()));    // consider different implementation
                CReportNode& node = m_Objs[kSuspectProductNames]["[*-1*]Product name does not contain letters"].Summ()["[n] feature[s] [does] not contain letters in product name"].Summ().Fatal();
                node.Add(*context.SeqFeatObjRef(cds ? *cds : feat)).Fatal();
            }
            else {
                size_t rule_num = 0;
                for (auto rule : rules->Get()) {
                    if (Hits[rule_num] && rule->StringMatchesSuspectProductRule(prot_name)) {
                        string leading_space = "[*" + NStr::NumericToString(rule_num) + "*]";
                        size_t rule_type = rule->GetRule_type();
                        string rule_name = "[*";
                        if (rule_type < 10) {
                            rule_name += " ";
                        }
                        rule_name += NStr::NumericToString(rule_type) + "*]" + GetRuleText(*rule);
                        string rule_text = leading_space + GetRuleMatch(*rule);
                        CReportNode& node = m_Objs[kSuspectProductNames][rule_name].Summ()[rule_text].Summ();
                        const CSeq_feat* cds = sequence::GetCDSForProduct(context.CurrentBioseq(), &(context.GetScope())); // needs to optimize
                        if (rule->CanGetReplace()) {
                            node.Add(*context.SeqFeatObjRef(cds ? *cds : feat, CDiscrepancyContext::eFixSet, (CObject*)&*rule)).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
                        }
                        else {
                            node.Add(*context.SeqFeatObjRef(cds ? *cds : feat)).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
                        }
                    }
                    rule_num++;
                }
            }
        } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
            if (feat.IsSetXref()) {
                ITERATE (CSeq_feat::TXref, it, feat.GetXref()) {
                    const CSeqFeatXref& xref = **it;
                    if (xref.IsSetData() && xref.GetData().IsProt() ) {
                        const CProt_ref& pref = xref.GetData().GetProt();
                        ITERATE (CProt_ref::TName, it, pref.GetName()) {
                            if ( !it->empty() ) {
                                string prot_name = *it;
                                CConstRef<CSuspect_rule_set> rules = context.GetProductRules();
                                vector<char> Hits(rules->Get().size());
                                std::fill(Hits.begin(), Hits.end(), 0);
                                rules->Screen(prot_name, Hits.data());
                                if (!ContainsLetters(prot_name)) {
                                    CReportNode& node = m_Objs[kSuspectProductNames]["[*-1*]Product name does not contain letters"].Summ()["[n] feature[s] [does] not contain letters in product name"].Summ().Fatal();
                                    node.Add(*context.SeqFeatObjRef(feat)).Fatal();
                                }
                                else {
                                    size_t rule_num = 0;
                                    for (auto rule : rules->Get()) {
                                        if (Hits[rule_num] && rule->StringMatchesSuspectProductRule(prot_name)) {
                                            string leading_space = "[*" + NStr::NumericToString(rule_num) + "*]";
                                            size_t rule_type = rule->GetRule_type();
                                            string rule_name = "[*";
                                            if (rule_type < 10) {
                                                rule_name += " ";
                                            }
                                            rule_name += NStr::NumericToString(rule_type) + "*]" + GetRuleText(*rule);
                                            string rule_text = leading_space + GetRuleMatch(*rule);
                                            CReportNode& node = m_Objs[kSuspectProductNames][rule_name].Summ()[rule_text].Summ();
                                            if (rule->CanGetReplace()) {
                                                node.Add(*context.SeqFeatObjRef(feat, CDiscrepancyContext::eFixSet, (CObject*)&*rule)).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
                                            }
                                            else {
                                                node.Add(*context.SeqFeatObjRef(feat)).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
                                            }
                                        }
                                        rule_num++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (subtype == CSeqFeatData::eSubtype_mRNA) {
            const CRNA_ref& rna = data.GetRna();
            if (rna.IsSetExt()) {
                const CRNA_ref::C_Ext& ext = rna.GetExt();
                if (ext.Which() == CRNA_ref::C_Ext::e_Name) {
                    string mrna_name = ext.GetName();
                    CConstRef<CSuspect_rule_set> rules = context.GetProductRules();
                    vector<char> Hits(rules->Get().size());
                    std::fill(Hits.begin(), Hits.end(), 0);
                    rules->Screen(mrna_name, Hits.data());
                    if (!ContainsLetters(mrna_name)) {
                        CReportNode& node = m_Objs[kSuspectProductNames]["[*-1*]Product name does not contain letters"].Summ()["[n] feature[s] [does] not contain letters in product name"].Summ().Fatal();
                        node.Add(*context.SeqFeatObjRef(feat)).Fatal();
                    }
                    else {
                        size_t rule_num = 0;
                        for (auto rule : rules->Get()) {
                            if (Hits[rule_num] && rule->StringMatchesSuspectProductRule(mrna_name)) {
                                string leading_space = "[*" + NStr::NumericToString(rule_num) + "*]";
                                size_t rule_type = rule->GetRule_type();
                                string rule_name = "[*";
                                if (rule_type < 10) {
                                    rule_name += " ";
                                }
                                rule_name += NStr::NumericToString(rule_type) + "*]" + GetRuleText(*rule);
                                string rule_text = leading_space + GetRuleMatch(*rule);
                                CReportNode& node = m_Objs[kSuspectProductNames][rule_name].Summ()[rule_text].Summ();
                                if (rule->CanGetReplace()) {
                                    node.Add(*context.SeqFeatObjRef(feat, CDiscrepancyContext::eFixSet, (CObject*)&*rule)).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
                                }
                                else {
                                    node.Add(*context.SeqFeatObjRef(feat)).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
                                }
                            }
                            rule_num++;
                        }
                    }
                }
            }
        }
    }
}


static string ReplaceNoCase(const string& input, const string& search, const string& replace)
{
    string find = search;
    NStr::TruncateSpacesInPlace(find);
    if (!find.length()) {
        return input;
    }
    size_t p;
    if ((p = NStr::FindNoCase(input, find)) != NPOS) {
        string tail = input.substr(p + find.length());
        return input.substr(0, p) + replace + ReplaceNoCase(tail, find, replace);
    }
    return input;
}


static void GetProtAndRnaForCDS(const CSeq_feat& cds, CScope& scope, CSeq_feat*& prot, CSeq_feat*& mrna)
{
    prot = 0;
    mrna = 0;
    CBioseq_Handle bsh = scope.GetBioseqHandle(cds.GetProduct());
    if (!bsh) {
        return;
    }
    CFeat_CI pr(bsh, CSeqFeatData::eSubtype_prot);
    if (pr) {
        prot = (CSeq_feat*)&pr->GetMappedFeature();
        string name = *prot->GetData().GetProt().GetName().begin();
        const CSeq_feat* rna = sequence::GetBestMrnaForCds(cds, scope);
        if (rna && rna->GetData().GetRna().CanGetExt() && rna->GetData().GetRna().GetExt().GetName() == name) {
            mrna = (CSeq_feat*)rna;
        }
    }
}


typedef std::function < CRef<CSeq_feat>() > GetFeatureFunc;
string FixProductName(const CSuspect_rule* rule, CScope&, string& prot_name, GetFeatureFunc get_mrna, GetFeatureFunc get_cds)
{
    string newtext;
    string orig_prot_name;

    const CReplace_rule& rr = rule->GetReplace();
    const CReplace_func& rf = rr.GetReplace_func();
    if (rf.IsSimple_replace()) {
        const CSimple_replace& repl = rf.GetSimple_replace();
        if (repl.GetWhole_string()) {
            newtext = repl.GetReplace();
        }
        else {
            const string& find = rule->GetFind().GetString_constraint().GetMatch_text();
            const string& subst = repl.GetReplace();
            newtext = ReplaceNoCase(prot_name, find, subst);
        }
    }
    else if (rf.IsHaem_replace()) {
        newtext = ReplaceNoCase(prot_name, "haem", "hem");
    }
    if (!newtext.empty() && newtext != prot_name) {
        orig_prot_name = std::move(prot_name);
        prot_name = std::move(newtext);
        auto mrna = get_mrna();
        if (mrna) {
            mrna->SetData().SetRna().SetExt().SetName() = prot_name;
        }
        if (rr.GetMove_to_note()) {
            auto cds = get_cds();
            if (cds)
                AddComment(*cds, orig_prot_name);
        }
    }
    return orig_prot_name;
}


DISCREPANCY_AUTOFIX(SUSPECT_PRODUCT_NAMES)
{
    CRef<CAutofixReport> ret;
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    const CSuspect_rule* rule = dynamic_cast<const CSuspect_rule*>(obj->GetMoreInfo().GetPointer());

    const CSeqFeatData &data = sf->GetData();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_cdregion) {
       if (sf->IsSetXref()) {
            ITERATE (CSeq_feat::TXref, it, sf->GetXref()) {
                const CSeqFeatXref& xref = **it;
                if (xref.IsSetData() && xref.GetData().IsProt() ) {
                    string prot_name = xref.GetData().GetProt().GetName().front();
                    if (!rule->StringMatchesSuspectProductRule(prot_name)) {
                        return ret;
                    }

                    string newtext;
                    string old_prot_name;

                    const CReplace_rule& rr = rule->GetReplace();
                    const CReplace_func& rf = rr.GetReplace_func();
                    if (rf.IsSimple_replace()) {
                        const CSimple_replace& repl = rf.GetSimple_replace();
                        if (repl.GetWhole_string()) {
                            newtext = repl.GetReplace();
                        }
                        else {
                            const string& find = rule->GetFind().GetString_constraint().GetMatch_text();
                            const string& subst = repl.GetReplace();
                            newtext = ReplaceNoCase(prot_name, find, subst);
                        }
                    }
                    else if (rf.IsHaem_replace()) {
                        newtext = ReplaceNoCase(prot_name, "haem", "hem");
                    }
                    if (!newtext.empty() && newtext != prot_name) {
                        old_prot_name = std::move(prot_name);
                        prot_name = std::move(newtext);
                        const_cast<string&>(xref.GetData().GetProt().GetName().front()) = prot_name;
                    }
                    if (prot_name != old_prot_name && !prot_name.empty()) {
                        string s = "Changed \'" + old_prot_name + "\' to \'" + prot_name + "\' at " + obj->GetLocation();
                        obj->SetFixed();
                        ret.Reset(new CAutofixReport("SUSPECT_PRODUCT_NAMES", 0));
                        CRef<CAutofixReport> report(new CAutofixReport(s, 1));
                        vector<CRef<CAutofixReport>> reports;
                        reports.push_back(report);
                        ret->AddSubitems(reports);
                    }
                }
            }
        }
        return ret;
    } else if (subtype == CSeqFeatData::eSubtype_mRNA) {
        const CRNA_ref& rna = data.GetRna();
        if (rna.IsSetExt()) {
            const CRNA_ref::C_Ext& ext = rna.GetExt();
            if (ext.Which() == CRNA_ref::C_Ext::e_Name) {
                string mrna_name = ext.GetName();
                if (!rule->StringMatchesSuspectProductRule(mrna_name)) {
                    return ret;
                }

                string newtext;
                string old_mrna_name;

                const CReplace_rule& rr = rule->GetReplace();
                const CReplace_func& rf = rr.GetReplace_func();
                if (rf.IsSimple_replace()) {
                    const CSimple_replace& repl = rf.GetSimple_replace();
                    if (repl.GetWhole_string()) {
                        newtext = repl.GetReplace();
                    }
                    else {
                        const string& find = rule->GetFind().GetString_constraint().GetMatch_text();
                        const string& subst = repl.GetReplace();
                        newtext = ReplaceNoCase(mrna_name, find, subst);
                    }
                }
                else if (rf.IsHaem_replace()) {
                    newtext = ReplaceNoCase(mrna_name, "haem", "hem");
                }
                if (!newtext.empty() && newtext != mrna_name) {
                    old_mrna_name = std::move(mrna_name);
                    mrna_name = std::move(newtext);
                    CRef<CSeq_feat> mrna = CRef<CSeq_feat>((CSeq_feat*)sf);
                    if (mrna) {
                        mrna->SetData().SetRna().SetExt().SetName() = mrna_name;
                    }
                }
                if (mrna_name != old_mrna_name && !mrna_name.empty()) {
                    string s = "Changed \'" + old_mrna_name + "\' to \'" + mrna_name + "\' at " + obj->GetLocation();
                    obj->SetFixed();
                    ret.Reset(new CAutofixReport("SUSPECT_PRODUCT_NAMES", 0));
                    CRef<CAutofixReport> report(new CAutofixReport(s, 1));
                    vector<CRef<CAutofixReport>> reports;
                    reports.push_back(report);
                    ret->AddSubitems(reports);
                }
            }
        }
        return ret;
    }

    CSeq_feat* prot;
    CSeq_feat* mrna;
    GetProtAndRnaForCDS(*sf, context.GetScope(), prot, mrna);
    if (prot) {
        string& prot_name = prot->SetData().SetProt().SetName().front();
        if (!rule->StringMatchesSuspectProductRule(prot_name)) {
            return ret;
        }
        string old_prot_name = FixProductName(rule, context.GetScope(),
            prot_name,
            [&mrna] { return CRef<CSeq_feat>(mrna); },
            [&sf] { return CRef<CSeq_feat>((CSeq_feat*)sf); });
        if (prot_name != old_prot_name && !prot_name.empty()) {
            string s = "Changed \'" + old_prot_name + "\' to \'" + prot_name + "\' at " + obj->GetLocation();
            obj->SetFixed();
            ret.Reset(new CAutofixReport("SUSPECT_PRODUCT_NAMES", 0));
            CRef<CAutofixReport> report(new CAutofixReport(s, 1));
            vector<CRef<CAutofixReport>> reports;
            reports.push_back(report);
            ret->AddSubitems(reports);
        }
    }
    return ret;
}

///////////////////////////////////// ORGANELLE_PRODUCTS


DISCREPANCY_CASE(ORGANELLE_PRODUCTS, FEAT, eOncaller, "Organelle products on non-organelle sequence: on when neither bacteria nor virus")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc) {
        const CBioSource& src = biosrc->GetSource();
        CBioSource::TGenome genome = src.GetGenome();
        if (genome == CBioSource::eGenome_mitochondrion || genome == CBioSource::eGenome_chloroplast || genome == CBioSource::eGenome_plastid || context.IsViral(&src) || context.IsBacterial(&src)) {
            return;
        }
        if (src.IsSetOrg() && src.GetOrg().IsSetTaxname() && CDiscrepancyContext::IsUnculturedNonOrganelleName(src.GetOrg().GetTaxname())) {
            return;
        }
    }
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_prot && feat.GetData().GetProt().IsSetName() && !feat.GetData().GetProt().GetName().empty() && !context.IsPseudo(feat)) {
            string prot_name = *feat.GetData().GetProt().GetName().begin();
            CConstRef<CSuspect_rule_set> rules = context.GetOrganelleProductRules();
            vector<char> Hits(rules->Get().size());
            std::fill(Hits.begin(), Hits.end(), 0);
            rules->Screen(prot_name, Hits.data());
            size_t rule_num = 0;
            for (auto rule : rules->Get()) {
                if (Hits[rule_num] && rule->StringMatchesSuspectProductRule(prot_name)) {
                    if (rule->CanGetReplace()) {
                        m_Objs["[n] suspect product[s] not organelle"].Add(*context.SeqFeatObjRef(feat, CDiscrepancyContext::eFixSet, (CObject*)&*rule)).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
                    }
                    else {
                        m_Objs["[n] suspect product[s] not organelle"].Add(*context.SeqFeatObjRef(feat)).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
                    }
                }
                rule_num++;
            }
        }
    }
}


DISCREPANCY_AUTOFIX(ORGANELLE_PRODUCTS) // There are currently no autofixable rules for ORGANELLE_PRODUCTS
{
    CRef<CAutofixReport> ret;
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    const CSuspect_rule* rule = dynamic_cast<const CSuspect_rule*>(obj->GetMoreInfo().GetPointer());
    CSeq_feat* prot;
    CSeq_feat* mrna;
    GetProtAndRnaForCDS(*sf, context.GetScope(), prot, mrna);
    if (prot) {
        string& prot_name = prot->SetData().SetProt().SetName().front();
        if (!rule->StringMatchesSuspectProductRule(prot_name)) {
            return ret;
        }
        string old_prot_name = FixProductName(rule, context.GetScope(),
            prot_name,
            [&mrna] { return CRef<CSeq_feat>(mrna); },
            [&sf] { return CRef<CSeq_feat>((CSeq_feat*)sf); });
        if (prot_name != old_prot_name && !prot_name.empty()) {
            string s = "Changed \'" + old_prot_name + "\' to \'" + prot_name + "\' at " + obj->GetLocation();
            obj->SetFixed();
            ret.Reset(new CAutofixReport("ORGANELLE_PRODUCTS", 0));
            CRef<CAutofixReport> report(new CAutofixReport(s, 1));
            vector<CRef<CAutofixReport>> reports;
            reports.push_back(report);
            ret->AddSubitems(reports);
        }
    }
    return ret;
} 


static CConstRef<CSuspect_rule_set> s_GetrRNAProductsSuspectRuleSet()
{
    DEFINE_STATIC_FAST_MUTEX(sx_RuleMutex);
    CFastMutexGuard guard(sx_RuleMutex);

    static CAutoInitRef<CSuspect_rule_set> rrna_products_suspect_rule_set;
    if( rrna_products_suspect_rule_set.IsInitialized() ) {
        // already built
        return ConstRef(&*rrna_products_suspect_rule_set);
    }

    CTempString rrna_products_suspect_rule_set_asn_text =
        "Suspect-rule-set ::= {\n"
        "        { find string-constraint { match-text \"domain\", whole-word FALSE } },\n"
        "        { find string-constraint { match-text \"partial\", whole-word FALSE } },\n"
        "        { find string-constraint { match-text \"5s_rRNA\", whole-word FALSE } },\n"
        "        { find string-constraint { match-text \"16s_rRNA\", whole-word FALSE } },\n"
        "        { find string-constraint { match-text \"23s_rRNA\", whole-word FALSE } },\n"
        "        {\n"
        "            find string-constraint { match-text \"8S\", whole-word TRUE },\n"
        "            except string-constraint { match-text \"5.8S\", whole-word TRUE } } }";

    CObjectIStreamAsn asn_istrm(rrna_products_suspect_rule_set_asn_text.data(), rrna_products_suspect_rule_set_asn_text.length());
    asn_istrm.Read(&*rrna_products_suspect_rule_set, rrna_products_suspect_rule_set->GetThisTypeInfo());

    return ConstRef(&*rrna_products_suspect_rule_set);
}

// gives a text description explaining what the string constraint matches.
// (e.g. "contains '5.8S' (whole word)")
//
// only bare minimum implementation and raises an exception if it the
// input goes beyond its capability
static void s_SummarizeStringConstraint(
    ostream & out_strm, const CString_constraint & string_constraint )
{
    if( string_constraint.IsSetMatch_location() ||
        string_constraint.IsSetCase_sensitive() ||
        string_constraint.IsSetIgnore_space() ||
        string_constraint.IsSetIgnore_punct() ||
        string_constraint.IsSetIgnore_words() ||
        string_constraint.IsSetNot_present() ||
        string_constraint.IsSetIs_all_caps() ||
        string_constraint.IsSetIs_all_lower() ||
        string_constraint.IsSetIs_all_punct() ||
        string_constraint.IsSetIgnore_weasel() ||
        string_constraint.IsSetIs_first_cap() ||
        string_constraint.IsSetIs_first_each_cap() )
    {
        NCBI_USER_THROW(
            "s_SummarizeStringConstraint input too complex.  "
            "Please expand the function or find/create a better one.");
    }

    out_strm << "contains '" << string_constraint.GetMatch_text() << "'";
    if( GET_FIELD_OR_DEFAULT(string_constraint, Whole_word, false) ) {
        out_strm << " (whole word)";
    }
}

// Gives a text description of what the given search_func matches.
//
// only bare minimum implementation and raises an exception if it the
// input goes beyond its capability
static void s_SummarizeSearchFunc(
    ostream & out_strm, const CSearch_func & search_func)
{
    if( ! search_func.IsString_constraint() ) {
        NCBI_USER_THROW(
            "s_SummarizeSearchFunc input too complex.  "
            "Please expand the function or find/create a better one.");
    }

    s_SummarizeStringConstraint(out_strm, search_func.GetString_constraint());
}

// Gives a text description of a suspect rule.
//
// examples:
//
// - "contains 'partial'"
// - "contains '8S' (whole word) but not contains '5.8S' (whole word)"
//
// only implements the barest subset of this and will surely need more
// complexity and to be moved later on.
//
// Raises an exception if the input is beyond its ability to handle.
//
static void s_SummarizeSuspectRule(
    ostream & out_strm, const CSuspect_rule & rule)
{
    if( rule.IsSetFeat_constraint() ||
        rule.IsSetRule_type() ||
        rule.IsSetReplace() ||
        rule.IsSetDescription() ||
        rule.IsSetFatal() )
    {
        NCBI_USER_THROW(
            "s_SummarizeSuspectRule input too complex.  "
            "Please expand the function or find/create a better one.");
    }

    _ASSERT(rule.IsSetFind());
    s_SummarizeSearchFunc(out_strm, rule.GetFind());
    if( rule.IsSetExcept() ) {
        out_strm << " but not ";
        s_SummarizeSearchFunc(out_strm, rule.GetExcept());
    }
}


DISCREPANCY_CASE(SUSPECT_RRNA_PRODUCTS, FEAT, eDisc | eSubmitter | eSmart, "rRNA product names should not contain 'partial' or 'domain'")
{
    static const string kMsg = "[n] rRNA product name[s] contain[S] suspect phrase";
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA) {
            const string product = GetRNAProductString(feat);
            CConstRef<CSuspect_rule_set> rules = s_GetrRNAProductsSuspectRuleSet();
            vector<char> Hits(rules->Get().size());
            std::fill(Hits.begin(), Hits.end(), 0);
            rules->Screen(product, Hits.data());
            size_t rule_num = 0;
            for (auto rule : rules->Get()) {
                if (Hits[rule_num] && rule->StringMatchesSuspectProductRule(product)) {
                    ostringstream detailed_msg;
                    detailed_msg << "[n] rRNA product name[s] ";
                    s_SummarizeSuspectRule(detailed_msg, *rule);
                    m_Objs[kMsg][detailed_msg.str()].Ext().Add(*context.SeqFeatObjRef(feat));
                }
                rule_num++;
            }
        }
    }
}


// _SUSPECT_PRODUCT_NAMES - used for asndisc -N option

DISCREPANCY_CASE(_SUSPECT_PRODUCT_NAMES, STRING, 0, "Suspect Product Names for asndisc -N option")
{
    CConstRef<CSuspect_rule_set> rules = context.GetProductRules();
    vector<char> Hits(rules->Get().size());
    std::fill(Hits.begin(), Hits.end(), 0);
    const string& str = context.CurrentText();
    rules->Screen(str, Hits.data());

    if (!ContainsLetters(str)) {
        CReportNode& node = m_Objs[kSuspectProductNames]["[*-1*]Product name does not contain letters"].Summ()["[n] feature[s] [does] not contain letters in product name"].Summ().Fatal();
        node.Add(*context.StringObjRef()).Fatal();
    }
    size_t rule_num = 0;
    for (auto rule : rules->Get()) {
        if (Hits[rule_num] && rule->StringMatchesSuspectProductRule(str)) {
            string leading_space = "[*" + NStr::NumericToString(rule_num) + "*]";
            size_t rule_type = rule->GetRule_type();
            string rule_name = "[*";
            if (rule_type < 10) {
                rule_name += " ";
            }
            rule_name += NStr::NumericToString(rule_type) + "*]" + GetRuleText(*rule);
            string rule_text = leading_space + GetRuleMatch(*rule);
            CReportNode& node = m_Objs[kSuspectProductNames][rule_name].Summ()[rule_text].Summ();
            node.Add(*context.StringObjRef()).Severity(rule->IsFatal() ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning);
        }
        rule_num++;
    }
}

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
