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
#include <corelib/ncbistd.hpp>

#include <serial/enumvalues.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/context.hpp>


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
static bool s_IsNote(CFFContext& ctx, IFlatQVal::TFlags flags)
{
    return ((flags & IFlatQVal::fIsNote)
            &&  ctx.GetMode() != CFlatFileGenerator::eMode_Dump);
}


void CFlatStringQVal::Format(TFlatQuals& q, const string& name,
                           CFFContext& ctx, IFlatQVal::TFlags flags) const
{
    x_AddFQ(q, s_IsNote(ctx, flags) ? "note" : name, m_Value, m_Style);
}


void CFlatCodeBreakQVal::Format(TFlatQuals& q, const string& name,
                              CFFContext& ctx, IFlatQVal::TFlags) const
{
    ITERATE (CCdregion::TCode_break, it, m_Value) {
        string pos = CFlatSeqLoc((*it)->GetLoc(), ctx).GetString();
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


CFlatCodonQVal::CFlatCodonQVal(unsigned int codon, unsigned char aa, bool is_ascii)
    : m_Codon(CGen_code_table::IndexToCodon(codon)),
      m_AA(s_AAName(aa, is_ascii)), m_Checked(true)
{
}


void CFlatCodonQVal::Format(TFlatQuals& q, const string& name, CFFContext& ctx,
                          IFlatQVal::TFlags) const
{
    if ( !m_Checked ) {
        // ...
    }
    x_AddFQ(q, name, "(seq:\"" + m_Codon + "\",aa:" + m_AA + ')');
}


void CFlatExpEvQVal::Format(TFlatQuals& q, const string& name,
                          CFFContext&, IFlatQVal::TFlags) const
{
    const char* s = 0;
    switch (m_Value) {
    case CSeq_feat::eExp_ev_experimental:      s = "experimental";      break;
    case CSeq_feat::eExp_ev_not_experimental:  s = "not_experimental";  break;
    default:                                   break;
    }
    if (s) {
        x_AddFQ(q, name, s, CFlatQual::eUnquoted);
    }
}


void CFlatIllegalQVal::Format(TFlatQuals& q, const string&, CFFContext &ctx,
                            IFlatQVal::TFlags) const
{
    // XXX - return if too strict
    x_AddFQ(q, m_Value->GetQual(), m_Value->GetVal());
}


void CFlatMolTypeQVal::Format(TFlatQuals& q, const string& name,
                            CFFContext& ctx, IFlatQVal::TFlags flags) const
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


void CFlatOrgModQVal::Format(TFlatQuals& q, const string& name,
                           CFFContext& ctx, IFlatQVal::TFlags flags) const
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


void CFlatOrganelleQVal::Format(TFlatQuals& q, const string& name,
                              CFFContext&, IFlatQVal::TFlags) const
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
        x_AddFQ(q, organelle, kEmptyStr, CFlatQual::eEmpty);
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


void CFlatPubSetQVal::Format(TFlatQuals& q, const string& name,
                           CFFContext& ctx, IFlatQVal::TFlags) const
{
    /* !!!
    bool found = false;
    ITERATE (vector<CRef<CFlatReference> >, it, ctx.GetReferences()) {
        if ((*it)->Matches(*m_Value)) {
            x_AddFQ(q, name, '[' + NStr::IntToString((*it)->GetSerial()) + ']',
                    CFlatQual::eUnquoted);
            found = true;
        }
    }
    // complain if not found?
    */
}


void CFlatSeqDataQVal::Format(TFlatQuals& q, const string& name,
                            CFFContext& ctx, IFlatQVal::TFlags) const
{
    string s;
    CSeqVector v = ctx.GetHandle().GetScope().GetBioseqHandle(*m_Value)
        .GetSequenceView(*m_Value, CBioseq_Handle::eViewConstructed,
                         CBioseq_Handle::eCoding_Iupac);
    v.GetSeqData(0, v.size(), s);
    x_AddFQ(q, name, s);
}


void CFlatSeqIdQVal::Format(TFlatQuals& q, const string& name,
                          CFFContext& ctx, IFlatQVal::TFlags) const
{
    // XXX - add link in HTML mode
    // !!!x_AddFQ(q, name, ctx.GetPreferredSynonym(*m_Value).GetSeqIdString(true));
}


void CFlatSubSourceQVal::Format(TFlatQuals& q, const string& name,
                              CFFContext& ctx, IFlatQVal::TFlags flags) const
{
    switch (m_Value->GetSubtype()) {
    case CSubSource::eSubtype_germline:
    case CSubSource::eSubtype_rearranged:
    case CSubSource::eSubtype_transgenic:
    case CSubSource::eSubtype_environmental_sample:
        x_AddFQ(q, name, kEmptyStr, CFlatQual::eEmpty);
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


void CFlatXrefQVal::Format(TFlatQuals& q, const string& name,
                         CFFContext& ctx, IFlatQVal::TFlags flags) const
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
* Revision 1.2  2003/12/18 17:43:35  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:24:04  shomrat
* Initial Revision (adapted from flat lib)
*
* Revision 1.4  2003/06/02 16:06:42  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.3  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
