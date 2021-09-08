#ifndef CLEANUP___CLEANUP__HPP
#define CLEANUP___CLEANUP__HPP

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
 * Author:  Robert Smith, Michael Kornbluh
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *   .......
 *
 */
#include <objmgr/scope.hpp>
#include <objtools/cleanup/cleanup_change.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/seqfeat/Cdregion.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeq_submit;
class COrgName;
class CSubmit_block;
class CAuthor;
class CAuth_list;
class CName_std;

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

class CCleanupChange;
class IObjtoolsListener;

class NCBI_CLEANUP_EXPORT CCleanup : public CObject
{
public:

    enum EValidOptions {
        eClean_NoReporting       = 0x1,
        eClean_GpipeMode         = 0x2,
        eClean_NoNcbiUserObjects = 0x4,
        eClean_SyncGenCodes      = 0x8,
        eClean_NoProteinTitles   = 0x10,
        eClean_KeepTopSet        = 0x20,
        eClean_KeepSingleSeqSet  = 0x40,
    };

    enum EScopeOptions {
        eScope_Copy,
        eScope_UseInPlace
    };

    // Construtor / Destructor
    CCleanup(CScope* scope = NULL, EScopeOptions scope_handling = eScope_Copy);
    ~CCleanup();

    void SetScope(CScope* scope);

    // BASIC CLEANUP

    CConstRef<CCleanupChange> BasicCleanup(CSeq_entry& se,  Uint4 options = 0);
    /// Cleanup a Seq-submit.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_submit& ss,  Uint4 options = 0);
    /// Cleanup a Bioseq.
    CConstRef<CCleanupChange> BasicCleanup(CBioseq& bs,     Uint4 ooptions = 0);
    /// Cleanup a Bioseq_set.
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_set& bss, Uint4 options = 0);
    /// Cleanup a Seq-Annot.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_annot& sa,  Uint4 options = 0);
    /// Cleanup a Seq-feat.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_feat& sf,   Uint4 options = 0);
    /// Cleanup a BioSource.
    CConstRef<CCleanupChange> BasicCleanup(CBioSource& src,   Uint4 options = 0);
    // Cleanup a Submit-block
    CConstRef<CCleanupChange> BasicCleanup(CSubmit_block& block, Uint4 options = 0);
    // Cleanup descriptors
    CConstRef<CCleanupChange> BasicCleanup(CSeqdesc& desc, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CSeq_descr & desc, Uint4 options = 0);

    // Handle versions.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_entry_Handle& seh, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_Handle& bsh,    Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_set_Handle& bssh, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CSeq_annot_Handle& sak, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CSeq_feat_Handle& sfh,  Uint4 options = 0);

    // Extended Cleanup
        /// Cleanup a Seq-entry.
    CConstRef<CCleanupChange> ExtendedCleanup(CSeq_entry& se,  Uint4 options = 0);
    /// Cleanup a Seq-submit.
    CConstRef<CCleanupChange> ExtendedCleanup(CSeq_submit& ss, Uint4 options = 0);
    /// Cleanup a Seq-Annot.
    CConstRef<CCleanupChange> ExtendedCleanup(CSeq_annot& sa,  Uint4 options = 0);

    // Handle versions
    static CConstRef<CCleanupChange> ExtendedCleanup(CSeq_entry_Handle& seh, Uint4 options = 0);

    // Useful cleanup functions

    static bool ShouldStripPubSerial(const CBioseq& bs);


/// Moves protein-specific features from nucleotide sequences in the Seq-entry to
/// the appropriate protein sequence.
/// @param seh Seq-entry Handle to edit [in]
/// @return Boolean return value indicates whether any changes were made
    static bool MoveProteinSpecificFeats(CSeq_entry_Handle seh);

    /// Moves one feature from nucleotide bioseq to
    /// the appropriate protein sequence.
    /// @param fh Feature to edit
    /// @return Boolean return value indicates whether any changes were made
    static bool MoveFeatToProtein(CSeq_feat_Handle fh);

