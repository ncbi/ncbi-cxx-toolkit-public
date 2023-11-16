/* $Id$
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
 * Author:  Colleen Bollin, Justin Foley
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/cleanup/influenza_set.hpp>

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// From SQD-4297
// Influenza is a multi-segmented virus. We would like to create
// small-genome sets when all segments of a particular viral strain
// are submitted together. This is made more difficult due to fact
// that submitters often have large submissions with multiple strains
// at one time.
// This function will segregate sequences with the same taxname
// plus additional qualifiers into small-genome sets, if there are enough
// sequences for that type of Influenza *AND* all CDS and gene features
// on the sequences are complete.
// * Influenza A virus: 8 or more nucleotide sequences with same strain and serotype
// * Influenza B virus: 8 or more nucleotide sequences with same strain
// * Influenza C virus: 7 or more nucleotide sequences with same strain
// * Influenza D virus: 7 or more records with same strain
// Note that as long as we are making strain-specific organism names,
// the taxname must only start with the Influenza designation, not match it.
// Can only make a set if at least one instance of each segment value is represented.

CInfluenzaSet::CInfluenzaSet(const string& key) : m_Key(key)
{
    m_FluType = GetInfluenzaType(key);
    m_Required = GetNumRequired(m_FluType);
}


size_t CInfluenzaSet::GetNumRequired(EInfluenzaType fluType) 
{
    if (fluType == eInfluenzaA || fluType == eInfluenzaB) {
        return 8;
    }
    return 7;
}


CInfluenzaSet::EInfluenzaType CInfluenzaSet::GetInfluenzaType(const string& taxname)
{
    if (NStr::StartsWith(taxname, "Influenza A virus", NStr::eNocase)) {
        return eInfluenzaA;
    } else if (NStr::StartsWith(taxname, "Influenza B virus", NStr::eNocase)) {
        return eInfluenzaB;
    } else if (NStr::StartsWith(taxname, "Influenza C virus", NStr::eNocase)) {
        return eInfluenzaC;
    } else if (NStr::StartsWith(taxname, "Influenza D virus", NStr::eNocase)) {
        return eInfluenzaD;
    } 
    return eNotInfluenza;
}


string CInfluenzaSet::GetKey(const COrg_ref& org)
{
    if (!org.IsSetTaxname() || !org.IsSetOrgname() || !org.GetOrgname().IsSetMod()) {
        return kEmptyStr;
    }
    EInfluenzaType flu_type = GetInfluenzaType(org.GetTaxname());
    if (flu_type == eNotInfluenza) {
        return kEmptyStr;
    }

    CTempString strain = kEmptyStr;
    CTempString serotype = kEmptyStr;

    for (auto pOrgMod : org.GetOrgname().GetMod()) {
        if (pOrgMod->IsSetSubtype() && pOrgMod->IsSetSubname()) {
            if (pOrgMod->GetSubtype() == COrgMod::eSubtype_strain) {
                strain = pOrgMod->GetSubname();
            } else if (pOrgMod->GetSubtype() == COrgMod::eSubtype_serotype &&
                flu_type == eInfluenzaA) {
                serotype = pOrgMod->GetSubname();
            }
        }
    }

    if(NStr::IsBlank(strain)) {
        return kEmptyStr;
    }
    if (flu_type == eInfluenzaA) {
        if (NStr::IsBlank(serotype)) {
            return kEmptyStr;
        } 
        return org.GetTaxname() + ":" + strain + ":" + serotype;
    } 
    
    return org.GetTaxname() + ":" + strain;
}


void CInfluenzaSet::AddBioseq(CBioseq_Handle bsh)
{
    m_Members.push_back(bsh);
}


bool g_FindSegs(const CBioSource& src, size_t numRequired, set<size_t>& segsFound) 
{
    if (!src.IsSetSubtype()) {
        return false;
    }

    bool foundSeg = false;
    for (auto pSubSource : src.GetSubtype()) {
        if (pSubSource && pSubSource->IsSetSubtype() && pSubSource->IsSetName() &&
            pSubSource->GetSubtype() == CSubSource::eSubtype_segment) {
            auto segment = NStr::StringToSizet(pSubSource->GetName(), NStr::fConvErr_NoThrow);
            if (segment < 1 || segment > numRequired ) {
                return false;
            }
            segsFound.insert(segment);
            foundSeg = true;
        }
    }
    return foundSeg;
}


bool CInfluenzaSet::OkToMakeSet() const
{
    if (m_Members.size() < m_Required) {
        return false;
    }

    set<size_t> segsFound;
    for(auto bsh : m_Members) {
        // check to make sure one of each segment is represented
        CSeqdesc_CI src(bsh, CSeqdesc::e_Source);
        if (!g_FindSegs(src->GetSource(), m_Required, segsFound)) {
            return false;
        }
        // make sure all coding regions and genes are complete
        SAnnotSelector sel;
        sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
        sel.IncludeFeatType(CSeqFeatData::e_Gene);
        CFeat_CI f(bsh, sel);
        while (f) {
            if (f->GetLocation().IsPartialStart(eExtreme_Biological) ||
                f->GetLocation().IsPartialStop(eExtreme_Biological)) {
                return false;
            }
            ++f;
        }
    }

    return (segsFound.size() == m_Required);
}


void CInfluenzaSet::MakeSet()
{
    if (m_Members.empty()) {
        return;
    }
    CBioseq_set_Handle parent = m_Members[0].GetParentBioseq_set();
    if (!parent) {
        return;
    }
    if (parent.IsSetClass() && parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
        parent = parent.GetParentBioseq_set();
    }
    if (!parent) {
        return;
    }
    CSeq_entry_Handle peh = parent.GetParentEntry();
    CSeq_entry_EditHandle peeh(peh);
    CBioseq_set_EditHandle parent_edit(parent);
    CRef<CSeq_entry> ns(new CSeq_entry());
    ns->SetSet().SetClass(CBioseq_set::eClass_small_genome_set);
    CSeq_entry_EditHandle new_set = parent_edit.AttachEntry(*ns, -1);
    ITERATE(TMembers, it, m_Members) {
        CBioseq_set_Handle np = it->GetParentBioseq_set();
        if (np && np.IsSetClass() && np.GetClass() == CBioseq_set::eClass_nuc_prot) {
            CSeq_entry_Handle nps = np.GetParentEntry();
            CSeq_entry_EditHandle npse(nps);
            npse.Remove();
            new_set.AttachEntry(npse);
        } else {
            CSeq_entry_Handle s = it->GetParentEntry();
            CSeq_entry_EditHandle se(s);
            se.Remove();
            new_set.AttachEntry(se);
        }
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
