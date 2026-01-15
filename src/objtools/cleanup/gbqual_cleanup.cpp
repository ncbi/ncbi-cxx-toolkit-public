/*
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
 * Author: 
 *
 * File Description:
 *   GBQual cleanup
 *
 * ===========================================================================
 */

// All this functionality is packed into this one file for ease of
// searching.  If it gets big enough, it will be broken up in the future.

#include <ncbi_pch.hpp>

#include <objects/misc/sequence_macros.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/cleanup/cleanup_change.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/objutil.hpp>

#include <optional>
#include <util/regexp/ctre/ctre.hpp>
#include <util/regexp/ctre/replace.hpp>
#include "newcleanupp.hpp"
#include "cleanup_utils.hpp"
#include "gbqual_cleanup.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGBQualCleanup::CGBQualCleanup(CScope& scope, CNewCleanup_imp& parent) 
    : m_Parent(parent) { m_Scope.Reset(&scope); }


void CGBQualCleanup::x_ChangeMade(CCleanupChange::EChanges e) 
{
    m_Parent.ChangeMade(e);
}

// Given the position of the opening paren in a string, this returns
// the position of the closing paren (keeping track of any nested parens
// in the middle.
// It returns NPOS if the paren is not closed.
// This function is not currently smart; it doesn't know about quotes
// or anything
static
SIZE_TYPE s_MatchingParenPos( const string &str, SIZE_TYPE open_paren_pos )
{
    _ASSERT( str[open_paren_pos] == '(' );
    _ASSERT( open_paren_pos < str.length() );

    // nesting level. start at 1 since we know there's an open paren
    int level = 1;

    SIZE_TYPE pos = open_paren_pos + 1;
    for( ; pos < str.length(); ++pos ) {
        switch( str[pos] ) {
        case '(':
            // nesting deeper
            ++level;
            break;
        case ')':
            // closed a level of nesting
            --level;
            if( 0 == level ) {
                // reached the top: we're closing the initial paren,
                // so we return our position
                return pos;
            }
            break;
        default:
            // ignore other characters.
            // maybe in the future we'll handle ignoring parens in quotes or
            // things like that.
            break;
        }
    }
    return NPOS;
}

static CRef<CTrna_ext> s_ParseTRnaFromAnticodonString (const string &str, const CSeq_feat& feat, CScope *scope)
{
    CRef<CTrna_ext> trna;

    if (NStr::IsBlank (str)) return trna;

    if (NStr::StartsWith (str, "(pos:")) {
        // find position of closing paren
        string::size_type pos_end = s_MatchingParenPos( str, 0 );
        if (pos_end != string::npos) {
            trna.Reset( new CTrna_ext );
            string pos_str = str.substr (5, pos_end - 5);
            string::size_type aa_start = NStr::FindNoCase (pos_str, "aa:");
            if (aa_start != string::npos) {
                string abbrev = pos_str.substr (aa_start + 3);
                auto iupac_aa = CCleanupUtils::GetIupacAa(abbrev);
                if (! iupac_aa) {
                    // unable to parse
                    return trna;
                }
                CRef<CTrna_ext::TAa> aa(new CTrna_ext::TAa);
                aa->SetIupacaa(*iupac_aa);
                trna->SetAa(*aa);
                pos_str = pos_str.substr (0, aa_start);
                NStr::TruncateSpacesInPlace (pos_str);
                if (NStr::EndsWith (pos_str, ",")) {
                    pos_str = pos_str.substr (0, pos_str.length() - 1);
                }
            }
            const CSeq_loc& loc = feat.GetLocation();
            CRef<CSeq_loc> anticodon = ReadLocFromText (pos_str, loc.GetId(), scope);
            if( anticodon ) {
                CBioseq_Handle bsh = scope->GetBioseqHandle(*(loc.GetId()));
                if (!bsh) {
                    trna.Reset();
                    return trna;
                }
                if (anticodon->GetStop(eExtreme_Positional) >= bsh.GetInst_Length()) {
                    trna.Reset();
                    return trna;
                }
                if (feat.GetLocation().IsSetStrand()) {
                    anticodon->SetStrand(loc.GetStrand());
                } else {
                    anticodon->SetStrand(eNa_strand_plus); // anticodon is always on plus strand
                }
            }
            if (!anticodon) {
                trna->ResetAa();
            } else {
                trna->SetAnticodon(*anticodon);
            }
        }
    }
    return trna;
}



static
const char *s_FindImpFeatType( const CImp_feat &imp )
{
    // keep sorted in ASCII-betical order
    static const char *allowed_types[] = {
        "-10_signal",     "-35_signal",   "3'UTR",          "3'clip",         "5'UTR",
        "5'clip",         "CAAT_signal",  "CDS",            "C_region",       "D-loop",
        "D_segment",      "GC_signal",    "Import",         "J_segment",      "LTR",
        "N_region",       "RBS",          "STS",            "S_region",       "Site-ref",
        "TATA_signal",    "V_region",     "V_segment",      "allele",         "attenuator",
        "centromere",     "conflict",     "enhancer",       "exon",           "gap",
        "iDNA",           "intron",       "mat_peptide",    "misc_RNA",       "misc_binding",
        "misc_difference","misc_feature", "misc_recomb",    "misc_signal",    "misc_structure",
        "mobile_element", "modified_base","mutation",       "old_sequence",   "operon",
        "oriT",           "polyA_signal", "polyA_site",     "precursor_RNA",  "prim_transcript",
        "primer_bind",    "promoter",     "protein_bind",   "regulatory",     "rep_origin",
        "repeat_region",  "repeat_unit",  "satellite",      "sig_peptide",    "source",
        "stem_loop",      "telomere",     "terminator",     "transit_peptide","unsure",
        "variation",      "virion"
    };
    static const int kAllowedTypesNumElems = ( sizeof(allowed_types) / sizeof(allowed_types[0]));

    static const char *kFeatBad = "???";

    if( ! RAW_FIELD_IS_EMPTY_OR_UNSET(imp, Key) ) {
        // the C logic is more complex than this
        const char *key = GET_FIELD(imp, Key).c_str();
        if( binary_search( allowed_types, allowed_types + kAllowedTypesNumElems,
            key, PCase_CStr() ) )
        {
            return key;
        }
    }

    return kFeatBad;
}