/// Calculates whether a Gene-xref is unnecessary (because it refers to the
/// same gene as would be calculated using overlap)
/// @param sf Seq-feat with the xref [in]
/// @param scope Scope in which to search for location [in]
/// @param gene_xref Gene-ref of gene-xref [in]
/// @return Boolean return value indicates whether gene-xref is unnecessary
    static bool IsGeneXrefUnnecessary(const CSeq_feat& sf, CScope& scope, const CGene_ref& gene_xref);

/// Removes unnecessary Gene-xrefs
/// @param f Seq-feat to edit [in]
/// @param scope Scope in which to search for locations [in]
/// @return Boolean return value indicates whether gene-xrefs were removed
    static bool RemoveUnnecessaryGeneXrefs(CSeq_feat& f, CScope& scope);

/// Removes unnecessary Gene-xrefs on features in Seq-entry
/// @param seh Seq-entry-Handle to edit [in]
/// @return Boolean return value indicates whether gene-xrefs were removed
    static bool RemoveUnnecessaryGeneXrefs(CSeq_entry_Handle seh);

/// Removes non-suppressing Gene-xrefs
/// @param f Seq-feat to edit [in]
/// @return Boolean return value indicates whether gene-xrefs were removed
    static bool RemoveNonsuppressingGeneXrefs(CSeq_feat& f);


/// Repairs non-reciprocal xref pairs for specified feature if xrefs between
/// subtypes are permitted and feature with missing xref does not have an
/// xref to a different feature of the same subtype
/// @param f Seq-feat to edit [in]
/// @param tse top-level Seq-entry in which to search for the other half of the xref pair
/// @return Boolean return value indicates whether xrefs were created
    static bool RepairXrefs(const CSeq_feat& f, const CTSE_Handle& tse);

/// Repairs non-reciprocal xref pairs for specified feature pair if xrefs between
/// subtypes are permitted and feature with missing xref does not have an
/// xref to a different feature of the same subtype
/// @param f Seq-feat to edit [in]
/// @param tse top-level Seq-entry in which to search for the other half of the xref pair
/// @return Boolean return value indicates whether xrefs were created
    static bool RepairXrefs(const CSeq_feat& src, CSeq_feat_Handle& dst, const CTSE_Handle& tse);

/// Repairs non-reciprocal xref pairs in specified seq-entry
/// @param seh Seq-entry to edit [in]
/// @return Boolean return value indicates whether xrefs were created
    static bool RepairXrefs(CSeq_entry_Handle seh);

/// Detects gene features with matching locus
/// @param f Seq-feat parent feature of gene_xref [in]
/// @param gene_xref Gene-ref of gene-xref [in]
/// @param bsh CBioseq_Handle parent bioseq in which to search for genes [in]
/// @return Boolean return value indicates whether a gene feature with matching locus has been found
    static bool FindMatchingLocusGene(CSeq_feat& f, const CGene_ref& gene_xref, CBioseq_Handle bsh);

/// Removes orphaned locus Gene-xrefs
/// @param f Seq-feat to edit [in]
/// @param bsh CBioseq_Handle in which to search for gene features [in]
/// @return Boolean return value indicates whether gene-xrefs were removed
    static bool RemoveOrphanLocusGeneXrefs(CSeq_feat& f, CBioseq_Handle bsh);

/// Detects gene features with matching locus_tag
/// @param f Seq-feat parent feature of gene_xref [in]
/// @param gene_xref Gene-ref of gene-xref [in]
/// @param bsh CBioseq_Handle parent bioseq in which to search for genes [in]
/// @return Boolean return value indicates whether a gene feature with matching locus_tag has been found
    static bool FindMatchingLocus_tagGene(CSeq_feat& f, const CGene_ref& gene_xref, CBioseq_Handle bsh);

/// Removes orphaned locus_tag Gene-xrefs
/// @param f Seq-feat to edit [in]
/// @param bsh CBioseq_Handle in which to search for gene features [in]
/// @return Boolean return value indicates whether gene-xrefs were removed
    static bool RemoveOrphanLocus_tagGeneXrefs(CSeq_feat& f, CBioseq_Handle bsh);

