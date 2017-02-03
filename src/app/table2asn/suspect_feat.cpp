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

#include <objmgr/annot_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

CFixSuspectProductName::CFixSuspectProductName()
{
}
CFixSuspectProductName::~CFixSuspectProductName()
{
}

bool CFixSuspectProductName::FixSuspectProductName(string& name)
{
    static const char PRODUCT_RULES_LIST[] =
#ifdef NCBI_OS_MSWIN
        "\\\\panfs\\pan1\\wgs_gb_sub\\processing\\rules_usethis.txt";
#else
        "/panfs/pan1.be-md.ncbi.nlm.nih.gov/wgs_gb_sub/processing/rules_usethis.txt";
#endif
    if (m_rules.Empty())
    {
        bool found = false;
        const string& rules_env = CNcbiApplication::Instance()->GetEnvironment().Get("PRODUCT_RULES_LIST", &found);
        if (found)
            m_rules_filename = rules_env.data();
        else
            m_rules_filename = PRODUCT_RULES_LIST;

        m_rules.Reset(new CSuspect_rule_set);
        if (CFile(m_rules_filename).Exists())
        {
            CNcbiIfstream instream(m_rules_filename.c_str());
            auto_ptr<CObjectIStream> pObjIstrm(CObjectIStream::Open(eSerial_AsnText, instream, eNoOwnership));
            pObjIstrm->Read(m_rules, m_rules->GetThisTypeInfo());
        }
    }


    if (m_rules.Empty() || m_rules->Get().empty())
        return false;

    ITERATE(CSuspect_rule_set::Tdata, it, m_rules->Get())
    {
        if ((**it).ApplyToString(name))
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
       *m_report_ostream << "Using " << m_rules_filename.c_str() << " rules file" << endl << endl;
    }

    string label;
    loc.GetLabel(&label);
    *m_report_ostream << "Changed " << oldproduct << " to " << newproduct << " " << label << " " << locustag << "\n\n";
}

bool CFixSuspectProductName::FixProductNames(objects::CSeq_feat& feature)
{
    static const char hypotetic_protein_name[] = "hypothetical protein";

    bool modified = false;
    if (feature.IsSetData() && feature.GetData().IsProt() && feature.GetData().GetProt().IsSetName())
    {
        NON_CONST_ITERATE(CProt_ref::TName, name_it, feature.SetData().SetProt().SetName())
        {
            if (NStr::Compare(*name_it, hypotetic_protein_name))
            {
                string orig = *name_it;
                if (FixSuspectProductName(*name_it))
                {
                    ReportFixedProduct(orig, *name_it, feature.GetLocation(), kEmptyStr);
                    modified = true;
                }
            }
        }
    }
    return modified;
}

void CFixSuspectProductName::FixProductNames(objects::CBioseq& bioseq)
{
    if (bioseq.IsAa() && bioseq.IsSetAnnot() && !bioseq.GetAnnot().empty())
    {
        NON_CONST_ITERATE(CBioseq::TAnnot, annot_it, bioseq.SetAnnot())
        {
            if (!(**annot_it).IsFtable())
                continue;

            NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, ft_it, (**annot_it).SetData().SetFtable())
            {
                FixProductNames(**ft_it);
            }
        }
    }
}


END_objects_SCOPE
END_NCBI_SCOPE


