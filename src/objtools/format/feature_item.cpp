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
*          Mati Shomrat
* Maintainer: Frank Ludwig
*                        
* File Description:
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
*
* WHEN EDITING THE LIST OF QUALIFIERS:
*
* - there is currently a lot of parallel logic for the FTable case 
*   (CFeatureItem::x_AddFTableQuals()) and the standard case 
*   (CFeatureItem::x_Add...Quals()). Make sure to edit both cases as 
*   appropriate.
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <algorithm>
#include <sstream>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Heterogen.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seqfeat/Gene_nomenclature.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/RNA_qual_set.hpp>
#include <objects/seqfeat/RNA_qual.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/weight.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <util/static_set.hpp>
#include <util/static_map.hpp>
#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <algorithm>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/format/formatter.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/gene_finder.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/items/qualifiers.hpp>
#include "utils.hpp"
#include "inst_info_map.hpp"

// On Mac OS X 10.3, FixMath.h defines ff as a one-argument macro(!)
#ifdef ff
#  undef ff
#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);

class CGoQualLessThan
{
public:
    bool operator() ( const CConstRef<CFlatGoQVal> &obj1, const CConstRef<CFlatGoQVal> &obj2 ) 
    {
        const CFlatGoQVal *qval1 = obj1.GetNonNullPointer();
        const CFlatGoQVal *qval2 = obj2.GetNonNullPointer();
        
        // sort by text string
        const string &str1 = qval1->GetTextString(); 
        const string &str2 = qval2->GetTextString();

        int textComparison = 0;

        // This whole paragraph should eventually be replaced with a mere NStr::CompareNocase stored into textComparison
        // We can't just use NStr::CompareNocase, because that compares using tolower, whereas
        // we must compare with toupper to maintain compatibility with C.
        SIZE_TYPE pos = 0;
        const SIZE_TYPE min_length = min( str1.length(), str2.length() );
        for( ; pos < min_length; ++pos ) {
            textComparison = toupper( str1[pos] ) - toupper( str2[pos] );
            if( textComparison != 0 ) {
                break;
            }
        }
        if( 0 == textComparison ) {
            // if we reached the end, compare via length (shorter first)
            textComparison = str1.length() - str2.length();
        }

        // compare by text, if possible
        if( textComparison < 0 ) {
            return true;
        } else if( textComparison > 0 ) {
            return false;
        }

        // if text is tied, then sort by pubmed id, if any
        int pmid1 = qval1->GetPubmedId();
        int pmid2 = qval2->GetPubmedId();

        if( 0 == pmid1 ) {
            return false;
        } else if( 0 == pmid2 ) {
            return true;
        } else {
            return pmid1 < pmid2;
        }
    }
};

// -- static functions

static bool s_ValidId(const CSeq_id& id)
{
    return id.IsGenbank()  ||  id.IsEmbl()    ||  id.IsDdbj()  ||
           id.IsOther()    ||  id.IsPatent()  ||  
           id.IsTpg()      ||  id.IsTpe()     ||  id.IsTpd()   ||
           id.IsGpipe();
}

static
bool s_StrEqualDisregardFinalPeriod( 
    const string &s1, const string &s2, 
    NStr::ECase use_case )
{
    if( s1.empty() || s2.empty() ) {
        return s1.empty() && s2.empty();
    }

    // set length to disregard final period, if any
    size_t s1_len = s1.length();
    if( s1[s1_len-1] == '.' ) {
        --s1_len;
    }
    size_t s2_len = s2.length();
    if( s2[s2_len-1] == '.' ) {
        --s2_len;
    }

    if( s1_len != s2_len ) {
        return false;
    }

    // NStr::Equal does not have exactly the function I want,
    // so I have to make my own.
    for( size_t ii = 0; ii < s1_len ; ++ii ) {
        const char ch1 = ( use_case == NStr::eNocase ? toupper(s1[ii]) : s1[ii] );
        const char ch2 = ( use_case == NStr::eNocase ? toupper(s2[ii]) : s2[ii] );
        if( ch1 != ch2 ) {
            return false;
        }
    }
    return true;
}

static bool s_CheckQuals_cdregion(const CMappedFeat& feat,
                                  const CSeq_loc& loc,
                                  CBioseqContext& ctx)
{
    if ( !ctx.Config().CheckCDSProductId() ) {
        return true;
    }
    
    CScope& scope = ctx.GetScope();

    // non-pseudo CDS must have /product
    bool pseudo = feat.IsSetPseudo()  &&  feat.GetPseudo() ;
    if ( !pseudo ) {
        const CGene_ref* grp = feat.GetGeneXref();
        if ( grp == NULL ) {
            CConstRef<CSeq_feat> gene = GetOverlappingGene(loc, scope);
            if (gene) {
                pseudo = gene->IsSetPseudo()  &&  gene->GetPseudo();
                if ( !pseudo ) {
                    grp = &(gene->GetData().GetGene());
                }
            }
        }
        if ( !pseudo  &&  grp != NULL ) {
            pseudo = grp->GetPseudo();
        }
    }

    bool just_stop = false;
    const CSeq_loc& Loc = feat.GetLocation();
    if ( Loc.IsPartialStart(eExtreme_Biological)  &&  !Loc.IsPartialStop(eExtreme_Biological) ) {
        if ( GetLength(Loc, &scope) <= 5 ) {
            just_stop = true;
        }
    }

    if ( pseudo ||  just_stop ) {
        return true;
    } 

    // make sure the product has a valid accession
    if (feat.IsSetProduct()) {
        CConstRef<CSeq_id> id;
        try {
            id.Reset(&(GetId(feat.GetProduct(), &scope)));
        } catch ( CException& ) {
            id.Reset(NULL);
        }
        if (id) {
            if ((id->IsGi()  &&  id->GetGi() > ZERO_GI) ||  id->IsLocal()) {
                CBioseq_Handle prod = scope.GetBioseqHandleFromTSE(*id, ctx.GetHandle());
                if (prod) {
                    ITERATE (CBioseq_Handle::TId, it, prod.GetId()) {
                        if (s_ValidId(*it->GetSeqId())) {
                            CConstRef<CTextseq_id> tsip(it->GetSeqId()->GetTextseq_Id());
                            if (tsip  &&  tsip->IsSetAccession()  &&
                                IsValidAccession(tsip->GetAccession())) {
                                return true;
                            }
                        }
                    }
                } else if (id->IsGi()  &&  id->GetGi() > ZERO_GI) {
                    // RELEASE_MODE requires that /protein_id is an accession
                    if (ctx.Config().IsModeRelease()) {
                        try {
                            if (IsValidAccession(GetAccessionForGi(id->GetGi(), scope))) {
                                return true;
                            }
                        } catch (CException&) {
                        }
                    }
                }
            } else if (s_ValidId(*id)) {
                CConstRef<CTextseq_id> tsip(id->GetTextseq_Id());
                if (tsip  &&  tsip->IsSetAccession()  &&
                    IsValidAccession(tsip->GetAccession())) {
                    return true;
                }
            }
        }
    } else {  // no product
        if (feat.IsSetExcept()  &&  feat.GetExcept()  &&
            feat.IsSetExcept_text() ) {
            if (NStr::Find(feat.GetExcept_text(),
                    "rearrangement required for product") != NPOS) {
                return true;
            }
        }
    }

    return false;
}



static bool s_HasPub(const CMappedFeat& feat, CBioseqContext& ctx)
{
    ITERATE(CBioseqContext::TReferences, it, ctx.GetReferences()) {
        if ((*it)->Matches(feat.GetCit())) {
            return true;
        }
    }

    return false;
}


static bool s_HasCompareOrCitation(const CMappedFeat& feat, CBioseqContext& ctx)
{
    // check for /compare
    if (!NStr::IsBlank(feat.GetNamedQual("compare"))) {
        return true;
    }

    // check for /citation
    if (feat.IsSetCit()) {
        return s_HasPub(feat, ctx);
    }

    return false;
}


// conflict requires /citation or /compare
static bool s_CheckQuals_conflict(const CMappedFeat& feat, CBioseqContext& ctx)
{
    // RefSeq allows conflict with accession in comment instead of sfp->cit
    if (ctx.IsRefSeq()  &&
        feat.IsSetComment()  &&  !NStr::IsBlank(feat.GetComment())) {
        return true;
    }

    return s_HasCompareOrCitation(feat, ctx);
}

// old_sequence requires /citation or /compare
static bool s_CheckQuals_old_seq(const CMappedFeat& feat, CBioseqContext& ctx)
{    
    return s_HasCompareOrCitation(feat, ctx);
}


static bool s_CheckQuals_gene(const CMappedFeat& feat)
{
    // gene requires /gene or /locus_tag, but desc or syn can be mapped to /gene
    const CSeqFeatData::TGene& gene = feat.GetData().GetGene();
    if ( (gene.IsSetLocus()      &&  !gene.GetLocus().empty())      ||
         (gene.IsSetLocus_tag()  &&  !gene.GetLocus_tag().empty())  ||
         (gene.IsSetDesc()       &&  !gene.GetDesc().empty())       ||
         (!gene.GetSyn().empty()  &&  !gene.GetSyn().front().empty()) ) {
        return true;
    }

    return false;
}


static bool s_CheckQuals_bind(const CMappedFeat& feat)
{
    // protein_bind or misc_binding require eFQ_bound_moiety
    return !NStr::IsBlank(feat.GetNamedQual("bound_moiety"));
}


static bool s_CheckQuals_mod_base(const CMappedFeat& feat)
{
    // modified_base requires eFQ_mod_base
    return !NStr::IsBlank(feat.GetNamedQual("mod_base"));
}


static bool s_CheckQuals_gap(const CMappedFeat& feat)
{
    // gap feature must have /estimated_length qual
    return !feat.GetNamedQual("estimated_length").empty();
}

static bool s_CheckQuals_assembly_gap(const CMappedFeat& feat)
{
    // assembly_gap feature must have /estimated_length qual
    // and /gap_type
    return ! feat.GetNamedQual("estimated_length").empty() &&
        ! feat.GetNamedQual("gap_type").empty();
}


static bool s_CheckQuals_ncRNA(const CMappedFeat& feat)
{
    if( !NStr::IsBlank(feat.GetNamedQual("ncRNA_class")) ) {
        return true;
    }

    // Look at this mess; if only we could use sequence_macros.hpp
    if( feat.GetData().GetRna().IsSetExt() && 
        feat.GetData().GetRna().GetExt().IsGen() && 
        feat.GetData().GetRna().GetExt().GetGen().IsSetClass() && 
        !NStr::IsBlank(feat.GetData().GetRna().GetExt().GetGen().GetClass()) )
    {
        return true;
    }

    return false;
}

static bool s_CheckMandatoryQuals(const CMappedFeat& feat,
                                  const CSeq_loc& loc,
                                  CBioseqContext& ctx)
{
    switch ( feat.GetData().GetSubtype() ) {
    case CSeqFeatData::eSubtype_cdregion:
        {
            return s_CheckQuals_cdregion(feat, loc, ctx);
        }
    case CSeqFeatData::eSubtype_conflict:
        {
            return s_CheckQuals_conflict(feat, ctx);
        }
    case CSeqFeatData::eSubtype_old_sequence:
        {
            return s_CheckQuals_old_seq(feat, ctx);
        }
    case CSeqFeatData::eSubtype_gene:
        {
            return s_CheckQuals_gene(feat);
        }
    case CSeqFeatData::eSubtype_protein_bind:
    case CSeqFeatData::eSubtype_misc_binding:
        {
            return s_CheckQuals_bind(feat);
        }
    case CSeqFeatData::eSubtype_modified_base:
        {
            return s_CheckQuals_mod_base(feat);
        }
    case CSeqFeatData::eSubtype_gap:
        {
            return s_CheckQuals_gap(feat);
        }
    case CSeqFeatData::eSubtype_assembly_gap:
        {
            return s_CheckQuals_assembly_gap(feat);
        }
    case CSeqFeatData::eSubtype_ncRNA:
        {
            return s_CheckQuals_ncRNA(feat);
        }
    default:
        break;
    }

    return true;
}

static bool s_SkipFeature(const CMappedFeat& feat,
                          const CSeq_loc& loc,
                          CBioseqContext& ctx)
{
    CSeqFeatData::E_Choice type    = feat.GetData().Which();
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();        

    if ( subtype == CSeqFeatData::eSubtype_pub              ||
         subtype == CSeqFeatData::eSubtype_non_std_residue  ||
         subtype == CSeqFeatData::eSubtype_biosrc           ||
         subtype == CSeqFeatData::eSubtype_rsite            ||
         subtype == CSeqFeatData::eSubtype_seq ) {
        return true;
    }
    
    const CFlatFileConfig& cfg = ctx.Config();

    // check feature customization flags
    if ( cfg.ValidateFeatures()  &&
        (subtype == CSeqFeatData::eSubtype_bad  ||
         subtype == CSeqFeatData::eSubtype_virion) ) {
        return true;
    }
    
    if ( cfg.ValidateFeatures() && type == CSeqFeatData::e_Imp ) {
        switch ( subtype ) {
        default:
            break;
        case CSeqFeatData::eSubtype_imp:
        case CSeqFeatData::eSubtype_site_ref:
        case CSeqFeatData::eSubtype_gene:
        case CSeqFeatData::eSubtype_mutation:
        case CSeqFeatData::eSubtype_allele:
            return true;
        }
    }
    
    if ( ctx.IsNuc()  &&  subtype == CSeqFeatData::eSubtype_het ) {
        return true;
    }
    
    if ( cfg.HideImpFeatures()  &&  type == CSeqFeatData::e_Imp ) {
        return true;
    }
    
    if ( cfg.HideSNPFeatures()  &&  subtype == CSeqFeatData::eSubtype_variation ) {
        return true;
    }

    if ( cfg.HideExonFeatures()  &&  subtype == CSeqFeatData::eSubtype_exon ) {
        return true;
    }

    if ( cfg.HideIntronFeatures()  &&  subtype == CSeqFeatData::eSubtype_intron ) {
        return true;
    }

    if ( cfg.HideRemoteImpFeatures()  &&  type == CSeqFeatData::e_Imp ) {
        if ( subtype == CSeqFeatData::eSubtype_variation  ||
             subtype == CSeqFeatData::eSubtype_exon       ||
             subtype == CSeqFeatData::eSubtype_intron     ||
             subtype == CSeqFeatData::eSubtype_misc_feature ) {
            return true;
        }
    }

    // skip genes in DDBJ format
    if ( cfg.IsFormatDDBJ()  &&  type == CSeqFeatData::e_Gene ) {
        return true;
    }

    // if RELEASE mode, make sure we have all info to create mandatory quals.
    if ( cfg.NeedRequiredQuals() ) {
        return !s_CheckMandatoryQuals(feat, loc, ctx);
    }

    return false;
}

class BadECNumberChar {
public:
    bool operator()( const char ch ) 
    {
        return( ! isdigit(ch) && ch != '.' && ch != '-' );
    }
};

// acceptable patterns are: (This might not be true anymore.  Check the code. )
// num.num.num.num
// num.num.num.-
// num.num.-.-
// num.-.-.-
// -.-.-.-
// (You can use "n" instead of "-" )
static bool s_IsLegalECNumber(const string& ec_number)
{
  if ( ec_number.empty() ) return false;

  bool is_ambig = false;
  int numperiods = 0;
  int numdigits = 0;
  int numdashes = 0;

  ITERATE( string, ec_iter, ec_number ) {
    if ( isdigit(*ec_iter) ) {
      numdigits++;
      if (is_ambig) return false;
    } else if (*ec_iter == '-' ) {
      numdashes++;
      is_ambig = true;
    } else if( *ec_iter == 'n') {
        string::const_iterator ec_iter_next = ec_iter;
        ++ec_iter_next;
        if( ec_iter_next != ec_number.end() && numperiods == 3 && numdigits == 0 && isdigit(*ec_iter_next) ) {
            // allow/ignore n in first position of fourth number to not mean ambiguous, if followed by digit
        } else {
            numdashes++;
            is_ambig = true;
        }
    } else if (*ec_iter == '.') {
      numperiods++;
      if (numdigits > 0 && numdashes > 0) return false;
      if (numdigits == 0 && numdashes == 0) return false;
      if (numdashes > 1) return false;
      numdigits = 0;
      numdashes = 0;
    }
  }

  if (numperiods == 3) {
    if (numdigits > 0 && numdashes > 0) return false;
    if (numdigits > 0 || numdashes == 1) return true;
  }

  return false;
}


static const string& s_GetBondName(CSeqFeatData::TBond bond)
{
    static const string kOther = "unclassified";
    return (bond == CSeqFeatData::eBond_other) ? kOther :
        CSeqFeatData::ENUM_METHOD_NAME(EBond)()->FindName(bond, true);
}
                                    
static void s_QualVectorToNote( 
    const CFlatFeature::TQuals& qualVector,
    bool noRedundancy,
    string& note, 
    string& punctuation,
    bool& addPeriod)
{
    // is there at least one note which is more than blank or a period?
    bool hasSubstantiveNote = false;
    // store this so we can chop off the extra stuff we added if there was no note of substance
    const string::size_type original_length = note.length();

    string prefix;
    ITERATE (CFlatFeature::TQuals, it, qualVector) {
        const string& qual = (*it)->GetValue();

        prefix.erase();
        if ( !note.empty() ) {
            prefix = punctuation;
            const string& next_prefix = (*it)->GetPrefix();
            if (!NStr::EndsWith(prefix, '\n') ) {
                prefix += next_prefix;
            }
        }

        if( !qual.empty() && qual != "." ) {
            hasSubstantiveNote = true;
        }

        // A qual may declare that it be shown even if redundant and override the
        // given noRedundancy variable
        const bool noRedundancyThisIteration = 
            ( 0 != ( (*it)->GetFlags() & CFormatQual::fFlags_showEvenIfRedund ) ? false : noRedundancy );
        JoinString(note, prefix, qual, noRedundancyThisIteration );

        addPeriod = (*it)->GetAddPeriod();
        punctuation = (*it)->GetSuffix();
    }

    // if there was no meaningful note, we clear it
    if( ! hasSubstantiveNote ) {
        note.resize( original_length );
    }
}


static void s_NoteFinalize(
   bool addPeriod,
   string& noteStr,
   CFlatFeature& flatFeature,
   ETildeStyle style = eTilde_newline ) {
   
    if (!noteStr.empty()) {
        if (addPeriod  &&  !NStr::EndsWith(noteStr, ".")) {

            AddPeriod(noteStr);
        }
        // Policy change: expand tilde on both descriptors and features
        ExpandTildes(noteStr, style);
        TrimSpacesAndJunkFromEnds( noteStr, true );

        CRef<CFormatQual> note(new CFormatQual("note", noteStr));
        flatFeature.SetQuals().push_back(note);
    }
}

static int s_GetOverlap(const CMappedFeat& feat ) 
{
    if (feat) {
        int total_length = 0;
        ITERATE( CSeq_loc, loc_iter, feat.GetLocation() ) {
            total_length += loc_iter.GetRange().GetLength();
        }
        return total_length;
    }
    return 0;
}


///
///  The best protein feature is defined as the one that has the most overlap
///  with the given DNA.
///  If there is a tie between two protein features in overlap then the one
///  with the lesser processing status is declared the winner.
///
static CMappedFeat s_GetBestProtFeature(const CBioseq_Handle& seq)
{
    SAnnotSelector sel(CSeqFeatData::e_Prot);
    sel.SetLimitTSE(seq.GetTSE_Handle());

    CMappedFeat best;
    CProt_ref::TProcessed best_processed = CProt_ref::eProcessed_transit_peptide;
    int best_overlap = 0;

    for (CFeat_CI it(seq, sel);  it;  ++it) {

        if ( !best ) {

            best = *it;
            best_processed = it->GetData().GetProt().GetProcessed();
            best_overlap = s_GetOverlap(best);

        } else {
            
            int current_overlap = s_GetOverlap(*it);
            CProt_ref::TProcessed current_processed = it->GetData().GetProt().GetProcessed();

            if ( best_overlap < current_overlap ) {

                best_overlap = current_overlap;
                best_processed = current_processed;
                best = *it;

            } else if ( (best_overlap == current_overlap) && (best_processed > current_processed) ) {
            
                best_processed = current_processed;
                best = *it;
            }
        }
    }
    return best;
}

// -- FeatureHeader

CFeatHeaderItem::CFeatHeaderItem(CBioseqContext& ctx) : CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
}


void CFeatHeaderItem::x_GatherInfo(CBioseqContext& ctx)
{
    if ( ctx.Config().IsFormatFTable() ) {
        m_Id.Reset(ctx.GetPrimaryId());
    }
}

static bool s_CheckFuzz(const CInt_fuzz& fuzz)
{
    return !(fuzz.IsLim()  &&  fuzz.GetLim() == CInt_fuzz::eLim_unk);
}

static bool s_LocIsFuzz(const CMappedFeat& feat, const CSeq_loc& loc)
{
    if ( feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_imp  &&
         feat.GetData().IsImp() ) {  // unmappable impfeats
        const CSeqFeatData::TImp& imp = feat.GetData().GetImp();
        if ( imp.IsSetLoc() ) {
            const string& imploc = imp.GetLoc();
            if ( imploc.find('<') != NPOS  ||  imploc.find('>') != NPOS ) {
                return true;
            }
        }
    } else {    // any regular feature test location for fuzz
        for ( CSeq_loc_CI it(loc, CSeq_loc_CI::eEmpty_Allow); it; ++it ) {
            const CSeq_loc& l = it.GetEmbeddingSeq_loc();
            switch ( l.Which() ) {
            case CSeq_loc::e_Pnt:
            {{
                if ( l.GetPnt().IsSetFuzz() ) {
                    if ( s_CheckFuzz(l.GetPnt().GetFuzz()) ) {
                        return true;
                    }
                }
                break;
            }}
            case CSeq_loc::e_Packed_pnt:
            {{
                if ( l.GetPacked_pnt().IsSetFuzz() ) {
                    if ( s_CheckFuzz(l.GetPacked_pnt().GetFuzz()) ) {
                        return true;
                    }
                }
                break;
            }}
            case CSeq_loc::e_Int:
            {{
                bool fuzz = false;
                if ( l.GetInt().IsSetFuzz_from() ) {
                    fuzz = s_CheckFuzz(l.GetInt().GetFuzz_from());
                }
                if ( !fuzz  &&  l.GetInt().IsSetFuzz_to() ) {
                    fuzz = s_CheckFuzz(l.GetInt().GetFuzz_to());
                }
                if ( fuzz ) {
                    return true;
                }
                break;
            }}
            case CSeq_loc::e_Packed_int:
            {{  
                if ( l.GetPacked_int().IsPartialStart(eExtreme_Biological) 
                  || l.GetPacked_int().IsPartialStop(eExtreme_Biological) ) {
                    return true;
                }
                break;
            }}
            case CSeq_loc::e_Null:
            {{
                return true;
            }}
            default:
                break;
            }
        }
    }

    return false;
}

static void s_AddPcrPrimersQualsAppend( string &output, const string &name, const string &str )
{
    if( ! str.empty() ) {
        if( ! output.empty() ) {
            output += ", ";
        }
        output += name + str;
    }
}

// This splits a string that's comma-separated with parens at start and end
// (or, string might just contain a single string, so no splitting is needed,
// in which case the output_vec will be of size 1)
static void s_SplitCommaSeparatedStringInParens( vector<string> &output_vec, const string &string_to_split )
{
    // nothing to do since no input
    if( string_to_split.empty() ) {
        return;
    }

    // no splitting required
    if( string_to_split[0] != '(' ) {
        output_vec.push_back( string_to_split );
        return;
    }

    // if ends with closing paren, chop that off.  
    // ( It's actually a data error if we DON'T end with a ')', but we continue anyway, since
    // we want to do the best we can with the data we get. )
    size_t amount_to_chop_off_end = 0;
    if( string_to_split[string_to_split.length() - 1] == ')' ) {
        amount_to_chop_off_end = 1;
    }
    
    NStr::Tokenize( string_to_split.substr( 1, string_to_split.length() - amount_to_chop_off_end - 1), ",", output_vec );
}

static const char* const sc_ValidPseudoGene[] = {
    "allelic",
    "processed",
    "unitary",
    "unknown",
    "unprocessed"
};
typedef CStaticArraySet<const char*, PNocase> TLegalPseudoGeneText;
DEFINE_STATIC_ARRAY_MAP(TLegalPseudoGeneText, sc_ValidPseudoGeneText, sc_ValidPseudoGene );

static bool s_IsValidPseudoGene( objects::CFlatFileConfig::TMode mode, const string& text)
{
    switch(mode)
    {
    case objects::CFlatFileConfig::eMode_Release:
    case objects::CFlatFileConfig::eMode_Entrez:
        return sc_ValidPseudoGeneText.find(text.c_str()) != sc_ValidPseudoGeneText.end();
    default:
        return ! text.empty();
    }
}

static const char* const sc_ValidExceptionText[] = {
    "annotated by transcript or proteomic data",
    "rearrangement required for product",
    "reasons given in citation",
    "RNA editing"
};
typedef CStaticArraySet<const char*, PNocase_CStr> TLegalExceptText;
DEFINE_STATIC_ARRAY_MAP(TLegalExceptText, sc_LegalExceptText, sc_ValidExceptionText);

static bool s_IsValidExceptionText(const string& text)
{
    return sc_LegalExceptText.find(text.c_str()) != sc_LegalExceptText.end();
}