static
const char *s_FindKeyFromFeatDefType( const CSeq_feat &feat )
{
    static const char *kFeatBad = "???";
    const CSeqFeatData& fdata = feat.GetData();

    switch (fdata.Which()) {
    case NCBI_SEQFEAT(Gene):
        return "Gene";
    case NCBI_SEQFEAT(Org):
        return "Org";
    case NCBI_SEQFEAT(Cdregion):
        return "CDS";
    case NCBI_SEQFEAT(Prot):
        if(fdata.GetProt().IsSetProcessed() ) {
            switch( feat.GetData().GetProt().GetProcessed() ) {
            case NCBI_PROTREF(not_set):
                return "Protein";
            case NCBI_PROTREF(preprotein):
                return "proprotein";
            case NCBI_PROTREF(mature):
                return "mat_peptide";
            case NCBI_PROTREF(signal_peptide):
                return "sig_peptide";
            case NCBI_PROTREF(transit_peptide):
                return "transit_peptide";
            case NCBI_PROTREF(propeptide):
                return "propeptide";
            default:
                return kFeatBad;
            }
        }
        return "Protein";
    case NCBI_SEQFEAT(Rna):
        if(fdata.GetRna().IsSetType() ) {
            const auto& rna = fdata.GetRna();
            switch (rna.GetType() )
            {
            case NCBI_RNAREF(unknown):
                return "misc_RNA"; // unknownrna mapped to otherrna
            case NCBI_RNAREF(premsg):
                return "precursor_RNA";
            case NCBI_RNAREF(mRNA):
                return "mRNA";
            case NCBI_RNAREF(tRNA):
                return "tRNA";
            case NCBI_RNAREF(rRNA):
                return "rRNA";
            case NCBI_RNAREF(snRNA):
                return "snRNA";
            case NCBI_RNAREF(scRNA):
                return "scRNA";
            case NCBI_RNAREF(snoRNA):
                return "snoRNA";
            case NCBI_RNAREF(ncRNA):
                return "ncRNA";
            case NCBI_RNAREF(tmRNA):
                return "tmRNA";
            case NCBI_RNAREF(miscRNA):
                return "misc_RNA";
            case NCBI_RNAREF(other):
                if ( FIELD_IS_SET_AND_IS(rna, Ext, Name) ) {
                    const string &name = rna.GetExt().GetName();
                    if ( NStr::EqualNocase(name, "misc_RNA")) return "misc_RNA";
                    if ( NStr::EqualNocase(name, "ncRNA") ) return "ncRNA";
                    if ( NStr::EqualNocase(name, "tmRNA") ) return "tmRNA";
                }
                return "misc_RNA";
            default:
                return kFeatBad;
            }
        }
        return kFeatBad;
    case NCBI_SEQFEAT(Pub):
        return "Cit";
    case NCBI_SEQFEAT(Seq):
        return "Xref";
    case NCBI_SEQFEAT(Imp):
        return s_FindImpFeatType( fdata.GetImp() );
    case NCBI_SEQFEAT(Region):
        return "Region";
    case NCBI_SEQFEAT(Comment):
        return "Comment";
    case NCBI_SEQFEAT(Bond):
        return "Bond";
    case NCBI_SEQFEAT(Site):
        return "Site";
    case NCBI_SEQFEAT(Rsite):
        return "Rsite";
    case NCBI_SEQFEAT(User):
        return "User";
    case NCBI_SEQFEAT(Txinit):
        return "TxInit";
    case NCBI_SEQFEAT(Num):
        return "Num";
    case NCBI_SEQFEAT(Psec_str):
        return "SecStr";
    case NCBI_SEQFEAT(Non_std_residue):
        return "NonStdRes";
    case NCBI_SEQFEAT(Het):
        return "Het";
    case NCBI_SEQFEAT(Biosrc):
        return "Src";
    case NCBI_SEQFEAT(Clone):
        return "CloneRef";
    case NCBI_SEQFEAT(Variation):
        return "VariationRef";
    default:
        return kFeatBad;
    }
    return kFeatBad;
}


static bool s_SetExceptFromGbqual(const CGb_qual& gb_qual, CSeq_feat& feat)
{
    bool rval = false;
    if (!feat.IsSetExcept() || !feat.GetExcept()) {
        feat.SetExcept(true);
        rval = true;
    }

    if (!gb_qual.IsSetQual()) {
        return rval;
    }
    if (feat.IsSetExcept_text() && !NStr::IsBlank(feat.GetExcept_text())) {
        return rval;
    }
    // for whatever reason, C Toolkit only sets text if Gbqual was blank
    if (gb_qual.IsSetVal() && !NStr::IsBlank(gb_qual.GetVal())) {
        return rval;
    }
    string exc = gb_qual.GetQual();
    NStr::ReplaceInPlace (exc, "-", " ");
    NStr::ReplaceInPlace (exc, "_", " ");
    feat.SetExcept_text(exc);
    return true;
}


static bool s_StringsAreEquivalent(const string& str1, const string& str2)
{
    string s1 = NStr::Replace(str1, " ", "_");
    NStr::ReplaceInPlace(s1, "-", "_");
    string s2 = NStr::Replace(str2, " ", "_");
    NStr::ReplaceInPlace(s2, "-", "_");
    return NStr::EqualNocase(s1, s2);
}


