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
*   Context structure holding all table2asn parameters
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objtools/readers/source_mod_parser.hpp>
#include <objects/general/Date.hpp>
#include <algo/sequence/orf.hpp>
#include <objtools/readers/message_listener.hpp>

#include "table2asn_context.hpp"


#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace
{

CRef<CSeq_annot> FindORF(const CBioseq& bioseq)
{
    if (bioseq.IsNa())
    {
        COrf::TLocVec orfs;
        CSeqVector  seq_vec(bioseq);
        COrf::FindOrfs(seq_vec, orfs);
        if (orfs.size()>0)
        {
            CRef<CSeq_id> seqid(new CSeq_id);
            seqid->Assign(*bioseq.GetId().begin()->GetPointerOrNull());
            CRef<CSeq_annot> annot = COrf::MakeCDSAnnot(orfs, 1, seqid);
            return annot;
        }
    }
    return CRef<CSeq_annot>();
}

void x_ApplySourceQualifiers(CBioseq& bioseq, CSourceModParser& smp)
{
   CSourceModParser::TMods unused_mods = smp.GetMods(CSourceModParser::fUnusedMods);
   CUser_object::TData data;
   NON_CONST_ITERATE(CSourceModParser::TMods, mod, unused_mods)
   {
       if (mod->key.compare("bioproject")==0 ||
           mod->key.compare("biosample")==0)
       {
            CRef<CUser_field> field(new CUser_field);
            field->SetLabel().SetStr(mod->key);
            field->SetData().SetStr(mod->value);
            data.push_back(field);                
       }
   }
   if (!data.empty())
   {
       CUser_object::TData& existing_data = CTable2AsnContext::SetUserObject(bioseq.SetDescr(), "DBLink").SetData();
       existing_data.insert(existing_data.end(), data.begin(), data.end());
   }
}

void x_ApplySourceQualifiers(objects::CBioseq& bioseq, const string& src_qualifiers)
{
    if (src_qualifiers.empty())
        return;

    CSourceModParser smp;
    if (true)
    {
        smp.ParseTitle(src_qualifiers, CConstRef<CSeq_id>(bioseq.GetFirstId()) );
        smp.ApplyAllMods(bioseq);

        x_ApplySourceQualifiers(bioseq, smp);
#if 0
//        if( TestFlag(fUnknModThrow) ) 
        {
            CSourceModParser::TMods unused_mods = smp.GetMods(CSourceModParser::fUnusedMods);
            if( ! unused_mods.empty() )
            {
                // there are unused mods and user specified to throw if any
                // unused
                CNcbiOstrstream err;
                err << "CFastaReader: Inapplicable or unrecognized modifiers on ";

                // get sequence ID
                const CSeq_id* seq_id = bioseq.GetFirstId();
                if( seq_id ) {
                    err << seq_id->GetSeqIdString();
                } else {
                    // seq-id unknown
                    err << "sequence";
                }

                err << ":";
                ITERATE(CSourceModParser::TMods, mod_iter, unused_mods) {
                    err << " [" << mod_iter->key << "=" << mod_iter->value << ']';
                }
                err << " around line " + NStr::NumericToString(iLineNum);
                NCBI_THROW2(CObjReaderParseException, eUnusedMods,
                    (string)CNcbiOstrstreamToString(err),
                    iLineNum);
            }
        }
#endif

#if 0
        smp.GetLabel(&title, CSourceModParser::fUnusedMods);

        copy( smp.GetBadMods().begin(), smp.GetBadMods().end(),
            inserter(m_BadMods, m_BadMods.begin()) );
        CSourceModParser::TMods unused_mods =
            smp.GetMods(CSourceModParser::fUnusedMods);
        copy( unused_mods.begin(), unused_mods.end(),
            inserter(m_UnusedMods, m_UnusedMods.begin() ) );
#endif
    }

}

};


CTable2AsnContext::CTable2AsnContext():
    m_output(0),
    m_ProjectVersionNumber(0),
    m_flipped_struc_cmt(false),
    m_RemoteTaxonomyLookup(false),
    m_RemotePubLookup(false),
    m_HandleAsSet(false),
    m_GenomicProductSet(false),
    m_SetIDFromFile(false),
    m_NucProtSet(false),
    m_taxid(0)
{
}

CTable2AsnContext::~CTable2AsnContext()
{
}

void CTable2AsnContext::AddUserTrack(CSeq_descr& SD, const string& type, const string& lbl, const string& data) const
{
    if (data.empty())
        return;

    CRef<CUser_field> uf(new CUser_field);
    uf->SetLabel().SetStr(lbl);
    uf->SetNum(1);
    uf->SetData().SetStr(data);
    SetUserObject(SD, type).SetData().push_back(uf);
}

void CTable2AsnContext::FindOpenReadingFrame(objects::CSeq_entry& entry) const
{
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        {
        CRef<CSeq_annot> annot = FindORF(entry.SetSeq());
        if (annot.NotEmpty())
        {
            entry.SetSeq().SetAnnot().push_back(annot);
        }
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            FindOpenReadingFrame(**it);
        }
        break;
    default:
        break;
    }
}

void CTable2AsnContext::ApplySourceQualifiers(CSeq_entry& entry, const string& src_qualifiers) const
{
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        x_ApplySourceQualifiers(entry.SetSeq(), m_source_qualifiers);
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            ApplySourceQualifiers(**it, m_source_qualifiers);
        }
        break;
    default:
        break;
    }
}

CUser_object& CTable2AsnContext::SetUserObject(CSeq_descr& descr, const string& type)
{
    CRef<CUser_object> user_obj;
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc_it, descr.Set())
    {
        if ((**desc_it).IsUser() && (**desc_it).GetUser().IsSetType() &&
            (**desc_it).GetUser().GetType().IsStr() &&
            (**desc_it).GetUser().GetType().GetStr() == type)
        {
            return (**desc_it).SetUser();
        }
    }

    CRef<CObject_id> oi(new CObject_id);
    oi->SetStr(type);
    CRef<CUser_object> uo(new CUser_object);
    uo->SetType(*oi);

    CRef<CSeqdesc> sd(new CSeqdesc());
    sd->Select(CSeqdesc::e_User);
    sd->SetUser(*uo);

    descr.Set().push_back(sd);
    return *uo;
}

CBioSource& CTable2AsnContext::SetBioSource(CSeq_descr& SD)
{
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc_it, SD.Set())
    {
        if ((**desc_it).IsSource())
        {
            CBioSource& biosource = (**desc_it).SetSource();
            return biosource;
        }
    }
    CRef<CSeqdesc> source_desc(new CSeqdesc());
    source_desc->Select(CSeqdesc::e_Source);
    SD.Set().push_back(source_desc);
    return source_desc->SetSource();
}

void CTable2AsnContext::ApplyCreateDate(CSeq_entry& entry) const
{
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        {
        CRef<CSeqdesc> create_date(new CSeqdesc);
        CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
        create_date->SetCreate_date(*date);

        entry.SetSeq().SetDescr().Set().push_back(create_date);
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            ApplyCreateDate(**it);
        }
        break;
    default:
        break;
    }
}

void CTable2AsnContext::ApplyAccession(objects::CSeq_entry& entry) const
{
    if (m_accession.empty())
        return;

    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        {
            CRef<CSeq_id> accession_id(new CSeq_id);
            accession_id->SetGenbank().SetAccession(m_accession);
            entry.SetSeq().SetId().clear();
            entry.SetSeq().SetId().push_back(accession_id);
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            ApplyAccession(**it);
        }
        break;
    default:
        break;
    }
}

END_NCBI_SCOPE