/// Extends a location to the specificed position.
/// @param loc Seq-loc to extend
/// @param pos position of new end of location
/// @param scope Scope in which to look for sequences
/// @return Boolean return value indicates whether the location was extended
    static bool SeqLocExtend(CSeq_loc& loc, size_t pos, CScope& scope);


/// Extends a coding region up to 50 nt. if the coding region:
/// 1. does not end with a stop codon
/// 2. is adjacent to a stop codon
/// 3. is not pseudo
/// @param f Seq-feat to edit
/// @param bsh CBioseq_Handle on which the feature is located
/// @return Boolean return value indicates whether the feature was extended
    static bool ExtendToStopIfShortAndNotPartial(CSeq_feat& f, CBioseq_Handle bsh, bool check_for_stop = true);

/// Checks whether it is possible to extend the original location up to improved one. It is possible only if
/// the original location is less than improved
/// @param orig Seq-loc to check
/// @param improved Seq-loc original location may be extended to
/// @return Boolean return value indicates whether the extention is possible
    static bool LocationMayBeExtendedToMatch(const CSeq_loc& orig, const CSeq_loc& improved);

/// Extends a feature up to limit nt to a stop codon, or to the end of the sequence
/// if limit == 0 (partial will be set if location extends to end of sequence but
/// no stop codon is found)
/// @param f Seq-feat to edit
/// @param bsh CBioseq_Handle on which the feature is located
/// @param limit maximum number of nt to extend, or 0 if unlimited
/// @return Boolean return value indicates whether the feature was extended
    static bool ExtendToStopCodon(CSeq_feat& f, CBioseq_Handle bsh, size_t limit);
    static bool ExtendStopPosition(CSeq_feat& f, const CSeq_feat* cdregion, size_t extension = 0);

/// Translates coding region and selects best frame (without stops, or longest)
/// @param cds Coding region Seq-feat to edit
/// @param scope Scope in which to find coding region
/// @return Boolean return value indicates whether the coding region was changed
    static bool SetBestFrame(CSeq_feat& cds, CScope& scope);

/// Chooses best frame based on location
/// 1.  If the location is 5' complete, then the frame must be one.
/// 2.  If the location is 5' partial and 3' complete, select a frame using the
///      value of the location length modulo 3.
/// @param cdregion Coding Region in which to set frame
/// @param loc      Location to use for setting frame
/// @param scope    Scope in which to find location sequence(s)
/// @return Boolean return value indicates whether the frame was changed
    static bool SetFrameFromLoc(CCdregion &cdregion, const CSeq_loc& loc, CScope& scope);
    static bool SetFrameFromLoc(CCdregion::EFrame &frame, const CSeq_loc& loc, CScope& scope);

/// 1. Set the partial flags when the CDS is partial and codon_start is 2 or 3
/// 2. Make the CDS partial at the 5' end if there is no start codon
/// 3. Make the CDS partial at the 3' end if there is no stop codon
/// @param cds Coding region Seq-feat to edit
/// @param scope Scope in which to find coding region and coding region's protein
///        product sequence
/// @return Boolean return value indicates whether the coding region changed
    static bool SetCDSPartialsByFrameAndTranslation(CSeq_feat& cds, CScope& scope);


/// Clear internal partials
    static bool ClearInternalPartials(CSeq_loc& loc, bool is_first = true, bool is_last = true);
    static bool ClearInternalPartials(CSeq_loc_mix& mix, bool is_first = true, bool is_last = true);
    static bool ClearInternalPartials(CPacked_seqint& pint, bool is_first = true, bool is_last = true);
    static bool ClearInternalPartials(CSeq_entry_Handle seh);

/// Set feature partial based on feature location
    static bool SetFeaturePartial(CSeq_feat& f);

/// Update EC numbers
/// @param ec_num_list Prot-ref ec number list to clean
/// @return Boolean value indicates whether any changes were made
    static bool UpdateECNumbers(CProt_ref::TEc & ec_num_list);

/// Delete EC numbers
/// @param ec_num_list Prot-ref ec number list to clean
/// @return Boolean value indicates whether any changes were made
    static bool RemoveBadECNumbers(CProt_ref::TEc & ec_num_list);