CGBQualCleanup::EAction CGBQualCleanup::Run(CGb_qual& gb_qual, CSeq_feat& feat)
{

    if( ! FIELD_IS_SET(feat, Data) ) {
        return eAction_Nothing;
    }
    CSeqFeatData &data = GET_MUTABLE(feat, Data);

    string& qual = GET_MUTABLE(gb_qual, Qual);
    string& val  = GET_MUTABLE(gb_qual, Val);

    if( FIELD_EQUALS(feat, Pseudo, false) ) {
        RESET_FIELD(feat, Pseudo);
        x_ChangeMade (CCleanupChange::eChangeQualifiers);
    }

    if( FIELD_EQUALS(feat, Partial, false) ) {
        RESET_FIELD(feat, Partial);
        x_ChangeMade (CCleanupChange::eChangeQualifiers);
    }

    if (NStr::EqualNocase(qual, "cons_splice")) {
        return eAction_Erase;
    } else if (s_StringsAreEquivalent(qual, "ribosomal-slippage") ||
               s_StringsAreEquivalent(qual, "trans-splicing") ||
               s_StringsAreEquivalent(qual, "artificial-location")) {
        if (s_SetExceptFromGbqual(gb_qual, feat)) {
            x_ChangeMade (CCleanupChange::eChangeException);
        }
        return eAction_Erase;
    } else if (NStr::EqualNocase(qual, "partial")) {
        feat.SetPartial(true);
        x_ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "evidence")) {
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "exception")) {
        if( ! FIELD_EQUALS(feat, Except, true ) ) {
            SET_FIELD(feat, Except, true);
            x_ChangeMade(CCleanupChange::eChangeQualifiers);
        }
        if (!NStr::IsBlank(val)  &&  !NStr::EqualNocase(val, "true")) {
            if (!feat.IsSetExcept_text()) {
                feat.SetExcept_text(val);
                x_ChangeMade(CCleanupChange::eChangeQualifiers);
            }
        }
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "experiment")) {
        if (NStr::EqualNocase(val, "experimental evidence, no additional details recorded")) {
            x_ChangeMade(CCleanupChange::eChangeQualifiers);
            return eAction_Erase;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "inference")) {
        if (NStr::EqualNocase(val, "non-experimental evidence, no additional details recorded")) {
            x_ChangeMade(CCleanupChange::eChangeQualifiers);
            return eAction_Erase;  // mark qual for deletion
        } else {
            x_CleanupAndRepairInference(val);
        }
    } else if (NStr::EqualNocase(qual, "note")  ||
               NStr::EqualNocase(qual, "notes")  ||
               NStr::EqualNocase(qual, "comment")) {
        if (!feat.IsSetComment()) {
            feat.SetComment(val);
        } else {
            (feat.SetComment() += "; ") += val;
        }
        x_ChangeMade(CCleanupChange::eChangeComment);
        x_ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;  // mark qual for deletion
    } else if( NStr::EqualNocase(qual, "label") ) {
        if ( NStr::EqualNocase(val, s_FindKeyFromFeatDefType(feat)) ) {
            // skip label that is simply the feature key
        } else if ( ! FIELD_IS_SET(feat, Comment) || NStr::FindNoCase(GET_FIELD(feat, Comment), "label") == NPOS) {
            // if label is not already in comment, append
            if( GET_STRING_FLD_OR_BLANK(feat, Comment).empty() ) {
                SET_FIELD(feat, Comment, "label: " + val );
            } else {
                GET_MUTABLE(feat, Comment) += "; label: " + val;
            }
            x_ChangeMade(CCleanupChange::eChangeComment);
        }
        return eAction_Erase;
    } else if (NStr::EqualNocase(qual, "regulatory_class")) {
        string::size_type colon_pos = val.find_first_of(":");
        if (colon_pos != string::npos && ! NStr::StartsWith (val, "other:")) {
            string comment = val.substr( colon_pos + 1 );
            val.resize( colon_pos );
            if( GET_STRING_FLD_OR_BLANK(feat, Comment).empty() ) {
                SET_FIELD(feat, Comment, comment );
            } else {
                GET_MUTABLE(feat, Comment) += "; " + comment;
            }
            x_ChangeMade(CCleanupChange::eChangeComment);
        }
    } else if (NStr::EqualNocase(qual, "db_xref")) {
        string tag, db;
        if (NStr::SplitInTwo(val, ":", db, tag)) {
            CRef<CDbtag> dbp(new CDbtag);
            dbp->SetDb(db);
            dbp->SetTag().SetStr(tag);

            feat.SetDbxref().push_back(dbp);
            x_ChangeMade(CCleanupChange::eChangeDbxrefs);
            return eAction_Erase;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "gdb_xref")) {
        CRef<CDbtag> dbp(new CDbtag);
        dbp->SetDb("GDB");
        dbp->SetTag().SetStr(val);
        feat.SetDbxref().push_back(dbp);
        x_ChangeMade(CCleanupChange::eChangeDbxrefs);
        return eAction_Erase;  // mark qual for deletion
    } else if ( NStr::EqualNocase(qual, "pseudo") ) {
        feat.SetPseudo(true);
        x_ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;  // mark qual for deletion
    } else if ( NStr::EqualNocase(qual, "pseudogene") )
    {
        if( ! FIELD_EQUALS(feat, Pseudo, true) ) {
            feat.SetPseudo(true);
            x_ChangeMade(CCleanupChange::eChangeQualifiers);
        }

        // lowercase pseudogene qual
        string new_val = val;
        NStr::ToLower(new_val);
        if( new_val != val ) {
            val = new_val;
            x_ChangeMade(CCleanupChange::eChangeQualifiers);
        }
    } else if ( FIELD_IS(data, Gene)  &&  x_GeneGBQualBC( GET_MUTABLE(data, Gene), gb_qual) == eAction_Erase) {
        return eAction_Erase;  // mark qual for deletion
    } else if ( FIELD_IS(data, Cdregion)  &&  x_SeqFeatCDSGBQualBC(feat, GET_MUTABLE(data, Cdregion), gb_qual) == eAction_Erase ) {
        return eAction_Erase;  // mark qual for deletion
    } else if (data.IsRna()  &&  x_SeqFeatRnaGBQualBC(feat, data.SetRna(), gb_qual) == eAction_Erase) {
        return eAction_Erase;  // mark qual for deletion
    } else if (data.IsProt()  &&  x_ProtGBQualBC(data.SetProt(), gb_qual, eGBQualOpt_normal) == eAction_Erase) {
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "gene")) {
        if (!NStr::IsBlank(val)) {
            if (!data.IsGene()) {
                CRef<CSeqFeatXref> xref(new CSeqFeatXref);
                xref->SetData().SetGene().SetLocus(val);
                feat.SetXref().insert(feat.SetXref().begin(), xref);
                x_ChangeMade(CCleanupChange::eCopyGeneXref);
            } else {
                auto& gene = data.SetGene();
                if (gene.IsSetDesc() && !NStr::IsBlank(gene.GetDesc())) {
                    gene.SetDesc() += "; ";
                } else {
                    gene.SetDesc(kEmptyStr);
                }
                gene.SetDesc() += "gene=" + val;
                x_ChangeMade(CCleanupChange::eChangeQualifiers);
            }
            return eAction_Erase;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "codon_start")) {
        if (!data.IsCdregion()) {
            // not legal on anything but CDS, so remove it
            return eAction_Erase;  // mark qual for deletion
        }
    } else if ( NStr::EqualNocase(qual, "EC_number") ) {
        x_CleanupECNumber(val);
    } else if( qual == "satellite" ) {
        x_MendSatelliteQualifier( val );
    } else if ( NStr::EqualNocase(qual, "replace") && data.GetSubtype() == CSeqFeatData::eSubtype_variation) {
        string orig = val;
        NStr::ToLower(val);
        if (!NStr::Equal(orig, val)) {
            x_ChangeMade(CCleanupChange::eCleanQualifiers);
        }
    }
    else if (NStr::EqualNocase(qual, "recombination_class")) {
        if (CGb_qual::FixRecombinationClassValue(val)) {
            x_ChangeMade(CCleanupChange::eCleanQualifiers);
        }
    }


    if( NStr::EqualNocase( qual, "mobile_element_type" ) ) {
        // trim spaces around first colon but only if there are no colons
        // with spaces before and after
        if( NPOS != NStr::Find(val, " :") || NPOS != NStr::Find(val, ": ") ) {
            if(ct::search_replace_one<"[ ]*:[ ]*", ":">(val)) {
                x_ChangeMade(CCleanupChange::eCleanQualifiers);
            }
        }

        if( data.IsImp() && STRING_FIELD_MATCH( data.GetImp(), Key, "repeat_region" ) && ! val.empty() ) {
            qual = "mobile_element_type";
            data.SetImp().SetKey( "mobile_element" );
            x_ChangeMade(CCleanupChange::eCleanQualifiers);
        }
    }

    // estimated_length must be a number or "unknown"
    if( NStr::EqualNocase( qual, "estimated_length" ) ) {
        if( ! CCleanupUtils::IsAllDigits(val) && ! NStr::EqualNocase(val, "unknown") ) {
            val = "unknown";
            x_ChangeMade(CCleanupChange::eCleanQualifiers);
        }
    }

    // conflict is obsolete.  Make it misc_difference, but add a note
    // to the feature comment as to what it used to be.
    if( data.IsImp() && STRING_FIELD_MATCH( data.GetImp(), Key, "conflict" ) ) {
        data.SetImp().SetKey( "misc_difference");
        if( feat.IsSetComment() ) {
            GET_MUTABLE(feat, Comment) = "conflict; " + GET_FIELD(feat, Comment);
        } else {
            SET_FIELD(feat, Comment, "conflict");
        }
        x_ChangeMade(CCleanupChange::eCleanQualifiers);
    }

    if( qual.empty() && val.empty() ) {
        return eAction_Erase;
    }

    return eAction_Nothing;
}


