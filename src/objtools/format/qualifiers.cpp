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
#include <ncbi_pch.hpp>
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

#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const string IFlatQVal::kSemicolon = "; ";
const string IFlatQVal::kComma     = ", ";
const string IFlatQVal::kEOL       = "\n";


static bool s_IsNote(IFlatQVal::TFlags flags)
{
    return (flags & IFlatQVal::fIsNote);
}


static bool s_StringIsJustQuotes(const string& str)
{
    ITERATE(string, it, str) {
        if ( (*it != '"')  ||  (*it != '\'') ) {
            return false;
        }
    }

    return true;
}


static string s_GetGOText(const CUser_field& field, bool is_ftable)
{
    string text_string, evidence, go_id;
    int pmid = 0;

    ITERATE (CUser_field::C_Data::TFields, it, field.GetData().GetFields()) {
        if ( !(*it)->CanGetLabel()  ||  !(*it)->GetLabel().IsStr() ) {
            continue;
        }
        
        const string& label = (*it)->GetLabel().GetStr();
        const CUser_field::C_Data& data = (*it)->GetData();
        
        if ( label == "text string"  &&  data.IsStr() ) {
            text_string = data.GetStr();
        }
        
        if ( label == "go id" ) {
            if ( data.IsStr() ) {
                go_id = data.GetStr();
            } else if ( data.IsInt() ) {
                go_id = NStr::IntToString(data.GetInt());
            }
        }
        
        if ( label == "evidence"  &&  data.IsStr() ) {
            evidence = data.GetStr();
        }
        
        if ( label == "pubmed id"  &&  data.IsInt() ) {
            pmid = data.GetInt();
        }
    }

    
    CNcbiOstrstream text;
    
    text << text_string;
    if ( is_ftable ) {
        text << "|" << go_id << "|";
        if ( pmid != 0 ) {
            text <<  pmid;
        }
        if ( !evidence.empty() ) {
            text << "|" << evidence;
        }
    } else { 
        if ( !go_id.empty() ) {
            text << " [goid " << go_id << "]";
            if ( !evidence.empty() ) {
                text << " [evidence " << evidence << "]";
            }
            if ( pmid != 0 ) {
                text << " [pmid " << pmid << "]";
            }
        }
    }

    return NStr::TruncateSpaces(CNcbiOstrstreamToString(text));
}


static size_t s_ComposeCodonRecognizedStr(const CTrna_ext& trna, string& recognized)
{
    recognized.erase();

    if (!trna.IsSetCodon()) {
        return 0;
    }

    list<string> codons;
    
    ITERATE (CTrna_ext::TCodon, it, trna.GetCodon()) {
        string codon = CGen_code_table::IndexToCodon(*it);
        // replace U for T
        NON_CONST_ITERATE (string, base, codon) {
            if (*base == 'T') {
                *base = 'U';
            }
        }
        if (!codon.empty()) {
            codons.push_back(codon);
        }
    }
    if (codons.empty()) {
        return 0;
    }

    codons.sort();

    recognized = NStr::Join(codons, ", ");
    return codons.size();
    /*
  for (i = 0; i < 256; i++) {
    chrToInt [i] = 0;
  }
  for (i = 1; i < 16; i++) {
    ch = intToChr [i];
    chrToInt [(int) ch] = i;
  }

  count = ValNodeLen (head);
  str1 = (CharPtr) head->data.ptrvalue;
  vnp = head->next;
  prev = (Pointer PNTR) &(head->next);
  while (vnp != NULL) {
    next = vnp->next;
    str2 = (CharPtr) vnp->data.ptrvalue;
    if (str1 != NULL && str2 != NULL &&
        str1 [0] == str2 [0] && str1 [1] == str2 [1]) {
      str1 [2] = MakeDegenerateBase (str1 [2], str2 [2], chrToInt, intToChr);
      *prev = next;
      vnp->next = NULL;
      ValNodeFreeData (vnp);
    } else {
      str1 = str2;
      prev = (Pointer PNTR) &(vnp->next);
    } 
    vnp = next;
  }

  for (vnp = head, ptr = buf, i = 0, prefix = NULL; vnp != NULL;
       vnp = vnp->next, prefix = ", ", i++) {
    ptr = StringMove (ptr, prefix);
    ptr = StringMove (ptr, (CharPtr) vnp->data.ptrvalue);
  }

  ValNodeFreeData (head);
  
  return count;
  */
}


