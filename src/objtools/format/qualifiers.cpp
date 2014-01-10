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
#include <util/sgml_entity.hpp>
#include <serial/enumvalues.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
//#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/format/items/qualifiers.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

const string IFlatQVal::kSpace     = " ";
const string IFlatQVal::kSemicolon = ";";
const string IFlatQVal::kSemicolonEOL = ";\n";
const string IFlatQVal::kComma     = ",";
const string IFlatQVal::kEOL       = "\n";

static bool s_IsNote(IFlatQVal::TFlags flags, CBioseqContext& ctx)
{
    return (flags & IFlatQVal::fIsNote)  &&  !ctx.Config().IsModeDump();
}


static bool s_StringIsJustQuotes(const string& str)
{
    ITERATE(string, it, str) {
        if ( (*it != '"')  &&  (*it != '\'') ) {
            return false;
        }
    }

    return true;
}

static string s_GetGOText(
    const CUser_field& field, 
    const bool is_ftable, 
    const bool is_html)
{
    const string* text_string = NULL,
                * evidence = NULL,
                * go_id = NULL,
                * go_ref = NULL;
    string temp;
    int pmid = 0;
    
    

    ITERATE (CUser_field::C_Data::TFields, it, field.GetData().GetFields()) {
        if ( !(*it)->IsSetLabel()  ||  !(*it)->GetLabel().IsStr() ) {
            continue;
        }
        
        const string& label = (*it)->GetLabel().GetStr();
        const CUser_field::C_Data& data = (*it)->GetData();
        
        if (data.IsStr()) {
            if (label == "text string") {
                text_string = &data.GetStr();
            } else if (label == "go id") {
                go_id = &data.GetStr();
            } else if (label == "evidence") {
                evidence = &data.GetStr();
            } else if (label == "go ref") {
                go_ref = &data.GetStr();
            }
        } else if (data.IsInt()) {
            if (label == "go id") {
                NStr::IntToString(temp, data.GetInt());
                go_id = &temp;
            } else if (label == "pubmed id") {
                pmid = data.GetInt();
            }
        }
    }
    
    string go_text;
    if (text_string != NULL) {
        go_text = *text_string;
    }
    if ( is_ftable ) {
        go_text += '|';
        if (go_id != NULL) {
            go_text += *go_id;
        }
        go_text += '|';
        if ( pmid != 0 ) {
            go_text +=  NStr::IntToString(pmid);
        }
        if (evidence != NULL) {
            go_text += '|';
            go_text += *evidence;
        }
    } else { 
        if (go_id != NULL) {
            go_text = string( "GO:" );
            if( is_html ) {
                go_text += "<a href=\"" + strLinkBaseGeneOntology + *go_id + "\">";
            }
            go_text += *go_id;
            if( is_html ) {
                go_text += "</a>";
            }
        } else {
            go_text.clear();
        }
        if ( text_string != 0 && text_string->length() > 0 ) {
            go_text += string( " - " );
            if( is_html && go_id != NULL ) {
                go_text += "<a href=\"" + strLinkBaseGeneOntology + *go_id + "\">";
            }
            // Yes, we have the dash here even if there's no go_id (compatibility with C)
            go_text += *text_string;
            if( is_html && go_id != NULL ) {
                go_text += "</a>";
            }
        }
        if ( evidence != 0 ) {
            go_text += string( " [Evidence " ) + *evidence + string( "]" );
        }
        if ( pmid != 0 ) {
            string pmid_str = NStr::IntToString(pmid);

            go_text += " [PMID ";
            if( is_html && go_id != NULL ) {
                go_text += "<a href=\"" + strLinkBasePubmed + pmid_str + "\">";
            }
            go_text += pmid_str;
            if( is_html && go_id != NULL ) {
                go_text += "</a>";
            }
            go_text += "]";
        }
        if ( go_ref != 0 ) {
            go_text += " [GO Ref ";
            if( is_html ) {
                go_text += "<a href=\"" + strLinkBaseGeneOntologyRef + *go_ref + "\">";
            }
            go_text += *go_ref;
            if( is_html ) {
                go_text += "</a>";
            }
            go_text += "]";
        }
    }
    NStr::TruncateSpacesInPlace(go_text);
    return go_text;
}


static void s_ReplaceUforT(string& codon)
{
    NON_CONST_ITERATE (string, base, codon) {
        if (*base == 'T') {
            *base = 'U';
        }
    }
}


static char s_MakeDegenerateBase(const string &str1, const string& str2)
{
    static const char kIdxToSymbol[] = "?ACMGRSVUWYHKDBN";
    
    vector<char> symbol_to_idx(256, '\0');
    for (size_t i = 0; i < sizeof(kIdxToSymbol) - 1; ++i) {
        symbol_to_idx[kIdxToSymbol[i]] = i;
    }

    size_t idx = symbol_to_idx[str1[2]] | symbol_to_idx[str2[2]];
    return kIdxToSymbol[idx];
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
        s_ReplaceUforT(codon);
        if (!codon.empty()) {
            codons.push_back(codon);
        }
    }
    if (codons.empty()) {
        return 0;
    }
    size_t size = codons.size();
    if (size > 1) {
        codons.sort();

        list<string>::iterator it = codons.begin();
        list<string>::iterator prev = it++;
        while (it != codons.end()) {
            string& codon1 = *prev;
            string& codon2 = *it;
            if (codon1[0] == codon2[0]  &&  codon1[1] == codon2[1]) {
                codon1[2] = s_MakeDegenerateBase(codon1, codon2);
                it = codons.erase(it);
            } else {
                prev = it;
                ++it;
            }
        }
    }

    recognized = NStr::Join(codons, ", ");
    return size;
}

