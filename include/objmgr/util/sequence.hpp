#ifndef SEQUENCE__HPP
#define SEQUENCE__HPP

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
* Author:  Clifford Clausen & Aaron Ucko
*
* File Description:
*   Sequence utilities requiring CScope
*   Obtains or constructs a sequence's title.  (Corresponds to
*   CreateDefLine in the C toolkit.)
*/

#include <corelib/ncbistd.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/seq_id_mapper.hpp>
#include <util/strsearch.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/create_defline.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_id;
class CSeq_loc_mix;
class CSeq_point;
class CPacked_seqpnt;
class CBioseq_Handle;
class CSeq_loc_Mapper;
class CSeqVector;
class CCdregion;
class CSeq_feat;
class CSeq_entry;
class CSeq_entry_handle;
class CGenetic_code;
class CMolInfo;

BEGIN_SCOPE(sequence)

/** @addtogroup ObjUtilSequence
 *
 * @{
 */


/** @name SeqIdConv
 * Conversions between seq-id types
 * @{
 */

 
enum EAccessionVersion {
    eWithAccessionVersion,    ///< accession.version (when possible)
    eWithoutAccessionVersion  ///< accession only, even if version is available
};


/// Retrieve a particular seq-id from a given bioseq handle.  This uses
/// CSynonymsSet internally to decide which seq-id should be used.
enum EGetIdFlags {
    eGetId_ForceGi  = 0x0000,  ///< return only a gi-based seq-id
    eGetId_ForceAcc = 0x0001,  ///< return only an accession based seq-id
    eGetId_Best     = 0x0002,  ///< return the "best" gi (uses FindBestScore(),
                               ///< with CSeq_id::CalculateScore() as the score
                               ///< function
    eGetId_HandleDefault = 0x0003, ///< returns the ID associated with a bioseq-handle

    eGetId_Seq_id_Score       = 0x0004, ///< use CSeq_id::Score() as the scoring function
    eGetId_Seq_id_BestRank    = 0x0005, ///< use CSeq_id::BestRank() as the scoring function
    eGetId_Seq_id_WorstRank   = 0x0006, ///< use CSeq_id::WorstRank() as the scoring function
    eGetId_Seq_id_FastaAARank = 0x0007, ///< use CSeq_id::FastaAARank() as the scoring function
    eGetId_Seq_id_FastaNARank = 0x0008, ///< use CSeq_id::FastaNARank() as the scoring function

    ///< "canonical" here means "most specific"; this differs from "best" in
    ///< that "best" is intended for display purposes
    eGetId_Canonical = eGetId_Seq_id_BestRank,

    eGetId_TypeMask = 0x00FF,  ///< Mask for requested id type

    /// Check if the seq-id is present in the scope
    eGetId_VerifyId = 0x0100,

    /// Throw exception on errors. If not set, an empty value is returned.
    eGetId_ThrowOnError = 0x0200,

    eGetId_Default = eGetId_Best | eGetId_ThrowOnError,
};
typedef int EGetIdType;


/// Given an accession string retrieve the GI id.
/// If no GI was found returns 0 or throws CSeqIdFromHandleException
/// depending on the flags.
/// Id type in the flags is ignored, only VerifyId and ThrowOnError
/// flags are checked.
NCBI_XOBJUTIL_EXPORT
int GetGiForAccession(const string& acc,
                      CScope& scope,
                      EGetIdType flags = 0);

/// Retrieve the accession for a given GI.
/// If no accession was found returns an empty string or throws
/// CSeqIdFromHandleException depending on the flags.
/// Id type in the flags is ignored, only VerifyId and ThrowOnError
/// flags are checked.
NCBI_XOBJUTIL_EXPORT
string GetAccessionForGi(int           gi,
                         CScope&       scope,
                         EAccessionVersion use_version = eWithAccessionVersion,
                         EGetIdType flags = 0);

/// Given a Seq-id retrieve the corresponding GI.
/// If no GI was found returns 0 or throws CSeqIdFromHandleException
/// depending on the flags.
/// Id type in the flags is ignored, only VerifyId and ThrowOnError
/// flags are checked.
NCBI_XOBJUTIL_EXPORT
int GetGiForId(const objects::CSeq_id& id,
               CScope& scope,
               EGetIdType flags = 0);

/// Retrieve the accession string for a Seq-id.
/// If no accession was found returns an empty string or throws
/// CSeqIdFromHandleException depending on the flags.
/// Id type in the flags is ignored, only VerifyId and ThrowOnError
/// flags are checked.
NCBI_XOBJUTIL_EXPORT
string GetAccessionForId(const objects::CSeq_id& id,
                         CScope&       scope,
                         EAccessionVersion use_version = eWithAccessionVersion,
                         EGetIdType flags = 0);

/// Return a selected ID type for a given bioseq handle.  This function
/// will try to use the most efficient method possible to determine which
/// ID fulfills the requested parameter.  This version will call
/// sequence::GetId() with the bioseq handle's seq-id.
///
/// @param id Source id to evaluate
/// @param scope Scope for seq-id resolution.
/// @param type Type of ID to return
/// @return A requested seq-id.
/// Depending on the flags set in 'type' this function can verify
/// if the requested ID exists in the scope and throw
/// CSeqIdFromHandleException if the request cannot be satisfied.
NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle GetId(const CBioseq_Handle& handle,
                     EGetIdType type = eGetId_Default);

