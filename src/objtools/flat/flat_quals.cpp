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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   new (early 2003) flat-file generator -- qualifier types
*   (mainly of interest to implementors)
*
* ===========================================================================
*/


#include <objects/flat/flat_quals.hpp>

#include <serial/enumvalues.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>

#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// in Ncbistdaa order
static const char* kAANames[] = {
    "---", "Ala", "Asx", "Cys", "Asp", "Glu", "Phe", "Gly", "His", "Ile",
    "Lys", "Leu", "Met", "Asn", "Pro", "Glu", "Arg", "Ser", "Thr", "Val",
    "Trp", "Xaa", "Tyr", "Glx", "Sec", "TERM"
};


inline
static const char* s_AAName(unsigned char aa, bool is_ascii)
{
    if (is_ascii) {
        aa = CSeqportUtil::GetMapToIndex
            (CSeq_data::e_Ncbieaa, CSeq_data::e_Ncbistdaa, aa);
    }
    return (aa < sizeof(kAANames)/sizeof(*kAANames)) ? kAANames[aa] : "OTHER";
}


inline
static bool s_IsNote(SFlatContext& ctx, IFlatQV::TFlags flags)
{
    return ((flags & IFlatQV::fIsNote)
            &&  ctx.m_Formatter->GetMode() != IFlatFormatter::eMode_Dump);
}


void CFlatStringQV::Format(TFlatQuals& q, const string& name,
                           SFlatContext& ctx, IFlatQV::TFlags flags) const
{
    x_AddFQ(q, s_IsNote(ctx, flags) ? "note" : name, m_Value, m_Style);
}


void CFlatCodeBreakQV::Format(TFlatQuals& q, const string& name,
                              SFlatContext& ctx, IFlatQV::TFlags) const
{
    ITERATE (CCdregion::TCode_break, it, m_Value) {
        string pos = SFlatLoc((*it)->GetLoc(), ctx).m_String;
        string aa  = "OTHER";
        switch ((*it)->GetAa().Which()) {
        case CCode_break::C_Aa::e_Ncbieaa:
            aa = s_AAName((*it)->GetAa().GetNcbieaa(), true);
            break;
        case CCode_break::C_Aa::e_Ncbi8aa:
            aa = s_AAName((*it)->GetAa().GetNcbi8aa(), false);
            break;
        case CCode_break::C_Aa::e_Ncbistdaa:
            aa = s_AAName((*it)->GetAa().GetNcbistdaa(), false);
            break;
        default:
            return;
        }
        x_AddFQ(q, name, "(pos:" + pos + ",aa:" + aa + ')');
    }
}


CFlatCodonQV::CFlatCodonQV(unsigned int codon, unsigned char aa, bool is_ascii)
    : m_Codon(CGen_code_table::IndexToCodon(codon)),
      m_AA(s_AAName(aa, is_ascii)), m_Checked(true)
{
}


void CFlatCodonQV::Format(TFlatQuals& q, const string& name, SFlatContext& ctx,
                          IFlatQV::TFlags) const
{
    if ( !m_Checked ) {
        // ...
    }
    x_AddFQ(q, name, "(seq:\"" + m_Codon + "\",aa:" + m_AA + ')');
}


void CFlatExpEvQV::Format(TFlatQuals& q, const string& name,
                          SFlatContext&, IFlatQV::TFlags) const
{
    const char* s = 0;
    switch (m_Value) {
    case CSeq_feat::eExp_ev_experimental:      s = "experimental";      break;
    case CSeq_feat::eExp_ev_not_experimental:  s = "not_experimental";  break;
    default:                                   break;
    }
    if (s) {
        x_AddFQ(q, name, s, SFlatQual::eUnquoted);
    }
}


void CFlatIllegalQV::Format(TFlatQuals& q, const string&, SFlatContext &ctx,
                            IFlatQV::TFlags) const
{
    // XXX - return if too strict
    x_AddFQ(q, m_Value->GetQual(), m_Value->GetVal());
}


void CFlatMolTypeQV::Format(TFlatQuals& q, const string& name,
                            SFlatContext& ctx, IFlatQV::TFlags flags) const
{
    const char* s = 0;
    switch (m_Biomol) {
    case CMolInfo::eBiomol_genomic:
        switch (m_Mol) {
        case CSeq_inst::eMol_dna:  s = "genomic DNA";  break;
        case CSeq_inst::eMol_rna:  s = "genomic RNA";  break;
        default:                   break;
        }
        break;

    case CMolInfo::eBiomol_pre_RNA:  s = "pre-mRNA";  break;
    case CMolInfo::eBiomol_mRNA:     s = "mRNA";      break;
    case CMolInfo::eBiomol_rRNA:     s = "rRNA";      break;
    case CMolInfo::eBiomol_tRNA:     s = "tRNA";      break;
    case CMolInfo::eBiomol_snRNA:    s = "snRNA";     break;
    case CMolInfo::eBiomol_scRNA:    s = "scRNA";     break;

    case CMolInfo::eBiomol_other_genetic:
    case CMolInfo::eBiomol_other:
        switch (m_Mol) {
        case CSeq_inst::eMol_dna:  s = "other DNA";  break;
        case CSeq_inst::eMol_rna:  s = "other RNA";  break;
        default:                   break;
        }
        break;

    case CMolInfo::eBiomol_cRNA:
    case CMolInfo::eBiomol_transcribed_RNA:  s = "other RNA";  break;
    case CMolInfo::eBiomol_snoRNA:           s = "snoRNA";     break;
    }

    if (s) {
        x_AddFQ(q, name, s);
    }
}