static bool s_AltitudeIsValid(const string & str )
{
    // this is the regex we're validating, but for speed and library dependency
    // reasons, we're using a more raw approach:
    //
    // ^[+-]?[0-9]+(\.[0-9]+)?[ ]m\.$

    // we use c-style strings so we don't have to worry about constantly
    // checking if we're out of bounds: we get a '\0' which lets us be cleaner
    const char *pch = str.c_str();
    
    // optional sign at start
    if( *pch == '+' || *pch == '-' ) {
        ++pch;
    }

    // then a number, maybe with one decimal point
    if( ! isdigit(*pch) ) {
        return false;
    }
    while( isdigit(*pch) ) {
        ++pch;
    }
    if( *pch == '.' ) {
        ++pch;
        if( ! isdigit(*pch) ) {
            return false;
        }
        while( isdigit(*pch) ) {
            ++pch;
        }
    }

    // should end with " m."
    return NStr::Equal(pch, " m.");
}

/////////////////////////////////////////////////////////////////////////////
// CFormatQual - low-level formatted qualifier

CFormatQual::CFormatQual
(const string& name,
 const string& value, 
 const string& prefix,
 const string& suffix,
 TStyle style,
 TFlags flags,
 ETrim trim ) :
    m_Name(name), m_Value(value), m_Prefix(prefix), m_Suffix(suffix),
    m_Style(style), m_Flags(flags), m_Trim(trim), m_AddPeriod(false)
{
    NStr::TruncateSpacesInPlace(m_Value, NStr::eTrunc_End);
}


CFormatQual::CFormatQual(const string& name, const string& value, TStyle style, TFlags flags, ETrim trim) :
    m_Name(name), m_Value(value), m_Prefix(" "), m_Suffix(kEmptyStr),
    m_Style(style), m_Flags(flags), m_Trim(trim), m_AddPeriod(false)
{
    NStr::TruncateSpacesInPlace(m_Value, NStr::eTrunc_End);
}


// === CFlatStringQVal ======================================================

CFlatStringQVal::CFlatStringQVal(const string& value, TStyle style, ETrim trim)
    :  IFlatQVal(&kSpace, &kSemicolon),
       m_Value(value), m_Style(style), m_Trim(trim), m_AddPeriod(0)
{
    NStr::TruncateSpacesInPlace(m_Value);
}


CFlatStringQVal::CFlatStringQVal
(const string& value,
 const string& pfx,
 const string& sfx,
 TStyle style,
 ETrim trim)
    :   IFlatQVal(&pfx, &sfx),
        m_Value(value),
        m_Style(style), m_Trim(trim), m_AddPeriod(0)
{
    NStr::TruncateSpacesInPlace(m_Value);
}

CFlatStringQVal::CFlatStringQVal(const string& value, 
    ETrim trim )
:   IFlatQVal(&kSpace, &kSemicolon),
    m_Value(value), m_Style(CFormatQual::eQuoted), m_Trim(trim), m_AddPeriod(0)
{
    NStr::TruncateSpacesInPlace(m_Value);
}


typedef SStaticPair<const char*, ETildeStyle> TNameTildeStylePair;
typedef CStaticPairArrayMap<const char*, ETildeStyle, PCase_CStr > TNameTildeStyleMap;
static const TNameTildeStylePair kNameTildeStyleMap[] = {
    { "function",     eTilde_tilde },
    { "prot_desc",    eTilde_note },
    { "prot_note",    eTilde_note },
    { "seqfeat_note", eTilde_note }
};
DEFINE_STATIC_ARRAY_MAP(TNameTildeStyleMap, sc_NameTildeStyleMap, kNameTildeStyleMap);

// a few kinds don't use the default tilde style
ETildeStyle s_TildeStyleFromName( const string &name )
{
    TNameTildeStyleMap::const_iterator result = sc_NameTildeStyleMap.find( name.c_str() ); 
    if( sc_NameTildeStyleMap.end() == result ) {
        return eTilde_space;
    } else {
        return result->second;
    }
}

void CFlatStringQVal::Format(TFlatQuals& q, const string& name,
                           CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    bool bHtml = ctx.Config().DoHTML();
    if ( bHtml && name == "EC_number" ) {
        string strLink = "<a href=\"";
        strLink += strLinkBaseExpasy;
        strLink += m_Value;
        strLink += "\">";
        strLink += m_Value;
        strLink += "</a>";
        x_AddFQ(q, name, strLink, m_Style, 0, m_Trim);
        return;
    }
    flags |= m_AddPeriod;

    ETildeStyle tilde_style = s_TildeStyleFromName( name );
    ExpandTildes(m_Value, tilde_style);
                
    const bool is_note = s_IsNote(flags, ctx);

    // e.g. CP001398
    // if( ! is_note ) {
    // e.g. NP_008173
    if( m_Style != CFormatQual::eUnquoted ) {
        ConvertQuotesNotInHTMLTags( m_Value );
    }
    // }

    // eventually, the last part will be replaced with a hash-table
    // or something, but this is fine while we're only checking for
    // one type.
    // This prevents quals like /metagenomic="metagenomic" (e.g. EP508672)
    const bool forceNoValue = ( 
        ! ctx.Config().SrcQualsToNote() &&
        name == m_Value && 
        name == "metagenomic" );

    const bool prependNewline = (flags & fPrependNewline) && ! q.empty();
    TFlatQual qual = x_AddFQ(q, (is_note ? "note" : name), 
        (  prependNewline ? "\n" + m_Value : m_Value ), 
        ( forceNoValue ? CFormatQual::eEmpty : m_Style ),
        0, m_Trim );
    
    if ((flags & fAddPeriod)  &&  qual) {
        qual->SetAddPeriod();
    }
}


// === CFlatNumberQVal ======================================================


void CFlatNumberQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    if (ctx.Config().CheckQualSyntax()) {
        if (NStr::IsBlank(m_Value)) {
            return;
        }
        bool has_space = false;
        ITERATE (string, it, m_Value) {
            if (isspace((unsigned char)(*it))) {
                has_space = true;
            } else if (has_space) {
                // non-space after space
                return;
            }
        }
    }

    CFlatStringQVal::Format(quals, name, ctx, flags);
}


// === CFlatBondQVal ========================================================

void CFlatBondQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    string value = m_Value;
    if (s_IsNote(flags, ctx)) {
        value += " bond";
    }
    x_AddFQ(quals, (s_IsNote(flags, ctx) ? "note" : name), value, m_Style);
}