void CGBQualCleanup::x_CleanupAndRepairInference( string &inference )
{
    if( inference.empty() ) {
        return;
    }

    const string original_inference = inference;
    inference = CGb_qual::CleanupAndRepairInference( original_inference );

    if( inference != original_inference ) {
        x_ChangeMade(CCleanupChange::eCleanQualifiers);
    }
}





CGBQualCleanup::EAction
CGBQualCleanup::x_GeneGBQualBC( CGene_ref& gene, const CGb_qual& gb_qual )
{
    const string& qual = GET_FIELD(gb_qual, Qual);
    const string& val  = GET_FIELD(gb_qual, Val);

    if( NStr::IsBlank(val) ) {
        return eAction_Nothing;
    }

    bool change_made = false;
    if (NStr::EqualNocase(qual, "gene")) {
        if (!gene.IsSetLocus()) {
            change_made = true;
            gene.SetLocus(val);
        }
    }
    else if (NStr::EqualNocase(qual, "map")) {
        if (! gene.IsSetMaploc() ) {
            change_made = true;
            gene.SetMaploc(val);
        }
    } else if (NStr::EqualNocase(qual, "allele")) {
        if ( gene.IsSetAllele() ) {
            return ( NStr::EqualNocase(val, gene.GetAllele()) ? eAction_Erase : eAction_Nothing );
        } else {
            change_made = true;
            gene.SetAllele(val);
        }
    } else if (NStr::EqualNocase(qual, "locus_tag")) {
        if ( ! gene.IsSetLocus_tag() ) {
            change_made = true;
            gene.SetLocus_tag(val);
        }
    } else if (NStr::EqualNocase(qual, "gene_synonym")) {
        change_made = true;
        gene.SetSyn().push_back(val);
    }

    if (change_made) {
        x_ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;
    }

    return eAction_Nothing;
}