void CFlatOrgModQV::Format(TFlatQuals& q, const string& name,
                           SFlatContext& ctx, IFlatQV::TFlags flags) const
{
    switch (m_Value->GetSubtype()) {
    case COrgMod::eSubtype_other:
        x_AddFQ(q, "note", m_Value->GetSubname());
        break;

    default:
        if (s_IsNote(ctx, flags)) {
            x_AddFQ(q, "note", name + ": " + m_Value->GetSubname());
        } else {
            x_AddFQ(q, name, m_Value->GetSubname());
        }
        break;
    }
}


void CFlatOrganelleQV::Format(TFlatQuals& q, const string& name,
                              SFlatContext&, IFlatQV::TFlags) const
{
    const string& organelle
        = CBioSource::GetTypeInfo_enum_EGenome()->FindName(m_Value, true);

    switch (m_Value) {
    case CBioSource::eGenome_chloroplast: case CBioSource::eGenome_chromoplast:
    case CBioSource::eGenome_cyanelle:    case CBioSource::eGenome_apicoplast:
    case CBioSource::eGenome_leucoplast:  case CBioSource::eGenome_proplastid:
        x_AddFQ(q, name, "plastid:" + organelle);
        break;

    case CBioSource::eGenome_kinetoplast:
        x_AddFQ(q, name, "mitochondrion:kinetoplast");
        break;

    case CBioSource::eGenome_mitochondrion: case CBioSource::eGenome_plastid:
    case CBioSource::eGenome_nucleomorph:
        x_AddFQ(q, name, organelle);
        break;

    case CBioSource::eGenome_macronuclear: case CBioSource::eGenome_proviral:
    case CBioSource::eGenome_virion:
        x_AddFQ(q, organelle, kEmptyStr, SFlatQual::eEmpty);
        break;

    case CBioSource::eGenome_plasmid: case CBioSource::eGenome_transposon:
        x_AddFQ(q, organelle, kEmptyStr);
        break;

    case CBioSource::eGenome_insertion_seq:
        x_AddFQ(q, "insertion_seq", kEmptyStr);
        break;

    default:
        break;
    }
    
}


void CFlatPubSetQV::Format(TFlatQuals& q, const string& name,
                           SFlatContext& ctx, IFlatQV::TFlags) const
{
    bool found = false;
    ITERATE (vector<CRef<SFlatReference> >, it, ctx.m_References) {
        if ((*it)->Matches(*m_Value)) {
            x_AddFQ(q, name, '[' + NStr::IntToString((*it)->m_Serial) + ']',
                    SFlatQual::eUnquoted);
            found = true;
        }
    }
    // complain if not found?
}


void CFlatSeqDataQV::Format(TFlatQuals& q, const string& name,
                            SFlatContext& ctx, IFlatQV::TFlags) const
{
    string s;
    CSeqVector v = ctx.m_Handle.GetScope().GetBioseqHandle(*m_Value)
        .GetSequenceView(*m_Value, CBioseq_Handle::eViewConstructed,
                         CBioseq_Handle::eCoding_Iupac);
    v.GetSeqData(0, v.size(), s);
    x_AddFQ(q, name, s);
}


void CFlatSeqIdQV::Format(TFlatQuals& q, const string& name,
                          SFlatContext& ctx, IFlatQV::TFlags) const
{
    // XXX - add link in HTML mode
    x_AddFQ(q, name, ctx.GetPreferredSynonym(*m_Value).GetSeqIdString(true));
}


void CFlatSubSourceQV::Format(TFlatQuals& q, const string& name,
                              SFlatContext& ctx, IFlatQV::TFlags flags) const
{
    switch (m_Value->GetSubtype()) {
    case CSubSource::eSubtype_germline:
    case CSubSource::eSubtype_rearranged:
    case CSubSource::eSubtype_transgenic:
    case CSubSource::eSubtype_environmental_sample:
        x_AddFQ(q, name, kEmptyStr, SFlatQual::eEmpty);
        break;

    case CSubSource::eSubtype_other:
        x_AddFQ(q, "note", m_Value->GetName());
        break;

    default:
        if (s_IsNote(ctx, flags)) {
            x_AddFQ(q, "note", name + ": " + m_Value->GetName());
        } else {
            x_AddFQ(q, name, m_Value->GetName());
        }
        break;
    }
}


void CFlatXrefQV::Format(TFlatQuals& q, const string& name,
                         SFlatContext& ctx, IFlatQV::TFlags flags) const
{
    // XXX - add link in HTML mode?
    ITERATE (TXref, it, m_Value) {
        string s((*it)->GetDb());
        const CObject_id& id = (*it)->GetTag();
        switch (id.Which()) {
        case CObject_id::e_Id: s += ':' + NStr::IntToString(id.GetId()); break;
        case CObject_id::e_Str: s += ':' + id.GetStr(); break;
        default: break; // complain?
        }
        x_AddFQ(q, name, s);
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