/////////////////////////////////////////////////////////////////////////////
// CFormatQual - low-level formatted qualifier

CFormatQual::CFormatQual
(const string& name,
 const string& value, 
 const string& prefix,
 const string& suffix,
 TStyle style) :
    m_Name(name), m_Value(value), m_Prefix(prefix), m_Suffix(suffix),
    m_Style(style)
{
}


CFormatQual::CFormatQual(const string& name, const string& value, TStyle style) :
    m_Name(name), m_Value(value), m_Prefix(kEmptyStr), m_Suffix(kEmptyStr),
    m_Style(style)
{
}


// === CFlatStringQVal ======================================================

CFlatStringQVal::CFlatStringQVal(const string& value, TStyle style)
    :  IFlatQVal(&kEmptyStr, &kSemicolon),
       m_Value(NStr::TruncateSpaces(value)), m_Style(style)
{
}


CFlatStringQVal::CFlatStringQVal
(const string& value,
 const string& pfx,
 const string& sfx,
 TStyle style)
    :   IFlatQVal(&pfx, &sfx),
        m_Value(NStr::TruncateSpaces(value)),
        m_Style(style)
{
}


void CFlatStringQVal::Format(TFlatQuals& q, const string& name,
                           CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    x_AddFQ(q, (s_IsNote(flags) ? "note" : name), m_Value, m_Style);
}


// === CFlatNumberQVal ======================================================


void CFlatNumberQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    if (ctx.Config().CheckQualSyntax()) {
        if (IsBlankString(m_Value)) {
            return;
        }
        bool has_space = false;
        ITERATE (string, it, m_Value) {
            if (isspace(*it)) {
                has_space = true;
            } else if (has_space) {
                // non-space after space
                return;
            }
        }
    }

    CFlatStringQVal::Format(quals, name, ctx, flags);
}


// === CFlatStringListQVal ==================================================


void CFlatStringListQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if ( s_IsNote(flags) ) {
        m_Suffix = &kSemicolon;
    }

    x_AddFQ(q, 
            (s_IsNote(flags) ? "note" : name),
            JoinNoRedund(m_Value, "; "),
            m_Style);
}


// === CFlatGeneSynonymsQVal ================================================

void CFlatGeneSynonymsQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    size_t num_syns = GetValue().size();
    if (num_syns > 0) {
        string syns = (num_syns > 1) ? "synonyms: " : "synonym: ";
        syns += JoinNoRedund(GetValue(), ", ");
        x_AddFQ(q, 
            (s_IsNote(flags) ? "note" : name),
            syns,
            m_Style);

        m_Suffix = &kSemicolon;
    }
    
}


// === CFlatCodeBreakQVal ===================================================

void CFlatCodeBreakQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    ITERATE (CCdregion::TCode_break, it, m_Value) {
        string pos = CFlatSeqLoc((*it)->GetLoc(), ctx).GetString();
        string aa  = "OTHER";
        switch ((*it)->GetAa().Which()) {
        case CCode_break::C_Aa::e_Ncbieaa:
            aa = GetAAName((*it)->GetAa().GetNcbieaa(), true);
            break;
        case CCode_break::C_Aa::e_Ncbi8aa:
            aa = GetAAName((*it)->GetAa().GetNcbi8aa(), false);
            break;
        case CCode_break::C_Aa::e_Ncbistdaa:
            aa = GetAAName((*it)->GetAa().GetNcbistdaa(), false);
            break;
        default:
            return;
        }
        x_AddFQ(q, name, "(pos:" + pos + ",aa:" + aa + ')', 
            CFormatQual::eUnquoted);
    }
}


