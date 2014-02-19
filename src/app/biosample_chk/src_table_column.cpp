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
 * Author:  Colleen Bollin
 *
 * File Description:
 *      Implementation of utility classes for handling source qualifiers by name
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/object_manager.hpp>

#include <vector>
#include <algorithm>
#include <list>

#include "src_table_column.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CSrcTableOrganismNameColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    in_out_bioSource.SetOrg().SetTaxname(newValue);
}

void CSrcTableOrganismNameColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    in_out_bioSource.SetOrg().ResetTaxname();
}


string CSrcTableOrganismNameColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetOrg() && in_out_bioSource.GetOrg().IsSetTaxname()) {
        val = in_out_bioSource.GetOrg().GetTaxname();
    }
    return val;
}


void CSrcTableTaxonIdColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    try {
        int val = NStr::StringToInt(newValue);
        in_out_bioSource.SetOrg().SetTaxId(val);
    } catch (...) {
    }
}


void CSrcTableTaxonIdColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    if (in_out_bioSource.IsSetOrg() && in_out_bioSource.GetOrg().IsSetDb()) {
        COrg_ref::TDb::iterator it = in_out_bioSource.SetOrg().SetDb().begin();
        while (it != in_out_bioSource.SetOrg().SetDb().end()) {
            if ((*it)->IsSetDb() && NStr::EqualNocase((*it)->GetDb(), "taxon")) {
                it = in_out_bioSource.SetOrg().SetDb().erase(it);
            } else {
                ++it;
            }
        }
    }
}


string CSrcTableTaxonIdColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetOrg()) {
        try {
            int taxid = in_out_bioSource.GetOrg().GetTaxId();
            val = NStr::NumericToString(taxid);
        } catch (...) {
        }
    }
    return val;
}


void CSrcTableGenomeColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    in_out_bioSource.SetGenome(objects::CBioSource::GetGenomeByOrganelle(newValue));
}

void CSrcTableGenomeColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    in_out_bioSource.ResetGenome();
}


string CSrcTableGenomeColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetGenome()) {
        val = in_out_bioSource.GetOrganelleByGenome( in_out_bioSource.GetGenome());
    }
    return val;
}



void CSrcTableSubSourceColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    if (!NStr::IsBlank(newValue)) {
        if (objects::CSubSource::NeedsNoText(m_Subtype)) {
            CRef<objects::CSubSource> s(new objects::CSubSource(m_Subtype, " "));        
            in_out_bioSource.SetSubtype().push_back(s);
        } else {
            CRef<objects::CSubSource> s(new objects::CSubSource(m_Subtype, newValue));        
            in_out_bioSource.SetSubtype().push_back(s);
        }
    }
}

void CSrcTableSubSourceColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    if (in_out_bioSource.IsSetSubtype()) {
        objects::CBioSource::TSubtype::iterator it = in_out_bioSource.SetSubtype().begin();
        while (it != in_out_bioSource.SetSubtype().end()) {
            if ((*it)->GetSubtype() == m_Subtype) {
                it = in_out_bioSource.SetSubtype().erase(it);
            } else {
                ++it;
            }
        }
        if (in_out_bioSource.SetSubtype().empty()) {
            in_out_bioSource.ResetSubtype();
        }
    }
}


string CSrcTableSubSourceColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetSubtype()) {
        objects::CBioSource::TSubtype::const_iterator it = in_out_bioSource.GetSubtype().begin();
        while (it != in_out_bioSource.GetSubtype().end()) {
            if ((*it)->GetSubtype() == m_Subtype && (*it)->IsSetName()) {
                val = (*it)->GetName();
                break;
            }
            ++it;
        }
    }
    return val;
}


string CSrcTableSubSourceColumn::GetLabel() const 
{ 
    if (m_Subtype == objects::CSubSource::eSubtype_other) {
        return "subsrc_note";
    } else {
        return objects::CSubSource::GetSubtypeName(m_Subtype); 
    }
}


void CSrcTableOrgModColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    if (!NStr::IsBlank(newValue)) {
        CRef<objects::COrgMod> s(new objects::COrgMod(m_Subtype, newValue));        
        in_out_bioSource.SetOrg().SetOrgname().SetMod().push_back(s);
    }
}

void CSrcTableOrgModColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    if (in_out_bioSource.IsSetOrg() && in_out_bioSource.GetOrg().IsSetOrgname() && in_out_bioSource.GetOrg().GetOrgname().IsSetMod()) {
        objects::COrgName::TMod::iterator it = in_out_bioSource.SetOrg().SetOrgname().SetMod().begin();
        while (it != in_out_bioSource.SetOrg().SetOrgname().SetMod().end()) {
            if ((*it)->GetSubtype() == m_Subtype) {
                it = in_out_bioSource.SetOrg().SetOrgname().SetMod().erase(it);
            } else {
                ++it;
            }
        }
    }
}


string CSrcTableOrgModColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetOrg() && in_out_bioSource.GetOrg().IsSetOrgname() && in_out_bioSource.GetOrg().GetOrgname().IsSetMod()) {
        objects::COrgName::TMod::const_iterator it = in_out_bioSource.GetOrg().GetOrgname().GetMod().begin();
        while (it != in_out_bioSource.GetOrg().GetOrgname().GetMod().end()) {
            if ((*it)->GetSubtype() == m_Subtype && (*it)->IsSetSubname()) {
                val = (*it)->GetSubname();
                break;
            }
            ++it;
        }
    }
    return val;
}


string CSrcTableOrgModColumn::GetLabel() const 
{ 
    if (m_Subtype == objects::COrgMod::eSubtype_other) {
        return "orgmod_note";
    } else {
        return objects::COrgMod::GetSubtypeName(m_Subtype); 
    }
}


void CSrcTableFwdPrimerSeqColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    objects::CPCRPrimerSeq seq(newValue);
    bool add_new_set = true;
    if (in_out_bioSource.IsSetPcr_primers() && in_out_bioSource.GetPcr_primers().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().size() > 0) {
        // some sets exist, can we add to the last one?
        if (in_out_bioSource.GetPcr_primers().Get().back()->IsSetForward()
            && in_out_bioSource.GetPcr_primers().Get().back()->GetForward().IsSet()) {
            if (!in_out_bioSource.GetPcr_primers().Get().back()->GetForward().Get().front()->IsSetSeq()) {
                in_out_bioSource.SetPcr_primers().Set().back()->SetForward().Set().front()->SetSeq(seq);
                add_new_set = false;
            }
        } else {
            CRef<objects::CPCRPrimer> primer (new objects::CPCRPrimer());
            primer->SetSeq(seq);
            in_out_bioSource.SetPcr_primers().Set().back()->SetForward().Set().push_back(primer);
            add_new_set = false;
        }
    }

    if (add_new_set) {
        CRef<objects::CPCRPrimer> primer (new objects::CPCRPrimer());
        primer->SetSeq(seq);
        CRef<objects::CPCRReaction> reaction (new objects::CPCRReaction());
        reaction->SetForward().Set().push_back(primer);
        in_out_bioSource.SetPcr_primers().Set().push_back(reaction);
    }
}

void CSrcTableFwdPrimerSeqColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    in_out_bioSource.ResetPcr_primers();
}


string CSrcTableFwdPrimerSeqColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetPcr_primers()
        && in_out_bioSource.GetPcr_primers().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().size() > 0
        && in_out_bioSource.GetPcr_primers().Get().front()->IsSetForward()
        && in_out_bioSource.GetPcr_primers().Get().front()->GetForward().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().front()->GetForward().Get().front()->IsSetSeq()) {
        val = in_out_bioSource.GetPcr_primers().Get().front()->GetForward().Get().front()->GetSeq();
    }
    return val;
}


void CSrcTableRevPrimerSeqColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    objects::CPCRPrimerSeq seq(newValue);
    bool add_new_set = true;
    if (in_out_bioSource.IsSetPcr_primers() && in_out_bioSource.GetPcr_primers().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().size() > 0) {
        // some sets exist, can we add to the last one?
        if (in_out_bioSource.GetPcr_primers().Get().back()->IsSetReverse()
            && in_out_bioSource.GetPcr_primers().Get().back()->GetReverse().IsSet()) {
            if (!in_out_bioSource.GetPcr_primers().Get().back()->GetReverse().Get().front()->IsSetSeq()) {
                in_out_bioSource.SetPcr_primers().Set().back()->SetReverse().Set().front()->SetSeq(seq);
                add_new_set = false;
            }
        } else {
            CRef<objects::CPCRPrimer> primer (new objects::CPCRPrimer());
            primer->SetSeq(seq);
            in_out_bioSource.SetPcr_primers().Set().back()->SetReverse().Set().push_back(primer);
            add_new_set = false;
        }
    }

    if (add_new_set) {
        CRef<objects::CPCRPrimer> primer (new objects::CPCRPrimer());
        primer->SetSeq(seq);
        CRef<objects::CPCRReaction> reaction (new objects::CPCRReaction());
        reaction->SetReverse().Set().push_back(primer);
        in_out_bioSource.SetPcr_primers().Set().push_back(reaction);
    }
}


void CSrcTableRevPrimerSeqColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    in_out_bioSource.ResetPcr_primers();
}


string CSrcTableRevPrimerSeqColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetPcr_primers()
        && in_out_bioSource.GetPcr_primers().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().size() > 0
        && in_out_bioSource.GetPcr_primers().Get().front()->IsSetReverse()
        && in_out_bioSource.GetPcr_primers().Get().front()->GetReverse().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().front()->GetReverse().Get().front()->IsSetSeq()) {
        val = in_out_bioSource.GetPcr_primers().Get().front()->GetReverse().Get().front()->GetSeq();
    }
    return val;
}


void CSrcTableFwdPrimerNameColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    objects::CPCRPrimerName name(newValue);
    bool add_new_set = true;

    if (in_out_bioSource.IsSetPcr_primers() && in_out_bioSource.GetPcr_primers().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().size() > 0) {
        // some sets exist, can we add to the last one?
        if (in_out_bioSource.GetPcr_primers().Get().back()->IsSetForward()
            && in_out_bioSource.GetPcr_primers().Get().back()->GetForward().IsSet()) {
            if (!in_out_bioSource.GetPcr_primers().Get().back()->GetForward().Get().front()->IsSetName()) {
                in_out_bioSource.SetPcr_primers().Set().back()->SetForward().Set().front()->SetName(name);
                add_new_set = false;
            }
        } else {
            CRef<objects::CPCRPrimer> primer (new objects::CPCRPrimer());
            primer->SetName(name);
            in_out_bioSource.SetPcr_primers().Set().back()->SetForward().Set().push_back(primer);
            add_new_set = false;
        }
    }

    if (add_new_set) {
        CRef<objects::CPCRPrimer> primer (new objects::CPCRPrimer());
        primer->SetName(name);
        CRef<objects::CPCRReaction> reaction (new objects::CPCRReaction());
        reaction->SetForward().Set().push_back(primer);
        in_out_bioSource.SetPcr_primers().Set().push_back(reaction);
    }
}

void CSrcTableFwdPrimerNameColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    in_out_bioSource.ResetPcr_primers();
}


string CSrcTableFwdPrimerNameColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetPcr_primers()
        && in_out_bioSource.GetPcr_primers().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().size() > 0
        && in_out_bioSource.GetPcr_primers().Get().front()->IsSetForward()
        && in_out_bioSource.GetPcr_primers().Get().front()->GetForward().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().front()->GetForward().Get().front()->IsSetName()) {
        val = in_out_bioSource.GetPcr_primers().Get().front()->GetForward().Get().front()->GetName();
    }
    return val;
}


