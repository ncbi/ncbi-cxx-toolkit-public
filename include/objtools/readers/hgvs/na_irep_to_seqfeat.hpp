#ifndef _NA_IREP_TO_SEQFEAT_HPP_ 
#define _NA_IREP_TO_SEQFEAT_HPP_

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
 * Authors:  Justin Foley
 *
 * File Description: Code to convert NA variants from the intermediate representation to Seqfeats
 *
 */

#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objmgr/scope.hpp>
#include <objects/varrep/varrep__.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>
#include <objtools/readers/hgvs/id_resolver.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// Helper class for constructing a CDelta_item instances 
/// encoding intronic offsets.
class CIntronOffsetHelper 
{
public:
    static CRef<CDelta_item> GetIntronOffset(const CNtSite& nt_site);
    static CRef<CDelta_item> GetIntronOffset(const CNtSiteRange& nt_site_range);
    static CRef<CDelta_item> GetStartIntronOffset(const CNtLocation& nt_loc);
    static CRef<CDelta_item> GetStopIntronOffset(const CNtLocation& nt_loc);
    static CRef<CDelta_item> GetStartIntronOffset(const CNtInterval& nt_int);
    static CRef<CDelta_item> GetStopIntronOffset(const CNtInterval& nt_int);

private:
    static void x_SetDeltaItemOffset(TSeqPos length,
                                     const CDelta_item::TMultiplier multiplier,
                                     bool fuzzy, 
                                     CDelta_item& delta_item);   
};

/// Helper class for constructing a CSeq_loc instance for a NA variant
class CNaSeqlocHelper
{
public:

    /// Construct the CSeq_loc for a "simple" variant
    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CSimpleVariant& simple_var,
                                       const CSequenceVariant::TSeqtype& seq_type,
                                       CScope& scope);

    /// Construct the CSeq_loc object for a given Seq-id and nucleotide location 
    /// in the intermediate variant representation
    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CNtLocation& nt_loc,
                                       const CSequenceVariant::TSeqtype& seq_type,
                                       CScope& scope);

private:
    /// Construct the CSeq_loc object for a given Seq-id and nucleotide site 
    /// in the intermediate rerpresentation.
    static CRef<CSeq_loc> x_CreateSeqloc(const CSeq_id& seq_id,
                                       const CNtSite& nt_site,
                                       const CSequenceVariant::TSeqtype& seq_type,
                                       CScope& scope);

    /// Construct the CSeq_loc object for a given Seq-id and nucleotide site range
    /// in the intermediate representation
    static CRef<CSeq_loc> x_CreateSeqloc(const CSeq_id& seq_id,
                                       const CNtSiteRange& nt_range,
                                       const CSequenceVariant::TSeqtype& seq_type,
                                       CScope& scope);

    /// Construct the CSeq_loc object for a given Seq-id and nucleotide site interval 
    /// in the intermediate representation
    static CRef<CSeq_loc> x_CreateSeqloc(const CSeq_id& seq_id,
                                         const CNtInterval& nt_int,
                                         const CSequenceVariant::TSeqtype& seq_type,
                                         CScope& scope);

    /// Attempt to translate a nucleotide interval limit in the intermediate 
    /// variant representation into a residue index on a Bioseq instance.
    static bool x_ComputeSiteIndex(const CSeq_id& seq_id,
                                   const CNtIntLimit& nt_limit,
                                   const CSequenceVariant::TSeqtype& seq_type,
                                   CScope& scope,
                                   TSeqPos& site_index);

    /// Attempt to translate a nucleotide site in the intermediate 
    /// variant representation into a residue index on a Bioseq instance.
    static bool x_ComputeSiteIndex(const CSeq_id& seq_id,
                                   const CNtSite& nt_site,
                                   const CSequenceVariant::TSeqtype& seq_type,
                                   CScope& scope,
                                   TSeqPos& site_index);

    /// Attempt to translate a nucleotide site range in the intermediate 
    /// variant representation into a residue index on a Bioseq instance.
    static bool x_ComputeSiteIndex(const CSeq_id& seq_id,
                                   const CNtSiteRange& nt_range,
                                   const CSequenceVariant::TSeqtype& seq_type,
                                   CScope& scope,
                                   TSeqPos& site_index);

    static CRef<CInt_fuzz> x_CreateIntFuzz(const CSeq_id& seq_id,
                                           const CNtSiteRange& nt_range,
                                           const CSequenceVariant::TSeqtype& seq_type,
                                           CScope& scope);

    static const CSeq_feat& x_GetCDS(const CSeq_id& seq_id,
                                     CScope& scope);

    static ENa_strand x_GetStrand(const CNtInterval& nt_int);

    static ENa_strand x_GetStrand(const CNtSiteRange& nt_range);

    static ENa_strand x_GetStrand(const CNtSite& nt_site);
};