CGBQualCleanup::EAction
CGBQualCleanup::x_ProtGBQualBC(CProt_ref& prot, const CGb_qual& gb_qual, EGBQualOpt opt)
{
    const string& qual = gb_qual.GetQual();
    const string& val = gb_qual.GetVal();

    if (NStr::EqualNocase(qual, "product") || NStr::EqualNocase(qual, "standard_name")) {
        if (opt == eGBQualOpt_CDSMode || !prot.IsSetName() || NStr::IsBlank(prot.GetName().front())) {
            CCleanup::SetProteinName(prot, val, false);
            x_ChangeMade(CCleanupChange::eChangeQualifiers);
        } else {
            return eAction_Nothing;
        }
    } else if (NStr::EqualNocase(qual, "function")) {
        ADD_STRING_TO_LIST(prot.SetActivity(), val);
        x_ChangeMade(CCleanupChange::eChangeQualifiers);
    } else if (NStr::EqualNocase(qual, "EC_number")) {
        ADD_STRING_TO_LIST(prot.SetEc(), val);
        x_ChangeMade(CCleanupChange::eChangeQualifiers);
    }

    // labels to leave alone
    static const char * const ignored_quals[] =
    { "label", "allele", "experiment", "inference", "UniProtKB_evidence",
    "dbxref", "replace", "rpt_unit_seq", "rpt_unit_range" };
    static set<string, PNocase> ignored_quals_raw;

    // the mutex is just there in the unlikely event that two separate
    // threads both try to initialized ignored_quals_raw.  It's NOT
    // needed for reading
    static CMutex ignored_quals_raw_initialization_mutex;
    {
        CMutexGuard guard(ignored_quals_raw_initialization_mutex);
        if (ignored_quals_raw.empty()) {
            copy(ignored_quals, ignored_quals + sizeof(ignored_quals) / sizeof(ignored_quals[0]),
                inserter(ignored_quals_raw, ignored_quals_raw.begin()));
        }
    }

    if (ignored_quals_raw.find(qual) != ignored_quals_raw.end()) {
        return eAction_Nothing;
    }

    // all other gbquals not appropriate on protein features
    return eAction_Erase;
}



CGBQualCleanup::EAction
CGBQualCleanup::x_SeqFeatCDSGBQualBC(CSeq_feat& feat, CCdregion& cds, const CGb_qual& gb_qual)
{
    const string& qual = gb_qual.GetQual();
    const string& val  = gb_qual.GetVal();

    // transl_except qual -> Cdregion.code_break
    if (NStr::EqualNocase(qual, "transl_except")) {
        // could not be parsed earlier
        return eAction_Nothing;
    }

    // codon_start qual -> Cdregion.frame
    if (NStr::EqualNocase(qual, "codon_start")) {
        CCdregion::TFrame frame = GET_FIELD(cds, Frame);
        CCdregion::TFrame new_frame = CCdregion::TFrame(NStr::StringToNonNegativeInt(val));
        if (new_frame == CCdregion::eFrame_one  ||
            new_frame == CCdregion::eFrame_two  ||
            new_frame == CCdregion::eFrame_three) {
            if (frame == CCdregion::eFrame_not_set  ||
                ( FIELD_EQUALS( feat, Pseudo, true ) && ! FIELD_IS_SET(feat, Product) )) {
                cds.SetFrame(new_frame);
                x_ChangeMade(CCleanupChange::eChangeQualifiers);
            }
            return eAction_Erase;
        }
    }

    // transl_table qual -> Cdregion.code
    if (NStr::EqualNocase(qual, "transl_table")) {
        if ( FIELD_IS_SET(cds, Code) ) {
            const CCdregion::TCode& code = GET_FIELD(cds, Code);
            int transl_table = 1;
            ITERATE (CCdregion::TCode::Tdata, it, code.Get()) {
                if ( FIELD_IS(**it, Id)  &&  GET_FIELD(**it, Id) != 0) {
                    transl_table = GET_FIELD(**it, Id);
                    break;
                }
            }

            if (NStr::EqualNocase(NStr::UIntToString(transl_table), val)) {
                return eAction_Erase;
            }
        } else {
            int new_val = NStr::StringToNonNegativeInt(val);
            if (new_val > 0) {
                CRef<CGenetic_code::C_E> gc(new CGenetic_code::C_E);
                SET_FIELD(*gc, Id, new_val);
                cds.SetCode().Set().push_back(gc);

                // we don't have to check except-text because we're
                // setting an unset genetic_code, not changing an existing one
                // (the except-text would be: "genetic code exception")
                x_ChangeMade(CCleanupChange::eChangeGeneticCode);
                return eAction_Erase;
            }
        }
    }

    // look for qualifiers that should be applied to protein feature
    // note - this should be moved to the "indexed" portion of basic cleanup,
    // because it needs to locate another sequence and feature
    if (NStr::Equal(qual, "product") || NStr::Equal (qual, "function") || NStr::EqualNocase (qual, "EC_number")
        || NStr::Equal (qual, "prot_note"))
    {
        // get protein sequence for product
        CRef<CSeq_feat> prot_feat;
        CRef<CProt_ref> prot_ref;

        // try to get existing prot_feat
        CBioseq_Handle prot_handle;
        if ( FIELD_IS_SET(feat, Product) ) {
            const CSeq_id *prod_seq_id = feat.GetProduct().GetId();
            if( prod_seq_id ) {
                prot_handle = m_Scope->GetBioseqHandle(*prod_seq_id);
            }
        }
        if (prot_handle) {
            // find main protein feature
            CConstRef<CBioseq> pseq = prot_handle.GetCompleteBioseq();
            if (pseq && pseq->IsSetAnnot()) {
                for (auto ait : pseq->GetAnnot()) {
                    if (ait->IsFtable()) {
                        for (auto fit : ait->GetData().GetFtable()) {
                            if (fit->IsSetData() && fit->GetData().GetSubtype() == CSeqFeatData::eSubtype_prot) {
                                prot_feat.Reset(const_cast<CSeq_feat*>(fit.GetPointer()));
                                prot_ref.Reset(&(prot_feat->SetData().SetProt()));
                            }
                        }
                    }
                }
            }
        }

        bool push_back_xref_on_success = false;
        CRef<CSeqFeatXref> xref;
        if ( ! prot_ref ) {
            // otherwise make cross reference
            prot_ref.Reset( new CProt_ref );

            // see if this seq-feat already has a prot xref
            EDIT_EACH_SEQFEATXREF_ON_SEQFEAT( xref_iter, feat ) {
                if( (*xref_iter)->IsSetData() && (*xref_iter)->GetData().IsProt() ) {
                    xref = *xref_iter;
                }
            }
            // seq-feat has no prot xref. We make our own.
            if ( ! xref ) {
                xref.Reset( new CSeqFeatXref );
                xref->SetData().SetProt( *prot_ref );
                // we will push the xref onto the feat if the add was successful
                push_back_xref_on_success = true;
            }
            prot_ref.Reset( &xref->SetData().SetProt() );
        }

        // replacement prot feature
        EAction action = eAction_Nothing;

        if (NStr::Equal(qual, "prot_note") ) {
            if( prot_feat ) {
                if (!prot_feat->IsSetComment() || NStr::IsBlank (prot_feat->GetComment())) {
                    SET_FIELD( *prot_feat, Comment, val);
                } else {
                    SET_FIELD( *prot_feat, Comment, (prot_feat->GetComment() + "; " + val) );
                }
                x_ChangeMade (CCleanupChange::eChangeComment);
                action = eAction_Erase;
            }
        } else {
            action = x_ProtGBQualBC( *prot_ref, gb_qual, eGBQualOpt_CDSMode );
        }

        if( push_back_xref_on_success ) {
            feat.SetXref().push_back( xref );
            x_ChangeMade(CCleanupChange::eCleanSeqFeatXrefs);
        }

        return action;
    }

    if (NStr::EqualNocase(qual, "translation")) {
        return eAction_Erase;
    }

    return eAction_Nothing;
}


