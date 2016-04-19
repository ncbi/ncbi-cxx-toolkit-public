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


class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

class CCleanupChange;

class NCBI_CLEANUP_EXPORT CCleanup : public CObject 
{
public:

    enum EValidOptions {
        eClean_NoReporting       = 0x1,
        eClean_GpipeMode         = 0x2,
        eClean_NoNcbiUserObjects = 0x4,
        eClean_SyncGenCodes      = 0x8,
        eClean_NoProteinTitles   = 0x10
    };

    // Construtor / Destructor
    CCleanup(CScope* scope = NULL);
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
    CConstRef<CCleanupChange> ExtendedCleanup(CSeq_entry_Handle& seh, Uint4 options = 0);

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

/// Extends a coding region 1-3 nt. if the coding region:
/// 1. is not partial
/// 2. does not end with a stop codon
/// 3. is adjacent to a stop codon
/// 4. is pseudo
/// @param f Seq-feat to edit
/// @param bsh CBioseq_Handle on which the feature is located
/// @return Boolean return value indicates whether the feature was extended
    static bool ExtendToStopIfShortAndNotPartial(CSeq_feat& f, CBioseq_Handle bsh, bool check_for_stop = true);


/// Extends a feature up to limit nt to a stop codon, or to the end of the sequence
/// if limit == 0 (partial will be set if location extends to end of sequence but
/// no stop codon is found)
/// @param f Seq-feat to edit
/// @param bsh CBioseq_Handle on which the feature is located
/// @param limit maximum number of nt to extend, or 0 if unlimited
/// @return Boolean return value indicates whether the feature was extended
    static bool ExtendToStopCodon(CSeq_feat& f, CBioseq_Handle bsh, size_t limit);

/// Translates coding region and selects best frame (without stops, or longest)
/// @param cds Coding region Seq-feat to edit
/// @param scope Scope in which to find coding region
/// @return Boolean return value indicates whether the coding region was changed
    static bool SetBestFrame(CSeq_feat& cds, CScope& scope);

/// Chooses best frame based on location
/// 1.	If the location is 5’ complete, then the frame must be one.
/// 2.	If the location is 5' partial and 3’ complete, select a frame using the
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

/// Set partialness of gene to match longest feature contained in gene
/// @param gene  Seq-feat to edit
/// @param scope Scope in which to find gene
/// @return Boolean return value indicates whether the gene changed
    static bool SetGenePartialByLongestContainedFeature(CSeq_feat& gene, CScope& scope);

    static void SetProteinName(CProt_ref& prot, const string& protein_name, bool append);
    static void SetProteinName(CSeq_feat& cds, const string& protein_name, bool append, CScope& scope);
    static const string& GetProteinName(const CProt_ref& prot);
    static const string& GetProteinName(const CSeq_feat& cds, CScope& scope);

/// Checks to see if a feature is pseudo. Looks for pseudo flag set on feature,
/// looks for pseudogene qualifier on feature, performs same checks for gene
/// associated with feature
/// @param feat Seq-feat to check
/// @param scope CScope to use when looking for associated gene
/// @return Boolean return value indicates whether any of the "pseudo" markers are found
    static bool IsPseudo(const CSeq_feat& feat, CScope& scope);

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

/// Performs WGS specific cleanup
/// @param entry Seq-entry to edit
/// @return Boolean return value indicates whether object was updated
    static bool WGSCleanup(CSeq_entry_Handle entry);

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
        vector<int>& pmids, vector<int>& muids, vector<int>& serials,
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

    static CConstRef <CSeq_feat> GetGeneForFeature(const CSeq_feat& feat, CScope& scope);

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

private:
    // Prohibit copy constructor & assignment operator
    CCleanup(const CCleanup&);
    CCleanup& operator= (const CCleanup&);

    CRef<CScope>            m_Scope;

    static bool x_MergeDupOrgNames(COrgName& on1, const COrgName& add);
    static bool x_MergeDupOrgRefs(COrg_ref& org1, const COrg_ref& add);

};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* CLEANUP___CLEANUP__HPP */
