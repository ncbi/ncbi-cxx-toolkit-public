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
#include <objects/seq/seq_id_handle.hpp>
#include <util/strsearch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_id;
class CSeq_loc_mix;
class CSeq_point;
class CPacked_seqpnt;
class CBioseq_Handle;
class CSeqVector;
class CCdregion;
class CSeq_feat;
class CSeq_entry;
class CSeq_entry_handle;
class CGenetic_code;

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

/// Given an accession string retrieve the GI id.
/// if no GI was found returns 0.
NCBI_XOBJUTIL_EXPORT
int GetGiForAccession(const string& acc, CScope& scope);

/// Retrieve the accession for a given GI.
/// if no accession was found returns and empty string.
NCBI_XOBJUTIL_EXPORT
string GetAccessionForGi(int           gi,
                         CScope&       scope,
                         EAccessionVersion use_version = eWithAccessionVersion);


/// Retrieve a particular seq-id from a given bioseq handle.  This uses
/// CSynonymsSet internally to decide which seq-id should be used.
enum EGetIdType {
    eGetId_ForceGi,         ///< return only a gi-based seq-id
    eGetId_ForceAcc,        ///< return only an accession based seq-id
    eGetId_Best,            ///< return the "best" gi (uses FindBestScore(),
                            ///< with CSeq_id::CalculateScore() as the score
                            ///< function
    eGetId_HandleDefault,   ///< returns the ID associated with a bioseq-handle

    eGetId_Default = eGetId_Best
};


/// Return a selected ID type for a given bioseq handle.  This function
/// will try to use the most efficient method possible to determine which
/// ID fulfills the requested parameter.  This version will call
/// sequence::GetId() with the bioseq handle's seq-id.
///
/// @param id Source id to evaluate
/// @param scope Scope for seq-id resolution.
/// @param type Type of ID to return
/// @return A requested seq-id.  This function will throw an exception of type
///  CSeqIdFromHandleException if the request cannot be satisfied.
NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle GetId(const CBioseq_Handle& handle,
                     EGetIdType type = eGetId_Default);

/// Return a selected ID type for a given bioseq handle.  This function
/// will try to use the most efficient method possible to determine which
/// ID fulfills the requested parameter.  The following logic is used:
///
/// - For seq-id type eGetId_HandleDefault, the original seq-id is returned.
///   This satisfies the condition of returning a bioseq-handle's seq-id if
///   sequence::GetId() is applied to a CBioseq_Handle.
///
/// - For seq-id type eGetId_ForceAcc, the returned set of seq-ids will first
///   be evaluated for a "best" id (which, given seq-id scoring, will be
///   a textseq-id if one exists).  If the returned best ID is a textseq-id,
///   this id will be returned.  Otherwise, an exception is thrown.
///
/// - For seq-id type eGetId_ForceGi, the returned set of IDs is scanned for
///   an ID of type gi.  If this is found, it is returned; otherwise, an
///   exception is thrown.  If the supplied ID is already a gi, no work is
///   done.
///
/// @param id Source id to evaluate
/// @param scope Scope for seq-id resolution.
/// @param type Type of ID to return
/// @return A requested seq-id.  This function will throw an exception of type
///  CSeqIdFromHandleException if the request cannot be satisfied.
NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle GetId(const CSeq_id& id, CScope& scope,
                     EGetIdType type = eGetId_Default);

NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle GetId(const CSeq_id_Handle& id, CScope& scope,
                     EGetIdType type = eGetId_Default);

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
    fGetTitle_AllProteins = 0x4  ///< normally just names the first
};
typedef int TGetTitleFlags;

NCBI_XOBJUTIL_EXPORT
string GetTitle(const CBioseq_Handle& hnd, TGetTitleFlags flags = 0);

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

    /// default options: do everything
    fBestFeat_Defaults = 0
};
typedef int TBestFeatOpts;

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::E_Choice feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope);
NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::ESubtype feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestGeneForMrna(const CSeq_feat& mrna_feat,
                   CScope& scope,
                   TBestFeatOpts opts = fBestFeat_Defaults);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestGeneForCds(const CSeq_feat& mrna_feat,
                  CScope& scope,
                  TBestFeatOpts opts = fBestFeat_Defaults);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestMrnaForCds(const CSeq_feat& cds_feat,
                  CScope& scope,
                  TBestFeatOpts opts = fBestFeat_Defaults);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestCdsForMrna(const CSeq_feat& mrna_feat,
                  CScope& scope,
                  TBestFeatOpts opts = fBestFeat_Defaults);