/// Return a selected ID type for a seq-id.  This function
/// will try to use the most efficient method possible to determine which
/// ID fulfills the requested parameter.  The following logic is used:
///
/// - For seq-id type eGetId_HandleDefault, the original seq-id is returned.
///   This satisfies the condition of returning a bioseq-handle's seq-id if
///   sequence::GetId() is applied to a CBioseq_Handle.
///
/// - For seq-id type eGetId_ForceAcc, the returned set of seq-ids will first
///   be evaluated for a "best" id (which, given seq-id scoring, will be
///   a textseq-id if one exists). If the returned best ID is a textseq-id,
///   this id will be returned.  Otherwise, an exception is thrown or an
///   empty handle returned.
///
/// - For seq-id type eGetId_ForceGi, the returned set of IDs is scanned for
///   an ID of type gi. If this is found, it is returned; otherwise, an
///   exception is thrown or an empty handle returned. If the supplied ID is
///   already a gi and eGetId_VerifyId flag is not set, no work is done.
///
/// @param id Source id to evaluate
/// @param scope Scope for seq-id resolution.
/// @param type Type of ID to return
/// @return A requested seq-id.
/// Depending on the flags set in 'type' this function can verify
/// if the requested ID exists in the scope and throw
/// CSeqIdFromHandleException if the request cannot be satisfied.
NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle GetId(const CSeq_id& id, CScope& scope,
                     EGetIdType type = eGetId_Default);

/// Return a selected ID type for a seq-id handle.
/// Arguments (except 'id') and behavior is the same as of
/// GetId(const CSeq_id& id, ...).
NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle GetId(const CSeq_id_Handle& id, CScope& scope,
                     EGetIdType type = eGetId_Default);

/* @} */


/** @name FindLatestSequence
 * Walk the replace history to find the latest revision of a sequence
 * @{
 */

/// Given a seq-id check its replace history and try to find the latest
/// revision. The function stops and returns NULL if it detects some
/// strange conditions like an infinite recursion. If the bioseq
/// contains no history information, the original id is returned.
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_id> FindLatestSequence(const CSeq_id& id, CScope& scope);

NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle FindLatestSequence(const CSeq_id_Handle& idh, CScope& scope);

/// Check replace history up to the specified date. Returns the latest
/// bioseq up to the date or the original id if the bioseq contains no
/// history or is already newer than the specified date.
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_id> FindLatestSequence(const CSeq_id& id,
                                      CScope&        scope,
                                      const CTime&   tlim);

NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle FindLatestSequence(const CSeq_id_Handle& idh,
                                  CScope&               scope,
                                  const CTime&          tlim);

/* @} */


/** @name GetTitle
 * Get sequence's title (used in various flat-file formats.)
 * @{
 */

/// This function is here rather than in CBioseq because it may need
/// to inspect other sequences.  The reconstruct flag indicates that it
/// should ignore any existing title Seqdesc.
enum EGetTitleFlags {
    fGetTitle_Reconstruct = 0x1, ///< ignore existing title Seqdesc.
    fGetTitle_Organism    = 0x2, ///< append [organism]
    fGetTitle_AllProteins = 0x4, ///< normally just names the first
    fGetTitle_NoExpensive = 0x8  ///< skip potential expensive operations
};
typedef int TGetTitleFlags;

NCBI_DEPRECATED NCBI_XOBJUTIL_EXPORT
string GetTitle(const CBioseq_Handle& hnd, TGetTitleFlags flags = 0);
NCBI_DEPRECATED NCBI_XOBJUTIL_EXPORT
bool GetTitle(const CBioseq& seq, string* title_ptr, TGetTitleFlags flags = 0);

/* @} */


/** @name Source and Product
 * Mapping locations through features
 * @{
 */

enum ES2PFlags {
    fS2P_NoMerge  = 0x1, ///< don't merge adjacent intervals on the product
    fS2P_AllowTer = 0x2  ///< map the termination codon as a legal location
};
typedef int TS2PFlags; // binary OR of ES2PFlags

NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> SourceToProduct(const CSeq_feat& feat,
                               const CSeq_loc& source_loc, TS2PFlags flags = 0,
                               CScope* scope = 0, int* frame = 0);

enum EP2SFlags {
    fP2S_Extend = 0x1  ///< if hitting ends, extend to include partial codons
};
typedef int TP2SFlags; // binary OR of ES2PFlags

NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> ProductToSource(const CSeq_feat& feat, const CSeq_loc& prod_loc,
                               TP2SFlags flags = 0, CScope* scope = 0);

/* @} */


/** @name Overlapping
 * Searching for features
 * @{
 */

enum EBestFeatOpts {
    /// requires explicit association, rather than analysis based on overlaps
    fBestFeat_StrictMatch = 0x01,