static const char* const sc_ValidRefSeqExceptionText[] = {
    "adjusted for low-quality genome",
    "alternative processing",
    "alternative start codon",
    "artificial frameshift",
    "dicistronic gene",
    "mismatches in transcription",
    "mismatches in translation",
    "modified codon recognition",
    "nonconsensus splice site",
    "transcribed product replaced",
    "transcribed pseudogene",
    "translated product replaced",
    "unclassified transcription discrepancy",
    "unclassified translation discrepancy",
    "unextendable partial coding region"
};
typedef CStaticArraySet<const char*, PNocase> TLegalRefSeqExceptText;
DEFINE_STATIC_ARRAY_MAP(TLegalRefSeqExceptText, sc_LegalRefSeqExceptText, sc_ValidRefSeqExceptionText);

static bool s_IsValidRefSeqExceptionText(const string& text)
{
    return sc_LegalRefSeqExceptText.find(text.c_str()) != sc_LegalRefSeqExceptText.end();
}

bool s_GetGbValue( CConstRef<CSeq_feat> feat, const string& key, string& value )
{
    if ( ! feat->IsSetQual() ) {
        return false;
    }
    const CSeq_feat_Base::TQual & qual = feat->GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
    ITERATE( CSeq_feat::TQual, it, qual ) {
        if (!(*it)->IsSetQual()  ||  !(*it)->IsSetVal()) {
            continue;
        }
        if ( (*it)->GetQual() != key ) {
            continue;
        }
        value = (*it)->GetVal(); 
        return true;
    }
    return false;
}


// -- FeatureItemBase

CFeatureItemBase::CFeatureItemBase
(const CMappedFeat& feat,
 CBioseqContext& ctx,
 CRef<feature::CFeatTree> ftree,
 const CSeq_loc* loc) :
    CFlatItem(&ctx), m_Feat(feat), m_Feat_Tree(ftree), m_Loc(loc ? loc :
                                         (feat ? &feat.GetLocation() : NULL))
{
    if (m_Feat) {
        x_SetObject(m_Feat.GetOriginalFeature());
    }
}

CConstRef<CFlatFeature> CFeatureItemBase::Format(void) const
{
    CRef<CFlatFeature> ff(new CFlatFeature(GetKey(),
                          *new CFlatSeqLoc(GetLoc(), *GetContext()),
                          m_Feat));
    if ( ff ) {
        x_FormatQuals(*ff);
    }
    return ff;
}


//  -- CFeatureItem

string CFeatureItem::GetKey(void) const
{
    CBioseqContext& ctx = *GetContext();

    CSeqFeatData::E_Choice type = m_Feat.GetData().Which();
    CSeqFeatData::ESubtype subtype = m_Feat.GetData().GetSubtype();

    if (GetContext()->IsProt()) {   // protein
        if ( IsMappedFromProt()  &&  type == CSeqFeatData::e_Prot ) {
            if ( subtype == CSeqFeatData::eSubtype_preprotein         ||
                 subtype == CSeqFeatData::eSubtype_mat_peptide_aa     ||
                subtype == CSeqFeatData::eSubtype_sig_peptide_aa     ||
                subtype == CSeqFeatData::eSubtype_transit_peptide_aa ) {
                return "Precursor";
            } 
        }
        switch ( subtype ) {
        case CSeqFeatData::eSubtype_region:
            return "Region";
        case CSeqFeatData::eSubtype_bond:
            return "Bond";
        case CSeqFeatData::eSubtype_site:
            return "Site";
        default:
            break;
        }
    } else {  // nucleotide
        switch ( subtype ) {

        case CSeqFeatData::eSubtype_ncRNA:
            return "ncRNA";

        case CSeqFeatData::eSubtype_tmRNA:
            return "tmRNA";

        case CSeqFeatData::eSubtype_preprotein:
            if ( !ctx.IsRefSeq() ) {
                return "misc_feature";
            }
            break;

        case CSeqFeatData::eSubtype_site:
        case CSeqFeatData::eSubtype_bond:
        case CSeqFeatData::eSubtype_region:
        case CSeqFeatData::eSubtype_comment:
            return "misc_feature";

        default:
            break;
        }
    }

    // deal with unmappable impfeats
    if (subtype == CSeqFeatData::eSubtype_imp  &&  type == CSeqFeatData::e_Imp) {
        const CSeqFeatData::TImp& imp = m_Feat.GetData().GetImp();
        if ( imp.IsSetKey() ) {
            return imp.GetKey();
        }
    }

    return CFeatureItemBase::GetKey();
}


// constructor from CSeq_feat
CFeatureItem::CFeatureItem
(const CMappedFeat& feat,
 CBioseqContext& ctx,
 CRef<feature::CFeatTree> ftree,
 const CSeq_loc* loc,
 EMapped mapped,
 CConstRef<CFeatureItem> parentFeatureItem) :
    CFeatureItemBase(feat, ctx, ftree, loc), m_Mapped(mapped)
{
    x_GatherInfoWithParent(ctx, parentFeatureItem);
}