/// Fix EC numbers
/// @param entry Seq-entry-handle to clean
/// @return Boolean value indicates whether any changes were made
    static bool FixECNumbers(CSeq_entry_Handle entry);

/// Set partialness of gene to match longest feature contained in gene
/// @param gene  Seq-feat to edit
/// @param scope Scope in which to find gene
/// @return Boolean return value indicates whether the gene changed
    static bool SetGenePartialByLongestContainedFeature(CSeq_feat& gene, CScope& scope);

    static void SetProteinName(CProt_ref& prot, const string& protein_name, bool append);
    static void SetProteinName(CSeq_feat& cds, const string& protein_name, bool append, CScope& scope);
    static void SetMrnaName(CSeq_feat& mrna, const string& protein_name);
    static const string& GetProteinName(const CProt_ref& prot);
    static const string& GetProteinName(const CSeq_feat& cds, CScope& scope);

/// Sets MolInfo::tech for a sequence
/// @param seq Bioseq to edit
/// @param tech tech value to set
/// @return Boolean tech was changed
    static bool SetMolinfoTech(CBioseq_Handle seq, CMolInfo::ETech tech);

/// Sets MolInfo::biomol for a sequence
/// @param seq Bioseq to edit
/// @param biomol biomol value to set
/// @return Boolean biomol was changed
    static bool SetMolinfoBiomol(CBioseq_Handle seq, CMolInfo::EBiomol biomol);


/// Adds missing MolInfo descriptor to sequence
/// @param seq Bioseq to edit
/// @return Boolean return value indicates whether descriptor was added
    static bool AddMissingMolInfo(CBioseq& seq, bool is_product);

/// Creates missing protein title descriptor
/// @param seq Bioseq to edit
/// @return Boolean return value indicates whether title was added
    static bool AddProteinTitle(CBioseq_Handle bsh);

/// Removes NcbiCleanup User Objects in the Seq-entry
/// @param seq_entry Seq-entry to edit
/// @return Boolean return value indicates whether object was removed
    static bool RemoveNcbiCleanupObject(CSeq_entry &seq_entry);

/// Looks up Org-refs in the Seq-entry
/// @param seh Seq-entry to edit
/// @return Boolean return value indicates whether object was updated
    static bool TaxonomyLookup(CSeq_entry_Handle seh);


/// Sets genetic codes for coding regions on Bioseq-Handle
/// @param Bioseq-Handle to examine
/// @return Boolean indicates whether any coding regions were updated
    static bool SetGeneticCodes(CBioseq_Handle bsh);

/// Adjusts protein title to reflect partialness
/// @param Bioseq to adjust
/// @return Boolean indicates whether title was updated
    static bool AddPartialToProteinTitle(CBioseq &bioseq);

/// Removes protein product from pseudo coding region
/// @param cds Seq-feat to adjust
/// @param scope Scope in which to find protein sequence and remove it
/// @return Boolean indicates whether anything changed
    static bool RemovePseudoProduct(CSeq_feat& cds, CScope& scope);

    static CRef<CSeq_entry> AddProtein(const CSeq_feat& cds, CScope& scope);

/// Expands gene to include features it cross-references
/// @param gene Seq-feat to adjust
/// @param tse Top-level Seq-entry in which to find other features
/// @return Boolean indicates whether anything changed
    static bool ExpandGeneToIncludeChildren(CSeq_feat& gene, CTSE_Handle& tse);

/// Performs WGS specific cleanup
/// @param entry Seq-entry to edit
/// @return Boolean return value indicates whether object was updated
    static bool WGSCleanup(CSeq_entry_Handle entry, bool instantiate_missing_proteins = true, Uint4 options = 0,
        bool run_extended_cleanup = true);

/// For table2asn -c s
/// Adds an exception of "low-quality sequence region" to coding regions
/// and mRNAs that are not pseudo and have an intron <11bp in length
/// @param entry Seq-entry to edit
/// @return Boolean return value indicates whether object was updated
    static bool AddLowQualityException(CSeq_entry_Handle entry);