NCBI_XOBJUTIL_EXPORT
void GetMrnasForGene(const CSeq_feat& gene_feat,
                     CScope& scope,
                     list< CConstRef<CSeq_feat> >& mrna_feats,
                     TBestFeatOpts opts = fBestFeat_Defaults);

NCBI_XOBJUTIL_EXPORT
void GetCdssForGene(const CSeq_feat& gene_feat,
                    CScope& scope,
                    list< CConstRef<CSeq_feat> >& cds_feats,
                    TBestFeatOpts opts = fBestFeat_Defaults);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestOverlappingFeat(const CSeq_feat& feat,
                       CSeqFeatData::E_Choice feat_type,
                       sequence::EOverlapType overlap_type,
                       CScope& scope,
                       TBestFeatOpts opts = fBestFeat_Defaults);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat>
GetBestOverlappingFeat(const CSeq_feat& feat,
                       CSeqFeatData::ESubtype feat_type,
                       sequence::EOverlapType overlap_type,
                       CScope& scope,
                       TBestFeatOpts opts = fBestFeat_Defaults);

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

/* @} */


END_SCOPE(sequence)


/// FASTA-format output; see also ReadFasta in <objtools/readers/fasta.hpp>

class NCBI_XOBJUTIL_EXPORT CFastaOstream {
public:
    enum EFlags {
        eAssembleParts   = 0x1,
        eInstantiateGaps = 0x2
    };
    typedef int TFlags; ///< binary OR of EFlags

    CFastaOstream(CNcbiOstream& out) : m_Out(out), m_Width(70), m_Flags(0) { }
    virtual ~CFastaOstream() { }

    /// Unspecified locations designate complete sequences
    void Write        (const CSeq_entry_Handle& handle,
                       const CSeq_loc* location = 0);
    void Write        (const CBioseq_Handle& handle,
                       const CSeq_loc* location = 0);
    void WriteTitle   (const CBioseq_Handle& handle);
    void WriteSequence(const CBioseq_Handle& handle,
                       const CSeq_loc* location = 0);

    /// These versions set up a temporary object manager scope
    void Write(CSeq_entry& entry, const CSeq_loc* location = 0);
    void Write(CBioseq&    seq,   const CSeq_loc* location = 0);

    /// Used only by Write(CSeq_entry[_Handle], ...); permissive by default
    virtual bool SkipBioseq(const CBioseq& /* seq */) { return false; }

    /// To adjust various parameters...
    TSeqPos GetWidth   (void) const    { return m_Width;   }
    void    SetWidth   (TSeqPos width) { m_Width = width;  }
    TFlags  GetAllFlags(void) const    { return m_Flags;   }
    void    SetAllFlags(TFlags flags)  { m_Flags = flags;  }
    void    SetFlag    (EFlags flag)   { m_Flags |=  flag; }
    void    ResetFlag  (EFlags flag)   { m_Flags &= ~flag; }

private:
    CNcbiOstream& m_Out;
    TSeqPos       m_Width;
    TFlags        m_Flags;
};


/// Public interface for coding region translation function
/// Uses CTrans_table in <objects/seqfeat/Genetic_code_table.hpp>
/// for rapid translation from a given genetic code, allowing all
/// of the iupac nucleotide ambiguity characters

class NCBI_XOBJUTIL_EXPORT CCdregion_translate
{
public:

    /// translation coding region into ncbieaa protein sequence
    static void TranslateCdregion (string& prot,
                                   const CBioseq_Handle& bsh,
                                   const CSeq_loc& loc,
                                   const CCdregion& cdr,
                                   bool include_stop = true,
                                   bool remove_trailing_X = false,
                                   bool* alt_start = 0);

    static void TranslateCdregion(string& prot,
                                  const CSeq_feat& cds,
                                  CScope& scope,
                                  bool include_stop = true,
                                  bool remove_trailing_X = false,
                                  bool* alt_start = 0);

    /// return iupac sequence letters under feature location
    static void ReadSequenceByLocation (string& seq,
                                        const CBioseq_Handle& bsh,
                                        const CSeq_loc& loc);

};


class NCBI_XOBJUTIL_EXPORT CSeqTranslator
{
public:

    /// translate a string using a specified genetic code
    /// if the code is NULL, then the default genetic code is used
    static void Translate(const string& seq,
                          string& prot,
                          const CGenetic_code* code = NULL,
                          bool include_stop = true,
                          bool remove_trailing_X = false);

    /// translate a seq-vector using a specified genetic code
    /// if the code is NULL, then the default genetic code is used
    static void Translate(const CSeqVector& seq,
                          string& prot,
                          const CGenetic_code* code = NULL,
                          bool include_stop = true,
                          bool remove_trailing_X = false);

