/*  $Id$
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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader and handler for suspect product rules files
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/macro/String_constraint_set.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/macro/Suspect_rule.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <serial/objistr.hpp>
#include <objmgr/util/feature.hpp>
#include <objects/seqfeat/Cdregion.hpp>

#include "suspect_feat.hpp"
#include "visitors.hpp"

#include <objmgr/annot_ci.hpp>
#include <misc/discrepancy/discrepancy.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE

CFixSuspectProductName::CFixSuspectProductName()
{
    bool found = false;
    std::string filename;
    auto rules_env = CNcbiApplication::Instance()->GetEnvironment().Get("PRODUCT_RULES_LIST", &found);
    if (found) {
        filename = rules_env;
    }
    m_rules = NDiscrepancy::GetProductRules(filename);
}

CFixSuspectProductName::~CFixSuspectProductName()
{
}

void CFixSuspectProductName::SetRulesFilename(const string& filename)
{
    m_rules = NDiscrepancy::GetProductRules(filename);
}

void CFixSuspectProductName::SetupOutput(std::function<std::ostream&()> f)
{
    m_output = f;
}

CConstRef<CSuspect_rule> CFixSuspectProductName::x_FixSuspectProductName(string& product_name) const
{
    if (m_rules.NotEmpty() && !m_rules->Get().empty())
    {
        CMatchString match(product_name);
        for (const auto& rule : m_rules->Get()) {
            if (rule->CanGetReplace() && rule->StringMatchesSuspectProductRule(product_name))
                return rule;
        }
    }

    return CConstRef<CSuspect_rule>();
}

void CFixSuspectProductName::ReportFixedProduct(const string& oldproduct, const string& newproduct, const CSeq_loc& loc, const string& locustag) const
{
    if (!m_output)
        return;

    std::ostream& report_ostream = m_output();

    string label;
    loc.GetLabel(&label);
    report_ostream << "Changed " << oldproduct << " to " << newproduct << " " << label << " " << locustag << "\n\n";
}

bool CFixSuspectProductName::FixSuspectProductNames(CSeq_feat& feature, CScope& scope, feature::CFeatTree& feattree) const
{
    static const char hypotetic_protein_name[] = "hypothetical protein"; // sema: interesting optimization...

    bool modified = false;
    if (feature.IsSetData() && feature.GetData().IsProt() && feature.GetData().GetProt().IsSetName())
    {
        if (feature.GetData().GetProt().IsSetProcessed() && feature.GetData().GetProt().GetProcessed() != CProt_ref::eProcessed_not_set)
            return false;


        //for (auto& name : feature.SetData().SetProt().SetName())
        auto& prot_name = feature.SetData().SetProt().SetName().front(); // only first name is fixed
        {
            if (0 != NStr::Compare(prot_name, hypotetic_protein_name)) {
                while (auto rule = x_FixSuspectProductName(prot_name)) {
                    auto mapped(scope.GetSeq_featHandle(feature));
                    auto old_prot_name = NDiscrepancy::FixProductName(
                        rule, scope, prot_name,
                        [&mapped, &feattree] {
                          auto mrna = feature::GetBestParentForFeat(mapped, CSeqFeatData::eSubtype_mRNA, &feattree);
                          CRef<CSeq_feat> orig_mrna;
                          if (mrna)
                              orig_mrna.Reset(&(CSeq_feat&)*mrna.GetOriginalSeq_feat());
                          return orig_mrna;
                        },
                        [&mapped, &feattree] {
                          CRef<CSeq_feat> orig_cds;
                          auto cds = feature::GetBestParentForFeat(mapped, CSeqFeatData::eSubtype_cdregion, &feattree);
                          if (cds)
                            orig_cds.Reset(&(CSeq_feat&)*cds.GetOriginalSeq_feat());
                          return orig_cds;
                        }
                    );

                    ReportFixedProduct(old_prot_name, prot_name, feature.GetLocation(), kEmptyStr);
                    modified = true;
                }
            }
        }
    }
    return modified;
}

void CFixSuspectProductName::FixSuspectProductNames(CSeq_entry& entry, CScope& scope) const
{
    auto feattree= Ref(new feature::CFeatTree(scope.GetTSE_Handle(entry)));

    VisitAllFeatures(entry,
        [](const CBioseq& bioseq){return bioseq.IsAa(); },
        [this, &scope, &feattree](CBioseq&bioseq, CSeq_feat& feature) { this->FixSuspectProductNames(feature, scope, *feattree); });
}


END_objects_SCOPE
END_NCBI_SCOPE