CFlatCodonQVal::CFlatCodonQVal(unsigned int codon, unsigned char aa, bool is_ascii)
    : m_Codon(CGen_code_table::IndexToCodon(codon)),
      m_AA(GetAAName(aa, is_ascii)), m_Checked(true)
{
}


void CFlatCodonQVal::Format(TFlatQuals& q, const string& name, CBioseqContext& ctx,
                          IFlatQVal::TFlags) const
{
    if ( !m_Checked ) {
        // ...
    }
    x_AddFQ(q, name, "(seq:\"" + m_Codon + "\",aa:" + m_AA + ')');
}


void CFlatExpEvQVal::Format(TFlatQuals& q, const string& name,
                          CBioseqContext&, IFlatQVal::TFlags) const
{
    const char* s = 0;
    switch (m_Value) {
    case CSeq_feat::eExp_ev_experimental:      s = "experimental";      break;
    case CSeq_feat::eExp_ev_not_experimental:  s = "not_experimental";  break;
    default:                                   break;
    }
    if (s) {
        x_AddFQ(q, name, s, CFormatQual::eUnquoted);
    }
}


void CFlatIllegalQVal::Format(TFlatQuals& q, const string&, CBioseqContext &ctx,
                            IFlatQVal::TFlags) const
{
    // XXX - return if too strict
    x_AddFQ(q, m_Value->GetQual(), m_Value->GetVal());
}


void CFlatMolTypeQVal::Format(TFlatQuals& q, const string& name,
                            CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    const char* s = 0;
    switch ( m_Biomol ) {
    case CMolInfo::eBiomol_unknown:
        switch ( m_Mol ) {
        case CSeq_inst::eMol_dna:  s = "unassigned DNA"; break;
        case CSeq_inst::eMol_rna:  s = "unassigned RNA"; break;
        default:                   break;
        }
        break;
    case CMolInfo::eBiomol_genomic:
        switch ( m_Mol ) {
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
        switch ( m_Mol ) {
        case CSeq_inst::eMol_dna:  s = "other DNA";  break;
        case CSeq_inst::eMol_rna:  s = "other RNA";  break;
        default:                   break;
        }
        break;
    case CMolInfo::eBiomol_cRNA:
    case CMolInfo::eBiomol_transcribed_RNA:  s = "other RNA";  break;
    case CMolInfo::eBiomol_snoRNA:           s = "snoRNA";     break;
    }

    if ( s == 0 ) {
        switch ( m_Mol ) {
        case CSeq_inst::eMol_rna:
            s = "unassigned RNA";
            break;
        case CSeq_inst::eMol_aa:
            s = 0;
            break;
        case CSeq_inst::eMol_dna:
        default:
            s = "unassigned DNA";
            break;
        }
    }

    if ( s != 0 ) {
        x_AddFQ(q, name, s);
    }
}


void CFlatOrgModQVal::Format(TFlatQuals& q, const string& name,
                           CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    string subname = m_Value->GetSubname();
    if ( s_StringIsJustQuotes(subname) ) {
        subname = kEmptyStr;
    }
    if ( subname.empty() ) {
        return;
    }
    ConvertQuotes(subname);

    if ( s_IsNote(flags) ) {
        m_Suffix = (m_Value->GetSubtype() == COrgMod::eSubtype_other ? &kEOL : &kSemicolon);
        if ( name != "orgmod_note" ) {
            x_AddFQ(q, "note", name + ": " + subname);
        } else {
            x_AddFQ(q, "note", subname);
        }
    } else {
        x_AddFQ(q, name, subname);
    }
}


void CFlatOrganelleQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext&, IFlatQVal::TFlags) const
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
        x_AddFQ(q, organelle, kEmptyStr, CFormatQual::eEmpty);
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
                           CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    ITERATE (vector< CRef<CReferenceItem> >, it, ctx.GetReferences()) {
        if ((*it)->Matches(*m_Value)) {
            x_AddFQ(q, name, '[' + NStr::IntToString((*it)->GetSerial()) + ']',
                    CFormatQual::eUnquoted);
        }
    }
}