    /// don't perform any expensive tests, such as ones that require fetching
    /// additional sequences
    fBestFeat_NoExpensive = 0x02,

    /// favor longer features over shorter features
    fBestFeat_FavorLonger = 0x04,

    /// default options: do everything
    fBestFeat_Defaults = 0
};
typedef int TBestFeatOpts;


/// Storage for features and scores.
typedef pair<Int8, CConstRef<CSeq_feat> > TFeatScore;
typedef vector<TFeatScore> TFeatScores;

// To avoid putting custom logic into the GetOverlappingFeatures
// function, we allow plugins
class CGetOverlappingFeaturesPlugin {
public:
    virtual ~CGetOverlappingFeaturesPlugin() {}
    virtual void processSAnnotSelector( 
        SAnnotSelector &sel ) = 0;

    virtual void setUpFeatureIterator ( 
        CBioseq_Handle &bioseq_handle,
        auto_ptr<CFeat_CI> &feat_ci,
        TSeqPos circular_length ,
        CRange<TSeqPos> &range,
        const CSeq_loc& loc,
        SAnnotSelector &sel,
        CScope &scope,
        ENa_strand &strand ) = 0;

    virtual void processLoc( 
        CBioseq_Handle &bioseq_handle,
        CRef<CSeq_loc> &loc,
        TSeqPos circular_length ) = 0;

    virtual void processMainLoop( 
        bool &shouldContinueToNextIteration,
        CRef<CSeq_loc> &cleaned_loc_this_iteration,
        CRef<CSeq_loc> &candidate_feat_loc,
        EOverlapType &overlap_type_this_iteration,
        bool &revert_locations_this_iteration,
        CBioseq_Handle &bioseq_handle,
        const CMappedFeat &feat,
        TSeqPos circular_length,
        SAnnotSelector::EOverlapType annot_overlap_type ) = 0;

    virtual void postProcessDiffAmount( 
        Int8 &cur_diff, 
        CRef<CSeq_loc> &cleaned_loc_this_iteration, 
        CRef<CSeq_loc> &candidate_feat_loc, 
        CScope &scope, 
        SAnnotSelector &sel, 
        TSeqPos circular_length ) = 0;
};

/// Find all features overlapping the location. Features and corresponding
/// scores are stored in the 'feats' vector. The scores are calculated as
/// difference between the input location and each feature's location.
NCBI_XOBJUTIL_EXPORT
void GetOverlappingFeatures(const CSeq_loc& loc,
                            CSeqFeatData::E_Choice feat_type,
                            CSeqFeatData::ESubtype feat_subtype,
                            EOverlapType overlap_type,
                            TFeatScores& feats,
                            CScope& scope,
                            const TBestFeatOpts opts = 0,
                            CGetOverlappingFeaturesPlugin *plugin = NULL );