void CGBQualCleanup::x_SeqFeatTRNABC(CTrna_ext & tRNA )
{
    m_Parent.SeqFeatTRNABC(tRNA);
}


// homologous to C's HandledGBQualOnRNA.
// That func was copy-pasted, then translated into C++.
// Later we can go back and actually refactor the code
// to make it more efficient or cleaner.
CGBQualCleanup::EAction
CGBQualCleanup::x_SeqFeatRnaGBQualBC(CSeq_feat& feat, CRNA_ref& rna, CGb_qual& gb_qual)
{
    if( ! gb_qual.IsSetVal()) {
        return eAction_Nothing;
    }
    const string &gb_qual_qual = gb_qual.GetQual();
    string &gb_qual_val = gb_qual.SetVal();
    TRNAREF_TYPE& rna_type = rna.SetType();

    if (NStr::EqualNocase(gb_qual_qual, "standard_name")) {
        return eAction_Nothing;
    }
    if (NStr::IsBlank(gb_qual_val)) {
        return eAction_Nothing;
    }

    if (NStr::EqualNocase( gb_qual_qual, "product" ))
    {
        if (rna_type == NCBI_RNAREF(unknown)) {
            rna_type = NCBI_RNAREF(other);
            x_ChangeMade(CCleanupChange::eChangeRNAref);
        }
        if ( rna.IsSetExt() && rna.GetExt().IsName() ) {
            const string &name = rna.SetExt().SetName();
            if ( name.empty() ) {
                rna.ResetExt();
                x_ChangeMade(CCleanupChange::eChangeRNAref);
            }
        }
        if (x_HandleTrnaProductGBQual(feat, rna, gb_qual_val) == eAction_Erase) {
            return eAction_Erase;
        }

        if (!rna.IsSetExt()) {
            string remainder;
            rna.SetRnaProductName(gb_qual_val, remainder);
            x_ChangeMade(CCleanupChange::eChangeRNAref);
            if (NStr::IsBlank(remainder)) {
                return eAction_Erase;
            } else {
                gb_qual.SetQual(remainder);
                return eAction_Nothing;
            }
        }
        if( rna.GetExt().IsGen() ) {
            CRNA_gen & rna_gen = rna.SetExt().SetGen();
            if( RAW_FIELD_IS_EMPTY_OR_UNSET(rna_gen, Product) ) {
                rna_gen.SetProduct(gb_qual_val);
                x_ChangeMade(CCleanupChange::eChangeRNAref);
                return eAction_Erase;
            }
            return eAction_Nothing;
        }
        if (rna.GetExt().IsName() && NStr::Equal(rna.GetExt().GetName(), gb_qual_val)) {
            return eAction_Erase;
        }
        if ( rna.IsSetExt() && ! rna.GetExt().IsName() ) return eAction_Nothing;
        const string &name = ( rna.IsSetExt() ? rna.GetExt().GetName() : kEmptyStr );
        if (! name.empty() ) {
            SIZE_TYPE rDNA_pos = NStr::Find( gb_qual_val, "rDNA");
            if (rDNA_pos != NPOS) {
                gb_qual_val[rDNA_pos+1] = 'R';
                x_ChangeMade(CCleanupChange::eChangeQualifiers);
            }
            if ( NStr::EqualNocase(name, gb_qual_val) ) {
                return eAction_Erase;
            }
            if (rna_type == NCBI_RNAREF(other) || rna_type == NCBI_RNAREF(ncRNA) ||
                rna_type == NCBI_RNAREF(tmRNA) || rna_type == NCBI_RNAREF(miscRNA) )
            {
                // new convention follows ASN.1 spec comments, allows new RNA types
                return eAction_Nothing;
            }
            // subsequent /product now added to comment
            x_AddToComment(feat, gb_qual_val);
            x_ChangeMade(CCleanupChange::eChangeComment);
            return eAction_Erase;
        }
        if (rna_type == NCBI_RNAREF(ncRNA) ||
            rna_type == NCBI_RNAREF(tmRNA) || rna_type == NCBI_RNAREF(miscRNA) )
        {
            // new convention follows ASN.1 spec comments, allows new RNA types
            return eAction_Nothing;
        }
        if ( ! FIELD_CHOICE_EQUALS( rna, Ext, Name, gb_qual_val) ) {
            rna.SetExt().SetName( gb_qual_val );
            x_ChangeMade(CCleanupChange::eChangeRNAref);
            return eAction_Erase;
        }
    } else if (NStr::EqualNocase(gb_qual_qual, "anticodon") ) {
        if (!rna.IsSetType() || rna.GetType() == CRNA_ref::eType_unknown) {
            rna.SetType(CRNA_ref::eType_other);
            x_ChangeMade(CCleanupChange::eChangeKeywords);
        }
        if (rna.GetType() != CRNA_ref::eType_tRNA) {
            return eAction_Nothing;
        }

        CRef<CTrna_ext> trna = s_ParseTRnaFromAnticodonString(gb_qual.GetVal(), feat, m_Scope);
        if (!trna) {
            return eAction_Nothing;
        }

        x_SeqFeatTRNABC(*trna);
        if (trna->IsSetAa() || trna->IsSetAnticodon()) {
            // don't apply at all if there are conflicts
            bool apply_aa = false;
            bool apply_anticodon = false;
            bool ok_to_apply = true;

            // look for conflict with aa
            if (!rna.IsSetExt() || !rna.GetExt().IsTRNA()) {
                if (trna->IsSetAa()) {
                    apply_aa = true;
                }
                if (trna->IsSetAnticodon()) {
                    apply_anticodon = true;
                }
            }
            else {
                if (trna->IsSetAa()) {
                    if (rna.GetExt().GetTRNA().IsSetAa()) {
                        if (rna.GetExt().GetTRNA().GetAa().IsIupacaa()) {
                            if (trna->GetAa().GetIupacaa() != rna.GetExt().GetTRNA().GetAa().GetIupacaa()) {
                                ok_to_apply = false;
                            }
                        }
                    }
                    else {
                        apply_aa = true;
                    }
                }
                // look for conflict with anticodon
                if (trna->IsSetAnticodon()) {
                    if (rna.GetExt().GetTRNA().IsSetAnticodon()) {
                        if (sequence::Compare(rna.GetExt().GetTRNA().GetAnticodon(),
                            trna->GetAnticodon(), m_Scope, sequence::fCompareOverlapping) != sequence::eSame) {
                            ok_to_apply = false;
                        }
                    } else {
                        apply_anticodon = true;
                    }
                }
            }
            if (ok_to_apply) {
                if (apply_aa) {
                    rna.SetExt().SetTRNA().SetAa().SetIupacaa(trna->GetAa().GetNcbieaa());
                    x_ChangeMade(CCleanupChange::eChange_tRna);
                }
                if (apply_anticodon) {
                    CRef<CSeq_loc> anticodon(new CSeq_loc());
                    anticodon->Add(trna->GetAnticodon());
                    rna.SetExt().SetTRNA().SetAnticodon(*anticodon);
                    x_ChangeMade(CCleanupChange::eChangeAnticodon);
                }
                return eAction_Erase;
            }
        }
    }
    return eAction_Nothing;
}