void CFlatSeqIdQVal::Format(TFlatQuals& q, const string& name,
                          CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    // XXX - add link in HTML mode
    string id_str;
    if ( m_Value->IsGi() ) {
        if ( m_GiPrefix ) {
            id_str = "GI:";
        }
        m_Value->GetLabel(&id_str, CSeq_id::eContent);
    } else {
        id_str = m_Value->GetSeqIdString(true);
    }
    x_AddFQ(q, name, id_str);
}


void CFlatSubSourceQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    string subname = m_Value->CanGetName() ? m_Value->GetName() : kEmptyStr;
    if ( s_StringIsJustQuotes(subname) ) {
        subname = kEmptyStr;
    }
    ConvertQuotes(subname);

    if ( s_IsNote(flags) ) {
        bool note = false;
        if ( m_Value->GetSubtype() == CSubSource::eSubtype_other ) {
            m_Suffix = &kEOL;
            note = true;
        } else {
            m_Suffix = &kSemicolon;
        }

        x_AddFQ(q, "note", note ? subname : name + ": " + subname);
    } else {
        CSubSource::TSubtype subtype = m_Value->GetSubtype();
        if ( subtype == CSubSource::eSubtype_germline            ||
             subtype == CSubSource::eSubtype_rearranged          ||
             subtype == CSubSource::eSubtype_transgenic          ||
             subtype == CSubSource::eSubtype_environmental_sample ) {
            x_AddFQ(q, name, kEmptyStr, CFormatQual::eEmpty);
        } else {
            x_AddFQ(q, name, subname);
        }
    }
}


void CFlatXrefQVal::Format(TFlatQuals& q, const string& name,
                         CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    // XXX - add link in HTML mode?
    ITERATE (TXref, it, m_Value) {
        if ( !m_Quals.Empty()  &&  x_XrefInGeneXref(**it) ) {
            continue;
        }
        string s((*it)->GetDb());
        const CObject_id& id = (*it)->GetTag();
        switch ( id.Which() ) {
        case CObject_id::e_Id:
            s += ':' + NStr::IntToString(id.GetId());
            break;
        case CObject_id::e_Str:
            s += ':' + id.GetStr();
            break;
        default:
            break;
        }
        x_AddFQ(q, name, s);
    }
}


bool CFlatXrefQVal::x_XrefInGeneXref(const CDbtag& dbtag) const
{
    if ( !m_Quals->HasQual(eFQ_gene_xref) ) {
        return false;
    }

    typedef TQuals::const_iterator TQCI;
    pair<TQCI, TQCI> gxrefs = m_Quals->GetQuals(eFQ_gene_xref);
    for ( TQCI it = gxrefs.first; it != gxrefs.second; ++it ) {
        const CFlatXrefQVal* xrefqv =
            dynamic_cast<const CFlatXrefQVal*>(it->second.GetPointerOrNull());
        if ( xrefqv != 0 ) {
            ITERATE (TXref, dbt, xrefqv->m_Value) {
                if ( dbtag.Match(**dbt) ) {
                    return true;
                }
            }
        }
    }
    return false;
}


void CFlatModelEvQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    const string* method = 0;
    if ( m_Value->HasField("Method") ) {
        const CUser_field& uf = m_Value->GetField("Method");
        if ( uf.GetData().IsStr() ) {
            method = &uf.GetData().GetStr();
        }
    }

    size_t nm = 0;
    if ( m_Value->HasField("mRNA") ) {
        const CUser_field& field = m_Value->GetField("mRNA");
        if ( field.GetData().IsFields() ) {
            ITERATE (CUser_field::C_Data::TFields, it1, field.GetData().GetFields()) {
                const CUser_field& uf = **it1;
                if ( !uf.CanGetData()  ||  !uf.GetData().IsFields() ) {
                    continue;
                }
                ITERATE (CUser_field::C_Data::TFields, it2, uf.GetData().GetFields()) {
                    if ( !(*it2)->CanGetLabel() ) {
                        continue;
                    }
                    const CObject_id& oid = (*it2)->GetLabel();
                    if ( oid.IsStr() ) {
                        if ( oid.GetStr() == "accession" ) {
                            ++nm;
                        }
                    }
                }
            }
        }
    }

    CNcbiOstrstream text;
    text << "Derived by automated computational analysis";
    if ( method != 0  &&  !method->empty() ) {
         text << " using gene prediction method: " << *method;
    }
    text << ".";

    if ( nm > 0 ) {
        text << " Supporting evidence includes similarity to: " << nm << " mRNA";
        if ( nm > 1 ) {
            text << "s";
        }
    }

    x_AddFQ(q, name, CNcbiOstrstreamToString(text));
}


void CFlatGoQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    _ASSERT(m_Value->GetData().IsFields());
    bool is_ftable = ctx.Config().IsFormatFTable();

    if ( s_IsNote(flags) ) {
        static const string sfx = ";";
        m_Prefix = &kEOL;
        m_Suffix = &sfx;
        x_AddFQ(q, "note", name + ": " + s_GetGOText(*m_Value, is_ftable));
    } else {
        x_AddFQ(q, name, s_GetGOText(*m_Value, is_ftable));
    }
}


void CFlatAnticodonQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if ( !m_Anticodon->IsInt()  ||  m_Aa.empty() ) {
        return;
    }

    CSeq_loc::TRange range = m_Anticodon->GetTotalRange();
    CNcbiOstrstream text;
    text << "(pos:" << range.GetFrom() + 1 << ".." << range.GetTo() + 1
         << ",aa:" << m_Aa << ")";

    x_AddFQ(q, name, CNcbiOstrstreamToString(text), CFormatQual::eUnquoted);
}


void CFlatTrnaCodonsQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if (!m_Value  ||  !m_Value->IsSetCodon()) {
        return;
    }

    string recognized;
    size_t num = s_ComposeCodonRecognizedStr(*m_Value, recognized);
    if (num > 0) {
        if (num == 1) {
            x_AddFQ(q, name, "codon recognized: " + recognized);
        } else {
            x_AddFQ(q, name, "codons recognized: " + recognized);
        }
    }
}


void CFlatProductQVal::Format
(TFlatQuals& q,
 const string& n,
 CBioseqContext& ctx,
 TFlags) const
{
    // !!!
    /*
    if ( ctx.DropIllegalQuals() ) {
        if ( !s_LegalQualForFeature(eFQ_product, m_Subtype)
    */
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.16  2004/08/19 16:37:52  shomrat
* + CFlatNumberQVal and CFlatGeneSynonymsQVal
*
* Revision 1.15  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.14  2004/05/06 17:58:52  shomrat
* CFlatQual -> CFormatQual
*
* Revision 1.13  2004/04/27 15:13:16  shomrat
* fixed SubSource formatting
*
* Revision 1.12  2004/04/22 15:57:06  shomrat
* Changes in context
*
* Revision 1.11  2004/04/07 14:28:44  shomrat
* Fixed GO quals formatting for FTable format
*
* Revision 1.10  2004/03/30 20:32:53  shomrat
* Fixed go and modelev qual formatting
*
* Revision 1.9  2004/03/25 20:46:19  shomrat
* implement CFlatPubSetQVal formatting
*
* Revision 1.8  2004/03/18 15:43:27  shomrat
* Fixes to quals formatting
*
* Revision 1.7  2004/03/08 21:01:44  shomrat
* GI prefix flag for Seq-id quals
*
* Revision 1.6  2004/03/08 15:24:27  dicuccio
* FIxed dereference of string pointer
*
* Revision 1.5  2004/03/05 18:47:32  shomrat
* Added qualifier classes
*
* Revision 1.4  2004/02/11 16:55:38  shomrat
* changes in formatting of OrgMod and SubSource quals
*
* Revision 1.3  2004/01/14 16:18:07  shomrat
* uncomment code
*
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