// === CFlatGeneQVal ========================================================

void CFlatGeneQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    CFlatStringQVal::Format(quals, name, ctx, flags);
}


// === CFlatSiteQVal ========================================================

void CFlatSiteQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    if ( m_Value == "transmembrane-region" ) {
        m_Value = "transmembrane region";
    }
    if ( m_Value == "signal-peptide" ) {
        m_Value = "signal peptide";
    }
    if ( m_Value == "transit-peptide" ) {
        m_Value = "transit peptide";
    }
    if (m_Value != "transit peptide" && m_Value != "signal peptide" &&
        m_Value != "transmembrane region" && s_IsNote(flags, ctx)) 
    {
        const static char *pchSiteSuffix = " site";
        if( ! NStr::EndsWith(m_Value, pchSiteSuffix) ) {
            m_Value += pchSiteSuffix;
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
    if (m_Value.empty()) {
        return;
    }

    if ( s_IsNote(flags, ctx) ) {
        m_Suffix = &kSemicolon;
    }

    x_AddFQ(q, 
            (s_IsNote(flags, ctx) ? "note" : name),
            JoinString(m_Value, "; "),
            m_Style);
}


// === CFlatGeneSynonymsQVal ================================================

class CLessThanNoCaseViaUpper {
public:
    bool operator()( const string &str1, const string &str2 ) {
        // C++'s built-in stuff compares via "tolower" which gets a different ordering
        // in some subtle cases than C's no-case comparison which uses "toupper"
        SIZE_TYPE pos = 0;
        const SIZE_TYPE min_length = min( str1.length(), str2.length() );
        for( ; pos < min_length; ++pos ) {
            const char textComparison = toupper( str1[pos] ) - toupper( str2[pos] );
            if( textComparison != 0 ) {
                return textComparison < 0;
            }
        }
        // if we reached the end, compare via length (shorter first)
        return ( str1.length() < str2.length() );
    }
};

void CFlatGeneSynonymsQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if (GetValue().empty()) {
        return;
    }

    string qual = "gene_synonym";
    const list<string> &synonyms = GetValue();
    vector<string> sub;
    std::copy(synonyms.begin(), synonyms.end(),
              back_inserter(sub));

    // For compatibility with C, we use a slightly different sorting algo
    // In the future, we might go back to the other one for simplicity
    // std::sort(sub.begin(), sub.end(), PNocase());
    stable_sort(sub.begin(), sub.end(), CLessThanNoCaseViaUpper() );

    if (ctx.IsRefSeq()) {
        x_AddFQ( q, qual, NStr::Join(sub, "; "), m_Style, 0, CFormatQual::eTrim_WhitespaceOnly );
    } else {
        ITERATE (vector<string>, it, sub) {
            x_AddFQ( q, qual, *it, m_Style, 0, CFormatQual::eTrim_WhitespaceOnly );
        }
    }
}

// === CFlatCodeBreakQVal ===================================================

void CFlatCodeBreakQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    static const char* kOTHER = "OTHER";

    ITERATE (CCdregion::TCode_break, it, m_Value) {
        const char* aa  = kOTHER;
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

        string pos = CFlatSeqLoc( (*it)->GetLoc(), ctx).GetString();
        x_AddFQ(q, name, "(pos:" + pos + ",aa:" + aa + ')', 
            CFormatQual::eUnquoted);
    }
}

void CFlatNomenclatureQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    if( m_Value.IsNull() ) {
        return;
    }

    if( ! m_Value->CanGetStatus() || ! m_Value->CanGetSymbol() || m_Value->GetSymbol().empty() ) {
        return;
    }

    // we build up the final result in this variable
    string nomenclature; 

    // add the status part
    switch( m_Value->GetStatus() ) {
        case CGene_nomenclature::eStatus_official:
            nomenclature += "Official ";
            break;
        case CGene_nomenclature::eStatus_interim:
            nomenclature += "Interim ";
            break;
        default:
            nomenclature += "Unclassified ";
            break;
    }
    nomenclature += "Symbol: ";

    // add the symbol part
    nomenclature += m_Value->GetSymbol();

    // add the name part, if any
    if( m_Value->CanGetName() && ! m_Value->GetName().empty() ) {
        nomenclature += " | Name: " + m_Value->GetName();
    }

    // add the source part, if any
    if( m_Value->CanGetSource() ) {
        const CGene_nomenclature_Base::TSource& source = m_Value->GetSource();
        
        if( source.CanGetDb() && ! source.GetDb().empty() && source.CanGetTag() ) {
            if( source.GetTag().IsId() || ( source.GetTag().IsStr() && ! source.GetTag().GetStr().empty() ) ) {
                nomenclature += " | Provided by: " + source.GetDb() + ":";
                if( source.GetTag().IsStr() ) {
                    nomenclature += source.GetTag().GetStr();
                } else {
                    nomenclature += NStr::IntToString( source.GetTag().GetId() );
                }
            }
        }
    }

    x_AddFQ(q, name, nomenclature, CFormatQual::eQuoted );
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

CFlatExperimentQVal::CFlatExperimentQVal(
    const string& value )
    : m_str( value ) 
{
    if ( m_str.empty() ) {
        m_str = "experimental evidence, no additional details recorded";
    }
}

void CFlatExperimentQVal::Format(TFlatQuals& q, const string& name,
                          CBioseqContext&, IFlatQVal::TFlags) const
{
    x_AddFQ(q, name, m_str.c_str(), CFormatQual::eQuoted);
}


CFlatInferenceQVal::CFlatInferenceQVal( const string& gbValue ) :
    m_str( "non-experimental evidence, no additional details recorded" )
{
    //
    //  the initial "non-experimental ..." is just a default value which may be
    //  overridden through an additional "inference" Gb-qual in the ASN.1.
    //  However, it can't be overriden to be just anything, only certain strings
    //  are allowed.
    //  The following code will change m_str from its default if gbValue is a
    //  legal replacement for "non-experimental ...", and leave it alone 
    //  otherwise.
    //
    string prefix = "";
    string remainder = "";
    CInferencePrefixList::GetPrefixAndRemainder (gbValue, prefix, remainder);
    if (!NStr::IsBlank(prefix)) {
        m_str = gbValue;
    }
}