CGBQualCleanup::EAction
CGBQualCleanup::x_HandleTrnaProductGBQual(CSeq_feat& feat, CRNA_ref& rna, const string& product)
{
    CRNA_ref::TType& rna_type = rna.SetType();

    if (rna_type != CRNA_ref::eType_tRNA &&
        rna_type != CRNA_ref::eType_other &&
        rna_type != CRNA_ref::eType_unknown) {
        return eAction_Nothing;
    }

    if (rna_type == NCBI_RNAREF(tRNA) && rna.IsSetExt() && rna.GetExt().IsName()) {
        string name = rna.GetExt().GetName();
        bool justTrnaText = false;
        string codon;
        char aa = CCleanupUtils::ParseSeqFeatTRnaString(name, &justTrnaText, codon, false);
        if (aa != '\0') {
            const bool is_fMet = (NStr::Find(name, "fMet") != NPOS);
            const bool is_iMet = (NStr::Find(name, "iMet") != NPOS);
            const bool is_Ile2 = (NStr::Find(name, "Ile2") != NPOS);
            CRNA_ref_Base::C_Ext::TTRNA &trp = rna.SetExt().SetTRNA();
            trp.SetAa().SetNcbieaa(aa);
            if (aa == 'M') {
                if (is_fMet) {
                    x_AddToComment(feat, "fMet");
                } else if (is_iMet) {
                    x_AddToComment(feat, "iMet");
                }
            } else if (aa == 'I') {
                if (is_Ile2) {
                    x_AddToComment(feat, "Ile2");
                }
            }
            x_SeqFeatTRNABC(trp);
            x_ChangeMade(CCleanupChange::eChangeRNAref);
        }
    }
    if (rna_type == NCBI_RNAREF(tRNA) && !rna.IsSetExt()) {
        // this part inserted from: AddQualifierToFeature (sfp, "product", gb_qual_val);
        bool justTrnaText = false;
        string codon;
        char aa = CCleanupUtils::ParseSeqFeatTRnaString(product, &justTrnaText, codon, false);
        if (aa != '\0') {

            CRNA_ref_Base::C_Ext::TTRNA& trna = rna.SetExt().SetTRNA();
            trna.SetAa().SetNcbieaa(aa);

            if (!justTrnaText || !NStr::IsBlank(codon)) {
                x_AddToComment(feat, product);
            }

            if (aa == 'M') {
                if (NStr::Find(product, "fMet") != NPOS &&
                    (!feat.IsSetComment() || NStr::Find(feat.GetComment(), "fMet") == NPOS)) {
                    // x_AddToComment(feat, "fMet");
                    x_ChangeMade(CCleanupChange::eChangeRNAref);
                    return eAction_Nothing;
                } else if (NStr::Find(product, "iMet") != NPOS &&
                    (!feat.IsSetComment() || NStr::Find(feat.GetComment(), "iMet") == NPOS)) {
                    // x_AddToComment(feat, "iMet");
                    x_ChangeMade(CCleanupChange::eChangeRNAref);
                    return eAction_Nothing;
                }
            } else if (aa == 'I') {
                if (NStr::Find(product, "Ile2") != NPOS &&
                    (!feat.IsSetComment() || NStr::Find(feat.GetComment(), "Ile2") == NPOS)) {
                    // x_AddToComment(feat, "Ile2");
                    x_ChangeMade(CCleanupChange::eChangeRNAref);
                    return eAction_Nothing;
                }
            }

            x_ChangeMade(CCleanupChange::eChangeRNAref);
        }
        else {
            x_AddToComment(feat, product);
        }
        return eAction_Erase;
    }
    if (rna_type == NCBI_RNAREF(tRNA) && rna.IsSetExt() && rna.GetExt().IsTRNA()) {
        CRNA_ref_Base::C_Ext::TTRNA& trp = rna.SetExt().SetTRNA();
        if (trp.IsSetAa() && trp.GetAa().IsNcbieaa()) {
            string ignored;
            if (trp.GetAa().GetNcbieaa() == CCleanupUtils::ParseSeqFeatTRnaString(product, nullptr, ignored, false) &&
                NStr::IsBlank(ignored)) {
            } else {
                // don't remove product qual because it conflicts with existing aa value
                return eAction_Nothing;
            }
            if (NStr::CompareNocase (product, "tRNA-fMet") == 0 || NStr::CompareNocase (product, "iRNA-fMet") == 0) {
                return eAction_Nothing;
            }
            if (NStr::CompareNocase (product, "tRNA-iMet") == 0 || NStr::CompareNocase (product, "iRNA-iMet") == 0) {
                return eAction_Nothing;
            }
            if (NStr::CompareNocase (product, "tRNA-Ile2") == 0 || NStr::CompareNocase (product, "iRNA-Ile2") == 0) {
                return eAction_Nothing;
            }
            return eAction_Erase;
        } else if (!trp.IsSetAa()) {
            string ignored;
            bool justTrnaText = false;
            char aa = CCleanupUtils::ParseSeqFeatTRnaString(product, &justTrnaText, ignored, false);
            if (aa != '\0') {
                trp.SetAa().SetNcbieaa(aa);
                if (!justTrnaText || !NStr::IsBlank(ignored)) {
                    x_AddToComment(feat, product);
                }
                if (NStr::CompareNocase(product, "tRNA-fMet") == 0 ||
                    NStr::CompareNocase(product, "iRNA-fMet") == 0 ||
                    NStr::CompareNocase(product, "tRNA-iMet") == 0 ||
                    NStr::CompareNocase(product, "iRNA-iMet") == 0 ||
                    NStr::CompareNocase(product, "tRNA-Ile2") == 0 ||
                    NStr::CompareNocase(product, "iRNA-Ile2") == 0) {
                    return eAction_Nothing;
                }
                return eAction_Erase;
            }
        }
    }

    if (rna.IsSetExt() && rna.GetExt().IsName() && NStr::Equal(rna.GetExt().GetName(), product)) {
        return eAction_Erase;
    }

    return eAction_Nothing;
}