void CSrcTableRevPrimerNameColumn::AddToBioSource(
        objects::CBioSource & in_out_bioSource, const string & newValue )
{
    objects::CPCRPrimerName name(newValue);
    bool add_new_set = true;

    if (in_out_bioSource.IsSetPcr_primers() && in_out_bioSource.GetPcr_primers().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().size() > 0) {
        // some sets exist, can we add to the last one?
        if (in_out_bioSource.GetPcr_primers().Get().back()->IsSetReverse()
            && in_out_bioSource.GetPcr_primers().Get().back()->GetReverse().IsSet()) {
            if (!in_out_bioSource.GetPcr_primers().Get().back()->GetReverse().Get().front()->IsSetName()) {
                in_out_bioSource.SetPcr_primers().Set().back()->SetReverse().Set().front()->SetName(name);
                add_new_set = false;
            }
        } else {
            CRef<objects::CPCRPrimer> primer (new objects::CPCRPrimer());
            primer->SetName(name);
            in_out_bioSource.SetPcr_primers().Set().back()->SetReverse().Set().push_back(primer);
            add_new_set = false;
        }
    }

    if (add_new_set) {
        CRef<objects::CPCRPrimer> primer (new objects::CPCRPrimer());
        primer->SetName(name);
        CRef<objects::CPCRReaction> reaction (new objects::CPCRReaction());
        reaction->SetReverse().Set().push_back(primer);
        in_out_bioSource.SetPcr_primers().Set().push_back(reaction);
    }
}


void CSrcTableRevPrimerNameColumn::ClearInBioSource(
      objects::CBioSource & in_out_bioSource )
{
    in_out_bioSource.ResetPcr_primers();
}


string CSrcTableRevPrimerNameColumn::GetFromBioSource(
      const objects::CBioSource & in_out_bioSource ) const
{
    string val = "";
    if (in_out_bioSource.IsSetPcr_primers()
        && in_out_bioSource.GetPcr_primers().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().size() > 0
        && in_out_bioSource.GetPcr_primers().Get().front()->IsSetReverse()
        && in_out_bioSource.GetPcr_primers().Get().front()->GetReverse().IsSet()
        && in_out_bioSource.GetPcr_primers().Get().front()->GetReverse().Get().front()->IsSetName()) {
        val = in_out_bioSource.GetPcr_primers().Get().front()->GetReverse().Get().front()->GetName();
    }
    return val;
}


bool IsFwdPrimerName (string name)
{
    return (NStr::EqualNocase(name, "fwd-primer-name") || NStr::EqualNocase(name, "fwd primer name")
        || NStr::EqualNocase(name, "fwd-name") || NStr::EqualNocase(name, "fwd name"));
}


bool IsRevPrimerName (string name)
{
    return (NStr::EqualNocase(name, "rev-primer-name") || NStr::EqualNocase(name, "rev primer name")
        || NStr::EqualNocase(name, "rev-name") || NStr::EqualNocase(name, "rev name"));
}


bool IsFwdPrimerSeq (string name)
{
    return (NStr::EqualNocase(name, "fwd-primer-seq") || NStr::EqualNocase(name, "fwd primer seq")
        || NStr::EqualNocase(name, "fwd-seq") || NStr::EqualNocase(name, "fwd seq"));
}


bool IsRevPrimerSeq (string name)
{
    return (NStr::EqualNocase(name, "rev-primer-seq") || NStr::EqualNocase(name, "rev primer seq")
        || NStr::EqualNocase(name, "rev-seq") || NStr::EqualNocase(name, "rev seq"));
}


