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
#include <serial/objistr.hpp>

#include "suspect_feat.hpp"
#include "visitors.hpp"

#include <objmgr/annot_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

CFixSuspectProductName::CFixSuspectProductName()
{
}
CFixSuspectProductName::~CFixSuspectProductName()
{
}

void CFixSuspectProductName::SetFilename(const string& filename)
{
    m_rules_filename = filename;
    m_rules.Reset();
}

bool CFixSuspectProductName::FixSuspectProductName(string& name)
{
    if (m_rules.Empty())
    {
        bool found = false;
        const string& rules_env = CNcbiApplication::Instance()->GetEnvironment().Get("PRODUCT_RULES_LIST", &found);
        if (found)
            m_rules_filename = rules_env.data();

        m_rules = CSuspect_rule_set::GetProductRules(m_rules_filename);
    }

    if (m_rules.Empty() || m_rules->Get().empty())
        return false;

    for (const auto& rule : m_rules->Get())
    {
        if (rule->ApplyToString(name))
            return true;
    }

    return false;
}

void CFixSuspectProductName::ReportFixedProduct(const string& oldproduct, const string& newproduct, const CSeq_loc& loc, const string& locustag)
{
    if (m_fixed_product_report_filename.empty())
        return;

    if (m_report_ostream.get() == 0)
    {
       m_report_ostream.reset(new CNcbiOfstream(m_fixed_product_report_filename.c_str()));
    }

    string label;
    loc.GetLabel(&label);
    *m_report_ostream << "Changed " << oldproduct << " to " << newproduct << " " << label << " " << locustag << "\n\n";
}

bool CFixSuspectProductName::FixSuspectProductNames(objects::CSeq_feat& feature)
{
    static const char hypotetic_protein_name[] = "hypothetical protein";

    bool modified = false;
    if (feature.IsSetData() && feature.GetData().IsProt() && feature.GetData().GetProt().IsSetName())
    {
        for (auto& name : feature.SetData().SetProt().SetName())
        {
            if (NStr::Compare(name, hypotetic_protein_name))
            {
                string orig = name;
                if (FixSuspectProductName(name))
                {
                    ReportFixedProduct(orig, name, feature.GetLocation(), kEmptyStr);
                    modified = true;
                }
            }
        }
    }
    return modified;
}

void CFixSuspectProductName::FixSuspectProductNames(objects::CSeq_entry& entry)
{
    VisitAllFeatures(entry,
        [](const CBioseq& bioseq){return bioseq.IsAa(); },
        [this](CBioseq&bioseq, CSeq_feat& feature) { this->FixSuspectProductNames(feature); });
}


END_objects_SCOPE
END_NCBI_SCOPE