void CFeatureItem::x_GatherInfoWithParent(CBioseqContext& ctx, CConstRef<CFeatureItem> parentFeatureItem )
{
    if ( s_SkipFeature(GetFeat(), GetLoc(), ctx) ) {
        x_SetSkip();
        return;
    }
    m_Type = m_Feat.GetData().GetSubtype();
    x_AddQuals(ctx, parentFeatureItem );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualPartial(
    CBioseqContext& ctx )
//
//  Note: /partial has been depricated since DEC-2001. Current policy is to 
//  suppress /partial in entrez and release modes and let it stand in gbench and 
//  dump modes
//  ----------------------------------------------------------------------------
{
    if ( !ctx.Config().HideUnclassPartial() ) {
        if ( !IsMappedFromCDNA() || !ctx.IsProt() ) {
            if ( m_Feat.IsSetPartial()  &&  m_Feat.GetPartial() ) {
                if ( eSeqlocPartial_Complete == sequence::SeqLocPartialCheck( GetLoc(), &ctx.GetScope() ) &&
                    !s_LocIsFuzz( m_Feat, GetLoc() ) ) 
                {
                    x_AddQual( eFQ_partial, new CFlatBoolQVal( true ) );
                }
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualOperon(
    CBioseqContext& ctx,
    CSeqFeatData::ESubtype subtype )
//  ----------------------------------------------------------------------------
{
    if ( subtype == CSeqFeatData::eSubtype_operon ||
         subtype == CSeqFeatData::eSubtype_gap ) {
        return;
    }

    // bail if this type of object is not allowed to carry an operon
    if( ! x_IsSeqFeatDataFeatureLegal( CSeqFeatData::eQual_operon ) ) {
        return;
    }

    const CGene_ref* gene_ref = m_Feat.GetGeneXref();
    if ( gene_ref == NULL  ||  !gene_ref->IsSuppressed()) {
            const CSeq_loc& operon_loc = ( ctx.IsProt() || !IsMapped() ) ? 
                m_Feat.GetLocation() : GetLoc();
        CConstRef<CSeq_feat> operon 
            = GetOverlappingOperon( operon_loc, ctx.GetScope() );
        if ( operon ) {
            const string& operon_name = operon->GetNamedQual( "operon" );
            if ( !operon_name.empty() ) {
                x_AddQual(eFQ_operon, new CFlatStringQVal(operon_name));
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualPseudo(
    CBioseqContext& ctx,
    CSeqFeatData::E_Choice type,
    CSeqFeatData::ESubtype subtype,
    bool pseudo )
//  ----------------------------------------------------------------------------
{
    if ( !pseudo || 
        subtype == CSeqFeatData::eSubtype_repeat_region ||
        subtype == CSeqFeatData::eSubtype_mobile_element ||
        subtype == CSeqFeatData::eSubtype_centromere ||
        subtype == CSeqFeatData::eSubtype_telomere ) 
    {
        return;
    }

    if (ctx.Config().DropIllegalQuals()  &&  
        ( type == CSeqFeatData::e_Rna || type == CSeqFeatData::e_Imp ) ) 
    {
        switch (subtype) {
            case  CSeqFeatData::eSubtype_allele:
            case  CSeqFeatData::eSubtype_attenuator:
            case  CSeqFeatData::eSubtype_CAAT_signal:
            case  CSeqFeatData::eSubtype_conflict:
            case  CSeqFeatData::eSubtype_D_loop:
            case  CSeqFeatData::eSubtype_enhancer:
            case  CSeqFeatData::eSubtype_GC_signal:
            case  CSeqFeatData::eSubtype_iDNA:
            case  CSeqFeatData::eSubtype_LTR:
            case  CSeqFeatData::eSubtype_misc_binding:
            case  CSeqFeatData::eSubtype_misc_difference:
            case  CSeqFeatData::eSubtype_misc_recomb:
            case  CSeqFeatData::eSubtype_misc_RNA:
            case  CSeqFeatData::eSubtype_misc_signal:
            case  CSeqFeatData::eSubtype_misc_structure:
            case  CSeqFeatData::eSubtype_modified_base:
            case  CSeqFeatData::eSubtype_mutation:
            case  CSeqFeatData::eSubtype_old_sequence:
            case  CSeqFeatData::eSubtype_polyA_signal:
            case  CSeqFeatData::eSubtype_polyA_site:
            case  CSeqFeatData::eSubtype_precursor_RNA:
            case  CSeqFeatData::eSubtype_prim_transcript:
            case  CSeqFeatData::eSubtype_primer_bind:
            case  CSeqFeatData::eSubtype_protein_bind:
            case  CSeqFeatData::eSubtype_RBS:
            case  CSeqFeatData::eSubtype_repeat_region:
            case  CSeqFeatData::eSubtype_repeat_unit:
            case  CSeqFeatData::eSubtype_rep_origin:
            case  CSeqFeatData::eSubtype_satellite:
            case  CSeqFeatData::eSubtype_stem_loop:
            case  CSeqFeatData::eSubtype_STS:
            case  CSeqFeatData::eSubtype_TATA_signal:
            case  CSeqFeatData::eSubtype_terminator:
            case  CSeqFeatData::eSubtype_unsure:
            case  CSeqFeatData::eSubtype_variation:
            case  CSeqFeatData::eSubtype_3clip:
            case  CSeqFeatData::eSubtype_3UTR:
            case  CSeqFeatData::eSubtype_5clip:
            case  CSeqFeatData::eSubtype_5UTR:
            case  CSeqFeatData::eSubtype_10_signal:
            case  CSeqFeatData::eSubtype_35_signal:
                return;
            default:
                break;
        }
    }
    x_AddQual( eFQ_pseudo, new CFlatBoolQVal( true ) );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualSeqfeatNote(CBioseqContext &ctx)
//  ----------------------------------------------------------------------------
{
    string precursor_comment;
    // set precursor_comment, if needed.
    // It's set from the feature's product's best protein's comment
    if( GetContext()->IsProt() && IsMappedFromProt() && m_Feat.IsSetProduct() ) {
        const CSeq_id* prod_id = m_Feat.GetProduct().GetId();
        if( prod_id != NULL ) {
            CBioseq_Handle prod_bioseq = GetContext()->GetScope().GetBioseqHandle(*prod_id);
            if( prod_bioseq ) {
                CMappedFeat best_prot_feat = s_GetBestProtFeature( prod_bioseq );
                if( best_prot_feat && best_prot_feat.IsSetComment() ) {
                    precursor_comment = best_prot_feat.GetComment() ;
                }
            }
        }
    }

    if (m_Feat.IsSetComment()) {
        string comment = m_Feat.GetComment();

        TrimSpacesAndJunkFromEnds( comment, true );
        if ( ! comment.empty() && comment != "~" && comment != precursor_comment) {
            bool bAddPeriod = RemovePeriodFromEnd( comment, true );
            ConvertQuotes(comment);
            CRef<CFlatStringQVal> seqfeat_note( new CFlatStringQVal( comment ) );
//            if ( bAddPeriod &&  ! x_GetStringQual(eFQ_prot_desc ) ) {
            // careful! Period must be removed if we have a valid eFQ_prot_desc
            // Examples to test some cases: AB001488, M96268
            if ( bAddPeriod ) { 
                seqfeat_note->SetAddPeriod();
            }
            x_AddQual( eFQ_seqfeat_note, seqfeat_note );
        }
    }

    /// also scan the annot to see if there is a comment there, if required
    if( ! ctx.ShowAnnotCommentAsCOMMENT() ) {
        if (m_Feat.GetAnnot().Seq_annot_IsSetDesc()) {
            ITERATE (CSeq_annot::TDesc::Tdata, it,
                m_Feat.GetAnnot().Seq_annot_GetDesc().Get()) {
                    if ((*it)->IsComment()) {
                        const string & comment = (*it)->GetComment();
                        // certain comments require special handling
                        const static string ktRNAscanSE = "tRNA features were annotated by tRNAscan-SE";
                        if( NStr::StartsWith(comment, ktRNAscanSE, NStr::eNocase) && 
                            ! x_HasMethodtRNAscanSE() ) 
                        {
                            // don't propagate tRNAscan-SE comments to irrelevant features
                            continue;
                        }
                        x_AddQual(eFQ_seqfeat_note,
                            new CFlatStringQVal(comment));
                    }
            }
        }
    }

}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualExpInv(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    if ( ! m_Feat.IsSetExp_ev() ) {
        return;
    }

    string value;
    if ( m_Feat.GetExp_ev() == CSeq_feat::eExp_ev_experimental ) {
        if ( ! x_GetGbValue( "experiment", value ) && ! x_GetGbValue( "inference", value ) ) {
            x_AddQual( eFQ_experiment, new CFlatExperimentQVal() );
        }
    }
    else if ( ! x_GetGbValue( "inference", value ) ) {
        x_AddQual(eFQ_inference, new CFlatInferenceQVal( "" ));
    }
}

static
bool s_TransSplicingFeatureAllowed(
    const CSeqFeatData& data )
{
    switch( data.GetSubtype() ) {
        case CSeqFeatData::eSubtype_gene:
        case CSeqFeatData::eSubtype_cdregion:
        case CSeqFeatData::eSubtype_mRNA:
        case CSeqFeatData::eSubtype_tRNA:
        case CSeqFeatData::eSubtype_preRNA:
        case CSeqFeatData::eSubtype_otherRNA:
        case CSeqFeatData::eSubtype_exon:
        case CSeqFeatData::eSubtype_intron:
        case CSeqFeatData::eSubtype_3clip:
        case CSeqFeatData::eSubtype_3UTR:
        case CSeqFeatData::eSubtype_5clip:
        case CSeqFeatData::eSubtype_5UTR:
            return true;
        default:
            return false;
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualExceptions( 
    CBioseqContext& ctx )
//
//  Add any existing exception qualifiers.
//  Note: These include /ribosomal_slippage and /trans-splicing as special 
//  cases. Also, some exceptions are listed as notes.
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data  = m_Feat.GetData();

    string raw_exception;
    const CFlatFileConfig& cfg = ctx.Config();

    if ( ( m_Feat.IsSetExcept() && m_Feat.GetExcept() ) && 
        (m_Feat.IsSetExcept_text()  &&  !m_Feat.GetExcept_text().empty()) ) {
            raw_exception = m_Feat.GetExcept_text();
    }
    if ( raw_exception == "" ) {
        return;
    }

    const bool bIsRefseq = ctx.IsRefSeq();
    const bool bIsRelaxed = ( ! cfg.DropIllegalQuals() );

    list<string> exceptions;
    NStr::Split( raw_exception, ",", exceptions );

    list<string> output_exceptions;
    list<string> output_notes;
    ITERATE( list<string>, it, exceptions ) {
        string cur = NStr::TruncateSpaces( *it );
        if( cur.empty() ) {
            continue;
        }

        //
        //  If exceptions are legal then it depends on the exception. Some are
        //  turned into their own custom qualifiers. Others are allowed to stand
        //  as exceptions, while others are turned into notes.
        //
        if ( s_IsValidExceptionText( cur ) ) {
            if( bIsRefseq || bIsRelaxed || data.IsCdregion() ) {
                output_exceptions.push_back( cur );
            } else {
                output_notes.push_back( cur );
            }
            continue;
        }
        if ( s_IsValidRefSeqExceptionText( cur ) ) {
            if( bIsRefseq || bIsRelaxed ) {
                output_exceptions.push_back( cur );
            } else {
                output_notes.push_back( cur );
            }
            continue;
        }
        if ( NStr::EqualNocase(cur, "ribosomal slippage") ) {
            if( data.IsCdregion() ) {
                x_AddQual( eFQ_ribosomal_slippage, new CFlatBoolQVal( true ) );
            } else {
                output_notes.push_back( cur );
            }
            continue;
        }
        if ( NStr::EqualNocase(cur, "trans-splicing") ) {
            if( s_TransSplicingFeatureAllowed( data ) ) {
                x_AddQual( eFQ_trans_splicing, new CFlatBoolQVal( true ) );
            } else {
                output_notes.push_back( cur );
            }
            continue;
        }
        const bool is_cds_or_mrna = ( data.IsCdregion() || 
            data.GetSubtype() == CSeqFeatData::eSubtype_mRNA );
        if( NStr::EqualNocase(cur, "artificial location") ) {
            if( is_cds_or_mrna ) {
                x_AddQual( eFQ_artificial_location, new CFlatBoolQVal( true ) );
            } else {
                output_notes.push_back( cur );
            }
            continue;
        }
        if( NStr::EqualNocase(cur, "heterogeneous population sequenced") || 
            NStr::EqualNocase(cur, "low-quality sequence region") ) 
        {
            if( is_cds_or_mrna ) {
                x_AddQual( eFQ_artificial_location, new CFlatStringQVal( cur ) );
            } else {
                output_notes.push_back( cur );
            }
            continue;
        }
        else {
            if ( bIsRelaxed ) {
                output_exceptions.push_back( cur );
            }
            else {
                output_notes.push_back( cur );
            }
        }
    }
    if ( ! output_exceptions.empty() ) {
        string exception = NStr::Join( output_exceptions, ", " );
        x_AddQual(eFQ_exception, new CFlatStringQVal( exception ) );
    }
    if ( ! output_notes.empty() ) {
        string note = NStr::Join( output_notes, ", " );
        x_AddQual(eFQ_exception_note, new CFlatStringQVal( note ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualNote(
    CConstRef<CSeq_feat> gene_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! gene_feat || ! gene_feat->IsSetComment() ) {
        return;
    }
    x_AddQual( eFQ_gene_note, new CFlatStringQVal( 
        gene_feat->GetComment() ) );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualGeneXref(
    const CGene_ref* gene_ref, 
    const CConstRef<CSeq_feat>& gene_feat )
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data  = m_Feat.GetData();
    CSeqFeatData::E_Choice type = data.Which();
    
    if ( type == CSeqFeatData::e_Cdregion || type == CSeqFeatData::e_Rna ) {    
        if ( ! gene_ref && gene_feat ) {
            gene_ref = &gene_feat->GetData().GetGene();
            if ( gene_ref != NULL  &&  gene_ref->IsSetDb() ) {
                x_AddQual(
                    eFQ_gene_xref, new CFlatXrefQVal( gene_ref->GetDb() ) );
            } else if ( gene_feat->IsSetDbxref() ) {
                x_AddQual(
                    eFQ_gene_xref, new CFlatXrefQVal( gene_feat->GetDbxref() ) );
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualOldLocusTag(
    CConstRef<CSeq_feat> gene_feat )
//
//  For non-gene features, add /old_locus_tag, if one exists somewhere.
//  ----------------------------------------------------------------------------
{
    if ( ! gene_feat ) {
        return;
    }

    const CSeq_feat::TQual& quals = gene_feat->GetQual();
    for ( size_t iPos = 0; iPos < quals.size(); ++iPos ) {
        CRef< CGb_qual > qual = quals[ iPos ];
        if ( ! qual->IsSetQual() || ! qual->IsSetVal() ) {
            continue;
        }
        if ( qual->GetQual() == "old_locus_tag" ) {
            x_AddQual(eFQ_old_locus_tag, 
                new CFlatStringQVal( qual->GetVal(), CFormatQual::eTrim_WhitespaceOnly ) );            
        }
    }
}

//  ----------------------------------------------------------------------------
bool CFeatureItem::x_GetPseudo(  
    const CGene_ref* gene_ref, 
    const CSeq_feat* gene_feat ) const
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data  = m_Feat.GetData();
    CSeqFeatData::E_Choice type = data.Which();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();
    
    bool pseudo = m_Feat.IsSetPseudo() ? m_Feat.GetPseudo() : false;
    if ( type != CSeqFeatData::e_Gene &&
         subtype != CSeqFeatData::eSubtype_operon &&
         subtype != CSeqFeatData::eSubtype_gap ) 
    {
        if ( gene_feat && gene_feat->IsSetPseudo() && gene_feat->GetPseudo() ) {
            return true;
            const CGene_ref* altref = &gene_feat->GetData().GetGene();
            if ( altref && altref->IsSetPseudo() && altref->GetPseudo() ) {
                return true;
            }
        }
        if ( gene_ref && gene_ref->IsSetPseudo() && gene_ref->GetPseudo() ) {
            return true;
        }
    }
    if ( type == CSeqFeatData::e_Gene ) {
        if ( data.GetGene().IsSetPseudo() && data.GetGene().GetPseudo() ) {
            return true;
        }
    }
    if ( type == CSeqFeatData::e_Rna ) {
        if ( data.GetRna().IsSetPseudo() && data.GetRna().GetPseudo() ) {
            return true;
        }
    }
    return pseudo;
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQuals(
    CBioseqContext& ctx,
    CConstRef<CFeatureItem> parentFeatureItem )
//
//  Add the various qualifiers to this feature. Top level function.
//  ----------------------------------------------------------------------------
{
//    /**fl**/
    // leaving this here since it's so useful for debugging purposes.
    //21822,22172
    /* if( 
        (GetLoc().GetStart(eExtreme_Biological) == 21821 &&
        GetLoc().GetStop(eExtreme_Biological) == 22171) ||
        (GetLoc().GetStop(eExtreme_Biological) == 21821 &&
        GetLoc().GetStart(eExtreme_Biological) == 22171)
        ) {
        cerr << ""; // a do-nothing statement in case we forget to comment it out
        } */
//    /**fl**/

    if ( ctx.Config().IsFormatFTable() ) {
        x_AddFTableQuals( ctx );
        return;
    }

    m_Feat_Tree->AddGenesForFeat (m_Feat);


    //
    //  Collect/Compute data that will be shared between several qualifier
    //  collectors:
    //
    const CSeqFeatData& data  = m_Feat.GetData();
    CSeqFeatData::E_Choice type = data.Which();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();
//  /**fl**/>>
//    if ( subtype == CSeqFeatData::eSubtype_sig_peptide_aa || 
//        subtype == CSeqFeatData::eSubtype_sig_peptide ) 
//    {
//        cerr << "Break" << endl;
//    }
//  <</**fl**/

    // check if this is some kind of Genbank record (some of the logic may be a little different in that case)
    bool is_not_genbank = false;
    {{
        ITERATE( CBioseq::TId, id_iter, ctx.GetBioseqIds() ) {
            const CSeq_id& id = **id_iter;

            switch ( id.Which() ) {
                case CSeq_id_Base::e_Embl:
                case CSeq_id_Base::e_Ddbj:
                case CSeq_id_Base::e_Tpe:
                case CSeq_id_Base::e_Tpd:
                    is_not_genbank = true;
                    break;
                default:
                    // do nothing
                    break;
            }
        }
    }}
    

    const CGene_ref* gene_ref = 0;
    CConstRef<CSeq_feat> gene_feat;

    const bool gene_forbidden_if_genbank = 
        ( subtype == CSeqFeatData::eSubtype_repeat_region || 
          subtype == CSeqFeatData::eSubtype_mobile_element ||
          subtype == CSeqFeatData::eSubtype_centromere ||
          subtype == CSeqFeatData::eSubtype_telomere );
    if ( type != CSeqFeatData::e_Gene &&
         subtype != CSeqFeatData::eSubtype_operon &&
         subtype != CSeqFeatData::eSubtype_gap && 
         (  ! gene_forbidden_if_genbank || is_not_genbank ) )
    {
        CMappedFeat mapped_gene = GetBestGeneForFeat (m_Feat, m_Feat_Tree);
        if (mapped_gene) {
            const CSeq_feat& gene_feat = mapped_gene.GetOriginalFeature();
            gene_ref = &gene_feat.GetData().GetGene();
        }

        /*
        CSeq_feat_Handle parent_feat_handle;
        if( parentFeatureItem ) {
            parent_feat_handle = parentFeatureItem->GetFeat();
        }
        CGeneFinder::GetAssociatedGeneInfo( m_Feat, ctx, m_Loc, m_GeneRef, gene_ref, 
            gene_feat, parent_feat_handle );
        */
    } else if( ! is_not_genbank && gene_forbidden_if_genbank ) {
        // We include a gene_ref on the genbank-forbidden features if there's
        // an explicit xref and the referenced gene does not exist
        // e.g. NC_014095.1
        const CGene_ref* feat_gene_xref = m_Feat.GetGeneXref();
        if( feat_gene_xref && ! CGeneFinder::ResolveGeneXref(feat_gene_xref, ctx.GetTopLevelEntry()) ) {
            gene_ref = feat_gene_xref;
        }
    }
    bool pseudo = x_GetPseudo(gene_ref, gene_feat );

    //
    //  Collect qualifiers that are specific to a single or just a few feature
    //  types:
    //
    switch ( type ) {
    case CSeqFeatData::e_Cdregion:
        x_AddQualsCdregion(m_Feat, ctx, pseudo);
        break;
    case CSeqFeatData::e_Rna:
        x_AddQualsRna(m_Feat, ctx, pseudo);
        break;
    case CSeqFeatData::e_Prot:
        x_AddQualsProt(ctx, pseudo);
        break;
    case CSeqFeatData::e_Region:
        x_AddQualsRegion( ctx );
        break;
    case CSeqFeatData::e_Site:
        x_AddQualsSite( ctx );
        break;
    case CSeqFeatData::e_Bond:
        x_AddQualsBond( ctx );
        break;    
    case CSeqFeatData::e_Psec_str:
        x_AddQualsPsecStr( ctx );
        break;
    case CSeqFeatData::e_Het:
        x_AddQualsHet( ctx );
        break;
    case CSeqFeatData::e_Variation:
        x_AddQualsVariation( ctx );
        break;
    default:
        break;
    }

    //
    //  Collect qualifiers that are common to most feature types:
    //
    x_AddQualPartial( ctx );
    x_AddQualDbXref( ctx );
    x_AddQualExt();
    x_AddQualExpInv( ctx );
    x_AddQualCitation();
    x_AddQualExceptions( ctx );
    x_AddQualNote( gene_feat );
    x_AddQualOldLocusTag( gene_feat );
    x_AddQualDb( gene_ref );
    x_AddQualGeneXref( gene_ref, gene_feat );
    x_AddQualOperon( ctx, subtype );
    x_AddQualsGene( gene_ref, gene_feat, gene_ref ? false : gene_feat.NotEmpty() );

    x_AddQualPseudo( ctx, type, subtype, pseudo );
    x_AddQualSeqfeatNote(ctx);
    x_AddQualsGb( ctx );

    // cleanup (drop illegal quals, duplicate information etc.)
    x_CleanQuals( gene_ref );
}


static const string s_TrnaList[] = {
  "tRNA-Gap",
  "tRNA-Ala",
  "tRNA-Asx",
  "tRNA-Cys",
  "tRNA-Asp",
  "tRNA-Glu",
  "tRNA-Phe",
  "tRNA-Gly",
  "tRNA-His",
  "tRNA-Ile",
  "tRNA-Xle",
  "tRNA-Lys",
  "tRNA-Leu",
  "tRNA-Met",
  "tRNA-Asn",
  "tRNA-Pyl",
  "tRNA-Pro",
  "tRNA-Gln",
  "tRNA-Arg",
  "tRNA-Ser",
  "tRNA-Thr",
  "tRNA-Sec",
  "tRNA-Val",
  "tRNA-Trp",
  "tRNA-OTHER",
  "tRNA-Tyr",
  "tRNA-Glx",
  "tRNA-TERM"
};


static const string& s_AaName(int aa)
{
    int idx = 255;

    if (aa != '*') {
        idx = aa - 64;
    } else {
        idx = 27;   
    }
    if ( idx > 0 && idx < ArraySize(s_TrnaList) ) {
        return s_TrnaList [idx];
    }
    return kEmptyStr;
}


static int s_ToIupacaa(int aa)
{
    vector<char> n(1, static_cast<char>(aa));
    vector<char> i;
    CSeqConvert::Convert(n, CSeqUtil::e_Ncbieaa, 0, 1, i, CSeqUtil::e_Iupacaa);
    return i.front();
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsRna(
    const CMappedFeat& feat,
    CBioseqContext& ctx,
    bool pseudo )
//  ----------------------------------------------------------------------------
{

    CSeqFeatData::ESubtype subtype = m_Feat.GetData().GetSubtype();
    const CRNA_ref& rna = feat.GetData().GetRna();
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();

    ///
    /// always output transcript_id
    ///
    {{
        EFeatureQualifier slot = 
            (ctx.IsRefSeq()  ||  cfg.IsModeDump()  ||  cfg.IsModeGBench()) ?
                eFQ_transcript_id : eFQ_transcript_id_note;
        try {
            if (feat.IsSetProduct()) {
                CConstRef<CSeq_id> sip(feat.GetProduct().GetId());
                if (sip) {
                    CBioseq_Handle prod = 
                        scope.GetBioseqHandleFromTSE(*sip, ctx.GetHandle());
                    if ( prod ) {
                        x_AddProductIdQuals(prod, slot);
                    } else {
                        if (sip->IsGi()) {
                            string acc = GetAccessionForGi(sip->GetGi(), scope);
                            if( acc.empty() && ! cfg.DropIllegalQuals() ) {
                                x_AddQual(slot, new CFlatStringQVal(
                                    NStr::NumericToString(sip->GetGi()) ) );
                            } else {
                                if ( !cfg.DropIllegalQuals()  ||  IsValidAccession(acc)) {
                                    CRef<CSeq_id> acc_id(new CSeq_id(acc));
                                    x_AddQual(slot, new CFlatSeqIdQVal(*acc_id));
                                }
                            }
                            x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(*sip, true));
                        }
                    }
                }
            }
        }
        catch (CObjmgrUtilException&) {
        }
     }}

    CRNA_ref::TType rna_type = rna.IsSetType() ?
        rna.GetType() : CRNA_ref::eType_unknown;
    switch ( rna_type ) {
    case CRNA_ref::eType_tRNA:
    {
        if ( !pseudo  &&  cfg.ShowTranscript() ) {
            CSeqVector vec(feat.GetLocation(), scope);
            vec.SetCoding(CBioseq_Handle::eCoding_Iupac);
            string transcription;
            vec.GetSeqData(0, vec.size(), transcription);
            x_AddQual(eFQ_transcription, new CFlatStringQVal(transcription));
        }
        if (rna.IsSetExt()) {
            const CRNA_ref::C_Ext& ext = rna.GetExt();
            switch (ext.Which()) {
            case CRNA_ref::C_Ext::e_Name:
            {
                // amino acid could not be parsed into structured form
                if (!cfg.DropIllegalQuals()) {
                    x_AddQual(eFQ_product,
                        new CFlatStringQVal(ext.GetName()));
                } else {
                    x_AddQual(eFQ_product,
                        new CFlatStringQVal("tRNA-OTHER"));
                }
                break;
            }
            case CRNA_ref::C_Ext::e_TRNA:
            {
                const CTrna_ext& trna = ext.GetTRNA();
                int aa = 0;
                if ( trna.IsSetAa()  &&  trna.GetAa().IsNcbieaa() ) {
                    aa = trna.GetAa().GetNcbieaa();
                }                 
                if ( cfg.IupacaaOnly() ) {
                    aa = s_ToIupacaa(aa);
                }
                const string& aa_str = s_AaName(aa);
                if ( !aa_str.empty() ) {
                    x_AddQual(eFQ_product, new CFlatStringQVal(aa_str));
                    if ( trna.IsSetAnticodon()  &&  !aa_str.empty() ) {
                        x_AddQual(eFQ_anticodon,
                            new CFlatAnticodonQVal(trna.GetAnticodon(),
                                                   aa_str.substr(5, NPOS)));
                    }
                }
                if ( trna.IsSetCodon() ) {
                    const string& comment =
                        m_Feat.IsSetComment() ? m_Feat.GetComment() : kEmptyStr;
                    x_AddQual(eFQ_trna_codons, new CFlatTrnaCodonsQVal(trna, comment));
                }
                //x_AddQual(eFQ_exception_note, new CFlatStringQVal("tRNA features were annotated by tRNAscan-SE."));
                break;
            }
            default:
                break;
            } // end of internal switch
        }
        break;
    }
    case CRNA_ref::eType_mRNA:
    {
        if ( !pseudo  &&  cfg.ShowTranscript() ) {
            CSeqVector vec(feat.GetLocation(), scope);
            vec.SetCoding(CBioseq_Handle::eCoding_Iupac);
            string transcription;
            vec.GetSeqData(0, vec.size(), transcription);
            x_AddQual(eFQ_transcription, new CFlatStringQVal(transcription));
        }
        // intentional fall through
    }
    default:
        switch ( subtype ) {

        case CSeqFeatData::eSubtype_ncRNA: {
            if ( ! rna.IsSetExt() ) {
                break;
            }
            const CRNA_ref_Base::TExt& ext = rna.GetExt();
            if ( ! ext.IsGen() ) {
                break;
            }
            break;
        }
        case CSeqFeatData::eSubtype_tmRNA: {
            if ( ! rna.IsSetExt() ) {
                break;
            }
            const CRNA_ref_Base::TExt& ext = rna.GetExt();
            if ( ext.IsGen()  &&  ext.GetGen().IsSetQuals() ) {

                const list< CRef< CRNA_qual > >& quals = ext.GetGen().GetQuals().Get();
                list< CRef< CRNA_qual > >::const_iterator it = quals.begin();
                for ( ; it != quals.end(); ++it ) {
                    if ( (*it)->IsSetQual() && (*it)->IsSetVal() ) {
                        if ( (*it)->GetQual() == "tag_peptide" ) {
                            x_AddQual( eFQ_tag_peptide, 
                                new CFlatStringQVal( 
                                    (*it)->GetVal(), CFormatQual::eUnquoted ) );
                            break;
                        }
                    }
                }
            }
            break;
        }
        case CSeqFeatData::eSubtype_misc_RNA:
        case CSeqFeatData::eSubtype_otherRNA: {
            if ( ! rna.IsSetExt() ) {
                break;
            }
            const CRNA_ref_Base::TExt& ext = rna.GetExt();
            if ( ext.IsName() ) {
                string strName = ext.GetName();
                if ( strName != "misc_RNA" ) {
                    x_AddQual( eFQ_product, new CFlatStringQVal( strName ) );
                }
            }
            break;
        }
        default:
            if ( rna.IsSetExt()  &&  rna.GetExt().IsName() ) {
                x_AddQual( eFQ_product, new CFlatStringQVal( rna.GetExt().GetName() ) );
            }
            break;
        }
    } // end of switch

    // some things to extract from RNA-gen
    if( rna.IsSetExt() && rna.GetExt().IsGen() ) {
        const CRNA_gen &gen = rna.GetExt().GetGen();
        if ( gen.IsSetClass() ) {
            if (gen.IsLegalClass()) {
                x_AddQual( eFQ_ncRNA_class, 
                    new CFlatStringQVal( gen.GetClass() ) );
            } else {
                x_AddQual( eFQ_ncRNA_class, 
                    new CFlatStringQVal( "other" ));
                x_AddQual( eFQ_seqfeat_note, 
                    new CFlatStringQVal( gen.GetClass() ) );
            }
        }

        if ( gen.IsSetProduct() && ! x_HasQual(eFQ_product) ) {
            x_AddQual( eFQ_product, 
                new CFlatStringQVal( gen.GetProduct() ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualTranslation( 
    CBioseq_Handle& bsh,
    CBioseqContext& ctx, 
    bool pseudo )
//  ----------------------------------------------------------------------------
{
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();

    if ( pseudo || cfg.NeverTranslateCDS() ) {
        return;
    }

    string translation;
    if ( cfg.AlwaysTranslateCDS() || (cfg.TranslateIfNoProduct() && !bsh) ) {
        CSeqTranslator::Translate(m_Feat.GetOriginalFeature(), scope,
                                  translation, false /* don't include stops */);
    }
    else if ( bsh ) {
        CSeqVector seqv = bsh.GetSeqVector();
        CSeq_data::E_Choice coding = cfg.IupacaaOnly() ?
            CSeq_data::e_Iupacaa : CSeq_data::e_Ncbieaa;
        seqv.SetCoding( coding );

        try {
            // an exception can occur here if the specified length doesn't match the actual length.
            // Although I don't know of any released .asn files with this problem, it can occur
            // in submissions.
            seqv.GetSeqData( 0, seqv.size(), translation );
        } catch( const CException & ) {
            // we're unable to do the translation
            translation.clear();
        }
    }

    if (!NStr::IsBlank(translation)) {
        x_AddQual(eFQ_translation, new CFlatStringQVal( translation ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualTranslationTable( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    if ( ! cdr.IsSetCode() ) {
        return;
    }
    int gcode = cdr.GetCode().GetId();
    if ( gcode == 1 || gcode == 255 ) {
        return;
    }
    if ( ctx.Config().IsFormatGBSeq() || gcode != 1 ) {
        x_AddQual(eFQ_transl_table, new CFlatIntQVal(gcode));
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualCodonStart( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    CCdregion::TFrame frame = cdr.GetFrame();

    if ( !ctx.IsProt() || !IsMappedFromCDNA() ) {
        if ( frame == CCdregion::eFrame_not_set ) {
            frame = CCdregion::eFrame_one;
        }
        x_AddQual( eFQ_codon_start, new CFlatIntQVal( frame ) );
    } 
    else {
        if ( frame > 1 ) {
            x_AddQual( eFQ_codon_start, new CFlatIntQVal( frame ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualTranslationException( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
     if ( !ctx.IsProt() || !IsMappedFromCDNA() ) {
        if ( cdr.IsSetCode_break() ) {
            x_AddQual( eFQ_transl_except, 
                new CFlatCodeBreakQVal( cdr.GetCode_break() ) );
        }
        
    } 
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProteinConflict( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    static const string conflict_msg = 
        "Protein sequence is in conflict with the conceptual translation";

    if ( !ctx.IsProt() || !IsMappedFromCDNA() ) {
        bool has_prot = false;
        if ( m_Feat.IsSetProduct() && m_Feat.GetProduct().GetId() != 0 ) {
            has_prot = ( sequence::GetLength( m_Feat.GetProduct(), &ctx.GetScope() ) > 0 );
        }
        const bool conflict_set = ( cdr.IsSetConflict()  &&  cdr.GetConflict() );
        if ( has_prot && conflict_set ) {
            x_AddQual( eFQ_prot_conflict, new CFlatStringQVal( conflict_msg ) );
        }
    } 
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualCodedBy( 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    //if ( ctx.IsProt()  &&  IsMappedFromCDNA() ) {
    if ( ctx.IsProt() ) {
        x_AddQual( eFQ_coded_by, new CFlatSeqLocQVal( m_Feat.GetLocation() ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtComment(
    const CBioseq_Handle& protHandle )
//  ----------------------------------------------------------------------------
{
    if ( ! protHandle ) {
        return;
    }   
    CSeqdesc_CI comm( protHandle, CSeqdesc::e_Comment, 1 );
    if ( comm && !comm->GetComment().empty() ) {
        string comment = comm->GetComment();

        TrimSpacesAndJunkFromEnds( comment, true );
        /* const bool bAddPeriod = */ RemovePeriodFromEnd( comment, true );
        CFlatStringQVal *commentQVal = new CFlatStringQVal( comment );
        /* if( bAddPeriod ) {
            commentQVal->SetAddPeriod();
        } */
        x_AddQual( eFQ_prot_comment, commentQVal );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtMethod(
    const CBioseq_Handle& protHandle )
//  ----------------------------------------------------------------------------
{
    if ( ! protHandle ) {
        return;
    }   
    CSeqdesc_CI mi( protHandle, CSeqdesc::e_Molinfo );
    if ( mi ) {
        CMolInfo::TTech prot_tech = mi->GetMolinfo().GetTech();
        if ( prot_tech >  CMolInfo::eTech_standard       &&
             prot_tech != CMolInfo::eTech_concept_trans  &&
             prot_tech != CMolInfo::eTech_concept_trans_a ) {
            if ( !GetTechString( prot_tech ).empty() ) {
                x_AddQual( eFQ_prot_method, new CFlatStringQVal( 
                    "Method: " + GetTechString( prot_tech) ) );
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_GetAssociatedProtInfo(
    CBioseqContext& ctx,
    CBioseq_Handle& protHandle,
    const CProt_ref*& protRef,
    CMappedFeat& protFeat,
    CConstRef<CSeq_id>& protId )
//  ----------------------------------------------------------------------------
{
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();

    protId.Reset( m_Feat.GetProduct().GetId() );
    if ( protId ) {
        if ( !cfg.AlwaysTranslateCDS() ) {
            CScope::EGetBioseqFlag get_flag = CScope::eGetBioseq_Loaded;
            if ( cfg.ShowFarTranslations() || ctx.IsGED() ) {
                get_flag = CScope::eGetBioseq_All;
            }
            protHandle =  scope.GetBioseqHandle(*protId, get_flag);
        }
    }
    
    protRef = 0;
    if ( protHandle ) {
        protFeat = s_GetBestProtFeature( protHandle );
        if ( protFeat ) {
            protRef = &( protFeat.GetData().GetProt() );
        }
    } 
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtNote( 
    const CProt_ref* protRef, 
    const CMappedFeat& protFeat )
//  ----------------------------------------------------------------------------
{
    if ( ! protRef ) {
        return;
    }
    if ( protFeat.IsSetComment() ) {
        if ( protRef->GetProcessed() == CProt_ref::eProcessed_not_set  ||
                protRef->GetProcessed() == CProt_ref::eProcessed_preprotein ) {
            string prot_note = protFeat.GetComment();
            TrimSpacesAndJunkFromEnds( prot_note, true );
            RemovePeriodFromEnd( prot_note, true );
            x_AddQual( eFQ_prot_note, new CFlatStringQVal( prot_note ) );
        }
    }
}


//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProteinId(
    CBioseqContext& ctx,
    const CBioseq_Handle& protHandle,
    CConstRef<CSeq_id> protId )
//  ----------------------------------------------------------------------------
{
    if ( protHandle ) {
        CConstRef<CBioseq> pBioseq( protHandle.GetCompleteBioseq() );

        // extract the *one* usable general seq-id (if there is one)
        // (the loop sets pTheOneGeneralSeqId, or leaves it NULL
        //  if there is zero or more than one usable general seqids)
        CConstRef<CSeq_id> pTheOneUsableGeneralSeqId;
        FOR_EACH_SEQID_ON_BIOSEQ(seqid_ci, *pBioseq) {
            const CSeq_id & seqid = **seqid_ci;
            if( ! seqid.IsGeneral() ) {
                // not just general, so ignore all of them
                pTheOneUsableGeneralSeqId.Reset();
                break;
            }

            const CDbtag & db_tag = seqid.GetGeneral();

            // db types to ignore
            static const char* const sc_IgnoredDbs[] = {
                "BankIt", 
                "NCBIFILE",
                "PID",
                "SMART", 
                "TMSMART", 
            };
            typedef CStaticArraySet<const char*, PNocase> TIgnoredDbSet;
            DEFINE_STATIC_ARRAY_MAP(TIgnoredDbSet, sc_IgnoredDbSet, sc_IgnoredDbs );

            // get db and tag
            const string & sDb = GET_STRING_FLD_OR_BLANK(db_tag, Db);
            string sTag;
            if( FIELD_IS_SET(db_tag, Tag) ) {
                stringstream sTagStrm;
                db_tag.GetTag().AsString(sTagStrm);
                // swap faster than assignment
                sTagStrm.str().swap(sTag);
            }

            if( ! sDb.empty() && ! sTag.empty() && 
                sc_IgnoredDbSet.find(sDb.c_str()) == sc_IgnoredDbSet.end() ) 
            {
                if( pTheOneUsableGeneralSeqId ) {
                    // more than one, so ignore all of them
                    pTheOneUsableGeneralSeqId.Reset();
                    break;
                } else {
                    pTheOneUsableGeneralSeqId = *seqid_ci;
                }
            }
        }

        CSeq_id::E_Choice eLastRegularChoice = CSeq_id::e_not_set;
        FOR_EACH_SEQID_ON_BIOSEQ(seqid_ci, *pBioseq) {
            const CSeq_id & seqid = **seqid_ci;

            switch( seqid.Which() ) {
            case CSeq_id::e_Genbank: case CSeq_id::e_Embl: case CSeq_id::e_Ddbj:
            case CSeq_id::e_Other:
            case CSeq_id::e_Tpg: case CSeq_id::e_Tpe: case CSeq_id::e_Tpd:
            case CSeq_id::e_Gpipe:
                x_AddQual( eFQ_protein_id, new CFlatSeqIdQVal( seqid ) );
                eLastRegularChoice = seqid.Which();
                break;

            case CSeq_id::e_Gi:
                if( seqid.GetGi() > ZERO_GI ) {
                    if ( eLastRegularChoice == CSeq_id::e_not_set ) {
                        // use as protein_id if it's the first usable one
                        x_AddQual( eFQ_protein_id, new CFlatSeqIdQVal( seqid ) );
                    }
                    x_AddQual( eFQ_db_xref, new CFlatSeqIdQVal( seqid, true ) );
                }
                break;

            case CSeq_id::e_General: 
                // show it if it's the *one* usable general seqid.  otherwise, ignore
                if( *seqid_ci == pTheOneUsableGeneralSeqId ) {
                    x_AddQual( eFQ_protein_id, new CFlatSeqIdQVal( seqid ) );
                }
                break;

            default:
                // ignore other types
                break;
            }
        }
    } else if( protId ) {

        TGi gi = ZERO_GI;
        string prot_acc;

        // get gi and prot_acc
        if ( protId->IsGi() ) {
            gi = protId->GetGi();
            if( gi > ZERO_GI ) {
                string prot_acc;
                try {
                    prot_acc = GetAccessionForGi( gi, ctx.GetScope() );
                } catch ( CException& ) {}
            }
        } else {

            // swap is faster than assignment
            // protId->GetSeqIdString(true).swap( prot_acc );
            prot_acc = protId->GetSeqIdString(true);

            // find prot_acc and gi
            //const CTextseq_id* pTextSeq_id = protId->GetTextseq_Id();
            //if( pTextSeq_id ) {
            //    stringstream protAccStrm;
            //    pTextSeq_id->AsFastaString(protAccStrm);
            //    // swap is faster than assignment
            //    protAccStrm.str().swap( prot_acc );

            //}
            try {
                gi = ctx.GetScope().GetGi( CSeq_id_Handle::GetHandle(*protId) );
            } catch(CException &) {
                // could not get gi
            }
        }

        if( ! prot_acc.empty() ) {
            if ( ! ctx.Config().DropIllegalQuals() || IsValidAccession( prot_acc ) ) {
                try {
                    CRef<CSeq_id> acc_id( new CSeq_id( prot_acc ) );
                    x_AddQual( eFQ_protein_id, new CFlatSeqIdQVal( *acc_id ) );
                } catch( CException & ) {
                    x_AddQual( eFQ_protein_id, new CFlatStringQVal(prot_acc) );
                }
            }
        }

        if( gi > ZERO_GI ) {
            CConstRef<CSeq_id> pGiSeqId(
                protId->IsGi() ?
                protId.GetPointer() :
                new CSeq_id(CSeq_id::e_Gi, gi) );
            x_AddQual( eFQ_db_xref, new CFlatSeqIdQVal( *pGiSeqId, true ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualCdsProduct(
    CBioseqContext& ctx,
    const CProt_ref* protRef )
//  ----------------------------------------------------------------------------
{
    if ( !protRef ) {
        return;
    }

    const CFlatFileConfig& cfg = ctx.Config();
    const CProt_ref::TName& names = protRef->GetName();
    if ( !names.empty() ) {
        if ( ! cfg.IsModeDump() ) {
            x_AddQual( eFQ_cds_product, 
                new CFlatStringQVal( names.front() ) );
            if ( names.size() > 1 ) {
                x_AddQual( eFQ_prot_names, 
                    new CFlatProductNamesQVal( names, m_Gene ) );
            }

        } else {
            ITERATE(CProt_ref::TName, it, names) {
                x_AddQual( eFQ_cds_product, new CFlatStringQVal(*it) );
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtDesc(
    const CProt_ref* protRef )
//  ----------------------------------------------------------------------------
{
    if ( !protRef || !protRef->IsSetDesc() ) {
        return;
    }

    string desc = protRef->GetDesc();
    TrimSpacesAndJunkFromEnds( desc, true );
    bool add_period = RemovePeriodFromEnd( desc, true );
    CRef<CFlatStringQVal> prot_desc( new CFlatStringQVal( desc ) );
    if ( add_period ) {
        prot_desc->SetAddPeriod();
    }
    x_AddQual( eFQ_prot_desc, prot_desc );
}
    
//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtActivity(
    const CProt_ref* protRef )
//  ----------------------------------------------------------------------------
{
    if ( !protRef || protRef->GetActivity().empty() ) {
        return;
    }
    ITERATE (CProt_ref::TActivity, it, protRef->GetActivity()) {
        x_AddQual(eFQ_prot_activity, new CFlatStringQVal(*it));
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtEcNumber(
    CBioseqContext& ctx,
    const CProt_ref* protRef )
//  ----------------------------------------------------------------------------
{
    if ( !protRef || !protRef->IsSetEc()  ||  protRef->GetEc().empty() ) {
        return;
    }

    const CFlatFileConfig& cfg = ctx.Config();
    ITERATE(CProt_ref::TEc, ec, protRef->GetEc()) {
        if ( !cfg.DropIllegalQuals()  ||  s_IsLegalECNumber( *ec ) ) {
            x_AddQual( eFQ_prot_EC_number, new CFlatStringQVal( *ec ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsCdregion(
    const CMappedFeat& cds,
    CBioseqContext& ctx,
    bool pseudo)
//  ----------------------------------------------------------------------------
{
    const CCdregion& cdr = cds.GetData().GetCdregion();

    const CProt_ref* protRef = 0;
    CMappedFeat protFeat;
    CConstRef<CSeq_id> prot_id;

    x_AddQualTranslationTable( cdr, ctx );
    x_AddQualCodonStart( cdr, ctx );
    x_AddQualTranslationException( cdr, ctx );
    x_AddQualProteinConflict( cdr, ctx );
    x_AddQualCodedBy( ctx );
    if ( ctx.IsProt()  &&  IsMappedFromCDNA() ) {
        return;
    }

    // protein qualifiers
    if (m_Feat.IsSetProduct()) {
        CBioseq_Handle prot =
            ctx.GetScope().GetBioseqHandle(m_Feat.GetProductId());
        x_GetAssociatedProtInfo( ctx, prot, protRef, protFeat, prot_id );
        x_AddQualProtComment( prot );
        x_AddQualProtMethod( prot );
        x_AddQualProtNote( protRef, protFeat );
        x_AddQualProteinId( ctx, prot, prot_id );
        x_AddQualTranslation( prot, ctx, pseudo );
    }

    // add qualifiers where associated xref overrides the ref:
    const CProt_ref* protXRef = m_Feat.GetProtXref();
    if ( ! protXRef ) {
        protXRef = protRef;
    }
    x_AddQualCdsProduct( ctx, protXRef );
    x_AddQualProtDesc( protXRef );
    x_AddQualProtActivity( protXRef );
    x_AddQualProtEcNumber( ctx, protXRef );
}

static int s_ScoreSeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::Score(id_non_const);
}


CSeq_id_Handle s_FindBestIdChoice(const CBioseq_Handle::TId& ids)
{
    //
    //  Objective:
    //  Find the best choice among a given subset of id types. I.e. if a certain
    //  id scores well but is not of a type we approve of, we still reject it.
    //
    CBestChoiceTracker< CSeq_id_Handle, int (*)(const CSeq_id_Handle&) > 
        tracker(s_ScoreSeqIdHandle);

    ITERATE( CBioseq_Handle::TId, it, ids ) {
        switch( (*it).Which() ) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Gi:
            case CSeq_id::e_Other:
            case CSeq_id::e_General:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
            case CSeq_id::e_Gpipe:
                tracker(*it);
                break;
            default:
                break;
        }
    }
    return tracker.GetBestChoice();
}

//  ---------------------------------------------------------------------------
void CFeatureItem::x_AddProductIdQuals(
    CBioseq_Handle& prod, 
    EFeatureQualifier slot)
//  ---------------------------------------------------------------------------
{
    //
    //  Objective (according to the C toolkit):
    //  We need one (and only one) /xxx_id tag. If there are multiple ids 
    //

    if (!prod) {
        return;
    }
    const CBioseq_Handle::TId& ids = prod.GetId();
    if (ids.empty()) {
        return;
    }

    CSeq_id_Handle best = s_FindBestIdChoice(ids);
    if (!best) {
        return;
    }
    x_AddQual(slot, new CFlatSeqIdQVal(*best.GetSeqId()));

    if( m_Feat.GetData().IsCdregion() || ! GetContext()->IsProt() ) {
        ITERATE( CBioseq_Handle::TId, id_iter, ids ) {
            if( id_iter->IsGi() ) {
                x_AddQual( eFQ_db_xref,
                    new CFlatStringQVal("GI:" + NStr::NumericToString(id_iter->GetGi()) ));
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsRegion(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    _ASSERT( m_Feat.GetData().IsRegion() );

    //cerr << MSerial_AsnText << m_Feat.GetOriginalFeature();

    const CSeqFeatData& data = m_Feat.GetData();
    const string &region = data.GetRegion();
    if ( region.empty() ) {
        return;
    }

    if ( ctx.IsProt()  &&
         data.GetSubtype() == CSeqFeatData::eSubtype_region ) 
    {
        x_AddQual(eFQ_region_name, new CFlatStringQVal(region));
    } else {
        x_AddQual(eFQ_region, new CFlatStringQVal("Region: " + region));
    }

    /// parse CDD data from the user object
    list< CConstRef<CUser_object> > objs;
    if (m_Feat.IsSetExt()) {
        objs.push_back(CConstRef<CUser_object>(&m_Feat.GetExt()));
    }
    if (m_Feat.IsSetExts()) {
        copy(m_Feat.GetExts().begin(), m_Feat.GetExts().end(),
             back_inserter(objs));
    }

    ITERATE (list< CConstRef<CUser_object> >, it, objs) {
        const CUser_object& obj = **it;
        bool found = false;
        if (obj.IsSetType()  &&
            obj.GetType().IsStr()  &&
            obj.GetType().GetStr() == "cddScoreData") {
            CConstRef<CUser_field> f = obj.GetFieldRef("definition");
            if (f) {
                CUser_field_Base::C_Data::TStr definition_str = f->GetData().GetStr();
                RemovePeriodFromEnd(definition_str, true);
                if( ! s_StrEqualDisregardFinalPeriod(definition_str, region, NStr::eNocase) ) {
                    x_AddQual(eFQ_region,
                        new CFlatStringQVal(definition_str));
                    found = true;
                }
                break;

                /**
                if (ctx.IsProt()) {
                    if (f->GetData().GetStr() != region  ||  added_raw) {
                        x_AddQual(eFQ_region,
                                  new CFlatStringQVal(f->GetData().GetStr()));
                    }
                } else {
                    x_AddQual(eFQ_region,
                              new CFlatStringQVal(f->GetData().GetStr()));
                }

                found = true;
                break;
                **/

                /**
                if (ctx.IsProt()  &&  region == f->GetData().GetStr()) {
                    /// skip
                } else {
                    x_AddQual(eFQ_region,
                              new CFlatStringQVal(f->GetData().GetStr()));
                    found = true;
                    break;
                }
                **/
            }
        }

        if (found) {
            break;
        }
    }
}


//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsBond(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    _ASSERT( m_Feat.GetData().IsBond() );

    const CSeqFeatData& data = m_Feat.GetData();
    const string& bond = s_GetBondName( data.GetBond() );
    if ( NStr::IsBlank( bond ) ) {
        return;
    }
    
    if ( ctx.IsGenbankFormat()  &&  ctx.IsProt() ) {
        x_AddQual( eFQ_bond_type, new CFlatStringQVal( bond ) );
    } else {
        x_AddQual( eFQ_bond, new CFlatBondQVal( bond ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsPsecStr(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    _ASSERT( m_Feat.GetData().IsPsec_str() );

    const CSeqFeatData& data = m_Feat.GetData();

    CSeqFeatData_Base::TPsec_str sec_str_type = data.GetPsec_str();

    string sec_str_as_str = CSeqFeatData_Base::GetTypeInfo_enum_EPsec_str()->FindName( sec_str_type, true );
    x_AddQual( eFQ_sec_str_type, new CFlatStringQVal( sec_str_as_str ) );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsHet(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    _ASSERT( m_Feat.GetData().IsHet() );

    const CSeqFeatData& data = m_Feat.GetData();

    CSeqFeatData_Base::THet het = data.GetHet();

    x_AddQual( eFQ_heterogen, new CFlatStringQVal( het.Get() ) );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsVariation( 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    _ASSERT( m_Feat.GetData().IsVariation() );

    const CSeqFeatData& data = m_Feat.GetData();
    const CSeqFeatData_Base::TVariation& variation = data.GetVariation();

    // Make the /db_xref qual
    if( variation.CanGetId() ) {
        const CVariation_ref_Base::TId& dbt = variation.GetId();
        // the id tag is quite specific (e.g. db must be "dbSNP", etc.) or it won't print 
        if ( dbt.IsSetDb()  &&  !dbt.GetDb().empty()  &&
                dbt.IsSetTag() && dbt.GetTag().IsStr() ) {
            const string &oid_str = dbt.GetTag().GetStr();
            if( dbt.GetDb() == "dbSNP" && NStr::StartsWith(oid_str, "rs" ) ) {
                x_AddQual(eFQ_db_xref,  new CFlatStringQVal( dbt.GetDb() + ":" + oid_str.substr( 2 ) ) );
            }
        }
    }

    // Make the /replace quals:
    if( variation.CanGetData() && variation.GetData().IsInstance() && 
            variation.GetData().GetInstance().CanGetDelta() ) {
        const CVariation_inst_Base::TDelta& delta = variation.GetData().GetInstance().GetDelta();
        ITERATE( CVariation_inst_Base::TDelta, delta_iter, delta ) {
            if( *delta_iter && (*delta_iter)->CanGetSeq() ) {
                const CDelta_item_Base::TSeq& seq = (*delta_iter)->GetSeq();
                if( seq.IsLiteral() && seq.GetLiteral().CanGetSeq_data() ) {
                    const CDelta_item_Base::C_Seq::TLiteral& seq_literal = seq.GetLiteral();
                    const CSeq_literal_Base::TSeq_data& seq_data = seq_literal.GetSeq_data();

                    // convert the data to the standard a,c,g,t
                    CSeq_data iupacna_seq_data;
                    CSeqportUtil::Convert( seq_data,
                        &iupacna_seq_data,
                        CSeq_data::e_Iupacna );
                    string nucleotides = iupacna_seq_data.GetIupacna().Get();

                    // if the specified length and the length of the data conflict,
                    // use the smaller
                    const string::size_type max_len_allowed = seq_literal.GetLength();
                    if( nucleotides.size() > max_len_allowed ) {
                        nucleotides.resize( max_len_allowed );
                    }

                    NStr::ToLower( nucleotides );

                    if (!NStr::IsBlank(nucleotides)) {
                        x_AddQual(eFQ_replace, new CFlatStringQVal(nucleotides));
                    }
                }
            }
        }
    }
}

static const string& s_GetSiteName(CSeqFeatData::TSite site)
{
    static const string kOther = "other";
    static const string kDnaBinding = "DNA binding";
    static const string kInhibit = "inhibition";

    switch (site) {
    case CSeqFeatData::eSite_other:
        return kOther;
    case CSeqFeatData::eSite_dna_binding:
        return kDnaBinding;
    case CSeqFeatData::eSite_inhibit:
        return kInhibit;

    default:
        return CSeqFeatData::ENUM_METHOD_NAME(ESite)()->FindName(site, true);
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsSite( 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    _ASSERT( m_Feat.GetData().IsSite() );

    const CSeqFeatData& data = m_Feat.GetData();
    CSeqFeatData::TSite site = data.GetSite();
    const string& site_name = s_GetSiteName( site );

    if ( ctx.Config().IsFormatGenbank()  &&  ctx.IsProt() ) {
        x_AddQual(eFQ_site_type, new CFlatSiteQVal( site_name ) );
    } else {
        if ( !m_Feat.IsSetComment() ||  
            ( NStr::Find( m_Feat.GetComment(), site_name ) == NPOS ) ) {
            x_AddQual( eFQ_site, new CFlatSiteQVal( site_name ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsExt(
    const CSeq_feat::TExt& ext )
//  ----------------------------------------------------------------------------
{
    ITERATE (CUser_object::TData, it, ext.GetData()) {
        const CUser_field& field = **it;
        if ( !field.IsSetData() ) {
            continue;
        }
        if ( field.GetData().IsObject() ) {
            const CUser_object& obj = field.GetData().GetObject();
            x_AddQualsExt(obj);
        } else if ( field.GetData().IsObjects() ) {
            ITERATE (CUser_field::C_Data::TObjects, o, field.GetData().GetObjects()) {
                x_AddQualsExt(**o);
            }
        }
    }
    if ( ext.IsSetType()  &&  ext.GetType().IsStr() ) {
        const string& oid = ext.GetType().GetStr();
        if ( oid == "ModelEvidence" ) {
            x_AddQual(eFQ_modelev, new CFlatModelEvQVal(ext));
        } else if ( oid == "GeneOntology" ) {
            x_AddGoQuals(ext);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualDbXref(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    if ( m_Feat.IsSetProduct()  &&
        ( !m_Feat.GetData().IsCdregion()  &&  ctx.IsProt() && ! IsMappedFromProt() ) ) {
        CBioseq_Handle prod = 
            ctx.GetScope().GetBioseqHandle( m_Feat.GetProductId() );
        if ( prod ) {
            const CBioseq_Handle::TId& ids = prod.GetId();
            if ( ! ids.empty() ) {
                ITERATE (CBioseq_Handle::TId, it, ids) {
                    if ( it->Which() != CSeq_id::e_Gi ) {
                        continue;
                    }
                    CConstRef<CSeq_id> id = it->GetSeqId();
                    if (!id->IsGeneral()) {
                        x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(*id, id->IsGi()));
                    }
                }
            }
        }
    }
    if ( ! m_Feat.IsSetDbxref() ) {
        return ;
    }
    x_AddQual( eFQ_db_xref, new CFlatXrefQVal( m_Feat.GetDbxref(), &m_Quals ) );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddGoQuals(
    const CUser_object& uo )
//  ----------------------------------------------------------------------------
{
    ITERATE (CUser_object::TData, uf_it, uo.GetData()) {
        const CUser_field& field = **uf_it;
        if ( field.IsSetLabel()  &&  field.GetLabel().IsStr() ) {
            const string& label = field.GetLabel().GetStr();
            EFeatureQualifier slot = eFQ_none;
            if ( label == "Process" ) {
                slot = eFQ_go_process;
            } else if ( label == "Component" ) {               
                slot = eFQ_go_component;
            } else if ( label == "Function" ) {
                slot = eFQ_go_function;
            }
            if ( slot == eFQ_none ) {
                continue;
            }

            ITERATE (CUser_field::TData::TFields, it, field.GetData().GetFields()) {
                if ( (*it)->GetData().IsFields() ) {
                    CRef<CFlatGoQVal> go_val( new CFlatGoQVal(**it) );

                    bool okay_to_add = true;

                    // check for dups
                    CFeatureItem::TQCI iter = x_GetQual(slot);
                    for ( ; iter != m_Quals.end()  &&  iter->first == slot; ++iter) {
                        const CFlatGoQVal & qual = dynamic_cast<const CFlatGoQVal &>( *iter->second );
                        if( qual.Equals(*go_val) )
                        {
                            okay_to_add = false;
                            break;
                        }
                    }

                    if( okay_to_add ) {
                        x_AddQual(slot, go_val);
                    }
                }
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsGene(
    const CGene_ref* gene_ref,
    CConstRef<CSeq_feat>& gene_feat,
    bool from_overlap )
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data = m_Feat.GetData();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();

    if ( m_Feat.GetData().Which() == CSeqFeatData::e_Gene ) {
        gene_ref = &( m_Feat.GetData().GetGene() );
    }
    if ( ! gene_ref && gene_feat ) {
        gene_ref = & gene_feat->GetData().GetGene();
    }

    if ( ! gene_ref || gene_ref->IsSuppressed() ) {
        return;
    }

    const bool is_gene = (subtype == CSeqFeatData::eSubtype_gene);

    const string* locus = (gene_ref->IsSetLocus()  &&  !NStr::IsBlank(gene_ref->GetLocus())) ?
        &gene_ref->GetLocus() : NULL;
    const string* desc = (gene_ref->IsSetDesc() &&  !NStr::IsBlank(gene_ref->GetDesc())) ?
        &gene_ref->GetDesc() : NULL;
    const TGeneSyn* syn = (gene_ref->IsSetSyn()  &&  !gene_ref->GetSyn().empty()) ?
        &gene_ref->GetSyn() : NULL;
    const string* locus_tag =
        (gene_ref->IsSetLocus_tag()  &&  !NStr::IsBlank(gene_ref->GetLocus_tag())) ?
        &gene_ref->GetLocus_tag() : 0;

    //  gene:
//    if ( subtype != CSeqFeatData::eSubtype_repeat_region ) {
    if ( !from_overlap  ||  subtype != CSeqFeatData::eSubtype_repeat_region ) {
        if ( locus != 0 ) {
            m_Gene = *locus;
        } 
        else if ( ( desc != 0 ) && (subtype != CSeqFeatData::eSubtype_repeat_region) ) {
            m_Gene = *desc;
        }
        else if (syn != NULL) {
            CGene_ref::TSyn syns = *syn;
            m_Gene = syns.front();
        }
        if( !m_Gene.empty() ) {
            // we suppress the /gene qual when there's no locus but there is a locus tag (imitates C toolkit)
            if ( NULL != locus || NULL == locus_tag ) {
                x_AddQual(eFQ_gene, new CFlatGeneQVal(m_Gene));
            }
        }
    }
    else { // for repeat regions
//        if ( from_overlap && locus != 0 ) {
//            m_Gene = *locus;
//            x_AddQual(eFQ_gene, new CFlatGeneQVal(m_Gene));
//        }
    }

    //  locus tag:
    if ( gene_ref  ||  subtype != CSeqFeatData::eSubtype_repeat_region ) {
        if (locus != NULL) {
            if (locus_tag != NULL) {
                x_AddQual(eFQ_locus_tag, new CFlatStringQVal(*locus_tag, CFormatQual::eTrim_WhitespaceOnly));
            }
        }
        else if (locus_tag != NULL) {
            x_AddQual(eFQ_locus_tag, new CFlatStringQVal(*locus_tag, CFormatQual::eTrim_WhitespaceOnly));
        }
    }

    //  gene desc:
    if ( gene_ref  ||  subtype != CSeqFeatData::eSubtype_repeat_region ) {
        if (locus != NULL) {
            if (is_gene  &&  desc != NULL) {
                string desc_cleaned = *desc;
                RemovePeriodFromEnd( desc_cleaned, true );
                x_AddQual(eFQ_gene_desc, new CFlatStringQVal(desc_cleaned));
            }
        }
        else if (locus_tag != NULL) {
            if (is_gene  &&  desc != NULL) {
                x_AddQual(eFQ_gene_desc, new CFlatStringQVal(*desc));
            }
        }
    }

    //  gene syn:
    if ( gene_ref  ||  subtype != CSeqFeatData::eSubtype_repeat_region ) {
        if (locus != NULL) {
            if (syn != NULL) {
                x_AddQual(eFQ_gene_syn, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (locus_tag != NULL) {
            if (syn != NULL) {
                x_AddQual(eFQ_gene_syn, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (desc != NULL) {
            if (syn != NULL) {
                x_AddQual(eFQ_gene_syn, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (syn != NULL) {
            CGene_ref::TSyn syns = *syn;
            syns.pop_front();
            // ... and the rest as synonyms
            if (syn != NULL) {
                x_AddQual(eFQ_gene_syn, new CFlatGeneSynonymsQVal(syns));
            }
        }
    }

    // gene nomenclature
    if( gene_ref->IsSetFormal_name() && subtype == CSeqFeatData::eSubtype_gene ) {
        x_AddQual( eFQ_nomenclature, new CFlatNomenclatureQVal(gene_ref->GetFormal_name()) );
    }

    // gene allele:
    {{
        // these bool vars just break up the if-statement to make it easier to understand
        const bool is_type_where_allele_from_gene_forbidden = (subtype == CSeqFeatData::eSubtype_variation);
        const bool is_type_where_allele_from_gene_forbidden_except_with_embl_or_ddbj = 
            ( subtype == CSeqFeatData::eSubtype_repeat_region ||
              subtype == CSeqFeatData::eSubtype_mobile_element || 
              subtype == CSeqFeatData::eSubtype_centromere ||
              subtype == CSeqFeatData::eSubtype_telomere );
        const bool is_embl_or_ddbj = ( GetContext()->IsEMBL() || GetContext()->IsDDBJ() );
        if ( ! is_type_where_allele_from_gene_forbidden && 
             ( is_embl_or_ddbj || ! is_type_where_allele_from_gene_forbidden_except_with_embl_or_ddbj ) ) 
        {
            if (gene_ref->IsSetAllele()  &&  !NStr::IsBlank(gene_ref->GetAllele())) {
                x_AddQual(eFQ_gene_allele, new CFlatStringQVal(gene_ref->GetAllele(), 
                    CFormatQual::eTrim_WhitespaceOnly));
            }
        }
    }}

    //  gene xref:
    if (gene_ref->IsSetDb()) {
        x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(gene_ref->GetDb()));
    }

    //  gene db-xref:
    switch (m_Feat.GetData().Which()) {
    case CSeqFeatData::e_Rna:
    case CSeqFeatData::e_Cdregion:
        if (gene_feat  &&  gene_feat->IsSetDbxref()) {
            CSeq_feat::TDbxref xrefs = gene_feat->GetDbxref();
            if (m_Feat.IsSetDbxref()) {
                ITERATE (CSeq_feat::TDbxref, it, m_Feat.GetDbxref()) {
                    for (CSeq_feat::TDbxref::iterator i = xrefs.begin();
                         i != xrefs.end();  ++i) {
                        if ((*i)->Equals(**it)) {
                            xrefs.erase(i);
                            break;
                        }
                    }
                }
            }
            if (xrefs.size()) {
                x_AddQual(eFQ_db_xref, new CFlatXrefQVal(xrefs));
            }
        }
        break;

    default:
        break;
    }

    //  gene map:
    if (!from_overlap  &&  gene_ref->IsSetMaploc() && subtype == CSeqFeatData::eSubtype_gene) {
        x_AddQual(eFQ_gene_map, new CFlatStringQVal(gene_ref->GetMaploc()));
    }

    // gene pseudogene qual:

    // inherit pseudogene, if possible
    if( gene_feat && ! x_HasQual(eFQ_pseudogene) ) {
        const string & strPseudoGene = gene_feat->GetNamedQual("pseudogene");
        x_AddQual(eFQ_pseudogene, new CFlatStringQVal(strPseudoGene) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsProt(
    CBioseqContext& ctx, 
    bool pseudo)
//  ----------------------------------------------------------------------------
{
    _ASSERT( m_Feat.GetData().IsProt() );

    const CSeqFeatData& data = m_Feat.GetData();
    const CProt_ref& pref = data.GetProt();
    CProt_ref::TProcessed processed = pref.GetProcessed();

    //cerr << MSerial_AsnText << m_Feat.GetOriginalFeature();

    if ( ctx.IsNuc()  ||  (ctx.IsProt()  &&  !IsMappedFromProt()) ) {
        if ( pref.IsSetName()  &&  !pref.GetName().empty() ) {
            const CProt_ref::TName& names = pref.GetName();
            x_AddQual(eFQ_product, new CFlatStringQVal(names.front()));
            if (names.size() > 1) {
                x_AddQual(eFQ_prot_names, new CFlatProductNamesQVal(names, m_Gene));
            }
        }
        if ( pref.IsSetDesc()  &&  !pref.GetDesc().empty() ) {
            if ( !ctx.IsProt() ) {
                string desc = pref.GetDesc();
                TrimSpacesAndJunkFromEnds(desc, true);
                bool add_period = RemovePeriodFromEnd(desc, true);
                CRef<CFlatStringQVal> prot_desc(new CFlatStringQVal(desc));
                if (add_period) {
                    prot_desc->SetAddPeriod();
                }
                x_AddQual(eFQ_prot_desc, prot_desc);
//                had_prot_desc = true;
            } else {
                x_AddQual(eFQ_prot_name, new CFlatStringQVal(pref.GetDesc()));
            }
        }
        if ( pref.IsSetActivity()  &&  !pref.GetActivity().empty() ) {
            if ( ctx.IsNuc()  ||  processed != CProt_ref::eProcessed_mature ) {
                ITERATE (CProt_ref::TActivity, it, pref.GetActivity()) {
                    if (!NStr::IsBlank(*it)) {
                        x_AddQual(eFQ_prot_activity, new CFlatStringQVal(*it));
                    }
                }
            }
        }
        if (pref.IsSetEc()  &&  !pref.GetEc().empty()) {
            ITERATE(CProt_ref::TEc, ec, pref.GetEc()) {
                if ( !ctx.Config().DropIllegalQuals() ||  s_IsLegalECNumber(*ec)) {
                    x_AddQual(eFQ_prot_EC_number, new CFlatStringQVal(*ec));
                }
            }
        }
        if ( m_Feat.IsSetProduct() ) {
            CBioseq_Handle prot = 
                ctx.GetScope().GetBioseqHandle( m_Feat.GetProductId() );
            if ( prot ) {
                x_AddProductIdQuals(prot, eFQ_protein_id);
            } else {
                try {
                    const CSeq_id& prod_id = 
                        GetId( m_Feat.GetProduct(), &ctx.GetScope());
                    if ( ctx.IsRefSeq()  ||  !ctx.Config().ForGBRelease() ) {
                        x_AddQual(eFQ_protein_id, new CFlatSeqIdQVal(prod_id));
                    }
                } catch (CObjmgrUtilException&) {}
            }
        }
    } else { // protein feature on subpeptide bioseq
        x_AddQual(eFQ_derived_from, new CFlatSeqLocQVal(m_Feat.GetLocation()));
    }
    if ( !pseudo  &&  ctx.Config().ShowPeptides() ) {
        if ( processed == CProt_ref::eProcessed_mature          ||
             processed == CProt_ref::eProcessed_signal_peptide  ||
             processed == CProt_ref::eProcessed_transit_peptide ) {
            CSeqVector pep(m_Feat.GetLocation(), ctx.GetScope());
            pep.SetCoding(CSeq_data::e_Ncbieaa);
            string peptide;
            pep.GetSeqData(pep.begin(), pep.end(), peptide);
            if (!NStr::IsBlank(peptide)) {
                x_AddQual(eFQ_peptide, new CFlatStringQVal(peptide));
            }
        }
    }

    ///
    /// report molecular weights
    ///
    if (ctx.IsProt() && ( ctx.IsRefSeq() || ctx.Config().IsFormatGBSeq() ) && ! IsMappedFromProt() && 
        ! ( m_Feat.IsSetPartial() && m_Feat.GetPartial() ) && 
        ! ( m_Feat.GetLocation().IsPartialStart(eExtreme_Biological) || 
            m_Feat.GetLocation().IsPartialStop(eExtreme_Biological)) && 
        ! pseudo )
    {
        double wt = 0;
        bool has_mat_peptide = false;
        bool has_signal_peptide = false;

        CConstRef<CSeq_loc> loc(&m_Feat.GetLocation());

        const bool is_pept_whole_loc = loc->IsWhole() || 
            ( loc->GetStart(eExtreme_Biological) == 0 && 
              loc->GetStop(eExtreme_Biological) == (ctx.GetHandle().GetBioseqLength() - 1) );

        if (processed == CProt_ref::eProcessed_not_set || 
                processed == CProt_ref::eProcessed_preprotein ) 
        {
            SAnnotSelector sel = ctx.SetAnnotSelector();
            sel.SetFeatType(CSeqFeatData::e_Prot);
            for (CFeat_CI feat_it(ctx.GetHandle(), sel);  feat_it;  ++feat_it) {
                bool copy_loc = false;
                switch (feat_it->GetData().GetProt().GetProcessed()) {
                case CProt_ref::eProcessed_signal_peptide:
                case CProt_ref::eProcessed_transit_peptide:
                    {{
                         has_signal_peptide = true;
                         if ( (feat_it->GetLocation().GetTotalRange().GetFrom() ==
                               m_Feat.GetLocation().GetTotalRange().GetFrom()) &&
                               ! feat_it->GetLocation().Equals( m_Feat.GetLocation() ) ) {
                             loc = loc->Subtract(feat_it->GetLocation(),
                                                 CSeq_loc::fSortAndMerge_All,
                                                 NULL, NULL);
                         }
                     }}
                    break;

                case CProt_ref::eProcessed_mature:
                    has_mat_peptide = true;
                    break;

                default:
                    break;
                }

                if (copy_loc) {
                    /// we need to adjust our location to the end of the signal
                    /// peptide
                    CRef<CSeq_loc> l(new CSeq_loc);
                    loc = l;
                    l->Assign(m_Feat.GetLocation());
                    l->SetInt().SetTo
                        (feat_it->GetLocation().GetTotalRange().GetTo());
                }
            }
        }

        /**
        CMolInfo::TCompleteness comp = CMolInfo::eCompleteness_partial;
        {{
             CConstRef<CMolInfo> molinfo
                 (sequence::GetMolInfo(ctx.GetHandle()));
             if (molinfo) {
                 comp = molinfo->GetCompleteness();
             }
         }}
         **/

        if ( !(loc->IsPartialStart(eExtreme_Biological) || loc->IsPartialStop(eExtreme_Biological)) ) {

            bool proteinIsAtLeastMature;
            switch( pref.GetProcessed() ) {
                case CProt_ref::eProcessed_not_set:
                case CProt_ref::eProcessed_preprotein:
                    proteinIsAtLeastMature = false;
                    break;
                default:
                    proteinIsAtLeastMature = true;
                    break;
            }

            if ( (!has_mat_peptide  ||  !has_signal_peptide) || (proteinIsAtLeastMature) || (!is_pept_whole_loc) ) { 
                try {
                    const TGetProteinWeight flags = 0;
                    wt = GetProteinWeight(m_Feat.GetOriginalFeature(),
                                          ctx.GetScope(), loc, flags);
                }
                catch (CException&) {
                }
            }
        }

        /// note: we report the weight rounded to the nearest int
        if (wt) {
            x_AddQual(eFQ_calculated_mol_wt,
                      new CFlatIntQVal((int(wt + 0.5))));
        }
    }

    // cleanup
    if ( processed == CProt_ref::eProcessed_signal_peptide  ||
         processed == CProt_ref::eProcessed_transit_peptide ) {
        if ( !ctx.IsRefSeq() ) {
           // Only RefSeq allows product on signal or transit peptide
           x_RemoveQuals(eFQ_product);
        }
    }
    if ( processed == CProt_ref::eProcessed_preprotein  &&
         !ctx.IsRefSeq()  &&  !ctx.IsProt()  &&  
         data.GetSubtype() == CSeqFeatData::eSubtype_preprotein ) {
        const CFlatStringQVal* product = x_GetStringQual(eFQ_product);
        if (product != NULL) {
            x_AddQual(eFQ_encodes, new CFlatStringQVal("encodes " + product->GetValue()));
            x_RemoveQuals(eFQ_product);
        }
    }
}


static void s_ParseParentQual(const CGb_qual& gbqual, list<string>& vals)
{
    vals.clear();

    if (!gbqual.IsSetVal()  || NStr::IsBlank(gbqual.GetVal())) {
        return;
    }

    const string& val = gbqual.GetVal();
    
    if (val.length() > 1  &&  NStr::StartsWith(val, '(')  &&
        NStr::EndsWith(val, ')')  && val.find(',') != NPOS) {
        NStr::Split(val, "(,)", vals);
    } else {
        vals.push_back(val);
    }

    list<string>::iterator it = vals.begin();
    while (it != vals.end()) {
        if (NStr::IsBlank(*it)) {
            it = vals.erase(it);
        } else {
            ConvertQuotes(*it);
            ExpandTildes(*it, eTilde_space);
            ++it;
        }
    }
}


struct SLegalImport {
    const char*       m_Name;
    EFeatureQualifier m_Value;

    operator string(void) const { return m_Name; }
};


static bool s_IsValidDirection(const string& direction) {
    return NStr::EqualNocase(direction, "LEFT")   ||
           NStr::EqualNocase(direction, "RIGHT")  ||
           NStr::EqualNocase(direction, "BOTH");
}


static bool s_IsValidnConsSplice(const string& cons_splice) {
    return NStr::EqualNocase(cons_splice, "(5'site:YES, 3'site:YES)")     ||
           NStr::EqualNocase(cons_splice, "(5'site:YES, 3'site:NO)")      ||
           NStr::EqualNocase(cons_splice, "(5'site:YES, 3'site:ABSENT)")  ||
           NStr::EqualNocase(cons_splice, "(5'site:NO, 3'site:YES)")      ||
           NStr::EqualNocase(cons_splice, "(5'site:NO, 3'site:NO)")       ||
           NStr::EqualNocase(cons_splice, "(5'site:NO, 3'site:ABSENT)")   ||
           NStr::EqualNocase(cons_splice, "(5'site:ABSENT, 3'site:YES)")  ||
           NStr::EqualNocase(cons_splice, "(5'site:ABSENT, 3'site:NO)")   ||
           NStr::EqualNocase(cons_splice, "(5'site:ABSENT, 3'site:ABSENT)");
}

// currently just converts PMIDs into links
static void
s_HTMLizeExperimentQual( string &out_new_val, const string &val)
{
    static const string kPmid("PMID:");

    // just to make sure
    out_new_val.clear();

    // str_pos should generally be considered as holding the first position
    // in val that we have not yet processed and copied to out_new_val.
    SIZE_TYPE str_pos = 0;
    while( str_pos < val.length() ) {

        // find next "PMID:" to process
        const SIZE_TYPE pmid_label_pos = val.find( "PMID:", str_pos );
        if( pmid_label_pos == NPOS ) {
            // no more PMIDs left.
            // copy the rest of the string and let's leave
            copy( val.begin() + str_pos, val.end(), back_inserter(out_new_val) );
            return;
        }

        // copy val up to just after "PMID:"
        const SIZE_TYPE first_pmid_pos = pmid_label_pos + kPmid.length();
        copy( val.begin() + str_pos, val.begin() + first_pmid_pos, back_inserter(out_new_val) );
        str_pos = first_pmid_pos;

        // push pmids (with links) onto the output
        // we consider the pmids to be numbers separated by one or more spaces and/or commas.
        bool first_num = true;
        while( str_pos < val.length() ) {
            // skip spaces and commas before pmid
            const SIZE_TYPE next_pmid_pos = val.find_first_not_of(" ,", str_pos);
            if( next_pmid_pos == NPOS || ! isdigit(val[next_pmid_pos]) ) {
                break;
            }

            // find end of pmid
            SIZE_TYPE end_of_pmid_pos = val.find_first_not_of("0123456789", next_pmid_pos );
            if( NPOS == end_of_pmid_pos ) {
                end_of_pmid_pos = val.length();
            }

            // extract the actual pmid
            string pmid = val.substr(next_pmid_pos, end_of_pmid_pos - next_pmid_pos );

            // write pmid with link
            if( ! first_num ) {
                out_new_val += ',';
            }
            out_new_val += "<a href=\"";
            out_new_val += strLinkBasePubmed;
            out_new_val += pmid;
            out_new_val += "\">";
            out_new_val += pmid;
            out_new_val += "</a>";
            str_pos = end_of_pmid_pos;

            first_num = false;
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_ImportQuals(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    _ASSERT(m_Feat.IsSetQual());

    typedef SStaticPair<const char*, EFeatureQualifier> TLegalImport;
    static const TLegalImport kLegalImports[] = {
        // Must be in case-insensitive alphabetical order!
#define DO_IMPORT(x) { #x, eFQ_##x }
        DO_IMPORT(allele),
        DO_IMPORT(bound_moiety),
        DO_IMPORT(clone),
        DO_IMPORT(codon),
        DO_IMPORT(compare),
        DO_IMPORT(cons_splice),
        DO_IMPORT(cyt_map),
        DO_IMPORT(direction),
        DO_IMPORT(EC_number),
        DO_IMPORT(estimated_length),
        DO_IMPORT(evidence),
        DO_IMPORT(experiment),
        DO_IMPORT(frequency),
        DO_IMPORT(function),
        DO_IMPORT(gap_type),
        DO_IMPORT(gen_map),
        DO_IMPORT(inference),
        DO_IMPORT(insertion_seq),
        DO_IMPORT(label),
        DO_IMPORT(linkage_evidence),
        DO_IMPORT(map),
        DO_IMPORT(mobile_element),
        DO_IMPORT(mobile_element_type),
        DO_IMPORT(mod_base),
        DO_IMPORT(ncRNA_class),
        DO_IMPORT(number),
        DO_IMPORT(old_locus_tag),
        DO_IMPORT(operon),
        DO_IMPORT(organism),
        DO_IMPORT(PCR_conditions),
        DO_IMPORT(phenotype),
        DO_IMPORT(product),
        DO_IMPORT(pseudogene),
        DO_IMPORT(rad_map),
        DO_IMPORT(replace),
        DO_IMPORT(ribosomal_slippage),
        DO_IMPORT(rpt_family),
        DO_IMPORT(rpt_type),
        DO_IMPORT(rpt_unit),
        DO_IMPORT(rpt_unit_range),
        DO_IMPORT(rpt_unit_seq),
        DO_IMPORT(satellite),
        DO_IMPORT(standard_name),
        DO_IMPORT(tag_peptide),
        DO_IMPORT(trans_splicing),
        DO_IMPORT(transposon),
        DO_IMPORT(UniProtKB_evidence),
        DO_IMPORT(usedin)
#undef DO_IMPORT
    };
    typedef const CStaticPairArrayMap<const char*, EFeatureQualifier, PNocase_CStr> TLegalImportMap;
    DEFINE_STATIC_ARRAY_MAP(TLegalImportMap, kLegalImportMap, kLegalImports);

    bool check_qual_syntax = ctx.Config().CheckQualSyntax();

    const bool old_locus_tag_added_elsewhere = x_HasQual(eFQ_old_locus_tag);

    bool first_pseudogene = true;

    vector<string> replace_quals;
    const CSeq_feat_Base::TQual & qual = m_Feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
    ITERATE( CSeq_feat::TQual, it, qual ) {
        if (!(*it)->IsSetQual()  ||  !(*it)->IsSetVal()) {
            continue;
        }
        const string& val = (*it)->GetVal();

        const char* name = (*it)->GetQual().c_str();
        const TLegalImportMap::const_iterator li = kLegalImportMap.find(name);
        EFeatureQualifier   slot = eFQ_illegal_qual;
        if ( li != kLegalImportMap.end() ) {
            slot = li->second;
        } else if (check_qual_syntax) {
            continue;
        }

        // only certain slot types may have an empty value (e.g. M96433)
        switch(slot) {
        case eFQ_replace:
        case eFQ_pseudogene:
            // empty value allowed for these slot types, so we don't check
            break;
        default:
            // empty value forbidden for other slot types
            if( val.empty() ) {
                continue;
            }
            break;
        }

        switch (slot) {
        case eFQ_allele:
            // if /allele inherited from gene, suppress allele gbqual on feature
            if (x_HasQual(eFQ_gene_allele)) {
                continue;
            } else {
                x_AddQual(slot, new CFlatStringQVal(val, 
                    CFormatQual::eTrim_WhitespaceOnly));
            }
            break;
        case eFQ_codon:
            if ((*it)->IsSetVal()  &&  !NStr::IsBlank(val)) {
                x_AddQual(slot, new CFlatStringQVal(val, CFormatQual::eUnquoted));
            }
            break;
        case eFQ_cons_splice:
            if ((*it)->IsSetVal()) {
                if (!check_qual_syntax  ||  s_IsValidnConsSplice(val)) {
                    x_AddQual(slot, new CFlatStringQVal(val));
                }
            }
            break;
        case eFQ_direction:
            if ((*it)->IsSetVal()) {
                if (!check_qual_syntax  ||  s_IsValidDirection(val)) {
                    x_AddQual(slot, new CFlatNumberQVal(val));
                }
            }
            break;
        case eFQ_estimated_length:
        case eFQ_mod_base:
        case eFQ_number:
            if ((*it)->IsSetVal()  &&  !NStr::IsBlank(val)) {
                x_AddQual(slot, new CFlatNumberQVal(val));
            }
            break;
        case eFQ_rpt_type:
            x_AddRptTypeQual(val, check_qual_syntax);
            break;
        case eFQ_rpt_unit:
            if ((*it)->IsSetVal()) {
                x_AddRptUnitQual(val);
            }
            break;
        case eFQ_usedin:
        {{
            list<string> vals;
            s_ParseParentQual(**it, vals);
            ITERATE (list<string>, i, vals) {
                x_AddQual(slot, new CFlatStringQVal(*i, CFormatQual::eQuoted));
            }
            break;
        }}
        case eFQ_old_locus_tag:
        {{
            if( ! old_locus_tag_added_elsewhere ) {
                list<string> vals;
                s_ParseParentQual(**it, vals);
                ITERATE (list<string>, i, vals) {
                    x_AddQual(slot, new CFlatStringQVal(*i, CFormatQual::eQuoted, CFormatQual::eTrim_WhitespaceOnly));
                }
            }
            break;
        }}
        case eFQ_rpt_family:
            if ((*it)->IsSetVal()  &&  !NStr::IsBlank(val)) {
                x_AddQual(slot, new CFlatStringQVal(val));
            }
            break;
        case eFQ_label:
            x_AddQual(slot, new CFlatLabelQVal(val));
            break;
        case eFQ_EC_number:
            if ((*it)->IsSetVal()  &&  
                ( ! ctx.Config().DropIllegalQuals() || s_IsLegalECNumber(val) ) ) {
                x_AddQual(slot, new CFlatStringQVal(val));
            }
            break;
        case eFQ_illegal_qual:
            x_AddQual(slot, new CFlatIllegalQVal(**it));
            break;
        case eFQ_product:
            if (!x_HasQual(eFQ_product)) {
                x_AddQual(slot, new CFlatStringQVal(val));
            } else {
                const CFlatStringQVal* gene = x_GetStringQual(eFQ_gene);
                const string& gene_val =
                    gene != NULL ? gene->GetValue() : kEmptyStr;
                const CFlatStringQVal* product = x_GetStringQual(eFQ_product);
                const string& product_val =
                    product != NULL ? product->GetValue() : kEmptyStr;
                if (val != gene_val  &&  val != product_val) {
                    
                    if ( ! ctx.Config().CodonRecognizedToNote() ||
                         ! x_HasQual(eFQ_trna_codons) || 
                         NStr::Find(val, "RNA") == NPOS )
                    {
                        x_AddQual(eFQ_xtra_prod_quals, new CFlatStringQVal(val));
                    }
                }
            }
            break;
        case eFQ_compare:
            {{
                list<string> vals;
                s_ParseParentQual(**it, vals);
                ITERATE (list<string>, i, vals) {
                    if (!ctx.Config().CheckQualSyntax()  ||
                        IsValidAccession(*i, eValidateAccDotVer)) {
                        x_AddQual(slot, new CFlatStringQVal(*i, CFormatQual::eUnquoted));
                    }
                }
            }}
            break;
        case eFQ_evidence:
            {{
                if ( val == "EXPERIMENTAL" ) {
                    x_AddQual(eFQ_experiment, new CFlatExperimentQVal());
                } else if ( val == "NOT_EXPERIMENTAL" ) {
                    x_AddQual(eFQ_inference, new CFlatInferenceQVal());
                }
            }}
            break;

        case eFQ_rpt_unit_range:
            x_AddQual(slot, new CFlatStringQVal(val, CFormatQual::eUnquoted));
            break;

        case eFQ_replace:
            {{
                 string s(val);
                 replace_quals.push_back(s);
             }}
            break;

        case eFQ_operon:
            {{
                if( ! x_HasQual(eFQ_operon) ) {
                    x_AddQual(slot, new CFlatStringQVal(val));
                }
            }}
            break;

        case eFQ_experiment:
            {{
                if( ctx.Config().DoHTML() && ! CommentHasSuspiciousHtml(val) ) {
                    string new_val;
                    s_HTMLizeExperimentQual(new_val, val);
                    x_AddQual(slot, new CFlatStringQVal(new_val));
                } else {
                    x_AddQual(slot, new CFlatStringQVal(val));
                }
            }}
            break;

        case eFQ_clone:
            x_AddQual(slot, new CFlatStringQVal(val, CFormatQual::eTrim_WhitespaceOnly));
            break;

        case eFQ_pseudogene:

            // our pseudogene(s) override(s) any that existed before
            if( first_pseudogene ) {
                first_pseudogene = false;
                x_RemoveQuals(eFQ_pseudogene);
            }
            x_AddQual(slot, new CFlatStringQVal(val));

            break;

        default:
            x_AddQual(slot, new CFlatStringQVal(val));
            break;
        }
    }

    if (replace_quals.size()) {
        std::sort(replace_quals.begin(), replace_quals.end());
        ITERATE (vector<string>, it, replace_quals) {
            x_AddQual(eFQ_replace, new CFlatStringQVal(*it));
        }
    }

    // some "map-related" qual adjustments
    if( ctx.Config().HideSpecificGeneMaps() && ! x_HasQual(eFQ_map) ) {
        if( x_HasQual(eFQ_cyt_map) ) {
            x_AddQual(eFQ_map, x_GetQual(eFQ_cyt_map)->second );
            x_RemoveQuals(eFQ_cyt_map);
        } else if( x_HasQual(eFQ_gen_map) ) {
            x_AddQual(eFQ_map, x_GetQual(eFQ_gen_map)->second );
            x_RemoveQuals(eFQ_gen_map);
        } else if( x_HasQual(eFQ_rad_map) ) {
            x_AddQual(eFQ_map, x_GetQual(eFQ_rad_map)->second );
            x_RemoveQuals(eFQ_rad_map);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddRptUnitQual(
    const string& rpt_unit )
//  ----------------------------------------------------------------------------
{
    if (rpt_unit.empty()) {
        return;
    }

    vector<string> units;

    if (NStr::StartsWith(rpt_unit, '(')  &&  NStr::EndsWith(rpt_unit, ')')  &&
        NStr::Find(rpt_unit, "(", 1) == NPOS) {
        string tmp = rpt_unit.substr(1, rpt_unit.length() - 2);
        NStr::Tokenize(tmp, ",", units);
    } else {
        units.push_back(rpt_unit);
    }

    NON_CONST_ITERATE (vector<string>, it, units) {
        if (!it->empty()) {
            NStr::TruncateSpacesInPlace(*it);
            x_AddQual(eFQ_rpt_unit, new CFlatStringQVal(*it));
        }
    }
}


static bool s_IsValidRptType(const string& type)
{
    static const char* const valid_rpt[] = {
        "direct", "dispersed", "flanking", "inverted", "other",
        "tandem", "terminal"
    };
    typedef CStaticArraySet<string, PNocase> TValidRptTypes;
    DEFINE_STATIC_ARRAY_MAP(TValidRptTypes, valid_types, valid_rpt);

    return valid_types.find(type) != valid_types.end();
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddRptTypeQual(
    const string& rpt_type, 
    bool check_qual_syntax )
//  ----------------------------------------------------------------------------
{
    if (rpt_type.empty()) {
        return;
    }
    
    string value( rpt_type );
    NStr::TruncateSpacesInPlace( value );

    vector<string> pieces;
    s_SplitCommaSeparatedStringInParens( pieces, value );

    ITERATE( vector<string>, it, pieces ) {
        if ( ! check_qual_syntax || s_IsValidRptType( *it ) ) {
            x_AddQual( eFQ_rpt_type, new CFlatStringQVal( *it, CFormatQual::eUnquoted ) );
        }
    }
}


void CFeatureItem::x_FormatQuals(CFlatFeature& ff) const
{
    const CFlatFileConfig& cfg = GetContext()->Config();

    if ( cfg.IsFormatFTable() ) {
        ff.SetQuals() = m_FTableQuals;
        return;
    }

    ff.SetQuals().reserve(m_Quals.Size());
    CFlatFeature::TQuals& qvec = ff.SetQuals();

#define DO_QUAL(x) x_FormatQual(eFQ_##x, #x, qvec)
    DO_QUAL(partial);
    DO_QUAL(gene);

    DO_QUAL(locus_tag);
    DO_QUAL(old_locus_tag);

    x_FormatQual(eFQ_gene_syn_refseq, "synonym", qvec);
    DO_QUAL(gene_syn);

    x_FormatQual(eFQ_gene_allele, "allele", qvec);

    DO_QUAL(operon);

    DO_QUAL(ncRNA_class);

    DO_QUAL(product);

    x_FormatQual(eFQ_prot_EC_number, "EC_number", qvec);
    x_FormatQual(eFQ_prot_activity,  "function", qvec);

    DO_QUAL(standard_name);
    DO_QUAL(coded_by);
    DO_QUAL(derived_from);

    x_FormatQual(eFQ_prot_name, "name", qvec);
    DO_QUAL(region_name);
    DO_QUAL(bond_type);
    DO_QUAL(site_type);
    DO_QUAL(sec_str_type);
    DO_QUAL(heterogen);

    DO_QUAL(tag_peptide);

    DO_QUAL(evidence);
    DO_QUAL(experiment);
    DO_QUAL(inference);
    DO_QUAL(exception);
    DO_QUAL(ribosomal_slippage);
    DO_QUAL(trans_splicing);
    DO_QUAL(artificial_location);

    if ( !cfg.GoQualsToNote() ) {
        if( cfg.GoQualsEachMerge() ) {
            // combine all quals of a given type onto the same qual
            x_FormatGOQualCombined(eFQ_go_component, "GO_component", qvec);
            x_FormatGOQualCombined(eFQ_go_function, "GO_function", qvec);
            x_FormatGOQualCombined(eFQ_go_process, "GO_process", qvec);
        } else {
            x_FormatQual(eFQ_go_component, "GO_component", qvec);
            x_FormatQual(eFQ_go_function, "GO_function", qvec);
            x_FormatQual(eFQ_go_process, "GO_process", qvec);
        }
    }

    DO_QUAL(nomenclature);

    x_FormatNoteQuals(ff);
    DO_QUAL(citation);

    DO_QUAL(number);

    DO_QUAL(pseudo);
    DO_QUAL(pseudogene);
    DO_QUAL(selenocysteine);
    DO_QUAL(pyrrolysine);

    DO_QUAL(codon_start);

    DO_QUAL(anticodon);
    if ( ! cfg.CodonRecognizedToNote() ) {
        DO_QUAL(trna_codons);
    }
    DO_QUAL(bound_moiety);
    DO_QUAL(clone);
    DO_QUAL(compare);
    // DO_QUAL(cons_splice);
    DO_QUAL(direction);
    DO_QUAL(function);
    DO_QUAL(frequency);
    DO_QUAL(EC_number);
    x_FormatQual(eFQ_gene_map, "map", qvec);
    // In certain modes, cyt_map, gen_map, and rad_map are 
    // moved to eFQ_gene_map by x_ImportQuals:
    DO_QUAL(cyt_map);
    DO_QUAL(gen_map);
    DO_QUAL(rad_map);
    DO_QUAL(estimated_length);
    DO_QUAL(gap_type);
    DO_QUAL(linkage_evidence);
    DO_QUAL(allele);
    DO_QUAL(map);
    DO_QUAL(mod_base);
    DO_QUAL(PCR_conditions);
    DO_QUAL(phenotype);
    DO_QUAL(rpt_family);
    DO_QUAL(rpt_type);
    DO_QUAL(rpt_unit);
    DO_QUAL(rpt_unit_range);
    DO_QUAL(rpt_unit_seq);
    DO_QUAL(satellite);
    DO_QUAL(mobile_element);
    DO_QUAL(mobile_element_type);
    DO_QUAL(usedin);

    // extra imports, actually...
    x_FormatQual(eFQ_illegal_qual, "illegal", qvec);

    DO_QUAL(replace);

    DO_QUAL(transl_except);
    DO_QUAL(transl_table);
    DO_QUAL(codon);
    DO_QUAL(organism);
    DO_QUAL(label);
    x_FormatQual(eFQ_cds_product, "product", qvec);
    DO_QUAL(UniProtKB_evidence);
    DO_QUAL(protein_id);
    DO_QUAL(transcript_id);
    DO_QUAL(db_xref);
    x_FormatQual(eFQ_gene_xref, "db_xref", qvec);
    DO_QUAL(mol_wt);
    DO_QUAL(calculated_mol_wt);
    DO_QUAL(translation);
    DO_QUAL(transcription);
    DO_QUAL(peptide);

#undef DO_QUAL
}

/*
// check if str2 is a sub string of str1
static bool s_IsRedundant(const string& str1, const string& str2)
{
    size_t pos = NPOS;
    bool whole = false;
    for (pos = NStr::Find(str1, str2); pos != NPOS  &&  !whole; pos += str2.length()) {
        whole = IsWholeWord(str1, pos);
    }
    return (pos != NPOS  && whole);
}


// Remove redundant elements that occur twice or as part of other elements.
static void s_PruneNoteQuals(CFlatFeature::TQuals& qvec)
{
    if (qvec.empty()) {
        return;
    }
    CFlatFeature::TQuals::iterator it1 = qvec.begin();
    while (it1 != qvec.end()) {
        CFlatFeature::TQuals::iterator it2 = it1 + 1;
        const string& val1 = (*it1)->GetValue();
        while (it2 != qvec.end()) {
            const string& val2 = (*it2)->GetValue();
            if (s_IsRedundant(val1, val2)) {
                it2 = qvec.erase(it2);
            } else if (s_IsRedundant(val2, val1)) {                
                break;
            } else {
                ++it2;
            }
        }
        if (it2 != qvec.end()) {
            it1 = qvec.erase(it1);
        } else {
            ++it1;
        }
    }
}
*/

void CFeatureItem::x_FormatNoteQuals(CFlatFeature& ff) const
{
    const CFlatFileConfig& cfg = GetContext()->Config();
    CFlatFeature::TQuals qvec;

#define DO_NOTE(x) x_FormatNoteQual(eFQ_##x, GetStringOfFeatQual(eFQ_##x), qvec)
#define DO_NOTE_PREPEND_NEWLINE(x) x_FormatNoteQual(eFQ_##x, GetStringOfFeatQual(eFQ_##x), qvec, IFlatQVal::fPrependNewline )
    DO_NOTE(transcript_id_note);
    DO_NOTE(gene_desc);

    if ( cfg.CodonRecognizedToNote() ) {
        DO_NOTE(trna_codons);
    }
    DO_NOTE(encodes);
    DO_NOTE(prot_desc);
    DO_NOTE(prot_note);
    DO_NOTE(prot_comment);
    DO_NOTE(prot_method);
    DO_NOTE(maploc);
    DO_NOTE(prot_conflict);
    DO_NOTE(prot_missing);
    DO_NOTE(seqfeat_note);
    DO_NOTE(region);
//    DO_NOTE(selenocysteine_note);
    DO_NOTE(prot_names);
    DO_NOTE(bond);
    DO_NOTE(site);
//    DO_NOTE(rrna_its);
    DO_NOTE(xtra_prod_quals);
//     DO_NOTE(inference_bad);
    DO_NOTE(modelev);
//     DO_NOTE(cdd_definition);
//    DO_NOTE(tag_peptide);
    DO_NOTE_PREPEND_NEWLINE(exception_note);

    string notestr;
    string suffix = kEmptyStr;
//    bool add_period = false;
    bool add_period = true/*fl*/;

    s_QualVectorToNote(qvec, true, notestr, suffix, add_period);

    if (GetContext()->Config().GoQualsToNote()) {
        qvec.clear();
        DO_NOTE(go_component);
        DO_NOTE(go_function);
        DO_NOTE(go_process);
        s_QualVectorToNote(qvec, false, notestr, suffix, add_period);
    }
    s_NoteFinalize(add_period, notestr, ff, eTilde_tilde);

#undef DO_NOTE
#undef DO_NOTE_PREPEND_NEWLINE
}

void CFeatureItem::x_FormatQual
(EFeatureQualifier slot,
 const string & name,
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    TQCI it = const_cast<const TQuals&>(m_Quals).LowerBound(slot);
    TQCI end = const_cast<const TQuals&>(m_Quals).end();
    while (it != end  &&  it->first == slot) {
        it->second->Format(qvec, name, *GetContext(), flags);
        ++it;
    }
}


void CFeatureItem::x_FormatNoteQual
(EFeatureQualifier slot,
 const string & name, 
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    flags |= IFlatQVal::fIsNote;

    TQCI it = const_cast<const TQuals&>(m_Quals).LowerBound(slot);
    TQCI end = const_cast<const TQuals&>(m_Quals).end();
    while (it != end  &&  it->first == slot) {
        it->second->Format(qvec, name, *GetContext(), flags);
        ++it;
    }
}

// This produces one qual out of all the GO quals of the given slot, with their
// values concatenated.
void CFeatureItem::x_FormatGOQualCombined
(EFeatureQualifier slot, 
 const string & name,
 CFlatFeature::TQuals& qvec, 
 TQualFlags flags) const
{
    // copy all the given quals with that name since we need to sort them
    vector<CConstRef<CFlatGoQVal> > goQuals;

    TQCI it = const_cast<const TQuals&>(m_Quals).LowerBound(slot);
    TQCI end = const_cast<const TQuals&>(m_Quals).end();
    while (it != end  &&  it->first == slot) {
        goQuals.push_back( CConstRef<CFlatGoQVal>( dynamic_cast<const CFlatGoQVal*>( it->second.GetNonNullPointer() ) ) );
        ++it;
    }

    if( goQuals.empty() ) {
        return;
    }

    stable_sort( goQuals.begin(), goQuals.end(), CGoQualLessThan() );

    CFlatFeature::TQuals temp_qvec;

    string combined;


    string::size_type this_part_beginning_text_string_pos = 0;

    // now concatenate their values into the variable "combined"
    const string *pLastQualTextString = NULL; 
    ITERATE( vector<CConstRef<CFlatGoQVal> >, iter, goQuals ) {

        // Use thisQualTextString to tell when we have consecutive quals with the
        // same text string.
        const string *pThisQualTextString = &(*iter)->GetTextString();
        if( NULL == pThisQualTextString ) {
            continue;
        }

        (*iter)->Format(temp_qvec, name, *GetContext(), flags);

        if( pLastQualTextString == NULL || ! NStr::EqualNocase( *pLastQualTextString, *pThisQualTextString ) ) {
            // normal case: each CFlatGoQVal has its own part
            if( ! combined.empty() ) {
                combined += "; ";
                this_part_beginning_text_string_pos = combined.length() - 1;
            }
            combined += temp_qvec.back()->GetValue();
        } else {
            // consecutive CFlatGoQVal with the same text string: merge
            // (chop off the part up to and including the text string )
            const string & new_value = temp_qvec.back()->GetValue();

            // let text_string_pos point to the part *after* the text string
            SIZE_TYPE post_text_string_pos = NStr::FindNoCase( new_value, *pLastQualTextString );
            _ASSERT( post_text_string_pos != NPOS );
            post_text_string_pos += pLastQualTextString->length();

            // append the new part after the text string, but only
            // if it's not a duplicate
            string str_to_append = new_value.substr( post_text_string_pos, 
                (pLastQualTextString->length() - post_text_string_pos) );
            if( NStr::Find(combined, str_to_append, this_part_beginning_text_string_pos) == NPOS ) {
                combined.append( str_to_append );
            }
        }

        pLastQualTextString = pThisQualTextString;
    }
    pLastQualTextString = NULL; // just to make sure we don't accidentally use it
    
    // add the final merged CFormatQual
    if( ! combined.empty() ) {
        const string prefix = " ";
        const string suffix = ";";
        TFlatQual res(new CFormatQual(name, combined, prefix, suffix, CFormatQual::eQuoted ));
        qvec.push_back(res); 
    }
}

const CFlatStringQVal* CFeatureItem::x_GetStringQual(EFeatureQualifier slot) const
{
    const IFlatQVal* qual = 0;
    if ( x_HasQual(slot) ) {
        qual = m_Quals.Find(slot)->second;
    }
    return dynamic_cast<const CFlatStringQVal*>(qual);
}


CFlatStringListQVal* CFeatureItem::x_GetStringListQual(EFeatureQualifier slot) const
{
    IFlatQVal* qual = 0;
    if (x_HasQual(slot)) {
        qual = const_cast<IFlatQVal*>(&*m_Quals.Find(slot)->second);
    }
    return dynamic_cast<CFlatStringListQVal*>(qual);
}

CFlatProductNamesQVal * CFeatureItem::x_GetFlatProductNamesQual(EFeatureQualifier slot) const
{
    IFlatQVal* qual = 0;
    if (x_HasQual(slot)) {
        qual = const_cast<IFlatQVal*>(&*m_Quals.Find(slot)->second);
    }
    return dynamic_cast<CFlatProductNamesQVal*>(qual);
}

// maps each valid mobile_element_type prefix to whether it
// must have more info after the prefix
typedef SStaticPair<const char *, bool> TMobileElemTypeKey;
static const TMobileElemTypeKey mobile_element_key_to_suffix_required [] = {
    {  "LINE",                     false  },
    {  "MITE",                     false  },
    {  "SINE",                     false  },
    {  "insertion sequence",       false  },
    {  "integron",                 false  },
    {  "non-LTR retrotransposon",  false  },
    {  "other",                    true   },
    {  "retrotransposon",          false  },
    {  "transposon",               false  }
};

typedef CStaticPairArrayMap <const char*, bool, PCase_CStr> TMobileElemTypeMap;
DEFINE_STATIC_ARRAY_MAP(TMobileElemTypeMap, sm_MobileElemTypeKeys, mobile_element_key_to_suffix_required);

// returns whether or not it's valid
bool s_ValidateMobileElementType( const string & mobile_element_type_value )
{
    if( mobile_element_type_value.empty() ) {
        return false;
    }

    // if there's a colon, we ignore the part after the colon for testing purposes
    string::size_type colon_pos = mobile_element_type_value.find( ':' );
    
    const string value_before_colon = ( string::npos == colon_pos 
        ? mobile_element_type_value 
        : mobile_element_type_value.substr( 0, colon_pos ) );

    TMobileElemTypeMap::const_iterator prefix_info = 
        sm_MobileElemTypeKeys.find( value_before_colon.c_str() );
    if( prefix_info == sm_MobileElemTypeKeys.end() ) {
        return false; // prefix not found
    }
    
    // check if info required after prefix (colon plus info, actually)
    if( prefix_info->second ) {
        if( string::npos == colon_pos ) {
            return false; // no additional info supplied, even though required
        }
    }
   
    // all tests passed
    return true; 
}

class CInStringPred
{
public:
    explicit CInStringPred( const string &comparisonString )
        : m_ComparisonString( comparisonString ) 
    { }

    bool operator()( const string &arg ) {
        return NStr::Find( m_ComparisonString, arg ) != NPOS;
    }
private:
    const string &m_ComparisonString;
};

void CFeatureItem::x_CleanQuals(
    const CGene_ref* gene_ref )
{ 
    const TGeneSyn* gene_syn = 
        (gene_ref && gene_ref->IsSetSyn() && !gene_ref->GetSyn().empty() ) 
        ?
        &gene_ref->GetSyn() 
        : 
        0;
    const CBioseqContext& ctx = *GetContext();

    if (ctx.Config().DropIllegalQuals()) {
        x_DropIllegalQuals();
    }

    CFlatProductNamesQVal * prot_names = x_GetFlatProductNamesQual(eFQ_prot_names);
    const CFlatStringQVal* gene = x_GetStringQual(eFQ_gene);
    const CFlatStringQVal* prot_desc = x_GetStringQual(eFQ_prot_desc);
    const CFlatStringQVal* standard_name = x_GetStringQual(eFQ_standard_name);
    const CFlatStringQVal* seqfeat_note = x_GetStringQual(eFQ_seqfeat_note);

    if (gene != NULL) {
        const string& gene_name = gene->GetValue();

        // /gene same as feature.comment will suppress /note
        if (m_Feat.IsSetComment()) {
            if (NStr::Equal(gene_name, m_Feat.GetComment())) {
                x_RemoveQuals(eFQ_seqfeat_note);
                seqfeat_note = NULL;
            }
        }

        // remove protein description that equals the gene name, case sensitive
        if (prot_desc != NULL) {
            if (s_StrEqualDisregardFinalPeriod(gene_name, prot_desc->GetValue(), NStr::eCase)) {
                x_RemoveQuals(eFQ_prot_desc);
                prot_desc = NULL;
            }
        }

        // remove prot name if equals gene
        if (prot_names != NULL) {

            CProt_ref::TName::iterator remove_start = prot_names->SetValue().begin();
            ++remove_start; // The "++" is because the first one shouldn't be erased since it's used for the product
            CProt_ref::TName::iterator new_end = 
                remove( remove_start, prot_names->SetValue().end(), gene_name );
            prot_names->SetValue().erase( new_end, prot_names->SetValue().end() );
 
            if (prot_names->GetValue().empty()) {
                x_RemoveQuals(eFQ_prot_names);
                prot_names = NULL;
            }
        }
    }

    if (prot_desc != NULL) {
        const string& pdesc = prot_desc->GetValue();

        // remove prot name if in prot_desc
        if (prot_names != NULL) {
            CProt_ref::TName::iterator remove_start = prot_names->SetValue().begin();
            ++remove_start; // The "++" is because the first one shouldn't be erased since it's used for the product
            CProt_ref::TName::iterator new_end = 
                remove_if( remove_start, prot_names->SetValue().end(),
                    CInStringPred(pdesc) );
            prot_names->SetValue().erase( new_end, prot_names->SetValue().end() );

            if (prot_names->GetValue().empty()) {
                x_RemoveQuals(eFQ_prot_names);
                prot_names = NULL;
            }
        }
        // remove protein description that equals the cds product, case sensitive
        const CFlatStringQVal* cds_prod = x_GetStringQual(eFQ_cds_product);
        if (cds_prod != NULL) {
            if (NStr::Equal(pdesc, cds_prod->GetValue())) {
                x_RemoveQuals(eFQ_prot_desc);
                prot_desc = NULL;
            }
        }

        // remove protein description that equals the standard name
        if (prot_desc != NULL  &&  standard_name != NULL) {
            // We use s_StrEqualDisregardFinalPeriod rather than plain NStr::EqualNoCase
            // because of, e.g., CU638784
            if (s_StrEqualDisregardFinalPeriod(pdesc, standard_name->GetValue(), NStr::eNocase )) {
                x_RemoveQuals(eFQ_prot_desc);
                prot_desc = NULL;
            }
        }

        // remove protein description that equals a gene synonym
        // NC_001823 leave in prot_desc if no cds_product
        if (prot_desc != NULL  &&  gene_syn != NULL  &&  cds_prod != NULL) {
            ITERATE (TGeneSyn, it, *gene_syn) {
                if (!NStr::IsBlank(*it)  &&  pdesc == *it) {
                    x_RemoveQuals(eFQ_prot_desc);
                    prot_desc = NULL;
                    break;
                }
            }
        }
    }

    // check if need to remove seqfeat_note
    // (This generally occurs when it's equal to (or, sometimes, contained in) another qual
    if (m_Feat.IsSetComment()) {
        const string &feat_comment = m_Feat.GetComment();
        const CFlatStringQVal* product     = x_GetStringQual(eFQ_product);
        const CFlatStringQVal* cds_product = x_GetStringQual(eFQ_cds_product);

        if (product != NULL) {
            if (NStr::EqualNocase(product->GetValue(), feat_comment)) {
                x_RemoveQuals(eFQ_seqfeat_note);
                seqfeat_note = NULL;
            }
        }
        if (cds_product != NULL && seqfeat_note != NULL) {
            if ( s_StrEqualDisregardFinalPeriod(cds_product->GetValue(), seqfeat_note->GetValue(), NStr::eCase ) ) {
                x_RemoveQuals(eFQ_seqfeat_note);
                seqfeat_note = NULL;
            }
        }
        // suppress selenocysteine note if already in comment
//        if (NStr::Find(feat_comment, "selenocysteine") != NPOS) {
//            x_RemoveQuals(eFQ_selenocysteine_note);
//        }

        // /EC_number same as feat.comment will suppress /note
        if( seqfeat_note != NULL ) {
            for (TQCI it = x_GetQual(eFQ_EC_number); it != m_Quals.end()  &&  it->first == eFQ_EC_number; ++it) {
                const CFlatStringQVal* ec = dynamic_cast<const CFlatStringQVal*>(it->second.GetPointerOrNull());
                if (ec != NULL) {
                    if (NStr::EqualNocase(seqfeat_note->GetValue(), ec->GetValue())) {
                        x_RemoveQuals(eFQ_seqfeat_note);
                        seqfeat_note = NULL;
                        break;
                    }
                }
            }
        }

        // this sort of note provides no additional info (we already know this is a tRNA by other places)
        if( feat_comment == "tRNA-" ) {
            x_RemoveQuals(eFQ_seqfeat_note);
            seqfeat_note = NULL;
        }
    }

    const CFlatStringQVal* note = x_GetStringQual(eFQ_seqfeat_note);
    if (note != NULL  &&  standard_name != NULL) {
        if (NStr::Equal(note->GetValue(), standard_name->GetValue())) {
            x_RemoveQuals(eFQ_seqfeat_note);
            note = NULL;
        }
    }
    if ( ! ctx.IsProt() && note != NULL  &&  gene_syn != NULL) {
        ITERATE (TGeneSyn, it, *gene_syn) {
            if (NStr::EqualNocase(note->GetValue(), *it)) {
                x_RemoveQuals(eFQ_seqfeat_note);
                note = NULL;
                break;
            }
        }
    }
    if( note != NULL && prot_desc != NULL ) { // e.g. L07143, U28372
        if( NStr::Find(prot_desc->GetValue(), note->GetValue()) != NPOS ) {
            x_RemoveQuals(eFQ_seqfeat_note);
            note = NULL;
        }
    }

    // if there is a prot_desc, then we don't add a period to seqfeat_note
    // (Obviously, this part must come after the part that cleans up
    // the prot_descs, otherwise we may think we have a prot_desc, when the
    // prot_desc is actually to be removed )
    if( note != NULL && x_GetStringQual(eFQ_prot_desc ) ) {
        const_cast<CFlatStringQVal*>(note)->SetAddPeriod( false );
    }

    // hide invalid mobile_element_quals
    if( ctx.Config().IsModeRelease() || ctx.Config().IsModeEntrez() ) {

        const CFlatStringQVal *mobile_element_type = x_GetStringQual( eFQ_mobile_element_type );
        if( NULL != mobile_element_type && ! s_ValidateMobileElementType(mobile_element_type->GetValue()) ) {
            x_RemoveQuals( eFQ_mobile_element_type );
        }

    }

    // remove invalid pseudogenes:
    {
        TQI pseudogene_iter = m_Quals.Find(eFQ_pseudogene);
        while( pseudogene_iter != m_Quals.end() && 
            pseudogene_iter->first == eFQ_pseudogene ) 
        {
            const CFlatStringQVal & qual = dynamic_cast<const CFlatStringQVal &>( *pseudogene_iter->second );
            if( s_IsValidPseudoGene(GetContext()->Config().GetMode(), qual.GetValue() ) ) {
                // keep valid pseudogene
                ++pseudogene_iter;
            } else {
                // erase invalid pseudogene
                TQI pseudogene_iter_to_erase = pseudogene_iter;
                ++pseudogene_iter;

                m_Quals.Erase(pseudogene_iter_to_erase);
            }
        }
    }

    // /pseudogene qual suppresses /pseudo qual if /pseudogene fits certain patterns
    if( // ( GetContext()->Config().IsModeRelease() || GetContext()->Config().IsModeEntrez() ) &&
        x_HasQual(eFQ_pseudo) && x_HasQual(eFQ_pseudogene) ) 
    {
        const CFlatStringQVal* qval = x_GetStringQual(eFQ_pseudogene);
        // in this part, always use release-mode validation logic, regardless of actual mode
        if( qval && s_IsValidPseudoGene( CFlatFileConfig::eMode_Release, qval->GetValue() ) ) {
            x_RemoveQuals(eFQ_pseudo);
        }
    }
}


typedef SStaticPair<EFeatureQualifier, CSeqFeatData::EQualifier> TQualPair;
static const TQualPair sc_GbToFeatQualMap[] = {
    { eFQ_none, CSeqFeatData::eQual_bad },
    { eFQ_allele, CSeqFeatData::eQual_allele },
    { eFQ_anticodon, CSeqFeatData::eQual_anticodon },
    { eFQ_artificial_location, CSeqFeatData::eQual_artificial_location },
    { eFQ_bond, CSeqFeatData::eQual_note },
    { eFQ_bond_type, CSeqFeatData::eQual_bond_type },
    { eFQ_bound_moiety, CSeqFeatData::eQual_bound_moiety },
    { eFQ_calculated_mol_wt, CSeqFeatData::eQual_calculated_mol_wt },
    { eFQ_cds_product, CSeqFeatData::eQual_product },
    { eFQ_citation, CSeqFeatData::eQual_citation },
    { eFQ_clone, CSeqFeatData::eQual_clone },
    { eFQ_coded_by, CSeqFeatData::eQual_coded_by },
    { eFQ_codon, CSeqFeatData::eQual_codon },
    { eFQ_codon_start, CSeqFeatData::eQual_codon_start },
    { eFQ_compare, CSeqFeatData::eQual_compare },
    { eFQ_cons_splice, CSeqFeatData::eQual_cons_splice },
    { eFQ_cyt_map, CSeqFeatData::eQual_map },
    { eFQ_db_xref, CSeqFeatData::eQual_db_xref },
    { eFQ_derived_from, CSeqFeatData::eQual_derived_from },
    { eFQ_direction, CSeqFeatData::eQual_direction },
    { eFQ_EC_number, CSeqFeatData::eQual_EC_number },
    { eFQ_encodes, CSeqFeatData::eQual_note },
    { eFQ_estimated_length, CSeqFeatData::eQual_estimated_length },
    { eFQ_experiment, CSeqFeatData::eQual_experiment },
    { eFQ_exception, CSeqFeatData::eQual_exception },
    { eFQ_exception_note, CSeqFeatData::eQual_note },
    { eFQ_figure, CSeqFeatData::eQual_note },
    { eFQ_frequency, CSeqFeatData::eQual_frequency },
    { eFQ_function, CSeqFeatData::eQual_function },
    { eFQ_gap_type, CSeqFeatData::eQual_gap_type },
    { eFQ_gene, CSeqFeatData::eQual_gene },
    { eFQ_gene_desc, CSeqFeatData::eQual_note },
    { eFQ_gene_allele, CSeqFeatData::eQual_allele },
    { eFQ_gene_map, CSeqFeatData::eQual_map },
    { eFQ_gene_syn, CSeqFeatData::eQual_note },
    { eFQ_gene_syn_refseq, CSeqFeatData::eQual_note },
    { eFQ_gene_note, CSeqFeatData::eQual_note },
    { eFQ_gene_xref, CSeqFeatData::eQual_db_xref },
    { eFQ_go_component, CSeqFeatData::eQual_note },
    { eFQ_go_function, CSeqFeatData::eQual_note },
    { eFQ_go_process, CSeqFeatData::eQual_note },
    { eFQ_heterogen, CSeqFeatData::eQual_heterogen },
    { eFQ_illegal_qual, CSeqFeatData::eQual_bad },
    { eFQ_inference, CSeqFeatData::eQual_inference },
    { eFQ_label, CSeqFeatData::eQual_label },
    { eFQ_linkage_evidence, CSeqFeatData::eQual_linkage_evidence },
    { eFQ_locus_tag, CSeqFeatData::eQual_locus_tag },
    { eFQ_map, CSeqFeatData::eQual_map },
    { eFQ_maploc, CSeqFeatData::eQual_note },
    { eFQ_mobile_element, CSeqFeatData::eQual_mobile_element },
    { eFQ_mobile_element_type, CSeqFeatData::eQual_mobile_element_type },
    { eFQ_mod_base, CSeqFeatData::eQual_mod_base },
    { eFQ_modelev, CSeqFeatData::eQual_note },
    { eFQ_mol_wt, CSeqFeatData::eQual_calculated_mol_wt },
    { eFQ_ncRNA_class, CSeqFeatData::eQual_ncRNA_class },
    { eFQ_nomenclature, CSeqFeatData::eQual_nomenclature },
    { eFQ_number, CSeqFeatData::eQual_number },
    { eFQ_old_locus_tag, CSeqFeatData::eQual_old_locus_tag },
    { eFQ_operon, CSeqFeatData::eQual_operon },
    { eFQ_organism, CSeqFeatData::eQual_organism },
    { eFQ_partial, CSeqFeatData::eQual_partial },
    { eFQ_PCR_conditions, CSeqFeatData::eQual_PCR_conditions },
    { eFQ_peptide, CSeqFeatData::eQual_bad },
    { eFQ_phenotype, CSeqFeatData::eQual_phenotype },
    { eFQ_product, CSeqFeatData::eQual_product },
    { eFQ_product_quals, CSeqFeatData::eQual_product },
    { eFQ_prot_activity, CSeqFeatData::eQual_function },
    { eFQ_prot_comment, CSeqFeatData::eQual_note },
    { eFQ_prot_EC_number, CSeqFeatData::eQual_EC_number },
    { eFQ_prot_note, CSeqFeatData::eQual_note },
    { eFQ_prot_method, CSeqFeatData::eQual_note },
    { eFQ_prot_conflict, CSeqFeatData::eQual_note },
    { eFQ_prot_desc, CSeqFeatData::eQual_note },
    { eFQ_prot_missing, CSeqFeatData::eQual_note },
    { eFQ_prot_name, CSeqFeatData::eQual_name },
    { eFQ_prot_names, CSeqFeatData::eQual_note },
    { eFQ_protein_id, CSeqFeatData::eQual_protein_id },
    { eFQ_pseudo, CSeqFeatData::eQual_pseudo },
    { eFQ_pseudogene, CSeqFeatData::eQual_pseudogene },
    { eFQ_region, CSeqFeatData::eQual_note },
    { eFQ_region_name, CSeqFeatData::eQual_region_name },
    { eFQ_replace, CSeqFeatData::eQual_replace },
    { eFQ_ribosomal_slippage, CSeqFeatData::eQual_ribosomal_slippage },
    { eFQ_rpt_family, CSeqFeatData::eQual_rpt_family },
    { eFQ_rpt_type, CSeqFeatData::eQual_rpt_type },
    { eFQ_rpt_unit, CSeqFeatData::eQual_rpt_unit },
    { eFQ_rpt_unit_range, CSeqFeatData::eQual_rpt_unit_range },
    { eFQ_rpt_unit_seq, CSeqFeatData::eQual_rpt_unit_seq },
    { eFQ_rrna_its, CSeqFeatData::eQual_note },
    { eFQ_satellite, CSeqFeatData::eQual_satellite },
    { eFQ_sec_str_type, CSeqFeatData::eQual_sec_str_type },
//    { eFQ_selenocysteine, CSeqFeatData::eQual_note },
//    { eFQ_selenocysteine_note, CSeqFeatData::eQual_note },
    { eFQ_seqfeat_note, CSeqFeatData::eQual_note },
    { eFQ_site, CSeqFeatData::eQual_note },
    { eFQ_site_type, CSeqFeatData::eQual_site_type },
    { eFQ_standard_name, CSeqFeatData::eQual_standard_name },
    { eFQ_tag_peptide, CSeqFeatData::eQual_tag_peptide },
    { eFQ_trans_splicing, CSeqFeatData::eQual_trans_splicing },
    { eFQ_transcription, CSeqFeatData::eQual_bad },
    { eFQ_transcript_id, CSeqFeatData::eQual_note },
    { eFQ_transcript_id_note, CSeqFeatData::eQual_note },
    { eFQ_transl_except, CSeqFeatData::eQual_transl_except },
    { eFQ_transl_table, CSeqFeatData::eQual_transl_table },
    { eFQ_translation, CSeqFeatData::eQual_translation },
    { eFQ_trna_aa, CSeqFeatData::eQual_bad },
    { eFQ_trna_codons, CSeqFeatData::eQual_note },
    { eFQ_UniProtKB_evidence, CSeqFeatData::eQual_UniProtKB_evidence },
    { eFQ_usedin, CSeqFeatData::eQual_usedin },
    { eFQ_xtra_prod_quals, CSeqFeatData::eQual_note }
};
typedef CStaticPairArrayMap<EFeatureQualifier, CSeqFeatData::EQualifier> TQualMap;
DEFINE_STATIC_ARRAY_MAP(TQualMap, sc_QualMap, sc_GbToFeatQualMap);

static CSeqFeatData::EQualifier s_GbToSeqFeatQual(EFeatureQualifier qual)
{
    TQualMap::const_iterator it = sc_QualMap.find(qual);
    if ( it != sc_QualMap.end() ) {
        return it->second;
    }
    return CSeqFeatData::eQual_bad;
}


void CFeatureItem::x_DropIllegalQuals(void) const
{
    const CSeqFeatData& data = m_Feat.GetData();

    TQI it = m_Quals.begin();
    while ( it != m_Quals.end() ) {
        CSeqFeatData::EQualifier qual = s_GbToSeqFeatQual(it->first);
        if ( !data.IsLegalQualifier(qual) ) {
            it = m_Quals.Erase(it);
        } else {
            ++it;
        }
    }
}

bool CFeatureItem::x_IsSeqFeatDataFeatureLegal( CSeqFeatData::EQualifier qual )
{
    const CSeqFeatData& data = m_Feat.GetData();
    return data.IsLegalQualifier(qual);
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableQuals(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    bool pseudo = m_Feat.IsSetPseudo()  &&  m_Feat.GetPseudo();

    const CSeqFeatData& data = m_Feat.GetData();

    switch ( m_Feat.GetData().Which() ) {
    case CSeqFeatData::e_Gene:
        pseudo |= x_AddFTableGeneQuals(data.GetGene());
        break;
    case CSeqFeatData::e_Rna:
        x_AddFTableRnaQuals(m_Feat, ctx);
        break;
    case CSeqFeatData::e_Cdregion:
        x_AddFTableCdregionQuals(m_Feat, ctx);
        break;
    case CSeqFeatData::e_Prot:
        x_AddFTableProtQuals(m_Feat);
        break;
    case CSeqFeatData::e_Region:
        x_AddFTableRegionQuals(data.GetRegion());
        break;
    case CSeqFeatData::e_Bond:
        x_AddFTableBondQuals(data.GetBond());
        break;
    case CSeqFeatData::e_Site:
        x_AddFTableSiteQuals(data.GetSite());
        break;
    case CSeqFeatData::e_Psec_str:
        x_AddFTablePsecStrQuals(data.GetPsec_str());
        break;
    case CSeqFeatData::e_Het:
        x_AddFTablePsecStrQuals(data.GetHet());
        break;
    case CSeqFeatData::e_Biosrc:
        x_AddFTableBiosrcQuals(data.GetBiosrc());
        break;
    default:
        break;
    }
    if ( pseudo ) {
        x_AddFTableQual("pseudo");
    }
    const CGene_ref* grp = m_Feat.GetGeneXref();
    if ( grp != 0  &&  grp->IsSuppressed() ) {
        x_AddFTableQual("gene", "-");
    }
    if ( m_Feat.IsSetComment()  &&  !m_Feat.GetComment().empty() ) {
        x_AddFTableQual("note", m_Feat.GetComment());
    }
    if ( m_Feat.IsSetExp_ev() ) {
        string ev;
        switch ( m_Feat.GetExp_ev() ) {
        case CSeq_feat::eExp_ev_experimental:
            ev = "experimental";
            break;
        case CSeq_feat::eExp_ev_not_experimental:
            ev = "not_experimental";
            break;
        }
        x_AddFTableQual("evidence", ev);
    }
    if ( m_Feat.IsSetExcept_text()  &&  !m_Feat.GetExcept_text().empty() ) {
        string exception_text = m_Feat.GetExcept_text();
        if ( exception_text == "ribosomal slippage" ) {
          x_AddFTableQual("ribosomal_slippage");
        }
        else if ( exception_text == "trans-splicing" ) {
          x_AddFTableQual("trans_splicing");
        }
        x_AddFTableQual("exception", m_Feat.GetExcept_text());
    } else if ( m_Feat.IsSetExcept()  &&  m_Feat.GetExcept() ) {
        x_AddFTableQual("exception");
    }
    const CSeq_feat_Base::TQual & qual = m_Feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
    ITERATE( CSeq_feat::TQual, it, qual ) {
        const CGb_qual& qual = **it;
        const string& key = qual.IsSetQual() ? qual.GetQual() : kEmptyStr;
        const string& val = qual.IsSetVal() ? qual.GetVal() : kEmptyStr;
        if ( !key.empty()  &&  !val.empty() ) {
            x_AddFTableQual(key, val);
        }
    }
    if ( m_Feat.IsSetExt() ) {
        x_AddFTableExtQuals(m_Feat.GetExt());
    }
    if ( data.IsGene() ) {
        x_AddFTableDbxref(data.GetGene().GetDb());
    } else if ( data.IsProt() ) {
        x_AddFTableDbxref(data.GetProt().GetDb());
    }
    x_AddFTableDbxref(m_Feat.GetDbxref());
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableExtQuals(
    const CSeq_feat::TExt& ext )
//  ----------------------------------------------------------------------------
{
    ITERATE (CUser_object::TData, it, ext.GetData()) {
        const CUser_field& field = **it;
        if ( !field.IsSetData() ) {
            continue;
        }
        if ( field.GetData().IsObject() ) {
            const CUser_object& obj = field.GetData().GetObject();
            x_AddQualsExt(obj);
            return;
        } else if ( field.GetData().IsObjects() ) {
            ITERATE (CUser_field::C_Data::TObjects, o, field.GetData().GetObjects()) {
                x_AddQualsExt(**o);
            }
            return;
        }
    }
    if ( ext.IsSetType()  &&  ext.GetType().IsStr() ) {
        const string& oid = ext.GetType().GetStr();
        if ( oid == "GeneOntology" ) {
            ITERATE (CUser_object::TData, uf_it, ext.GetData()) {
                const CUser_field& field = **uf_it;
                if ( field.IsSetLabel()  &&  field.GetLabel().IsStr() ) {
                    const string& label = field.GetLabel().GetStr();
                    string name;
                    if ( label == "Process" ) {
                        name = "GO_process";
                    } else if ( label == "Component" ) {               
                        name = "GO_component";
                    } else if ( label == "Function" ) {
                        name = "GO_function";
                    }
                    if ( name.empty() ) {
                        continue;
                    }
                    
                    ITERATE (CUser_field::TData::TFields, it, field.GetData().GetFields()) {
                        if ( (*it)->GetData().IsFields() ) {
                            CFlatGoQVal(**it).Format(m_FTableQuals, name, *GetContext(), 0);;
                        }
                    }
                }
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableDbxref(
    const CSeq_feat::TDbxref& dbxref )
//  ----------------------------------------------------------------------------
{
    ITERATE (CSeq_feat::TDbxref, it, dbxref) {
        const CDbtag& dbt = **it;
        if ( dbt.IsSetDb()  &&  !dbt.GetDb().empty()  &&
             dbt.IsSetTag() ) {
            const CObject_id& oid = dbt.GetTag();
            switch ( oid.Which() ) {
            case CObject_id::e_Str:
                if ( !oid.GetStr().empty() ) {
                    x_AddFTableQual("db_xref", dbt.GetDb() + ":" + oid.GetStr());
                }
                break;
            case CObject_id::e_Id:
                x_AddFTableQual("db_xref", dbt.GetDb() + ":" + NStr::IntToString(oid.GetId()));
                break;
            default:
                break;
            }
        }
    }
}

//  ----------------------------------------------------------------------------
bool CFeatureItem::x_AddFTableGeneQuals(
    const CGene_ref& gene )
//  ----------------------------------------------------------------------------
{
    if ( gene.IsSetLocus()  &&  !gene.GetLocus().empty() ) {
        x_AddFTableQual("gene", gene.GetLocus(), CFormatQual::eTrim_WhitespaceOnly);
    }
    ITERATE (CGene_ref::TSyn, it, gene.GetSyn()) {
        x_AddFTableQual("gene_syn", *it, CFormatQual::eTrim_WhitespaceOnly);
    }
    if ( gene.IsSetDesc()  &&  !gene.GetDesc().empty() ) {
        x_AddFTableQual("gene_desc", gene.GetDesc());
    }
    if ( gene.IsSetMaploc()  &&  !gene.GetMaploc().empty() ) {
        x_AddFTableQual("map", gene.GetMaploc());
    }
    if ( gene.IsSetLocus_tag()  &&  !gene.GetLocus_tag().empty() ) {
        x_AddFTableQual("locus_tag", gene.GetLocus_tag(), CFormatQual::eTrim_WhitespaceOnly);
    }

    return (gene.IsSetPseudo()  &&  gene.GetPseudo());
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableRnaQuals(
    const CMappedFeat& feat, 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    string label;

    if ( !feat.GetData().IsRna() ) {
        return;
    }
    const CSeqFeatData::TRna& rna = feat.GetData().GetRna();
    if (rna.IsSetExt()) {
        const CRNA_ref::TExt& ext = rna.GetExt();
        if (ext.IsName()) {
            if (!ext.GetName().empty()) {
                x_AddFTableQual("product", ext.GetName());
            }
        } else if (ext.IsTRNA()) {
            feature::GetLabel(feat.GetOriginalFeature(), &label,
                              feature::fFGL_Content, &ctx.GetScope());
            x_AddFTableQual("product", label);
        }
    }

    if ( feat.IsSetProduct() ) {
        CBioseq_Handle prod = 
            ctx.GetScope().GetBioseqHandle(m_Feat.GetProductId());
        if ( prod ) {
            CConstRef<CSeq_id> id = GetId(prod, eGetId_Best).GetSeqId();
            string id_str;
            if ( id->IsGenbank()  ||  id->IsEmbl()  ||  id->IsDdbj()  ||
                 id->IsTpg()  ||  id->IsTpd()  ||  id->IsTpe()  ||
                 id->IsOther() ||
                 (id->IsLocal()  &&  !ctx.Config().SuppressLocalId()) ) {
                id_str = id->GetSeqIdString(true);
            } else if ( id->IsGeneral() ) {
                id_str = id->AsFastaString();
            }
            x_AddFTableQual("transcript_id", id_str);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableCdregionQuals(
    const CMappedFeat& feat, 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    CBioseq_Handle prod;
    if ( feat.IsSetProduct() ) {
        prod = ctx.GetScope().GetBioseqHandle(feat.GetProductId());
    }
    if ( prod ) {
        CMappedFeat prot_ref = s_GetBestProtFeature(prod);
        if ( prot_ref ) {
            /// FIXME: we take the first; we want the longest
            x_AddFTableProtQuals(prot_ref);
        }
    }
    const CCdregion& cdr = feat.GetData().GetCdregion();
    if ( cdr.IsSetFrame()  &&  cdr.GetFrame() > CCdregion::eFrame_one ) {
        x_AddFTableQual("codon_start", NStr::IntToString(cdr.GetFrame()));
    }
    ITERATE (CCdregion::TCode_break, it, cdr.GetCode_break()) {
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
            break;
        }
        x_AddFTableQual("transl_except", "(pos:" + pos + ",aa:" + aa + ")");
    }
    CConstRef<CSeq_id> id;
    string id_str;
    if ( prod ) {
        id = GetId(prod, eGetId_Best).GetSeqId();
    } else if ( feat.IsSetProduct() ) {
        try { 
            id.Reset(&GetId(feat.GetProduct(), &ctx.GetScope()));
            if ( id->IsGi() ) {
                // get "normal" id 
            }
        } catch (CObjmgrUtilException&) {
            id.Reset();
        }
    }
    if ( id ) {
        if ( id->IsGenbank()  ||  id->IsEmbl()  ||  id->IsDdbj()  ||
             id->IsTpg()  ||  id->IsTpd()  ||  id->IsTpe()  ||
             id->IsOther() ||
             (id->IsLocal()  &&  !ctx.Config().SuppressLocalId()) ) {
            id_str = id->GetSeqIdString(true);
        } else if ( id->IsGi() ) {
            id_str = id->AsFastaString();
        }
        x_AddFTableQual("protein_id", id_str);
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableProtQuals(
    const CMappedFeat& prot )
//  ----------------------------------------------------------------------------
{
    if ( !prot.GetData().IsProt() ) {
        return;
    }
    const CProt_ref& pref = prot.GetData().GetProt();
    ITERATE (CProt_ref::TName, it, pref.GetName()) {
        if ( !it->empty() ) {
            x_AddFTableQual("product", *it);
        }
    }
    if ( pref.IsSetDesc()  &&  !pref.GetDesc().empty() ) {
        x_AddFTableQual("prot_desc", pref.GetDesc());
    }
    ITERATE (CProt_ref::TActivity, it, pref.GetActivity()) {
        if ( !it->empty() ) {
            x_AddFTableQual("function", *it);
        }
    }
    ITERATE (CProt_ref::TEc, it, pref.GetEc()) {
        if ( !it->empty() ) {
            x_AddFTableQual("EC_number", *it);
        }
    }
    if ( prot.IsSetComment()  &&  !prot.GetComment().empty() ) {
        x_AddFTableQual("prot_note", prot.GetComment());
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableRegionQuals(
    const CSeqFeatData::TRegion& region )
//  ----------------------------------------------------------------------------
{
    if ( !region.empty() ) {
        x_AddFTableQual("region", region);
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableBondQuals(
    const CSeqFeatData::TBond& bond )
//  ----------------------------------------------------------------------------
{
    x_AddFTableQual("bond_type", s_GetBondName(bond));
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableSiteQuals(
    const CSeqFeatData::TSite& site)
//  ----------------------------------------------------------------------------
{
    x_AddFTableQual("site_type", s_GetSiteName(site));
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTablePsecStrQuals(
    const CSeqFeatData::TPsec_str& psec_str )
//  ----------------------------------------------------------------------------
{
    const string& psec = CSeqFeatData::ENUM_METHOD_NAME(EPsec_str)()->FindName(
        psec_str, true );
    x_AddFTableQual("sec_str_type", psec);
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTablePsecStrQuals(
    const CSeqFeatData::THet& het)
//  ----------------------------------------------------------------------------
{
    if ( !het.Get().empty() ) {
        x_AddFTableQual("heterogen", het.Get());
    }
}


static const string s_GetSubtypeString(const COrgMod::TSubtype& subtype)
{
    switch ( subtype ) {
        case COrgMod::eSubtype_strain:           return "strain";
        case COrgMod::eSubtype_substrain:        return "substrain";
        case COrgMod::eSubtype_type:             return "type";
        case COrgMod::eSubtype_subtype:          return "subtype";
        case COrgMod::eSubtype_variety:          return "variety";
        case COrgMod::eSubtype_serotype:         return "serotype";
        case COrgMod::eSubtype_serogroup:        return "serogroup";
        case COrgMod::eSubtype_serovar:          return "serovar";
        case COrgMod::eSubtype_cultivar:         return "cultivar";
        case COrgMod::eSubtype_pathovar:         return "pathovar";
        case COrgMod::eSubtype_chemovar:         return "chemovar";
        case COrgMod::eSubtype_biovar:           return "biovar";
        case COrgMod::eSubtype_biotype:          return "biotype";
        case COrgMod::eSubtype_group:            return "group";
        case COrgMod::eSubtype_subgroup:         return "subgroup";
        case COrgMod::eSubtype_isolate:          return "isolate";
        case COrgMod::eSubtype_common:           return "common";
        case COrgMod::eSubtype_acronym:          return "acronym";
        case COrgMod::eSubtype_dosage:           return "dosage";
        case COrgMod::eSubtype_nat_host:         return "nat_host";
        case COrgMod::eSubtype_sub_species:      return "sub_species";
        case COrgMod::eSubtype_specimen_voucher: return "specimen_voucher";
        case COrgMod::eSubtype_authority:        return "authority";
        case COrgMod::eSubtype_forma:            return "forma";
        case COrgMod::eSubtype_forma_specialis:  return "dosage";
        case COrgMod::eSubtype_ecotype:          return "ecotype";
        case COrgMod::eSubtype_synonym:          return "synonym";
        case COrgMod::eSubtype_anamorph:         return "anamorph";
        case COrgMod::eSubtype_teleomorph:       return "teleomorph";
        case COrgMod::eSubtype_breed:            return "breed";
        case COrgMod::eSubtype_gb_acronym:       return "gb_acronym";
        case COrgMod::eSubtype_gb_anamorph:      return "gb_anamorph";
        case COrgMod::eSubtype_gb_synonym:       return "gb_synonym";
        case COrgMod::eSubtype_old_lineage:      return "old_lineage";
        case COrgMod::eSubtype_old_name:         return "old_name";
        case COrgMod::eSubtype_culture_collection: return "culture_collection";
        case COrgMod::eSubtype_bio_material:     return "bio_material";
        case COrgMod::eSubtype_metagenome_source: return "metagenome_source";
        case COrgMod::eSubtype_type_material:    return "type_material";
        case COrgMod::eSubtype_other:            return "note";
        default:                                 return kEmptyStr;
    }
    return kEmptyStr;
}


static const string s_GetSubsourceString(const CSubSource::TSubtype& subtype)
{
    switch ( subtype ) {
        case CSubSource::eSubtype_chromosome: return "chromosome";
        case CSubSource::eSubtype_map: return "map";
        case CSubSource::eSubtype_clone: return "clone";
        case CSubSource::eSubtype_subclone: return "subclone";
        case CSubSource::eSubtype_haplogroup: return "haplogroup";
        case CSubSource::eSubtype_haplotype: return "haplotype";
        case CSubSource::eSubtype_genotype: return "genotype";
        case CSubSource::eSubtype_sex: return "sex";
        case CSubSource::eSubtype_cell_line: return "cell_line";
        case CSubSource::eSubtype_cell_type: return "cell_type";
        case CSubSource::eSubtype_tissue_type: return "tissue_type";
        case CSubSource::eSubtype_clone_lib: return "clone_lib";
        case CSubSource::eSubtype_dev_stage: return "dev_stage";
        case CSubSource::eSubtype_frequency: return "frequency";
        case CSubSource::eSubtype_germline: return "germline";
        case CSubSource::eSubtype_rearranged: return "rearranged";
        case CSubSource::eSubtype_lab_host: return "lab_host";
        case CSubSource::eSubtype_pop_variant: return "pop_variant";
        case CSubSource::eSubtype_tissue_lib: return "tissue_lib";
        case CSubSource::eSubtype_plasmid_name: return "plasmid_name";
        case CSubSource::eSubtype_transposon_name: return "transposon_name";
        case CSubSource::eSubtype_insertion_seq_name: return "insertion_seq_name";
        case CSubSource::eSubtype_plastid_name: return "plastid_name";
        case CSubSource::eSubtype_country: return "country";
        case CSubSource::eSubtype_segment: return "segment";
        case CSubSource::eSubtype_endogenous_virus_name: return "endogenous_virus_name";
        case CSubSource::eSubtype_transgenic: return "transgenic";
        case CSubSource::eSubtype_environmental_sample: return "environmental_sample";
        case CSubSource::eSubtype_isolation_source: return "isolation_source";
        case CSubSource::eSubtype_other: return "note";
        default: return kEmptyStr;
    }
    return kEmptyStr;
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableBiosrcQuals(
    const CBioSource& src )
//  ----------------------------------------------------------------------------
{
    if ( src.IsSetOrg() ) {
        const CBioSource::TOrg& org = src.GetOrg();

        if ( org.IsSetTaxname()  &&  !org.GetTaxname().empty() ) {
            x_AddFTableQual("organism", org.GetTaxname());
        }

        if ( org.IsSetOrgname() ) {
            ITERATE (COrgName::TMod, it, org.GetOrgname().GetMod()) {
                if ( (*it)->IsSetSubtype() ) {
                    string str = s_GetSubtypeString((*it)->GetSubtype());
                    if ( str.empty() ) {
                        continue;
                    }
                    if ( (*it)->IsSetSubname()  &&  !(*it)->GetSubname().empty() ) {
                        str += (*it)->GetSubname();
                    }
                    x_AddFTableQual(str);
                }
            }
        }
    }

    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        if ( (*it)->IsSetSubtype() ) {
            string str = s_GetSubsourceString((*it)->GetSubtype());
            if ( str.empty() ) {
                continue;
            }
            if ( (*it)->IsSetName() ) {
                str += (*it)->GetName();
            }
            x_AddFTableQual(str);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//   Source Feature
/////////////////////////////////////////////////////////////////////////////

CSourceFeatureItem::CSourceFeatureItem
(const CMappedFeat& feat,
 CBioseqContext& ctx,
 CRef<feature::CFeatTree> ftree,
 const CSeq_loc* loc)
    : CFeatureItemBase(feat, ctx, ftree, loc ? loc : &feat.GetLocation()),
      m_WasDesc(false), m_IsFocus(false), m_IsSynthetic(false)
{
    x_GatherInfo(ctx);
}


void CSourceFeatureItem::x_GatherInfo(CBioseqContext& ctx)
{
    const CBioSource& bsrc = GetSource();
    if (!bsrc.IsSetOrg()) {
        m_Feat = CMappedFeat();
        x_SetSkip();
        return;
    }

    m_IsFocus = bsrc.IsSetIs_focus();
    if (bsrc.GetOrigin() == CBioSource::eOrigin_synthetic) {
        m_IsSynthetic = true;
    }
    if (!m_IsSynthetic  &&  bsrc.GetOrg().IsSetOrgname()) {
        m_IsSynthetic = bsrc.GetOrg().GetOrgname().IsSetDiv()  &&
            NStr::EqualNocase(bsrc.GetOrg().GetOrgname().GetDiv(), "SYN");
    }
    if (!m_IsSynthetic  &&  bsrc.IsSetOrg() && bsrc.GetOrg().IsSetTaxname()) {
        if (NStr::EqualNocase(bsrc.GetOrg().GetTaxname(), "synthetic construct")) {
            m_IsSynthetic = true;
        }
    }
    x_AddQuals(ctx);
}


void CSourceFeatureItem::x_AddQuals(CBioseqContext& ctx)
{
    const CSeqFeatData& data = m_Feat.GetData();
    _ASSERT(data.IsOrg()  ||  data.IsBiosrc());
    // add various generic qualifiers...
    x_AddQual(eSQ_mol_type,
              new CFlatMolTypeQVal(ctx.GetBiomol(), ctx.GetMol()));
    if (m_Feat.IsSetComment()) {
        x_AddQual(eSQ_seqfeat_note, new CFlatStringQVal(m_Feat.GetComment()));
    }
    if (m_Feat.IsSetTitle()) {
        x_AddQual(eSQ_label, new CFlatLabelQVal(m_Feat.GetTitle()));
    }
    if (m_Feat.IsSetCit()) {
        x_AddQual(eSQ_citation, new CFlatPubSetQVal(m_Feat.GetCit()));
    }
    if (m_Feat.IsSetDbxref()) {
        x_AddQual(eSQ_org_xref, new CFlatXrefQVal(m_Feat.GetDbxref()));
    }

    // add qualifiers from biosource fields
    x_AddQuals(data.GetBiosrc(), ctx);
}


static ESourceQualifier s_OrgModToSlot(const COrgMod& om)
{
    return GetSourceQualOfOrgMod( static_cast<COrgMod::ESubtype>(om.GetSubtype()) );
}

static string s_GetSpecimenVoucherText(
    CBioseqContext& ctx,
    const string& strRawName )
{
    if ( ! ctx.Config().DoHTML() ) {
        return strRawName;
    }
    
    // doesn't COrgMod already have the code for this?
    string inst;
    string id;
    {
        string coll;
        if( ! COrgMod::ParseStructuredVoucher(strRawName, inst, coll, id) ) {
            return strRawName;
        }
        if( ! coll.empty() ) {
            inst += ':' + coll;
        }
    }
    
    CInstInfoMap::TVoucherInfoRef voucher_info_ref = CInstInfoMap::GetInstitutionVoucherInfo( inst );
    if( voucher_info_ref ) {
        CNcbiOstrstream text;

        text << "<acronym title=\"" 
             << NStr::Replace(voucher_info_ref->m_InstFullName, "\"", "&quot;") 
             << "\" class=\"voucher\">"
             << inst << "</acronym>"
             << ":"
             << "<a href=\"" << *voucher_info_ref->m_Links;
        if( voucher_info_ref->m_PrependInstitute) {
            text << inst;
        }
        if( voucher_info_ref->m_Prefix != NULL ) {
            text << *voucher_info_ref->m_Prefix;
        }
        text << id;
        if( voucher_info_ref->m_Suffix ) {
            text << *voucher_info_ref->m_Suffix;
        }
        text << "\">" << id << "</a>";
        return CNcbiOstrstreamToString(text);
    } else {
        // fall back on at least getting institution name
        const string &inst_full_name =  COrgMod::GetInstitutionFullName( inst );
        if( ! inst_full_name.empty() ) {
            CNcbiOstrstream text;

            text << "<acronym title=\"" << NStr::Replace(inst_full_name, "\"", "&quot;") << "\" class=\"voucher\">"
                << inst << "</acronym>"
                << ":" << id;

            return CNcbiOstrstreamToString(text);
        } else {
            // if all else fails, return the string we were initially given
            return strRawName;
        }
    }
}


void CSourceFeatureItem::x_AddQuals(const COrg_ref& org, CBioseqContext& ctx) const
{
    string taxname, common;
    if ( org.IsSetTaxname() ) {
        taxname = org.GetTaxname();
    }
    if ( taxname.empty()  &&  ctx.Config().NeedOrganismQual() ) {
        taxname = "unknown";
        if ( org.IsSetCommon() ) {
            common = org.GetCommon();
        }
    }
    if ( !taxname.empty() ) {
        x_AddQual(eSQ_organism, new CFlatStringQVal(taxname));
    }
    if ( !common.empty() ) {
        x_AddQual(eSQ_common_name, new CFlatStringQVal(common));
    }
    if ( org.IsSetOrgname() ) {
        set<string> ecotypesSeen;  // holds the ones we've seen so don't show them again
        ecotypesSeen.insert(kEmptyStr); // empty string is always considered seen so we hide it
        ITERATE (COrgName::TMod, it, org.GetOrgname().GetMod()) {
            
            const COrgMod& mod = **it;
            const string & sSubname = ( 
                mod.CanGetSubname() ? mod.GetSubname() : kEmptyStr );

            ESourceQualifier slot = s_OrgModToSlot(**it);
            switch( slot ) {
            case eSQ_ecotype:
                if( ecotypesSeen.find(sSubname) != ecotypesSeen.end() ) {
                    break; // already seen
                }
                ecotypesSeen.insert( sSubname );
                x_AddQual(slot, new CFlatOrgModQVal(mod));
                break;
            case eSQ_none:
                break;
            default:
                if( COrgMod::HoldsInstitutionCode(mod.GetSubtype()) ) {
                    CRef<COrgMod> new_mod( new COrgMod(mod.GetSubtype(), 
                        (  sSubname.empty() ? kEmptyStr : s_GetSpecimenVoucherText(ctx, sSubname) ) ));
                    x_AddQual(slot, new CFlatOrgModQVal(*new_mod));
                } else {
                    x_AddQual(slot, new CFlatOrgModQVal(**it));
                }
                break;
            }
        }
    }
    if (!WasDesc()  &&  org.IsSetMod()) {
        x_AddQual(eSQ_unstructured, new CFlatStringListQVal(org.GetMod()));
    }
    if ( org.IsSetDb() ) {
        x_AddQual(eSQ_db_xref, new CFlatXrefQVal(org.GetDb()));
    }
}

void CSourceFeatureItem::x_AddPcrPrimersQuals(const CBioSource& src, CBioseqContext& ctx) const
{
    if( ! src.IsSetPcr_primers() ) {
        return;
    }

    const CBioSource_Base::TPcr_primers & primers = src.GetPcr_primers();
    if( primers.CanGet() ) {
        ITERATE( CBioSource_Base::TPcr_primers::Tdata, it, primers.Get() ) {
            string primer_value;

            bool has_fwd_seq = false;
            bool has_rev_seq = false;

            if( (*it)->IsSetForward() ) {
                const CPCRReaction_Base::TForward &forward = (*it)->GetForward();
                if( forward.CanGet() ) {
                    ITERATE( CPCRReaction_Base::TForward::Tdata, it2, forward.Get() ) {
                        const string &fwd_name = ( (*it2)->CanGetName() ? (*it2)->GetName().Get() : kEmptyStr );
                        if( ! fwd_name.empty() ) {
                            s_AddPcrPrimersQualsAppend( primer_value, "fwd_name: ", fwd_name);
                        }
                        const string &fwd_seq = ( (*it2)->CanGetSeq() ? (*it2)->GetSeq().Get() : kEmptyStr );
                        // NStr::ToLower( fwd_seq );
                        if( ! fwd_seq.empty() ) {
                            s_AddPcrPrimersQualsAppend( primer_value, "fwd_seq: ", fwd_seq);
                            has_fwd_seq = true;
                        }
                    }
                }
            }
            if( (*it)->IsSetReverse() ) {
                const CPCRReaction_Base::TReverse &reverse = (*it)->GetReverse();
                if( reverse.CanGet() ) {
                    ITERATE( CPCRReaction_Base::TReverse::Tdata, it2, reverse.Get() ) {
                        const string &rev_name = ((*it2)->CanGetName() ? (*it2)->GetName().Get() : kEmptyStr );
                        if( ! rev_name.empty() ) {
                            s_AddPcrPrimersQualsAppend( primer_value, "rev_name: ", rev_name);
                        }
                        const string &rev_seq = ( (*it2)->CanGetSeq() ? (*it2)->GetSeq().Get() : kEmptyStr );
                        // NStr::ToLower( rev_seq ); // do we need this? 
                        if( ! rev_seq.empty() ) {
                            s_AddPcrPrimersQualsAppend( primer_value, "rev_seq: ", rev_seq);
                            has_rev_seq = true;
                        }
                    }
                }
            }

            if( ! primer_value.empty() ) {
                const bool is_in_note = ( ! has_fwd_seq || ! has_rev_seq );
                if( is_in_note ) {
                    primer_value = "PCR_primers=" + primer_value;
                }
                const ESourceQualifier srcQual = ( is_in_note ? eSQ_pcr_primer_note : eSQ_PCR_primers );
                x_AddQual( srcQual, new CFlatStringQVal( primer_value ) );
            }
        }
    }
}

static ESourceQualifier s_SubSourceToSlot(const CSubSource& ss)
{
    return GetSourceQualOfSubSource( static_cast<CSubSource::ESubtype>(ss.GetSubtype()) );
}

void CSourceFeatureItem::x_AddQuals(const CBioSource& src, CBioseqContext& ctx) const
{
    // add qualifiers from Org_ref field
    if ( src.IsSetOrg() ) {
        x_AddQuals(src.GetOrg(), ctx);
    }
    x_AddQual(eSQ_focus, new CFlatBoolQVal(src.IsSetIs_focus()));

    
    bool insertion_seq_name = false,
         plasmid_name = false,
         transposon_name = false;

    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        ESourceQualifier slot = s_SubSourceToSlot(**it);

        switch( slot ) {

        case eSQ_insertion_seq_name:
            insertion_seq_name = true;
            x_AddQual(slot, new CFlatSubSourceQVal(**it));
            break;

        case eSQ_plasmid_name:
            plasmid_name = true;
            x_AddQual(slot, new CFlatSubSourceQVal(**it));
            break;

        case eSQ_transposon_name:
            transposon_name = true;
            x_AddQual(slot, new CFlatSubSourceQVal(**it));
            break;

        case eSQ_metagenomic:
            x_AddQual( eSQ_metagenomic, new CFlatStringQVal( "metagenomic") );
            break;

        default:
            if (slot != eSQ_none) {
                x_AddQual(slot, new CFlatSubSourceQVal(**it));
            }
            break;
        }
    }

    // Gets direct "pcr-primers" tag from file and adds the quals from that
    x_AddPcrPrimersQuals(src, ctx);

    // some qualifiers are flags in genome and names in subsource,
    // print once with name
    CBioSource::TGenome genome = src.GetGenome();
    CRef<CFlatOrganelleQVal> organelle(new CFlatOrganelleQVal(genome));
    if ( (insertion_seq_name  &&  genome == CBioSource::eGenome_insertion_seq)  ||
         (plasmid_name  &&  genome == CBioSource::eGenome_plasmid)  ||
         (transposon_name  &&  genome == CBioSource::eGenome_transposon) ) {
        organelle.Reset();
    }
    if ( organelle ) {
        x_AddQual(eSQ_organelle, organelle);
    }

    if ( !WasDesc()  &&  m_Feat.IsSetComment() ) {
        x_AddQual(eSQ_seqfeat_note, new CFlatStringQVal(m_Feat.GetComment()));
    }
}


void CSourceFeatureItem::x_FormatQuals(CFlatFeature& ff) const
{
    ff.SetQuals().reserve(m_Quals.Size());
    CFlatFeature::TQuals& qvec = ff.SetQuals();

#define DO_QUAL(x) x_FormatQual(eSQ_##x, GetStringOfSourceQual(eSQ_##x), qvec)
    DO_QUAL(organism);

    DO_QUAL(organelle);

    DO_QUAL(mol_type);

    DO_QUAL(strain);
    DO_QUAL(substrain);
    DO_QUAL(variety);
    DO_QUAL(serotype);
    DO_QUAL(serovar);
    DO_QUAL(cultivar);
    DO_QUAL(isolate);
    DO_QUAL(isolation_source);
    DO_QUAL(spec_or_nat_host);
    DO_QUAL(sub_species);

    DO_QUAL(specimen_voucher);
    DO_QUAL(culture_collection);
    DO_QUAL(bio_material);

    DO_QUAL(type_material);

    DO_QUAL(db_xref);
    DO_QUAL(org_xref);

    DO_QUAL(chromosome);

    DO_QUAL(segment);

    DO_QUAL(map);
    DO_QUAL(clone);
    DO_QUAL(subclone);
    DO_QUAL(haplotype);
    DO_QUAL(haplogroup);
    DO_QUAL(sex);
    DO_QUAL(mating_type);
    DO_QUAL(cell_line);
    DO_QUAL(cell_type);
    DO_QUAL(tissue_type);
    DO_QUAL(clone_lib);
    DO_QUAL(dev_stage);
    DO_QUAL(ecotype);

    if( ! GetContext()->Config().FrequencyToNote() ) {
        DO_QUAL(frequency);
    }

    DO_QUAL(germline);
    DO_QUAL(rearranged);
    DO_QUAL(transgenic);
    DO_QUAL(environmental_sample);

    DO_QUAL(lab_host);
    DO_QUAL(pop_variant);
    DO_QUAL(tissue_lib);

    DO_QUAL(plasmid_name);
    DO_QUAL(mobile_element);
    DO_QUAL(transposon_name);
    DO_QUAL(insertion_seq_name);

    DO_QUAL(country);

    DO_QUAL(focus);

    DO_QUAL(lat_lon);
    DO_QUAL(altitude);
    DO_QUAL(collection_date);
    DO_QUAL(collected_by);
    DO_QUAL(identified_by);
    DO_QUAL(PCR_primers);

    if ( !GetContext()->Config().SrcQualsToNote() ) {
        // some note qualifiers appear as regular quals in GBench or Dump mode
        x_FormatGBNoteQuals(ff);
    }

    DO_QUAL(sequenced_mol);
    DO_QUAL(label);
    DO_QUAL(usedin);
    // DO_QUAL(citation);
#undef DO_QUAL

    // Format the rest of the note quals (ones that weren't formatted above)
    // as a single note qualifier
    x_FormatNoteQuals(ff);
}


void CSourceFeatureItem::x_FormatGBNoteQuals(CFlatFeature& ff) const
{
    _ASSERT(!GetContext()->Config().SrcQualsToNote());
    CFlatFeature::TQuals& qvec = ff.SetQuals();

#define DO_QUAL(x) x_FormatQual(eSQ_##x, GetStringOfSourceQual(eSQ_##x), qvec)
    DO_QUAL(metagenomic);
    DO_QUAL(linkage_group);

    DO_QUAL(type);
    DO_QUAL(subtype);
    DO_QUAL(serogroup);
    DO_QUAL(pathovar);
    DO_QUAL(chemovar);
    DO_QUAL(biovar);
    DO_QUAL(biotype);
    DO_QUAL(group);
    DO_QUAL(subgroup);
    DO_QUAL(common);
    DO_QUAL(acronym);
    DO_QUAL(dosage);
    
    DO_QUAL(authority);
    DO_QUAL(forma);
    DO_QUAL(forma_specialis);
    DO_QUAL(synonym);
    DO_QUAL(anamorph);
    DO_QUAL(teleomorph);
    DO_QUAL(breed);
    if( GetContext()->Config().FrequencyToNote() ) {
        DO_QUAL(frequency);
    }

    DO_QUAL(metagenome_source),

//    DO_QUAL(collection_date);
//    DO_QUAL(collected_by);
//    DO_QUAL(identified_by);
//    DO_QUAL(pcr_primer);
    DO_QUAL(genotype);
    DO_QUAL(plastid_name);
    
    DO_QUAL(endogenous_virus_name);

    DO_QUAL(zero_orgmod);
    DO_QUAL(one_orgmod);
    DO_QUAL(zero_subsrc);
#undef DO_QUAL
}


void CSourceFeatureItem::x_FormatNoteQuals(CFlatFeature& ff) const
{
    CFlatFeature::TQuals qvec;
    bool add_period = false;

#define DO_NOTE(x) x_FormatNoteQual(eSQ_##x, #x, qvec)
    if (m_WasDesc) {
        x_FormatNoteQual(eSQ_seqfeat_note, "note", qvec);
        DO_NOTE(orgmod_note);
        DO_NOTE(subsource_note);
    } else {
        DO_NOTE(unstructured);
    }

    if ( GetContext()->Config().SrcQualsToNote() ) {
        DO_NOTE(metagenomic);
        DO_NOTE(linkage_group);
        DO_NOTE(type);
        DO_NOTE(subtype);
        DO_NOTE(serogroup);
        DO_NOTE(pathovar);
        DO_NOTE(chemovar);
        DO_NOTE(biovar);
        DO_NOTE(biotype);
        DO_NOTE(group);
        DO_NOTE(subgroup);
        DO_NOTE(common);
        DO_NOTE(acronym);
        DO_NOTE(dosage);
        
        DO_NOTE(authority);
        DO_NOTE(forma);
        DO_NOTE(forma_specialis);
        DO_NOTE(synonym);
        DO_NOTE(anamorph);
        DO_NOTE(teleomorph);
        DO_NOTE(breed);
        if( GetContext()->Config().FrequencyToNote() ) {
            DO_NOTE(frequency);
        }

        x_FormatNoteQual(eSQ_metagenome_source, "derived from metagenome", qvec);
        
        DO_NOTE(genotype);
        x_FormatNoteQual(eSQ_plastid_name, "plastid", qvec);
        
        x_FormatNoteQual(eSQ_endogenous_virus_name, "endogenous_virus", qvec);
    }
    DO_NOTE(pcr_primer_note);

    if (!m_WasDesc) {
        x_FormatNoteQual(eSQ_seqfeat_note, "note", qvec);
        DO_NOTE(orgmod_note);
        DO_NOTE(subsource_note);
    }

    x_FormatNoteQual(eSQ_common_name, "common", qvec);

    if ( GetContext()->Config().SrcQualsToNote() ) {
        x_FormatNoteQual(eSQ_zero_orgmod, "?", qvec);
        x_FormatNoteQual(eSQ_one_orgmod,  "?", qvec);
        x_FormatNoteQual(eSQ_zero_subsrc, "?", qvec);
    }
#undef DO_NOTE

    string notestr;
    string suffix = kEmptyStr;

    if ( GetSource().IsSetGenome()  &&  
        GetSource().GetGenome() == CBioSource::eGenome_extrachrom ) {
        static const string kEOL = "\n";
        notestr += "extrachromosomal";
        suffix = kEOL;
    }

    s_QualVectorToNote(qvec, true, notestr, suffix, add_period);
    s_NoteFinalize(add_period, notestr, ff, eTilde_note);
}


CSourceFeatureItem::CSourceFeatureItem
(const CBioSource& src,
 TRange range,
 CBioseqContext& ctx,
 CRef<feature::CFeatTree> ftree)
    : CFeatureItemBase(CMappedFeat(), ctx, ftree),
      m_WasDesc(true), m_IsFocus(false), m_IsSynthetic(false)
{
    if (!src.IsSetOrg()) {
        m_Feat = CMappedFeat();
        x_SetSkip();
        return;
    }
    x_SetObject(src);

    /// THIS IS A HACK
    /// We build a fake BioSource feature - even though BioSource features are
    /// a thing of the past, and the source descriptor is more common
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->SetData().SetBiosrc(const_cast<CBioSource&>(src));
    if ( range.IsWhole() ) {
        feat->SetLocation().SetWhole(*ctx.GetPrimaryId());
    } else {
        CSeq_interval& ival = feat->SetLocation().SetInt();
        ival.SetFrom(range.GetFrom());
        ival.SetTo(range.GetTo());
        ival.SetId(*ctx.GetPrimaryId());
    }

    CRef<CSeq_annot> an(new CSeq_annot);
    an->SetData().SetFtable().push_back(feat);

    CRef<CScope> local_scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_annot_Handle sah = local_scope->AddSeq_annot(*an);
    m_Feat = *(CFeat_CI(sah));
    m_Loc = &m_Feat.GetLocation();
    x_SetObject(m_Feat.GetOriginalFeature());

    /// END HACK

    x_GatherInfo(ctx);
}


void CSourceFeatureItem::x_FormatQual
(ESourceQualifier slot,
 const string& name,
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    TQCI it = const_cast<const TQuals&>(m_Quals).LowerBound(slot);
    TQCI end = const_cast<const TQuals&>(m_Quals).end();
    while (it != end  &&  it->first == slot) {
        const IFlatQVal* qual = it->second;
        qual->Format(qvec, name, *GetContext(),
                     flags | IFlatQVal::fIsSource);
        ++it;
    }
}


void CSourceFeatureItem::Subtract(const CSourceFeatureItem& other, CScope &scope)
{
    m_Loc = Seq_loc_Subtract(GetLoc(), other.GetLoc(), CSeq_loc::fStrand_Ignore, &scope);
}


void CSourceFeatureItem::SetLoc(const CSeq_loc& loc)
{
    m_Loc.Reset(&loc);
}


//  ----------------------------------------------------------------------------
void CFeatureItemGff::x_AddQualsRna(
    const CMappedFeat& feat,
    CBioseqContext& ctx,
    bool pseudo )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype subtype = m_Feat.GetData().GetSubtype();
    const CRNA_ref& rna = feat.GetData().GetRna();
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();

    CRNA_ref::TType rna_type = rna.IsSetType() ?
        rna.GetType() : CRNA_ref::eType_unknown;
    switch ( rna_type ) {
    case CRNA_ref::eType_tRNA:
    {
        if ( !pseudo  &&  cfg.ShowTranscript() ) {
            CSeqVector vec(feat.GetLocation(), scope);
            vec.SetCoding(CBioseq_Handle::eCoding_Iupac);
            string transcription;
            vec.GetSeqData(0, vec.size(), transcription);
            x_AddQual(eFQ_transcription, new CFlatStringQVal(transcription));
        }
        if (rna.IsSetExt()) {
            const CRNA_ref::C_Ext& ext = rna.GetExt();
            switch (ext.Which()) {
            case CRNA_ref::C_Ext::e_Name:
            {
                // amino acid could not be parsed into structured form
                if (!cfg.DropIllegalQuals()) {
                    x_AddQual(eFQ_product,
                        new CFlatStringQVal(ext.GetName()));
                } else {
                    x_AddQual(eFQ_product,
                        new CFlatStringQVal("tRNA-OTHER"));
                }
                break;
            }
            case CRNA_ref::C_Ext::e_TRNA:
            {
                const CTrna_ext& trna = ext.GetTRNA();
                int aa = 0;
                if ( trna.IsSetAa()  &&  trna.GetAa().IsNcbieaa() ) {
                    aa = trna.GetAa().GetNcbieaa();
                } else {
                    // !!!
                    return;
                }
/*                if (aa == 'U') {
                    if ( ctx.Config().SelenocysteineToNote() ) {
                        x_AddQual(eFQ_selenocysteine_note,
                            new CFlatStringQVal("selenocysteine"));
                    } else {
                        x_AddQual(eFQ_selenocysteine, new CFlatBoolQVal(true));
                    }
                }
*/                if ( cfg.IupacaaOnly() ) {
                    aa = s_ToIupacaa(aa);
                }
                const string& aa_str = s_AaName(aa);
                if ( !aa_str.empty() ) {
                    x_AddQual(eFQ_product, new CFlatStringQVal(aa_str));
                    if ( trna.IsSetAnticodon()  &&  !aa_str.empty() ) {
                        x_AddQual(eFQ_anticodon,
                            new CFlatAnticodonQVal(trna.GetAnticodon(),
                                                   aa_str.substr(5, NPOS)));
                    }
                }
                if ( trna.IsSetCodon() ) {
                    const string& comment =
                        m_Feat.IsSetComment() ? m_Feat.GetComment() : kEmptyStr;
                    x_AddQual(eFQ_trna_codons, new CFlatTrnaCodonsQVal(trna, comment));
                }
                break;
            }
            default:
                break;
            } // end of internal switch
        }
        break;
    }
    case CRNA_ref::eType_mRNA:
    {
        if ( !pseudo  &&  cfg.ShowTranscript() ) {
            CSeqVector vec(feat.GetLocation(), scope);
            vec.SetCoding(CBioseq_Handle::eCoding_Iupac);
            string transcription;
            vec.GetSeqData(0, vec.size(), transcription);
            x_AddQual(eFQ_transcription, new CFlatStringQVal(transcription));
        }
        // intentional fall through
    }
    default:
        switch ( subtype ) {

        case CSeqFeatData::eSubtype_ncRNA:
        case CSeqFeatData::eSubtype_tmRNA:
            break;
        case CSeqFeatData::eSubtype_misc_RNA:
        case CSeqFeatData::eSubtype_otherRNA:
            if ( rna.IsSetExt()  &&  rna.GetExt().IsName() ) {
                string strName = rna.GetExt().GetName();
                if ( strName != "misc_RNA" ) {
                    x_AddQual( eFQ_product, new CFlatStringQVal( strName ) );
                }
            }
            break;
        default:
            if ( rna.IsSetExt()  &&  rna.GetExt().IsName() ) {
                x_AddQual( eFQ_product, new CFlatStringQVal( rna.GetExt().GetName() ) );
            }
            break;
        }
    } // end of switch

    try {
        if (feat.IsSetProduct()) {
            CConstRef<CSeq_id> sip(feat.GetProduct().GetId());
            if (sip && sip->IsGi() ) {
                x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(*sip, true));
            }
        }
    } catch (CObjmgrUtilException&) {}
}

//  ----------------------------------------------------------------------------
bool CFeatureItem::x_GetGbValue(
    const string& key,
    string& value ) const
//  ----------------------------------------------------------------------------
{
    CSeq_feat::TQual gbQuals = m_Feat.GetQual();
    for ( CSeq_feat::TQual::iterator it = gbQuals.begin();
        it != gbQuals.end(); ++it ) 
    {
        //
        //  Idea:
        //  If a gbqual specifying the inference exists then bail out and let
        //  gbqual processing take care of this qualifier. If no such gbqual is
        //  present then add a default inference qualifier.
        //
        if (!(*it)->IsSetQual()  ||  !(*it)->IsSetVal()) {
            continue;
        }
        if ( (*it)->GetQual() == key ) {
            value = (*it)->GetVal();
            return true;
        }          
    }
    return false;
}

bool CFeatureItem::x_HasMethodtRNAscanSE(void) const
{
    // try to make this fast, since it could be checked by every feature.

    // try to do cheap checks first

    if( ! m_Feat.IsSetExt() ) {
        return false;
    }
    const CUser_object & ext = m_Feat.GetExt();
    if( ! ext.IsSetType() || ! ext.IsSetData() ) {
        return false;
    }
    const CUser_object_Base::TType & ext_type = ext.GetType();
    if( ! ext_type.IsStr() || ext_type.GetStr() != "CombinedFeatureUserObjects" ) {
        return false;
    }
    const CUser_object::TData & ext_data = ext.GetData();
    ITERATE( CUser_object::TData, field_iter, ext_data ) {
        const CUser_field & field = **field_iter;
        if( ! field.IsSetLabel() || ! field.IsSetData()  ) {
            continue;
        }
        const CUser_field::TLabel & field_label = field.GetLabel();
        const CUser_field::TData & field_data = field.GetData();
        if( ! field_label.IsStr() || ! field_data.IsObject() || 
            field_label.GetStr() != "ModelEvidence" ) 
        {
            continue;
        }
        const CUser_object & evidence_object = field_data.GetObject();
        if( ! evidence_object.IsSetData() ||
            ! evidence_object.IsSetType() || 
            ! evidence_object.GetType().IsStr() || 
            evidence_object.GetType().GetStr() != "ModelEvidence" ) 
        {
            continue;
        }
        const CUser_object::TData & evidence_data = evidence_object.GetData();
        ITERATE( CUser_object::TData, evidence_iter, evidence_data ) {
            const CUser_field & evidence_field = **evidence_iter;
            if( ! evidence_field.IsSetLabel() ||  
                ! evidence_field.GetLabel().IsStr() || 
                evidence_field.GetLabel().GetStr() != "Method" ||
                ! evidence_field.IsSetData() || 
                ! evidence_field.GetData().IsStr() ||
                evidence_field.GetData().GetStr() != "tRNAscan-SE" )
            {
                continue;
            }
            // we found proof of method tRNAscan-SE, so we return true
            return true;
        }
    }

    // didn't find any proof of method tRNAscan-SE
    return false;
}

END_SCOPE(objects)
END_NCBI_SCOPE