    /// utility function: translate a given location on a sequence
    static void Translate(const CSeq_loc& loc,
                          const CBioseq_Handle& handle,
                          string& prot,
                          const CGenetic_code* code = NULL,
                          bool include_stop = true,
                          bool remove_trailing_X = false);
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

/*
* ===========================================================================
* $Log$
* Revision 1.62  2005/05/09 18:45:08  ucko
* Ensure that widely-included classes with virtual methods have virtual dtors.
*
* Revision 1.61  2005/04/11 14:47:54  dicuccio
* Optimized feature overlap functions GetBestCdsForMrna(), GetBestMrnaForCds(),
* GetBestGenForMrna(), GetBestMrnaForGene(), GetBestGeneForCds(): rearranged
* order of tests; provide early check for only one possible feature and avoid
* complex checks if so
*
* Revision 1.60  2005/02/17 15:58:41  grichenk
* Changes sequence::GetId() to return CSeq_id_Handle
*
* Revision 1.59  2005/01/13 15:24:15  dicuccio
* Added optional flags to GetBestXxxForXxx() functions to control the types of
* checks performed
*
* Revision 1.58  2005/01/10 15:09:53  shomrat
* Changed GetAccessionForGi
*
* Revision 1.57  2005/01/06 21:04:00  ucko
* CFastaOstream: support CSeq_entry_Handle.
*
* Revision 1.56  2005/01/04 14:51:09  dicuccio
* Improved documentation for sequence::GetId(CBioseq_Handle, ...) and
* sequence::GetId(const CSeq_id&, ...)
*
* Revision 1.55  2004/12/09 18:09:14  shomrat
* Changes to CSeqSearch
*
* Revision 1.54  2004/12/06 15:04:50  shomrat
* Added GetParentForPart and GetBioseqFromSeqLoc
*
* Revision 1.53  2004/11/22 16:04:07  grichenk
* Fixed/added doxygen comments
*
* Revision 1.52  2004/11/19 15:09:33  shomrat
* Added GetBestOverlapForSNP
*
* Revision 1.51  2004/11/18 15:56:51  grichenk
* Added Doxigen comments, removed THROWS.
*
* Revision 1.50  2004/11/17 21:25:13  grichenk
* Moved seq-loc related functions to seq_loc_util.[hc]pp.
* Replaced CNotUnique and CNoLength exceptions with CObjmgrUtilException.
*
* Revision 1.49  2004/11/01 17:14:04  shomrat
* + GetGiForAccession and GetAccessionForGi
*
* Revision 1.48  2004/10/14 20:29:50  ucko
* Add a flag fGetTitle_AllProteins (off by default) that governs whether
* to include all protein names or just the first, per recent changes to
* the C Toolkit.
*
* Revision 1.47  2004/10/12 18:57:57  dicuccio
* Added variant of sequence::GetId() that takes a seq-id instead of a bioseq
* handle
*
* Revision 1.46  2004/10/12 13:57:21  dicuccio
* Added convenience routines for finding: best mRNA for CDS feature; best gene
* for mRNA; best gene for CDS; all mRNAs for a gene; all CDSs for a gene.  Added
* new variant of GetBestOverlappingFeat() that takes a feature and uses the
* convenience routines above.
*
* Revision 1.45  2004/10/01 15:33:46  grichenk
* TestForOverlap -- try to get bioseq's length for whole locations.
* Perform all calculations with Int8, check for overflow when
* returning int.
*
* Revision 1.44  2004/05/06 18:58:31  shomrat
* removed export specifier from templated function
*
* Revision 1.43  2004/05/06 18:34:11  ucko
* Reorder SeqLocMerge<> and SeqLocMergeOne to avoid compilation errors
* on (at least) GCC 3.4.
*
* Revision 1.42  2004/05/06 17:38:37  shomrat
* + Changed SeqLocMerge API
*
* Revision 1.41  2004/04/06 14:03:15  dicuccio
* Added API to extract the single Org-ref from a bioseq handle.  Added API to
* retrieve a single tax-id from a bioseq handle
*
* Revision 1.40  2004/03/25 20:02:30  vasilche
* Added several method variants with CBioseq_Handle as argument.
*
* Revision 1.39  2004/03/01 18:22:07  shomrat
* Added alternative start flag to TranslateCdregion
*
* Revision 1.38  2004/02/19 17:59:40  shomrat
* Added function to reverse a location
*
* Revision 1.37  2004/02/05 19:41:10  shomrat
* Convenience functions for popular overlapping types
*
* Revision 1.36  2004/01/30 17:22:52  dicuccio
* Added sequence::GetId(const CBioseq_Handle&) - returns a selected ID class
* (best, GI).  Added CSeqTranslator - utility class to translate raw sequence
* data
*
* Revision 1.35  2004/01/28 17:17:14  shomrat
* Added SeqLocMerge
*
* Revision 1.34  2003/12/16 19:37:13  shomrat
* Retrieve encoding feature and bioseq of a protein
*
* Revision 1.33  2003/10/15 19:51:13  ucko
* More adjustments to SRelLoc: rearrange so that the constructors appear
* next to each other, and support resolving against an alternate parent.
*
* Revision 1.32  2003/10/09 18:53:24  ucko
* SRelLoc: clarified and doxygen-ized comments
*
* Revision 1.31  2003/10/08 21:07:32  ucko
* CCdregion_translate: take const Bioseq_Handles, since there's no need
* to modify them.
* SRelLoc: add a raw constructor.
*
* Revision 1.30  2003/09/22 18:38:14  grichenk
* Fixed circular seq-locs processing by TestForOverlap()
*
* Revision 1.29  2003/08/21 16:09:44  ucko
* Correct path to header for FASTA reader
*
* Revision 1.28  2003/05/09 15:36:55  ucko
* Take const CBioseq_Handle references in CFastaOstream::Write et al.
*
* Revision 1.27  2003/02/13 14:35:23  grichenk
* + eOverlap_Contains
*
* Revision 1.26  2003/01/22 21:02:44  ucko
* Add a comment about LocationOffset's return value.
*
* Revision 1.25  2003/01/22 20:14:27  vasilche
* Removed compiler warning.
*
* Revision 1.24  2003/01/09 17:48:26  ucko
* Include Seq_interval.hpp rather than just forward-declaring
* CSeq_interval now that we use CRef<CSeq_interval> here.
*
* Revision 1.23  2003/01/08 20:43:05  ucko
* Adjust SRelLoc to use (ID-less) Seq-intervals for ranges, so that it
* will be possible to add support for fuzz and strandedness/orientation.
*
* Revision 1.22  2003/01/03 19:27:45  shomrat
* Added Win32 export specifier where missing
*
* Revision 1.21  2002/12/30 19:38:34  vasilche
* Optimized CGenbankWriter::WriteSequence.
* Implemented GetBestOverlappingFeat() with CSeqFeatData::ESubtype selector.
*
* Revision 1.20  2002/12/27 13:09:59  dicuccio
* Added missing #include for SeqFeatData.hpp
*
* Revision 1.19  2002/12/26 21:45:28  grichenk
* + GetBestOverlappingFeat()
*
* Revision 1.18  2002/12/26 12:44:39  dicuccio
* Added Win32 export specifiers
*
* Revision 1.17  2002/12/23 13:48:34  dicuccio
* Added predeclaration for CSeq_entry.
*
* Revision 1.16  2002/12/20 16:57:36  kans
* ESeqlocPartial, SeqLocPartialCheck added
*
* Revision 1.15  2002/12/09 20:38:34  ucko
* +sequence::LocationOffset
*
* Revision 1.14  2002/11/25 21:24:45  grichenk
* Added TestForOverlap() function.
*
* Revision 1.13  2002/11/18 19:58:40  shomrat
* Add CSeqSearch - a nucleotide search utility
*
* Revision 1.12  2002/11/12 20:00:19  ucko
* +SourceToProduct, ProductToSource, SRelLoc
*
* Revision 1.11  2002/11/04 22:03:34  ucko
* Pull in <objects/seqloc/Na_strand.hpp> rather than relying on previous headers
*
* Revision 1.10  2002/10/23 19:22:52  ucko
* Move the FASTA reader from objects/util/sequence.?pp to
* objects/seqset/Seq_entry.?pp because it doesn't need the OM.
*
* Revision 1.9  2002/10/23 18:23:43  ucko
* Add a FASTA reader (known to compile, but not otherwise tested -- take care)
*
* Revision 1.8  2002/10/08 12:34:18  clausen
* Improved comments
*
* Revision 1.7  2002/10/03 18:45:45  clausen
* Removed extra whitespace
*
* Revision 1.6  2002/10/03 16:33:10  clausen
* Added functions needed by validate
*
* Revision 1.5  2002/09/12 21:38:43  kans
* added CCdregion_translate and CCdregion_translate
*
* Revision 1.4  2002/08/27 21:41:09  ucko
* Add CFastaOstream.
*
* Revision 1.3  2002/06/10 16:30:22  ucko
* Add forward declaration of CBioseq_Handle.
*
* Revision 1.2  2002/06/07 16:09:42  ucko
* Move everything into the "sequence" namespace.
*
* Revision 1.1  2002/06/06 18:43:28  clausen
* Initial version
*
* ===========================================================================
*/

#endif  /* SEQUENCE__HPP */