/// Normalize Descriptor Order on a specific Seq-entry
/// @param entry Seq-entry to edit
/// @return Boolean return value indicates whether object was updated
    static bool NormalizeDescriptorOrder(CSeq_descr& descr);

/// Normalize Descriptor Order on a specific Seq-entry
/// @param seh Seq-entry-Handle to edit
/// @return Boolean return value indicates whether object was updated
    static bool NormalizeDescriptorOrder(CSeq_entry_Handle seh);

/// Remove all titles in Seqdescr except the last, because it is the
/// only one that would be displayed in the flatfile
/// @param seq Bioseq-Handle to edit
/// @return Boolean return value indicates whether any titles were removed
    static bool RemoveUnseenTitles(CSeq_entry_EditHandle::TSeq seq);

/// Remove all titles in Seqdescr except the last, because it is the
/// only one that would be displayed in the flatfile
/// @param set Bioseq-set-Handle to edit
/// @return Boolean return value indicates whether any titles were removed
    static bool RemoveUnseenTitles(CSeq_entry_EditHandle::TSet set);

/// Add GenBank Wrapper Set
/// @param entry Seq-entry to edit
/// @return Boolean return value indicates whether object changed
    static bool AddGenBankWrapper(CSeq_entry_Handle seh);


/// For Publication Citations
/// Get labels for a pubdesc. To be used in citations.
    static void GetPubdescLabels
        (const CPubdesc& pd,
        vector<TEntrezId>& pmids, vector<TEntrezId>& muids, vector<int>& serials,
        vector<string>& published_labels, vector<string>& unpublished_labels);

/// Get list of pubs that can be used for citations for Seq-feat on a Bioseq-handle
/// @param bsh Bioseq-handle to search
/// @return vector<CConstRef<CPub> > ordered list of pubs
/// Note that Seq-feat.cit appear in the flatfile using the position
/// in the list
    static vector<CConstRef<CPub> > GetCitationList(CBioseq_Handle bsh);

/// Remove duplicate publications
    static bool RemoveDuplicatePubs(CSeq_descr& descr);

    /// Some pubs should not be promoted to nuc-prot set from sequence
    static bool OkToPromoteNpPub(const CPubdesc& pd);

    /// For some sequences, pubs should not be promoted to nuc-prot set from sequence
    static bool OkToPromoteNpPub(const CBioseq& b);

    static bool PubAlreadyInSet(const CPubdesc& pd, const CSeq_descr& descr);

/// Convert full-length publication features to publication descriptors.
/// @param seh Seq-entry to edit
/// @return bool indicates whether any changes were made
    static bool ConvertPubFeatsToPubDescs(CSeq_entry_Handle seh);

/// Rescue pubs from Site-ref features
/// @param seh Seq-entry to edit
/// @return bool indicates whether any changes were made
    static bool RescueSiteRefPubs(CSeq_entry_Handle seh);

/// Is this a "minimal" pub? (If yes, do not rescue from a Seq-feat.cit)
    static bool IsMinPub(const CPubdesc& pd, bool is_refseq_prot);

    //helper function for moving feature to pubdesc descriptor
    static void MoveOneFeatToPubdesc(CSeq_feat_Handle feat, CRef<CSeqdesc> d, CBioseq_Handle b, bool remove_feat = true);

/// Remove duplicate biosource descriptors
    static bool RemoveDupBioSource(CSeq_descr& descr);

/// Get BioSource from feature to use for source descriptor
    static CRef<CBioSource> BioSrcFromFeat(const CSeq_feat& f);

    static bool AreBioSourcesMergeable(const CBioSource& src1, const CBioSource& src2);
    static bool MergeDupBioSources(CSeq_descr& descr);
    static bool MergeDupBioSources(CBioSource& src1, const CBioSource& add);


/// Convert full-length source features to source descriptors
/// @param seh Seq-entry to edit
/// @return bool indicates whether any changes were made
    static bool ConvertSrcFeatsToSrcDescs(CSeq_entry_Handle seh);