CRef<CSrcTableColumnBase>
CSrcTableColumnBaseFactory::Create(string sTitle)
{
    // the default is a CSrcTableEditCommandFactory that does nothing

    if( sTitle.empty() ) {
        return CRef<CSrcTableColumnBase>( new CSrcTableColumnBase );
    }

    if( NStr::EqualNocase(sTitle, kOrganismName) || NStr::EqualNocase(sTitle, "org") || NStr::EqualNocase(sTitle, "taxname") ) {
        return CRef<CSrcTableColumnBase>( new CSrcTableOrganismNameColumn );
    }

    if (NStr::EqualNocase(sTitle, kTaxId)) {
        return CRef<CSrcTableColumnBase>( new CSrcTableTaxonIdColumn );
    }

    if (NStr::EqualNocase( sTitle, "genome" )) {
        return CRef<CSrcTableColumnBase>( new CSrcTableGenomeColumn );
    }

    if (NStr::EqualNocase(sTitle, "SubSource Note") ) {
        return CRef<CSrcTableColumnBase>( new CSrcTableSubSourceColumn (objects::CSubSource::eSubtype_other));
    }

    if (NStr::EqualNocase(sTitle, "OrgMod Note") ) {
        return CRef<CSrcTableColumnBase>( new CSrcTableOrgModColumn (objects::COrgMod::eSubtype_other));
    }

    if (IsFwdPrimerName(sTitle)) {
        return CRef<CSrcTableColumnBase>( new CSrcTableFwdPrimerNameColumn ());
    }

    if (IsFwdPrimerSeq(sTitle)) {
        return CRef<CSrcTableColumnBase>( new CSrcTableFwdPrimerSeqColumn ());
    }

    if (IsRevPrimerName(sTitle)) {
        return CRef<CSrcTableColumnBase>( new CSrcTableRevPrimerNameColumn ());
    }

    if (IsRevPrimerSeq(sTitle)) {
        return CRef<CSrcTableColumnBase>( new CSrcTableRevPrimerSeqColumn ());
    }

    // see if it's a SubSource subtype:
    try {
        objects::CSubSource::TSubtype st = objects::CSubSource::GetSubtypeValue (sTitle, objects::CSubSource::eVocabulary_insdc);
        return CRef<CSrcTableColumnBase>( new CSrcTableSubSourceColumn(st) );
    } catch (...) {
        try {
            objects::COrgMod::TSubtype st = objects::COrgMod::GetSubtypeValue (sTitle, objects::COrgMod::eVocabulary_insdc);
            return CRef<CSrcTableColumnBase>( new CSrcTableOrgModColumn(st) );
        } catch (...) {
        }
    }

    return CRef<CSrcTableColumnBase>( NULL );
}


TSrcTableColumnList GetSourceFields(const CBioSource& src)
{
    TSrcTableColumnList fields;

    if (src.IsSetSubtype()) {
        ITERATE(CBioSource::TSubtype, it, src.GetSubtype())  {
            fields.push_back(CRef<CSrcTableColumnBase>(new CSrcTableSubSourceColumn((*it)->GetSubtype())));
        }
    }
    if (src.IsSetPcr_primers()) {
        fields.push_back(CRef<CSrcTableColumnBase>(new CSrcTableFwdPrimerSeqColumn()));
        fields.push_back(CRef<CSrcTableColumnBase>(new CSrcTableRevPrimerSeqColumn()));
        fields.push_back(CRef<CSrcTableColumnBase>(new CSrcTableFwdPrimerNameColumn()));
        fields.push_back(CRef<CSrcTableColumnBase>(new CSrcTableRevPrimerNameColumn()));
    }

    if (src.IsSetOrg()) {
        if (src.GetOrg().IsSetTaxname()) {
            fields.push_back(CRef<CSrcTableColumnBase>(new CSrcTableOrganismNameColumn()));
            try {
                src.GetOrg().GetTaxId();
                fields.push_back(CRef<CSrcTableColumnBase>(new CSrcTableTaxonIdColumn()));
            } catch (...) {
            }
        }
        if (src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetMod()) {
            ITERATE(COrgName::TMod, it, src.GetOrg().GetOrgname().GetMod()) {
                fields.push_back(CRef<CSrcTableColumnBase>(new CSrcTableOrgModColumn((*it)->GetSubtype())));
            }
        }            
    }
    return fields;
}




END_SCOPE(objects)
END_NCBI_SCOPE