void CGBQualCleanup::x_AddToComment(CSeq_feat& feat, const string& comment)
{
    m_Parent.AddToComment(feat, comment);
}


void CGBQualCleanup::x_CleanupECNumber( string &ec_num )
{
    m_Parent.CleanupECNumber(ec_num);
}


void CGBQualCleanup::x_MendSatelliteQualifier(string& val)
{
    if ( val.empty() ){
        return;
    }

    if (auto match = ctre::starts_with<"(micro|mini|)satellite">(val)) {
        auto spot_just_after_match = match.get<0>().size();

        if( spot_just_after_match < val.length() &&
            val[spot_just_after_match] == ' ' )
        {
            val[spot_just_after_match] = ':';
            x_ChangeMade(CCleanupChange::eChangeQualifiers);
        }

        // remove spaces after first colon
        size_t pos = NStr::Find(val, ":");
        if (pos != string::npos && (pos < val.length()-1) && isspace(val[pos + 1])) {
            if (ct::search_replace_one<":[ ]+", ":">(val)) {
                x_ChangeMade(CCleanupChange::eChangeQualifiers);
            }
        }
    } else {
        NStr::TruncateSpacesInPlace( val, NStr::eTrunc_Begin );
        val = "satellite:" + val;
        x_ChangeMade(CCleanupChange::eChangeQualifiers);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