/// Examine all genes and gene xrefs in the Seq-entry.
/// If no genes have locus and some have locus tag AND no gene xrefs have locus-tag
/// and some gene xrefs have locus, change all gene xrefs to use locus tag.
/// If no genes have locus tag and some have locus AND no gene xrefs have locus
/// and some gene xrefs have locus tag, change all gene xrefs to use locus.
/// @param seh Seq-entry to edit
/// @return bool indicates whether any changes were made
    static bool FixGeneXrefSkew(CSeq_entry_Handle seh);

/// Convert nuc-prot sets with just one sequence to just the sequence
/// can't be done during the explore phase because it changes a seq to a set
/// @param seh Seq-entry to edit
/// @return bool indicates whether any changes were made
    static bool RenormalizeNucProtSets(CSeq_entry_Handle seh);

/// decodes various tags, including carriage-return-line-feed constructs
    static bool DecodeXMLMarkChanged(std::string & str);

    static CRef<CSeq_loc> GetProteinLocationFromNucleotideLocation(const CSeq_loc& nuc_loc, CScope& scope);
    static CRef<CSeq_loc> GetProteinLocationFromNucleotideLocation(const CSeq_loc& nuc_loc, const CSeq_feat& cds, CScope& scope, bool require_inframe = false);

/// Find proteins that are not packaged in the same nuc-prot set as the
/// coding region for which they are a product, and move them to that
/// nuc-prot set. Ignore coding regions that are in gen-prod-sets.
/// @param seh Seq-entry to edit
/// @return bool indicates whether any changes were made
    static bool RepackageProteins(CSeq_entry_Handle seh);
    static bool RepackageProteins(const CSeq_feat& cds, CBioseq_set_Handle np);

    static bool ConvertDeltaSeqToRaw(CSeq_entry_Handle seh, CSeq_inst::EMol filter = CSeq_inst::eMol_not_set);

/// Parse string into code break and add to coding region.
/// @param feat feature that contains coding region - necessary to determine codon boundaries
/// @param cds  coding region to which code breaks will be added
/// @param str  string from which to parse code break
/// @param scope scope in which to find sequences referenced (used for location comparisons)
/// @return bool indicates string was successfully parsed and code break was added
    static bool ParseCodeBreak(const CSeq_feat& feat,
            CCdregion& cds,
            const CTempString& str,
            CScope& scope,
            IObjtoolsListener* pMessageListener=nullptr);

/// Parses all valid transl_except Gb-quals into code-breaks for cdregion,
/// then removes the transl_except Gb-quals that were successfully parsed
/// @param feat feature that contains coding region
/// @param scope scope in which to find sequences referenced (used for location comparisons)
/// @return bool indicates changes were made
    static bool ParseCodeBreaks(CSeq_feat& feat, CScope& scope);

    static size_t MakeSmallGenomeSet(CSeq_entry_Handle entry);

/// From SQD-4329
/// For each sequence with a source that has an IRD db_xref, create a misc_feature
/// across the entire span and move the IRD db_xref from the source to the misc_feature.
/// Create a suppressing gene xref for the misc_feature.
/// @param entry Seq-entry on which to search for sources and create features
/// @return bool indicates changes were made
    static bool MakeIRDFeatsFromSourceXrefs(CSeq_entry_Handle entry);