/// overlap_type defines how the location must be related to the feature.
/// For eOverlap_Subset, eOverlap_SubsetRev, eOverlap_CheckIntervals,
/// eOverlap_CheckIntRev and eOverlap_Interval the relationship is
/// reversed. E.g. with eOverlap_Contains, the location will contain
/// the feature, but with eOverlap_Subset the feature will be defined
/// on a subset of the location.
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::E_Choice feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope,
                                            TBestFeatOpts opts = fBestFeat_Defaults,
                                            CGetOverlappingFeaturesPlugin *plugin = NULL );
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::ESubtype feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope,
                                            TBestFeatOpts opts = fBestFeat_Defaults,
                                            CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestGeneForMrna(const CSeq_feat& mrna_feat,
                   CScope& scope,
                   TBestFeatOpts opts = fBestFeat_Defaults,
                   CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestGeneForCds(const CSeq_feat& cds_feat,
                  CScope& scope,
                  TBestFeatOpts opts = fBestFeat_Defaults,
                  CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestMrnaForCds(const CSeq_feat& cds_feat,
                  CScope& scope,
                  TBestFeatOpts opts = fBestFeat_Defaults,
                  CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestCdsForMrna(const CSeq_feat& mrna_feat,
                  CScope& scope,
                  TBestFeatOpts opts = fBestFeat_Defaults,
                  CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
void GetMrnasForGene(const CSeq_feat& gene_feat,
                     CScope& scope,
                     list< CConstRef<CSeq_feat> >& mrna_feats,
                     TBestFeatOpts opts = fBestFeat_Defaults,
                     CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
void GetCdssForGene(const CSeq_feat& gene_feat,
                    CScope& scope,
                    list< CConstRef<CSeq_feat> >& cds_feats,
                    TBestFeatOpts opts = fBestFeat_Defaults,
                    CGetOverlappingFeaturesPlugin *plugin = NULL );

/////////////////////////////////////////////////////////////////////////////
// Versions of functions with lookup by feature id
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestGeneForMrna(const CSeq_feat& mrna_feat,
                   const CTSE_Handle& tse,
                   TBestFeatOpts opts = fBestFeat_Defaults,
                   CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestGeneForCds(const CSeq_feat& cds_feat,
                  const CTSE_Handle& tse,
                  TBestFeatOpts opts = fBestFeat_Defaults,
                  CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestMrnaForCds(const CSeq_feat& cds_feat,
                  const CTSE_Handle& tse,
                  TBestFeatOpts opts = fBestFeat_Defaults,
                  CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestCdsForMrna(const CSeq_feat& mrna_feat,
                  const CTSE_Handle& tse,
                  TBestFeatOpts opts = fBestFeat_Defaults,
                  CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
void GetMrnasForGene(const CSeq_feat& gene_feat,
                     const CTSE_Handle& tse,
                     list< CConstRef<CSeq_feat> >& mrna_feats,
                     TBestFeatOpts opts = fBestFeat_Defaults,
                     CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
void GetCdssForGene(const CSeq_feat& gene_feat,
                    const CTSE_Handle& tse,
                    list< CConstRef<CSeq_feat> >& cds_feats,
                    TBestFeatOpts opts = fBestFeat_Defaults,
                    CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestOverlappingFeat(const CSeq_feat& feat,
                       CSeqFeatData::E_Choice feat_type,
                       sequence::EOverlapType overlap_type,
                       CScope& scope,
                       TBestFeatOpts opts = fBestFeat_Defaults,
                       CGetOverlappingFeaturesPlugin *plugin = NULL );

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestOverlappingFeat(const CSeq_feat& feat,
                       CSeqFeatData::ESubtype feat_type,
                       sequence::EOverlapType overlap_type,
                       CScope& scope,
                       TBestFeatOpts opts = fBestFeat_Defaults,
                       CGetOverlappingFeaturesPlugin *plugin = NULL );

/// Get the best overlapping feature for a SNP (variation) feature
/// @param snp_feat
///   SNP feature object
/// @param type
///   type of overlapping feature
/// @param scope
/// @param search_both_strands
///   search is performed on both strands, starting with the one specified
///   by the feature's location.
/// @return
///   the overlapping fetaure or NULL if not found
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestOverlapForSNP(const CSeq_feat& snp_feat,
                                          CSeqFeatData::E_Choice type,
                                          CScope& scope,
                                          bool search_both_strands = true);

/// Get the best overlapping feature for a SNP (variation)
/// @param snp_feat
///   SNP feature object
/// @param subtype
///   subtype of overlapping feature
/// @param scope
/// @param search_both_strands
///   search is performed on both strands, starting with the one specified
///   by the feature's location.
/// @return
///   the overlapping fetaure or NULL if not found
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestOverlapForSNP(const CSeq_feat& snp_feat,
                                          CSeqFeatData::ESubtype subtype,
                                          CScope& scope,
                                          bool search_both_strands = true);


/// Convenience functions for popular overlapping types
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetOverlappingGene(const CSeq_loc& loc, CScope& scope);


NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetOverlappingmRNA(const CSeq_loc& loc, CScope& scope);


NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetOverlappingCDS(const CSeq_loc& loc, CScope& scope);


NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetOverlappingPub(const CSeq_loc& loc, CScope& scope);


NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetOverlappingSource(const CSeq_loc& loc, CScope& scope);


NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetOverlappingOperon(const CSeq_loc& loc, CScope& scope);


/// Get the encoding CDS feature of a given protein sequence.
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetCDSForProduct(const CBioseq& product, CScope* scope);
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetCDSForProduct(const CBioseq_Handle& product);
NCBI_XOBJUTIL_EXPORT
CMappedFeat GetMappedCDSForProduct(const CBioseq_Handle& product);


/// Get the mature peptide feature of a protein
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetPROTForProduct(const CBioseq& product, CScope* scope);
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetPROTForProduct(const CBioseq_Handle& product);


/// Get the encoding mRNA feature of a given mRNA (cDNA) bioseq.
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetmRNAForProduct(const CBioseq& product, CScope* scope);
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetmRNAForProduct(const CBioseq_Handle& product);
NCBI_XOBJUTIL_EXPORT
CMappedFeat GetMappedmRNAForProduct(const CBioseq_Handle& product);

/* @} */


/** @name Sequences
 * Searching for bioseqs etc.
 * @{
 */

/// Get the encoding nucleotide sequnce of a protein.
NCBI_XOBJUTIL_EXPORT
const CBioseq* GetNucleotideParent(const CBioseq& product, CScope* scope);
NCBI_XOBJUTIL_EXPORT
CBioseq_Handle GetNucleotideParent(const CBioseq_Handle& product);

/// Get the parent bioseq for a part of a segmented bioseq
NCBI_XOBJUTIL_EXPORT
CBioseq_Handle GetParentForPart(const CBioseq_Handle& part);

/// Return the org-ref associated with a given sequence.  This will throw
/// a CException if there is no org-ref associated with the sequence
NCBI_XOBJUTIL_EXPORT
const COrg_ref& GetOrg_ref(const CBioseq_Handle& handle);

/// return the tax-id associated with a given sequence.  This will return 0
/// if no tax-id can be found.
NCBI_XOBJUTIL_EXPORT
int GetTaxId(const CBioseq_Handle& handle);

/// Retrieve the MolInfo object for a given bioseq handle.  If the supplied
/// sequence does not have a MolInfo associated with it, this will return NULL
NCBI_XOBJUTIL_EXPORT
const CMolInfo* GetMolInfo(const CBioseq_Handle& handle);

/// Retrieve the BioSource object for a given bioseq handle.  If the supplied
/// sequence does not have a MolInfo associated with it, this will return NULL
NCBI_XOBJUTIL_EXPORT
const CBioSource* GetBioSource(const CBioseq_Handle& handle);


/// Retrieve the Bioseq Handle from a location.
/// location refers to a single bioseq:
///   return the bioseq
/// location referes to multiple bioseqs:
///   if parts of a segmentd bioseq, returns the segmentd bioseq.
///   otherwise, return the first bioseq that could be found (first localy then,
///   if flag is eGetBioseq_All, remote)
NCBI_XOBJUTIL_EXPORT
CBioseq_Handle GetBioseqFromSeqLoc(const CSeq_loc& loc, CScope& scope,
    CScope::EGetBioseqFlag flag = CScope::eGetBioseq_Loaded);


/// Create a constructed bioseq from a location
/// This function will construct a bioseq from a given location, using a set of
/// options to control how the bioseq should be represented.  Options here
/// include:
///  - create a far-pointing delta-seq or a raw sequence
///      - specify the depth of recursion for the delta bioseq
///  - copy all bioseq descriptors
/// @param bsh
///   Source bioseq handle
/// @param from
///   Starting position of sequence
/// @param to
///   Ending position of sequence
/// @param new_seq_id
///   Seq-id for the newly created sequence
/// @param opts
///   The set of flags controlling how the sequence should be created
/// @param delta_seq_level
///   Level of recursion from which to pull the delta-seq components
///   Level 0 = use the components in the source sequence;
///   Level 1 = use the components from the components in the source sequence
/// @return
///   Newly created bioseq

enum ECreateBioseqFlags {
    //< Create a delta sequence.  If this is not present, a raw sequence
    //< is created
    fBioseq_CreateDelta = 0x01,

    //< Copy all descriptors into the new bioseq
    fBioseq_CopyDescriptors = 0x02,

    //< Project all annotations onto the new bioseq, and
    //< create a new annotation for these
    fBioseq_CopyAnnot = 0x04,

    fBioseq_Defaults = fBioseq_CreateDelta
};
typedef int TCreateBioseqFlags;

NCBI_XOBJUTIL_EXPORT
CRef<CBioseq> CreateBioseqFromBioseq(const CBioseq_Handle& bsh,
                                     TSeqPos from, TSeqPos to,
                                     const CSeq_id_Handle& new_seq_id,
                                     TCreateBioseqFlags opts = fBioseq_Defaults,
                                     int delta_seq_level = 1);


/* @} */


class NCBI_XOBJUTIL_EXPORT
CSeqIdFromHandleException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    // Enumerated list of document management errors
    enum EErrCode {
        eNoSynonyms,
        eRequestedIdNotFound
    };

    // Translate the specific error code into a string representations of
    // that error code.
    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CSeqIdFromHandleException, CException);
};


END_SCOPE(sequence)


/// FASTA-format output; see also ReadFasta in <objtools/readers/fasta.hpp>

class NCBI_XOBJUTIL_EXPORT CFastaOstream {
public:
    enum EFlags {
        fAssembleParts   = 0x001, ///< assemble FAR delta sequences; on by dflt
        fInstantiateGaps = 0x002, ///< honor specifed gap mode; on by default
        fSuppressRange   = 0x004, ///< never include location details in defline
        fReverseStrand   = 0x008, ///< flip the (implicit) location
        fKeepGTSigns     = 0x010, ///< don't convert '>' to '_' in title
        fMapMasksUp      = 0x020, ///< honor masks specified at a lower level
        fMapMasksDown    = 0x040, ///< honor masks specified at a higher level
        fNoExpensiveOps  = 0x080, ///< don't try too hard to find titles
        fShowModifiers   = 0x100, ///< show key-value pair modifiers (e.g. "[organism=Homo sapiens]")
        // historically misnamed as eFlagName
        eAssembleParts   = fAssembleParts,
        eInstantiateGaps = fInstantiateGaps
    };
    typedef int TFlags; ///< binary OR of EFlags

    /// How to represent gaps with fInstantiateGaps enabled, as it is
    /// by default.  (Disabling fInstantiateGaps is equivalent to
    /// requesting eGM_one_dash.)
    enum EGapMode {
        eGM_one_dash, ///< A single dash, followed by a line break.
        eGM_dashes,   ///< Multiple inline dashes.
        eGM_letters,  ///< Multiple inline Ns or Xs as appropriate (default).
        eGM_count     ///< >?N or >?unk100, as appropriate.
    };

    CFastaOstream(CNcbiOstream& out);
    virtual ~CFastaOstream();

    /// Unspecified locations designate complete sequences;
    /// non-empty custom titles override the usual title determination logic
    virtual void Write        (const CSeq_entry_Handle& handle,
                               const CSeq_loc* location = 0);
    virtual void Write        (const CBioseq_Handle& handle,
                               const CSeq_loc* location = 0,
                               const string& custom_title = kEmptyStr);
    virtual void WriteTitle   (const CBioseq_Handle& handle,
                               const CSeq_loc* location = 0,
                               const string& custom_title = kEmptyStr);
    virtual void WriteSequence(const CBioseq_Handle& handle,
                               const CSeq_loc* location = 0);

    /// These versions may set up a temporary object manager scope
    /// In the common case of a raw bioseq, no scope is needed
    void Write(const CSeq_entry& entry, const CSeq_loc* location = 0);
    void Write(const CBioseq&    seq,   const CSeq_loc* location = 0,
               bool no_scope = false,   const string& custom_title = kEmptyStr);
    void WriteTitle(const CBioseq& seq, const CSeq_loc* location = 0,
                    bool no_scope=false, const string& custom_title=kEmptyStr);

    /// Used only by Write(CSeq_entry[_Handle], ...); permissive by default
    virtual bool SkipBioseq(const CBioseq& /* seq */) { return false; }
    /// Delegates to the non-handle version by default for
    /// compatibility with older code; newer code should override this
    /// version.
    virtual bool SkipBioseq(const CBioseq_Handle& handle)
    { return SkipBioseq(*handle.GetCompleteBioseq()); }

    /// Which residues to mask out in subsequent output.
    /// These do NOT automatically reset between calls to Write;
    /// you must do so yourself by setting them to null.
    enum EMaskType {
        eSoftMask = 1, ///< write as lowercase rather than uppercase
        eHardMask = 2  ///< write as N for nucleotides, X for peptides
    };
    CConstRef<CSeq_loc> GetMask(EMaskType type) const;
    void                SetMask(EMaskType type, CConstRef<CSeq_loc> location);

    /// Other parameters...
    TSeqPos GetWidth   (void) const    { return m_Width;   }
    void    SetWidth   (TSeqPos width) { m_Width = width;  }
    TFlags  GetAllFlags(void) const    { return m_Flags;   }
    void    SetAllFlags(TFlags flags)  { m_Flags = flags;  }
    void    SetFlag    (EFlags flag)   { m_Flags |=  flag; }
    void    ResetFlag  (EFlags flag)   { m_Flags &= ~flag; }
    void    SetGapMode (EGapMode mode) { m_GapMode = mode; }
    EGapMode GetGapMode(void) const    { return m_GapMode; }

private:
    CNcbiOstream&       m_Out;
    auto_ptr<sequence::CDeflineGenerator> m_Gen;
    CConstRef<CSeq_loc> m_SoftMask;
    CConstRef<CSeq_loc> m_HardMask;
    TSeqPos             m_Width;
    TFlags              m_Flags;
    EGapMode            m_GapMode;
    TSeq_id_HandleSet   m_PreviousWholeIds;

    void x_WriteSeqIds    ( const CBioseq& bioseq,
                            const CSeq_loc* location);
    void x_WriteModifiers ( const CBioseq_Handle & handle );
    void x_WriteSeqTitle  ( const CBioseq& bioseq,
                            CScope* scope,
                            const string& custom_title);

    void x_PrintStringModIfNotDup(
        bool *seen, const CTempString & key, const CTempString & value );
    void x_PrintIntModIfNotDup(
        bool *seen, const CTempString & key, const int value );

    CConstRef<CSeq_loc> x_MapMask(CSeq_loc_Mapper& mapper, const CSeq_loc& mask,
                                  const CSeq_id* base_seq_id, CScope* scope);

    typedef map<TSeqPos, int> TMSMap;
    void x_GetMaskingStates(TMSMap& masking_states,
                            const CSeq_id* base_seq_id,
                            const CSeq_loc* location,
                            CScope* scope);

    void x_WriteSequence(const CSeqVector& vec,
                         const TMSMap& masking_state);
};


/// Public interface for coding region translation function
/// Uses CTrans_table in <objects/seqfeat/Genetic_code_table.hpp>
/// for rapid translation from a given genetic code, allowing all
/// of the iupac nucleotide ambiguity characters

class NCBI_XOBJUTIL_EXPORT CCdregion_translate
{
public:

    enum ETranslationLengthProblemOptions {
        eThrowException = 0,
        eTruncate,
        ePad
    };

    /// translation coding region into ncbieaa protein sequence
    NCBI_DEPRECATED
    static void TranslateCdregion (string& prot,
                                   const CBioseq_Handle& bsh,
                                   const CSeq_loc& loc,
                                   const CCdregion& cdr,
                                   bool include_stop = true,
                                   bool remove_trailing_X = false,
                                   bool* alt_start = 0,
                                   ETranslationLengthProblemOptions options = eThrowException);

    NCBI_DEPRECATED
    static void TranslateCdregion(string& prot,
                                  const CSeq_feat& cds,
                                  CScope& scope,
                                  bool include_stop = true,
                                  bool remove_trailing_X = false,
                                  bool* alt_start = 0,
                                  ETranslationLengthProblemOptions options = eThrowException);
};


class NCBI_XOBJUTIL_EXPORT CSeqTranslator
{
public:
    /// @sa TTranslationFlags
    enum ETranslationFlags {
        fDefault = 0,
        fNoStop = (1<<0), ///< = 0x1 Do not include stop in translation
        fRemoveTrailingX = (1<<1), ///< = 0x2 Remove trailing Xs from protein
        fIs5PrimePartial = (1<<2) ///< = 0x4 Translate first codon even if not start codon (because sequence is 5' partial)
    };

    typedef int TTranslationFlags;



    /// Translate a string using a specified genetic code
    /// @param seq
    ///   String containing IUPAC representation of sequence to be translated
    /// @param code
    ///   Genetic code to use for translation (NULL to use default)
    /// @param include_stop
    ///   If true, translate through stop codons and include trailing stop
    ///   (true by default)
    /// @param remove_trailing_X
    ///   If true, remove trailing Xs from protein translation (false by
    ///   default)
    /// @param alt_start
    ///   Pointer to bool to indicate whether an alternative start codon was
    ///   used
    /// @param is_5prime_complete
    ///   If true, only translate first codon if start codon, otherwise
    ///   translate as dash (-) to indicate problem with sequence

    NCBI_DEPRECATED static void Translate(const string& seq,
                          string& prot,
                          const CGenetic_code* code,
                          bool include_stop = true,
                          bool remove_trailing_X = false,
                          bool* alt_start = NULL,
                          bool is_5prime_complete = true);

    /// Translate a string using a specified genetic code
    /// @param seq
    ///   String containing IUPAC representation of sequence to be translated
    /// @param code
    ///   Genetic code to use for translation (NULL to use default)
    /// @param flags
    ///   Binary OR of "ETranslationFlags"
    /// @param alt_start
    ///   Pointer to bool to indicate whether an alternative start codon was
    ///   used
    static void Translate(const string& seq,
                          string& prot,
                          TTranslationFlags flags = fDefault,
                          const CGenetic_code* code = NULL,
                          bool* alt_start = NULL);

    /// Translate a seq-vector using a specified genetic code
    /// if the code is NULL, then the default genetic code is used
    /// @param seq
    ///   CSeqVector of sequence to be translated
    /// @param code
    ///   Genetic code to use for translation (NULL to use default)
    /// @param include_stop
    ///   If true, translate through stop codons and include trailing stop
    ///   (true by default)
    /// @param remove_trailing_X
    ///   If true, remove trailing Xs from protein translation (false by
    ///   default)
    /// @param alt_start
    ///   Pointer to bool to indicate whether an alternative start codon was
    ///   used
    /// @param is_5prime_complete
    ///   If true, only translate first codon if start codon, otherwise
    ///   translate as dash (-) to indicate problem with sequence
    NCBI_DEPRECATED static void Translate(const CSeqVector& seq,
                          string& prot,
                          const CGenetic_code* code,
                          bool include_stop = true,
                          bool remove_trailing_X = false,
                          bool* alt_start = NULL,
                          bool is_5prime_complete = true);

    /// Translate a seq-vector using a specified genetic code
    /// if the code is NULL, then the default genetic code is used
    /// @param seq
    ///   CSeqVector of sequence to be translated
    /// @param code
    ///   Genetic code to use for translation (NULL to use default)
    /// @param flags
    ///   Binary OR of "ETranslationFlags"
    /// @param alt_start
    ///   Pointer to bool to indicate whether an alternative start codon was
    ///   used
    static void Translate(const CSeqVector& seq,
                          string& prot,
                          TTranslationFlags flags = fDefault,
                          const CGenetic_code* code = NULL,
                          bool* alt_start = NULL);

    /// utility function: translate a given location on a sequence
    NCBI_DEPRECATED
    static void Translate(const CSeq_loc& loc,
                          const CBioseq_Handle& handle,
                          string& prot,
                          const CGenetic_code* code = NULL,
                          bool include_stop = true,
                          bool remove_trailing_X = false,
                          bool* alt_start = 0);

    /// utility function: translate a given location on a sequence
    static void Translate(const CSeq_loc& loc,
                          CScope& scope,
                          string& prot,
                          const CGenetic_code* code = NULL,
                          bool include_stop = true,
                          bool remove_trailing_X = false,
                          bool* alt_start = 0);

    /// Translate a CDRegion into a protein
    static void Translate(const CSeq_feat& cds,
                          CScope& scope,
                          string& prot,
                          bool include_stop = true,
                          bool remove_trailing_X = false,
                          bool* alt_start = 0);

    static CRef<CBioseq> TranslateToProtein(const CSeq_feat& cds,
                                              CScope& scope);

};



/// Location relative to a base Seq-loc: one (usually) or more ranges
/// of offsets.
/// XXX - handle fuzz?
struct NCBI_XOBJUTIL_EXPORT SRelLoc
{
    enum EFlags {
        fNoMerge = 0x1 ///< don't merge adjacent intervals
    };
    typedef int TFlags; ///< binary OR of EFlags

    /// For relative ranges (ONLY), id is irrelevant and normally unset.
    typedef CSeq_interval         TRange;
    typedef vector<CRef<TRange> > TRanges;

    /// Beware: treats locations corresponding to different sequences as
    /// disjoint, even if one is actually a segment of the other. :-/
    SRelLoc(const CSeq_loc& parent, const CSeq_loc& child, CScope* scope = 0,
            TFlags flags = 0);

    /// For manual work.  As noted above, ranges need not contain any IDs.
    SRelLoc(const CSeq_loc& parent, const TRanges& ranges)
        : m_ParentLoc(&parent), m_Ranges(ranges) { }

    CRef<CSeq_loc> Resolve(CScope* scope = 0, TFlags flags = 0) const
        { return Resolve(*m_ParentLoc, scope, flags); }
    CRef<CSeq_loc> Resolve(const CSeq_loc& new_parent, CScope* scope = 0,
                           TFlags flags = 0) const;

    CConstRef<CSeq_loc> m_ParentLoc;
    TRanges             m_Ranges;
};



///============================================================================//
///                             Sequence Search                                //
///============================================================================//

/// CSeqSearch
/// ==========
///
/// Search a nucleotide sequence for one or more patterns
///

class NCBI_XOBJUTIL_EXPORT CSeqSearch
{
public:
   
    /// Holds information associated with a pattern, such as the name of the
    /// restriction enzyme, location of cut site etc.
    class CPatternInfo
    {
    public:
        /// constructor
        CPatternInfo(const      string& name,
                     const      string& sequence,
                     Int2       cut_site) :
            m_Name(name), m_Sequence(sequence), m_CutSite(cut_site),
            m_Strand(eNa_strand_unknown)
        {}

        const string& GetName     (void) const { return m_Name;     }
        const string& GetSequence (void) const { return m_Sequence; }
        Int2          GetCutSite  (void) const { return m_CutSite;  }
        ENa_strand    GetStrand   (void) const { return m_Strand;   }

    private:
        friend class CSeqSearch;

        /// data
        string      m_Name;      /// user defined name
        string      m_Sequence;  /// nucleotide sequence
        Int2        m_CutSite;
        ENa_strand  m_Strand;        
    };
    typedef CPatternInfo    TPatternInfo;

    /// Client interface:
    /// ==================
    /// A class that uses the SeqSearch facility should implement the Client 
    /// interface and register itself with the search utility to be notified 
    /// of matches detection.
    class IClient
    {
    public:
        virtual ~IClient() { }

        typedef CSeqSearch::TPatternInfo    TPatternInfo;

        virtual bool OnPatternFound(const TPatternInfo& pat_info, TSeqPos pos) = 0;
    };

public:

    enum ESearchFlag {
        fNoFlags       = 0,
        fJustTopStrand = 1,
        fExpandPattern = 2,
        fAllowMismatch = 4
    };
    typedef unsigned int TSearchFlags; ///< binary OR of ESearchFlag

    /// constructors
    /// @param client
    ///   pointer to a client object (receives pattern match notifications)
    /// @param flags
    ///   specify search flags
    CSeqSearch(IClient *client = 0, TSearchFlags flags = fNoFlags);
    /// destructor
    ~CSeqSearch(void);
        
    /// Add nucleotide pattern or restriction site to sequence search.
    /// Uses ambiguity codes, e.g., R = A and G, H = A, C and T
    void AddNucleotidePattern(const string& name,       /// pattern's name
                              const string& sequence,   /// pattern's sequence
                              Int2          cut_site,
                              TSearchFlags  flags = fNoFlags);

    /// Search the sequence for patterns
    /// @sa
    ///   AddNucleotidePattern
    void Search(const CBioseq_Handle& seq);
    
    /// Low level search method.
    /// The user is responsible for feeding each character in turn,
    /// keep track of the position in the text and provide the length in case of
    /// a circular topoloy.
    int Search(int current_state, char ch, int position, int length = kMax_Int);

    /// Get / Set client.
    const IClient* GetClient() const { return m_Client; }
    void SetClient(IClient* client) { m_Client = client; }

private:

    void x_AddNucleotidePattern(const string& name, string& pattern,
        Int2 cut_site, ENa_strand strand, TSearchFlags flags);
 
    void x_ExpandPattern(string& sequence, string& buffer, size_t pos,
        TPatternInfo& pat_info, TSearchFlags flags);

    void x_AddPattern(TPatternInfo& pat_info, string& sequence, TSearchFlags flags);
    void x_StorePattern(TPatternInfo& pat_info, string& sequence);

    bool x_IsJustTopStrand(TSearchFlags flags) const {
        return ((m_Flags | flags) & fJustTopStrand) != 0;
    }
    bool x_IsExpandPattern(TSearchFlags flags) const {
        return ((m_Flags | flags) & fExpandPattern) != 0;
    }
    bool x_IsAllowMismatch(TSearchFlags flags) const {
        return ((m_Flags | flags) & fAllowMismatch) != 0;
    }

    // data
    IClient*                m_Client;          // pointer to client object
    TSearchFlags            m_Flags;           // search flags
    size_t                  m_LongestPattern;  // longets search pattern
    CTextFsm<TPatternInfo>  m_Fsa;             // finite state machine
};  //  end of CSeqSearch


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* SEQUENCE__HPP */