class CHgvsNaIrepReader : public CHgvsIrepReader
{
public:
        using TSet = CVariation_ref::C_Data::C_Set;
        using TDelta = CVariation_inst::TDelta::value_type;

        CHgvsNaIrepReader(CScope& scope) :
            CHgvsIrepReader(scope) {}

        CRef<CSeq_feat> CreateSeqfeat(const CVariantExpression& variant_expr) const;
private:
        /// Return a CSeq_feat for "simple" NA variants
        CRef<CSeq_feat> x_CreateSimpleVariantFeat(const string& var_name,
                                                  const string& identifier,
                                                  const CSequenceVariant::TSeqtype& seq_type,
                                                  const CSimpleVariant& simple_var) const;

        /// Return the top-level CVariation_ref object appearing in the variant Seq-feat
        CRef<CVariation_ref> x_CreateVarref(const string& var_name, 
                                            const CSimpleVariant& simple_var) const;

        /// Return a CVariation_ref describing a "no-change" variant
        CRef<CVariation_ref> x_CreateNaIdentityVarref(const CNaIdentity& identity,
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        /// Return a CVariation_ref describing a "no-change" variant
        CRef<CVariation_ref> x_CreateNaIdentityVarref(const CNtLocation& nt_loc,
                                                      const string& nucleotide,
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        /// Return a CVariation_ref describing a nucleotide substituion (SNP or MNP)
        CRef<CVariation_ref> x_CreateSubstVarref(const CNaSub& sub,
                                                 const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        /// Return a CVariation_ref describing a nucleotide substituion (SNP or MNP)
        CRef<CVariation_ref> x_CreateSubstVarref(const CNtLocation& nt_loc,
                                                 const string& initial_nt,
                                                 const string& final_nt,
                                                 const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDuplicationVarref(const CDuplication& dup,
                                                       const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateInsertionVarref(const CInsertion& ins,
                                                     const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDeletionVarref(const CDeletion& del,
                                                    const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDelinsVarref(const CDelins& delins,
                                                  const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDelinsVarref(const CNtLocation& nt_loc, 
                                                  const string& initial_nt,
                                                  const string& final_nt,
                                                  const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateInversionVarref(const CInversion& inv,
                                                     const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateConversionVarref(const CConversion& conv, 
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateSSRVarref(const CRepeat& ssr, 
                                               const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        /// Return a CSeq_literal encapsulating a known nucleotide sequence
        CRef<CSeq_literal> x_CreateNtSeqLiteral(const string& raw_seq) const;

        /// Return a CSeq_literal corresponding to a nucleotide sequence of unknown content, but known length
        CRef<CSeq_literal> x_CreateNtSeqLiteral(TSeqPos length) const;

        bool x_LooksLikePolymorphism(const CDelins& delins) const;

        CIUPACna x_CreateIUPACna(const string& na_seq) const;

}; // CHgvsNaIrepReader

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _NA_IREP_TO_SEQFEAT_HPP_