/// From GB-7563
/// An action has been requested that will do the following:
///    1. This action should be limited to protein sequences where the product
///       is an exact match to a specified text (the usual string constraint
///       is not needed).
///    2. Protein sequences for which the coding region is 5' partial should
///       not be affected.
///    3. When the protein name matches, the following actions should be taken
///       if and only if the first amino acid of the protein sequence is not
///       M (methionine):
///     a. The first amino acid of the protein sequence should be changed to
///       methionine.
///     b. The coding region should have the text "RNA editing" added to
///       Seq-feat.except_text (separated from any existing text by a semicolon).
///       If Seq-feat.except is not already true, it should be set to true.
///     c. A code-break should be added to Cdregion.code-break where the
///       Code-break.loc is the location of the first codon of the coding region
///       and Code-break.aa is ncbieaa 'M' (Indexers will refer to "code-breaks"
///       as "translation exceptions" because these appear in the flatfile as a
///       /transl_except qualifier.
///
/// It will be the responsibility of the caller to only invoke this function
/// for coding regions where the product name is a match, and the protein sequence
/// does not already start with an M.

    static bool FixRNAEditingCodingRegion(CSeq_feat& cds);

    /// utility function for setting code break location given offset
    /// pos is the position of the amino acid where the translation exception
    /// occurs (starts with 1)
    static void SetCodeBreakLocation(CCode_break& cb, size_t pos, const CSeq_feat& cds);

    static bool IsMethionine(const CCode_break& cb);

    /// utility function for finding the code break for a given amino acid position
    /// pos is the position of the amino acid where the translation exception
    /// occurs (starts with 1)
    static CConstRef<CCode_break> GetCodeBreakForLocation(size_t pos, const CSeq_feat& cds);

    // From the request in GB-7166, we want to be able to move /gene
    // qualifiers that have been added to the coding region but not the
    // parent gene to the parent gene.
    // If the coding region also has /locus_tag qualifier which is different
    // from the one on the parent gene features, do not move the qualifier.
    // If there are two coding regions that are mapped to the same gene,
    // do not move the qualifier.
    static bool NormalizeGeneQuals(CSeq_feat& cds, CSeq_feat& gene);
    static bool NormalizeGeneQuals(CBioseq_Handle bsh);
    static bool NormalizeGeneQuals(CSeq_entry_Handle seh);
    typedef pair<CSeq_feat_Handle, CSeq_feat_Handle> TFeatGenePair; // by convention, cds first, gene second
    static vector<TFeatGenePair> GetNormalizableGeneQualPairs(CBioseq_Handle bsh);

    // This function is used to do generic string cleanup on User-object string fields
    // and apply specific cleanups to known types of User-object
    static bool CleanupUserObject(CUser_object& object);

    // for cleaning up authors, lists of authors, and affiliation
    static bool CleanupAuthor(CAuthor& author, bool fix_initials = true);
    static bool CleanupAuthList(CAuth_list& al, bool fix_initials = true);
    static void ResetAuthorNames(CAuth_list::TNames& names);
    static bool CleanupAffil(CAffil& af);
    static bool IsEmpty(const CAuth_list::TAffil& affil);

    // for cleaning up collection-date subsource qualifiers
    static bool CleanupCollectionDates(CSeq_entry_Handle seh, bool month_first);

    static void AutodefId(CSeq_entry_Handle seh);

private:
    // Prohibit copy constructor & assignment operator
    CCleanup(const CCleanup&);
    CCleanup& operator= (const CCleanup&);

    CRef<CScope>            m_Scope;

    static bool x_CleanupUserField(CUser_field& field);

    static bool x_MergeDupOrgNames(COrgName& on1, const COrgName& add);
    static bool x_MergeDupOrgRefs(COrg_ref& org1, const COrg_ref& add);

    static bool x_HasShortIntron(const CSeq_loc& loc, size_t min_len = 11);
    static bool x_AddLowQualityException(CSeq_feat& feat);
    static bool x_AddLowQualityException(CSeq_entry_Handle entry, CSeqFeatData::ESubtype subtype);

    static bool s_IsProductOnFeat(const CSeq_feat& cds);
    static void s_SetProductOnFeat(CSeq_feat& feat, const string& protein_name, bool append);

    static bool s_CleanupGeneOntology(CUser_object& obj);
    static bool s_CleanupStructuredComment(CUser_object& obj);
    static bool s_RemoveEmptyFields(CUser_object& obj);
    static bool s_CleanupGenomeAssembly(CUser_object& obj);
    static bool s_CleanupDBLink(CUser_object& obj);
    static bool s_AddNumToUserField(CUser_field &field);

    static bool s_CleanupNameStdBC(CName_std& name, bool fix_initials);
    static void s_ExtractSuffixFromInitials(CName_std& name);
    static void s_FixEtAl(CName_std& name);

    // for cleaning pubdesc
    static bool s_Flatten(CPub_equiv& pub_equiv);
};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* CLEANUP___CLEANUP__HPP */