void CFlatInferenceQVal::Format(TFlatQuals& q, const string& name,
                          CBioseqContext&, IFlatQVal::TFlags) const
{
    x_AddFQ(q, name, m_str, CFormatQual::eQuoted);
}


void CFlatIllegalQVal::Format(TFlatQuals& q, const string&, CBioseqContext &ctx,
                            IFlatQVal::TFlags) const
{
    // XXX - return if too strict

    // special case: in sequin mode, orig_protein_id and orig_transcript_id removed,
    // even though we usually keep illegal quals in sequin mode
    if( m_Value->GetQual() == "orig_protein_id" || 
        m_Value->GetQual() == "orig_transcript_id" )
    {
        return;
    }
        
    x_AddFQ(q, m_Value->GetQual(), m_Value->GetVal());
}


void CFlatMolTypeQVal::Format(TFlatQuals& q, const string& name,
                            CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    const char* s = 0;
    switch ( m_Biomol ) {

    default:
        break;

    case CMolInfo::eBiomol_unknown:
        switch ( m_Mol ) {
        case CSeq_inst::eMol_dna:  
            s = "unassigned DNA"; 
            break;
        case CSeq_inst::eMol_rna:  
            s = "unassigned RNA"; 
            break;
        default:                   
            break;
        }
        break;

    case CMolInfo::eBiomol_genomic:
        switch ( m_Mol ) {
        case CSeq_inst::eMol_dna:  
            s = "genomic DNA";  
            break;
        case CSeq_inst::eMol_rna:  
            s = "genomic RNA";  
            break;
        default:                   
            break;
        }
        break;

    case CMolInfo::eBiomol_mRNA:     
        s = "mRNA";      
        break;

    case CMolInfo::eBiomol_rRNA:     
        s = "rRNA";      
        break;
    
    case CMolInfo::eBiomol_tRNA:     
        s = "tRNA";      
        break;

    case CMolInfo::eBiomol_pre_RNA:  
    case CMolInfo::eBiomol_snRNA:    
    case CMolInfo::eBiomol_scRNA:    
    case CMolInfo::eBiomol_snoRNA:
    case CMolInfo::eBiomol_ncRNA:
    case CMolInfo::eBiomol_tmRNA:
    case CMolInfo::eBiomol_transcribed_RNA:
        s = "transcribed RNA";     
        break;

    case CMolInfo::eBiomol_other_genetic:
    case CMolInfo::eBiomol_other:
        switch ( m_Mol ) {
        case CSeq_inst::eMol_dna:  
            s = "other DNA";  
            break;
        case CSeq_inst::eMol_rna:  
            s = "other RNA";  
            break;
        default:                   
            break;
        }
        break;

    case CMolInfo::eBiomol_cRNA:
        s = "viral cRNA";     
        break;

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
    TFlatQual qual;

    string subname = m_Value->GetSubname();
    if ( s_StringIsJustQuotes(subname) ) {
        subname = kEmptyStr;
    }
    ConvertQuotesNotInHTMLTags(subname);
    ExpandTildes(subname, ( (flags & IFlatQVal::fIsNote) != 0  ? eTilde_note : eTilde_space ) );
    
    if (s_IsNote(flags, ctx)) {
        bool add_period = RemovePeriodFromEnd(subname, true);
        if (!subname.empty() || add_period ) {
            bool is_src_orgmod_note = 
                (flags & IFlatQVal::fIsSource)  &&  (name == "orgmod_note");
            if (is_src_orgmod_note) {
                if (add_period) {
                    AddPeriod(subname);
                }
                m_Prefix = &kEOL;
                m_Suffix = ( add_period ? &kEOL : &kSemicolonEOL );
                qual = x_AddFQ(q, "note", subname);
            } else {
                qual = x_AddFQ(q, "note", name + ": " + subname, 
                    CFormatQual::eQuoted, CFormatQual::fFlags_showEvenIfRedund );
            }
            if (add_period  &&  qual) {
                qual->SetAddPeriod();
            }
        }
    } else {
        // TODO: consider adding this back when we switch to just C++
        // x_AddFQ(q, name, s_GetSpecimenVoucherText(ctx, subname) );
        x_AddFQ(q, name, subname );
    }
}


void CFlatOrganelleQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext&, IFlatQVal::TFlags) const
{
    const string& organelle
        = CBioSource::GetTypeInfo_enum_EGenome()->FindName(m_Value, true);

    switch (m_Value) {
    case CBioSource::eGenome_chloroplast: 
    case CBioSource::eGenome_chromoplast:
    case CBioSource::eGenome_cyanelle:    
    case CBioSource::eGenome_apicoplast:
    case CBioSource::eGenome_leucoplast:  
    case CBioSource::eGenome_proplastid:
        x_AddFQ(q, name, "plastid:" + organelle);
        break;

    case CBioSource::eGenome_kinetoplast:
        x_AddFQ(q, name, "mitochondrion:kinetoplast");
        break;

    case CBioSource::eGenome_mitochondrion: 
    case CBioSource::eGenome_plastid:
    case CBioSource::eGenome_nucleomorph:
    case CBioSource::eGenome_hydrogenosome:
    case CBioSource::eGenome_chromatophore:
        x_AddFQ(q, name, organelle);
        break;

    case CBioSource::eGenome_macronuclear: 
    case CBioSource::eGenome_proviral:
        x_AddFQ(q, organelle, kEmptyStr, CFormatQual::eEmpty);
        break;
    case CBioSource::eGenome_virion:
//        x_AddFQ(q, organelle, kEmptyStr, CFormatQual::eEmpty);
        break;

    case CBioSource::eGenome_plasmid: 
    case CBioSource::eGenome_transposon:
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
    const bool bHtml = ctx.Config().DoHTML();

    if( ! m_Value->IsPub() ) {
        return; // TODO: is this right?
    }

    // copy the list
    list< CRef< CPub > > unusedPubs = m_Value->GetPub();

    if( ctx.GetReferences().empty() ) {
        // Yes, this even skips creating "/citation=[PUBMED ...]" items
        return;
    }

    ITERATE (vector< CRef<CReferenceItem> >, ref_iter, ctx.GetReferences()) {
        CPub_set_Base::TPub::iterator pub_iter = unusedPubs.begin();
        for( ; pub_iter != unusedPubs.end() ; ++pub_iter ) {
            if( (*ref_iter)->Matches( **pub_iter ) ) {
                // We have a match, so create the qual
                string value;
                if( (*ref_iter)->GetPMID() > 0 && bHtml ) {
                    // create a link
                    const int pmid = (*ref_iter)->GetPMID();
                    value = "[<a href=\"" + strLinkBasePubmed + NStr::IntToString(pmid) + "\">" + 
                        NStr::IntToString((*ref_iter)->GetSerial()) + 
                        "</a>]";
                } else {
                    value = '[' + NStr::IntToString((*ref_iter)->GetSerial()) + ']';
                }
                x_AddFQ(q, name, value, CFormatQual::eUnquoted);
                
                pub_iter = unusedPubs.erase( pub_iter ); // only one citation should be created per reference
                break; // break so we don't show the same ref more than once
            }
        }
    }

    // out of the pubs which are still unused, we may still be able to salvage some 
    // under certain conditions.
    if( ctx.IsRefSeq() && ! ctx.Config().IsModeRelease() ) {
        CPub_set_Base::TPub::iterator pub_iter = unusedPubs.begin();
        for( ; pub_iter != unusedPubs.end() ; ++pub_iter ) {
            if( (*pub_iter)->IsPmid() ) {
                const int pmid = (*pub_iter)->GetPmid().Get();

                CNcbiOstrstream pubmed;
                pubmed << "[PUBMED ";
                if( bHtml ) {
                    pubmed << "<a href=\"" << strLinkBasePubmed << pmid << "\">";
                }
                pubmed << pmid;
                if( bHtml ) {
                    pubmed << "</a>";
                }
                pubmed << ']';

                x_AddFQ(q, name, CNcbiOstrstreamToString(pubmed),
                    CFormatQual::eUnquoted);
            }
        }
    }
}

void CFlatIntQVal::Format(TFlatQuals& q, const string& name, 
                          CBioseqContext& ctx, TFlags) const
{ 
    bool bHtml = ctx.Config().DoHTML();

    string value = NStr::IntToString(m_Value);
    if ( bHtml && name == "transl_table" ) {
        string link = "<a href=\"";
        link += strLinkBaseTransTable;
        link += value;
        link += "\">";
        link += value;
        link += "</a>";
        value = link;
    }
    x_AddFQ( q, name, value, CFormatQual::eUnquoted); 
}


void CFlatSeqIdQVal::Format(TFlatQuals& q, const string& name,
                            CBioseqContext& ctx, IFlatQVal::TFlags) const
{
    bool bHtml = ctx.Config().DoHTML();

    string id_str;
    if ( m_Value->IsGi() ) {
        if ( m_GiPrefix ) {
            id_str = "GI:";
        }
        m_Value->GetLabel(&id_str, CSeq_id::eContent);
    } else {
        id_str = m_Value->GetSeqIdString(true);
    }

    if ( bHtml && name == "protein_id" ) {
        string raw_id_str = id_str;
        string raw_link_str = id_str;
        CBioseq_Handle bsh = ctx.GetScope().GetBioseqHandle( *m_Value );
        vector< CSeq_id_Handle > ids = bsh.GetId();
        ITERATE( vector< CSeq_id_Handle >, it, ids ) {
            CSeq_id_Handle hid = *it;
            if ( hid.IsGi() ) {
                raw_link_str = NStr::NumericToString(hid.GetGi());
                break;
            }
        }
        id_str = "<a href=\"";
        id_str += strLinkBaseProt;
        id_str += raw_link_str;
        id_str += "\">";
        id_str += raw_id_str;
        id_str += "</a>";
    }
    x_AddFQ(q, name, id_str);
}


void s_ConvertGtLt(string& subname)
{
    SIZE_TYPE pos;
    for (pos = subname.find('<'); pos != NPOS; pos = subname.find('<', pos)) {
        subname.replace(pos, 1, "&lt;");
    }
    for (pos = subname.find('>'); pos != NPOS; pos = subname.find('>', pos)) {
        subname.replace(pos, 1, "&gt;");
    }
}

void s_HtmlizeLatLon( string &subname ) {
    string lat;
    string north_or_south;
    string lon;
    string east_or_west;

    // extract the pieces
    CNcbiIstrstream lat_lon_stream( subname.c_str() );
    lat_lon_stream >> lat;
    lat_lon_stream >> north_or_south;
    lat_lon_stream >> lon;
    lat_lon_stream >> east_or_west;
    if( lat_lon_stream.bad() ) {
        return;
    }

    if( north_or_south != "N" && north_or_south != "S" ) {
        return;
    }

    if( east_or_west != "E" && east_or_west != "W" ) {
        return;
    }

    // see if lat and lon make numerical sense
    try {
        double lat_num = NStr::StringToDouble( lat );
        double lon_num = NStr::StringToDouble( lon );

        // cap ranges
        if( lon_num < -180.0 ) {
            lon = "-180";
        } else if( lon_num > 180.0 ) {
            lon = "180";
        }
        if( lat_num < -90.0 ) {
            lat = "-90";
        } else if( lat_num > 90.0 ) {
            lat = "90";
        }
    } catch( CStringException & ) {
        // error parsing numbers
        return;
    }

    // negate the numbers if we're west of prime meridian or south of equator
    if( east_or_west == "W" && ! NStr::StartsWith(lon, "-") ) {
        lon = "-" + lon;
    }
    if( north_or_south == "S" && ! NStr::StartsWith(lat, "-")) {
        lat = "-" + lat;
    }

    // now we can form the HTML
    CNcbiOstrstream result;
    result << "<a href=\"" << strLinkBaseLatLon << "?lat=" 
        << lat 
        << "&amp;lon=" 
        << lon 
        << "\">"
        << subname << "</a>";
    subname = CNcbiOstrstreamToString(result);
}

void CFlatSubSourcePrimer::Format(
    TFlatQuals& q, 
    const string& name,
    CBioseqContext& ctx, 
    IFlatQVal::TFlags flags) const
{
    vector< string > fwd_names;
    if ( ! m_fwd_name.empty() ) {
        string fwd_name = m_fwd_name;
        if ( NStr::StartsWith( m_fwd_name, "(" ) && NStr::EndsWith( m_fwd_name, ")" ) ) {
            fwd_name = m_fwd_name.substr( 1, m_fwd_name.size() - 2 );
        }
        NStr::Tokenize( fwd_name, ",", fwd_names );
    }
    
    vector< string > rev_names;
    if ( ! m_rev_name.empty() ) {
        string rev_name = m_rev_name;
        if ( NStr::StartsWith( m_rev_name, "(" ) && NStr::EndsWith( m_rev_name, ")" ) ) {
            rev_name = m_rev_name.substr( 1, m_rev_name.size() - 2 );
        }
        NStr::Tokenize( rev_name, ",", rev_names );
    }

    vector< string > fwd_seqs;
    if ( ! m_fwd_seq.empty() ) {
        string fwd_seq = NStr::Replace( m_fwd_seq, "(", "" );
        NStr::ReplaceInPlace( fwd_seq, ")", "" );
        NStr::Tokenize( fwd_seq, ",", fwd_seqs );
    }
    if ( fwd_seqs.empty() ) {
        return;
    }

    vector< string > rev_seqs;
    if ( ! m_rev_seq.empty() ) {
        string rev_seq = NStr::Replace( m_rev_seq, "(", "" );
        NStr::ReplaceInPlace( rev_seq, ")", "" );
        NStr::Tokenize( rev_seq, ",", rev_seqs );
    }

    for ( size_t i=0; i < fwd_seqs.size(); ++i ) {

        string value;
        string sep = "";
        if ( i < fwd_names.size() ) {
            value += sep + "fwd_name: ";
            value += fwd_names[i];
            sep = ", ";
        }
        if( i < fwd_seqs.size() ) {
            value += sep + "fwd_seq: ";
            value += fwd_seqs[i];
            sep = ", ";
        }
        if ( i < rev_names.size() ) {
            value += sep + "rev_name: ";
            value += rev_names[i];
            sep = ", ";
        }
        if( i < rev_seqs.size() ) {
            value += sep + "rev_seq: ";
            value += rev_seqs[i];
            sep = ", ";
        }
        x_AddFQ( q, "PCR_primers", value );
    }
}

void CFlatSubSourceQVal::Format(TFlatQuals& q, const string& name,
                              CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    TFlatQual qual;
    string subname = m_Value->CanGetName() ? m_Value->GetName() : kEmptyStr;
    if ( s_StringIsJustQuotes(subname) ) {
        subname = kEmptyStr;
    }
    ConvertQuotes(subname);
    if (ctx.Config().DoHTML()) {
        s_ConvertGtLt(subname);
    }

    if ( s_IsNote(flags, ctx) ) {
        bool add_period = RemovePeriodFromEnd(subname, true);
        if (!subname.empty()) {
            bool is_subsource_note =
                m_Value->GetSubtype() == CSubSource::eSubtype_other;
            if (is_subsource_note) {
                if (add_period) {
                    AddPeriod(subname);
                }
                m_Suffix = ( add_period ? &kEOL : &kSemicolonEOL );
                qual = x_AddFQ(q, "note", subname);
            } else {
                qual = x_AddFQ(q, "note", name + ": " + subname);        
            }
            if (add_period  &&  qual) {
                qual->SetAddPeriod();
            }
        }
    } else {
        CSubSource::TSubtype subtype = m_Value->GetSubtype();
        switch( subtype ) {

        case CSubSource::eSubtype_germline:
        case CSubSource::eSubtype_rearranged:
        case CSubSource::eSubtype_transgenic:
        case CSubSource::eSubtype_environmental_sample:
        case CSubSource::eSubtype_metagenomic:
            x_AddFQ(q, name, kEmptyStr, CFormatQual::eEmpty);
            break;

        case CSubSource::eSubtype_plasmid_name:
            ExpandTildes(subname, eTilde_space);
            x_AddFQ(q, name, subname);
            break;

        case CSubSource::eSubtype_lat_lon:
            if( ctx.Config().DoHTML() ) {
                s_HtmlizeLatLon(subname);
            }
            ExpandTildes(subname, eTilde_space);
            x_AddFQ(q, name, subname);
            break;

        case CSubSource::eSubtype_altitude:
            // skip invalid /altitudes
            if( s_AltitudeIsValid(subname) || 
                ( ! ctx.Config().IsModeRelease() && ! ctx.Config().IsModeEntrez() ) ) 
            {
                x_AddFQ(q, name, subname);
            }
            break;

        default:
            if ( ! subname.empty() ) {
                ExpandTildes(subname, eTilde_space);
                x_AddFQ(q, name, subname);
            }
            break;
        }
    }
}


void CFlatXrefQVal::Format(TFlatQuals& q, const string& name,
                         CBioseqContext& ctx, IFlatQVal::TFlags flags) const
{
    // to avoid duplicates, keep track of ones we've already done
    set<string> quals_already_done;

    ITERATE (TXref, it, m_Value) {
        const CDbtag& dbt = **it;
        if (!m_Quals.Empty()  &&  x_XrefInGeneXref(dbt)) {
            continue;
        }

        CDbtag::TDb db = dbt.GetDb();
        if (db == "PID"  ||  db == "GI") {
            continue;
        }

        if( db == "taxon" &&  (flags & fIsSource) == 0 ) {
            continue;
        }

        if (db == "cdd") {
            db = "CDD"; // canonicalize
        }

        if (ctx.Config().DropBadDbxref()) {

            // Special case for EST or GSS: we don't filter the dbtags as toughly
            CDbtag::EIsEstOrGss is_est_or_gss = CDbtag::eIsEstOrGss_No;
            const CMolInfo* mol_info = ctx.GetMolinfo();
            if( NULL != mol_info && mol_info->CanGetTech() ) {
                if( mol_info->GetTech() == CMolInfo::eTech_est || mol_info->GetTech() == CMolInfo::eTech_survey ) {
                    is_est_or_gss = CDbtag::eIsEstOrGss_Yes;
                }
            }

            const CDbtag::EIsRefseq is_refseq = ( ctx.IsRefSeq() ? CDbtag::eIsRefseq_Yes : CDbtag::eIsRefseq_No );
            const CDbtag::EIsSource is_source = ( (flags & IFlatQVal::fIsSource) ? CDbtag::eIsSource_Yes : CDbtag::eIsSource_No );
            if (!dbt.IsApproved( is_refseq, is_source, is_est_or_gss )) {
                continue;
            }
        }

        const CDbtag::TTag& tag = (*it)->GetTag();
        string id;
        if (tag.IsId()) {
            id = NStr::IntToString(tag.GetId());
        } else if (tag.IsStr()) {        
            id = tag.GetStr();
            if (NStr::EqualNocase(db, "MGI")  ||  NStr::EqualNocase(db, "MGD")) {
                if (NStr::StartsWith(id, "MGI:", NStr::eNocase)  ||
                    NStr::StartsWith(id, "MGD:", NStr::eNocase)) {
                    db = "MGI";
                    id.erase(0, 4);
                }
            }
            // trim
            TrimSpacesAndJunkFromEnds( id, true );
        }
        if (NStr::IsBlank(id)) {
            continue;
        }

        CNcbiOstrstream db_xref;
        db_xref << db << ':';
        if (ctx.Config().DoHTML()) {
            string url = dbt.GetUrl( ctx.GetTaxname() );
            if (!NStr::IsBlank(url)) {
                db_xref <<  "<a href=\"" << url << "\">" << id << "</a>";
            } else {
                db_xref << id;
            }
        } else {
            db_xref << id;
        }

        // add quals not already done
        string db_xref_str = CNcbiOstrstreamToString(db_xref);
        if( quals_already_done.find(db_xref_str) == quals_already_done.end() ) {
            quals_already_done.insert( db_xref_str );
            x_AddFQ(q, name, db_xref_str);
        }
    }
}


bool CFlatXrefQVal::x_XrefInGeneXref(const CDbtag& dbtag) const
{
    if ( !m_Quals->HasQual(eFQ_gene_xref) ) {
        return false;
    }

    typedef TQuals::const_iterator TQCI;
    TQCI gxref = m_Quals->LowerBound(eFQ_gene_xref);
    TQCI end = m_Quals->end();
    while (gxref != end  &&  gxref->first == eFQ_gene_xref) {
        const CFlatXrefQVal* xrefqv =
            dynamic_cast<const CFlatXrefQVal*>(gxref->second.GetPointerOrNull());
        if (xrefqv != NULL) {
            ITERATE (TXref, dbt, xrefqv->m_Value) {
                if (dbtag.Match(**dbt)) {
                    return true;
                }
            }
        }
        ++gxref;
    }
    /*pair<TQCI, TQCI> gxrefs = m_Quals->GetQuals(eFQ_gene_xref);
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
    }*/
    return false;
}


static size_t s_CountAccessions(const CUser_field& field)
{
    size_t count = 0;

    if (!field.IsSetData()  ||  !field.GetData().IsFields()) {
        return 0;
    }
    
    //
    //  Each accession consists of yet another block of "Fields" one of which carries
    //  a label named "accession":
    //
    ITERATE (CUser_field::TData::TFields, it, field.GetData().GetFields()) {
        const CUser_field& uf = **it;
        if ( uf.CanGetData()  &&  uf.GetData().IsFields() ) {
        
            ITERATE( CUser_field::TData::TFields, it2, uf.GetData().GetFields() ) {
                const CUser_field& inner = **it2;
                if ( inner.IsSetLabel() && inner.GetLabel().IsStr() ) {
                    if ( inner.GetLabel().GetStr() == "accession" ) {
                        ++count;
                    }
                }
            }
        }
    }
    return count;
}


void CFlatModelEvQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    size_t num_mrna = 0, num_prot = 0, num_est = 0, num_long_sra = 0;
    size_t rnaseq_base_coverage = 0, rnaseq_biosamples_introns_full = 0;
    const string* method = 0;

    ITERATE (CUser_object::TData, it, m_Value->GetData()) {
        const CUser_field& field = **it;
        if (! field.IsSetLabel()  || !  field.GetLabel().IsStr()) {
            continue;
        }
        const string& label = field.GetLabel().GetStr();
        if (label == "Method") {
            method = &label;
            if ( field.CanGetData() && field.GetData().IsStr() ) {
                method = &( field.GetData().GetStr() );
            }
        } else if (label == "Counts") {
            ITERATE (CUser_field::TData::TFields, i, field.GetData().GetFields()) {
                if (!(*i)->IsSetLabel()  &&  (*i)->GetLabel().IsStr()) {
                    continue;
                }
                const string& label = (*i)->GetLabel().GetStr();
                if (label == "mRNA") {
                    num_mrna = (*i)->GetData().GetInt();
                } else if (label == "EST") {
                    num_est  = (*i)->GetData().GetInt();
                } else if (label == "Protein") {
                    num_prot = (*i)->GetData().GetInt();
                } else if (label == "long SRA read") {
                    num_long_sra = (*i)->GetData().GetInt();
                }
            }
        } else if (label == "mRNA") {
            num_mrna = s_CountAccessions(field);
        } else if (label == "EST") {
            num_est  = s_CountAccessions(field);
        } else if (label == "Protein") {
            num_prot = s_CountAccessions(field);
        } else if (label == "long SRA read") {
            num_long_sra = s_CountAccessions(field);
        } else if (label == "rnaseq_base_coverage" ) {
            if ( field.CanGetData() && field.GetData().IsInt() ) {
                rnaseq_base_coverage = field.GetData().GetInt();
            }
        } else if (label == "rnaseq_biosamples_introns_full" ) {
            if ( field.CanGetData() && field.GetData().IsInt() ) {
                rnaseq_biosamples_introns_full = field.GetData().GetInt();
            }
        }
    }

    CNcbiOstrstream text;
    text << "Derived by automated computational analysis";
    if (method != NULL  &&  !NStr::IsBlank(*method)) {
         text << " using gene prediction method: " << *method;
    }
    text << ".";

    if (num_mrna > 0  ||  num_est > 0  ||  num_prot > 0 || num_long_sra > 0 ||
        rnaseq_base_coverage > 0 ) 
    {
        text << " Supporting evidence includes similarity to:";
    }
    string section_prefix = " ";
    // The countable section
    if( num_mrna > 0  ||  num_est > 0  ||  num_prot > 0 || num_long_sra > 0 )
    {
        text << section_prefix;
        string prefix = "";
        if (num_mrna > 0) {
            text << prefix << num_mrna << " mRNA";
            if (num_mrna > 1) {
                text << 's';
            }
            prefix = ", ";
        }
        if (num_est > 0) {
            text << prefix << num_est << " EST";
            if (num_est > 1) {
                text << 's';
            }
            prefix = ", ";
        }
        if (num_prot > 0) {
            text << prefix << num_prot << " Protein";
            if (num_prot > 1) {
                text << 's';
            }
            prefix = ", ";
        }
        if (num_long_sra > 0) {
            text << prefix << num_long_sra << " long SRA read";
            if (num_long_sra > 1) {
                text << 's';
            }
            prefix = ", ";
        }
        section_prefix = ", and ";
    }
    // The RNASeq section
    if( rnaseq_base_coverage > 0 )
    {
        text << section_prefix;

        text << rnaseq_base_coverage << "% coverage of the annotated genomic feature by RNAseq alignments";
        if( rnaseq_biosamples_introns_full > 0 ) {
            text << ", including " << rnaseq_biosamples_introns_full;
            text << " sample";
            if( rnaseq_biosamples_introns_full > 1 ) {
                text << 's';
            }
            text << " with support for all annotated introns";
        }

        section_prefix = ", and ";
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
    const bool is_ftable = ctx.Config().IsFormatFTable();
    const bool is_html = ctx.Config().DoHTML();

    if ( s_IsNote(flags, ctx) ) {
        static const string sfx = ";";
        m_Prefix = &kEOL;
        m_Suffix = &sfx;
        x_AddFQ(q, "note", name + ": " + s_GetGOText(*m_Value, is_ftable, is_html));
    } else {
        x_AddFQ(q, name, s_GetGOText(*m_Value, is_ftable, is_html));
    }
}

bool CFlatGoQVal::Equals( const CFlatGoQVal &rhs ) const
{
    return m_Value->Equals( *rhs.m_Value );
}

const string & CFlatGoQVal::GetTextString(void) const
{
    if( m_Value.IsNull() ) {
        return kEmptyStr;
    }

    CConstRef<CUser_field> textStringField = m_Value->GetFieldRef("text string");
    if( textStringField.IsNull() ) {
        return kEmptyStr;
    }

    const CUser_field_Base::TData & textStringData = textStringField->GetData();
    if( ! textStringData.IsStr() ) {
        return kEmptyStr;
    }

    return textStringData.GetStr();
}

int CFlatGoQVal::GetPubmedId(void) const
{
    if( m_Value.IsNull() ) {
        return 0;
    }

    CConstRef<CUser_field> pmidField = m_Value->GetFieldRef("pubmed id");
    if( pmidField.IsNull() ) {
        return 0;
    }

    const CUser_field_Base::TData & pmidData = pmidField->GetData();
    if( ! pmidData.IsInt() ) {
        return 0;
    }
    
    return pmidData.GetInt();
}

void CFlatAnticodonQVal::Format
(TFlatQuals& q,
 const string& name,
 CBioseqContext& ctx,
 IFlatQVal::TFlags flags) const
{
    if ( m_Aa.empty() ) {
        return;
    }

    string locationString = CFlatSeqLoc(*m_Anticodon, ctx).GetString();

    CNcbiOstrstream text;
    text << "(pos:" << locationString;
    text << ",aa:" << m_Aa;

    CScope & scope = ctx.GetScope();
    if( sequence::GetLength(*m_Anticodon, &scope) == 3 ) { // "3" because 3 nucleotides to an amino acid (and thus anticodon sequence)
        
        try {
            CSeqVector seq_vector(*m_Anticodon, scope, CBioseq_Handle::eCoding_Iupac);
            if( seq_vector.size() == 3 ) {
                string seq("---");
                seq_vector.GetSeqData(0, 3, seq);
                NStr::ToLower(seq);
                text << ",seq:" << seq;
            }
        } catch(...) {
            // ignore any sort of error that occurs in this process
        }
    }
    text << ')' ;

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
    if ( 0 == num ) {
        return;
    }

    if ( ctx.Config().CodonRecognizedToNote() ) {
        if (num == 1) {
            string str = "codon recognized: " + recognized;
            if (NStr::Find(m_Seqfeat_note, str) == NPOS) {
                x_AddFQ(q, name, str);
            }
        }
        else {
            x_AddFQ(q, name, "codons recognized: " + recognized);
        }
    } else {
        x_AddFQ(q, "codon_recognized", recognized);
    }
}


void CFlatProductNamesQVal::Format
(TFlatQuals& quals,
 const string& name,
 CBioseqContext& ctx,
 TFlags flags) const
{
    if (m_Value.size() < 2) {
        return;
    }
    bool note = s_IsNote(flags, ctx);
    
    CProt_ref::TName::const_iterator it = m_Value.begin();
    ++it;  // first is used for /product
    while (it != m_Value.end()  &&  !NStr::IsBlank(*it)) {
        if (*it != m_Gene) {
            x_AddFQ(quals, (note ? "note" : name), *it);
        }
        ++it;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
