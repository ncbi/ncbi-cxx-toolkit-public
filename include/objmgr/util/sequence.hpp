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
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_id;
class CSeq_loc_mix;
class CSeq_point;
class CPacked_seqpnt;
class CScope;
class CBioseq_Handle;
class CSeqVector;
class CCdregion;
class CSeq_feat;
class CSeq_entry;
class CGenetic_code;

BEGIN_SCOPE(sequence)

NCBI_XOBJUTIL_EXPORT
int GetGiForAccession(const string& acc, CScope& scope);

NCBI_XOBJUTIL_EXPORT
string GetAccessionForGi(int gi, CScope& scope, bool with_version = true);

// Get sequence's title (used in various flat-file formats.)
// This function is here rather than in CBioseq because it may need
// to inspect other sequences.  The reconstruct flag indicates that it
// should ignore any existing title Seqdesc.
enum EGetTitleFlags {
    fGetTitle_Reconstruct = 0x1, // ignore existing title Seqdesc.
    fGetTitle_Organism    = 0x2, // append [organism]
    fGetTitle_AllProteins = 0x4  // normally just names the first
};
typedef int TGetTitleFlags;
NCBI_XOBJUTIL_EXPORT
string GetTitle(const CBioseq_Handle& hnd, TGetTitleFlags flags = 0);


/// Retrieve a particular seq-id from a given bioseq handle.  This uses
/// CSynonymsSet internally to decide which seq-id should be used.
enum EGetIdType {
    eGetId_ForceGi,         // return only a gi-based seq-id
    eGetId_ForceAcc,        // return only an accession based seq-id
    eGetId_Best,            // return the "best" gi (uses FindBestScore(),
                            // with CSeq_id::CalculateScore() as the score
                            // function
    eGetId_HandleDefault,   // returns the ID associated with a bioseq-handle

    eGetId_Default = eGetId_Best
};
NCBI_XOBJUTIL_EXPORT
const CSeq_id& GetId(const CBioseq_Handle& handle,
                     EGetIdType type = eGetId_Default);

NCBI_XOBJUTIL_EXPORT
const CSeq_id& GetId(const CSeq_id& id, CScope& scope,
                     EGetIdType type = eGetId_Default);

NCBI_XOBJUTIL_EXPORT
const CSeq_id& GetId(const CSeq_id_Handle& id, CScope& scope,
                     EGetIdType type = eGetId_Default);



enum ES2PFlags {
    fS2P_NoMerge  = 0x1, // don't merge adjacent intervals on the product
    fS2P_AllowTer = 0x2  // map the termination codon as a legal location
};
typedef int TS2PFlags; // binary OR of ES2PFlags
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> SourceToProduct(const CSeq_feat& feat,
                               const CSeq_loc& source_loc, TS2PFlags flags = 0,
                               CScope* scope = 0, int* frame = 0);

enum EP2SFlags {
    fP2S_Extend = 0x1  // if hitting ends, extend to include partial codons
};
typedef int TP2SFlags; // binary OR of ES2PFlags
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> ProductToSource(const CSeq_feat& feat, const CSeq_loc& prod_loc,
                               TP2SFlags flags = 0, CScope* scope = 0);


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
CConstRef<CSeq_feat> GetBestGeneForMrna(const CSeq_feat& mrna_feat,
                                        CScope& scope);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestGeneForCds(const CSeq_feat& mrna_feat,
                                       CScope& scope);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestMrnaForCds(const CSeq_feat& cds_feat,
                                       CScope& scope);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestCdsForMrna(const CSeq_feat& mrna_feat,
                                       CScope& scope);

NCBI_XOBJUTIL_EXPORT
void GetMrnasForGene(const CSeq_feat& gene_feat,
                     CScope& scope,
                     list< CConstRef<CSeq_feat> >& mrna_feats);

NCBI_XOBJUTIL_EXPORT
void GetCdssForGene(const CSeq_feat& gene_feat,
                    CScope& scope,
                    list< CConstRef<CSeq_feat> >& cds_feats);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_feat& feat,
                                            CSeqFeatData::E_Choice feat_type,
                                            sequence::EOverlapType overlap_type,
                                            CScope& scope);

NCBI_XOBJUTIL_EXPORT
CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_feat& feat,
                                            CSeqFeatData::ESubtype feat_type,
                                            sequence::EOverlapType overlap_type,
                                            CScope& scope);


// Convenience functions for popular overlapping types

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


// Get the encoding CDS feature of a given protein sequence.
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetCDSForProduct(const CBioseq& product, CScope* scope);
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetCDSForProduct(const CBioseq_Handle& product);


// Get the mature peptide feature of a protein
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetPROTForProduct(const CBioseq& product, CScope* scope);
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetPROTForProduct(const CBioseq_Handle& product);


// Get the encoding mRNA feature of a given mRNA (cDNA) bioseq.
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetmRNAForProduct(const CBioseq& product, CScope* scope);
NCBI_XOBJUTIL_EXPORT
const CSeq_feat* GetmRNAForProduct(const CBioseq_Handle& product);


// Get the encoding nucleotide sequnce of a protein.
NCBI_XOBJUTIL_EXPORT
const CBioseq* GetNucleotideParent(const CBioseq& product, CScope* scope);
NCBI_XOBJUTIL_EXPORT
CBioseq_Handle GetNucleotideParent(const CBioseq_Handle& product);


// return the org-ref associated with a given sequence.  This will throw
// a CException if there is no org-ref associated with the sequence
NCBI_XOBJUTIL_EXPORT
const COrg_ref& GetOrg_ref(const CBioseq_Handle& handle);

// return the tax-id associated with a given sequence.  This will return 0
// if no tax-id can be found.
NCBI_XOBJUTIL_EXPORT
int GetTaxId(const CBioseq_Handle& handle);


END_SCOPE(sequence)

// FASTA-format output; see also ReadFasta in <objtools/readers/fasta.hpp>
class NCBI_XOBJUTIL_EXPORT CFastaOstream {
public:
    enum EFlags {
        eAssembleParts   = 0x1,
        eInstantiateGaps = 0x2
    };
    typedef int TFlags; // binary OR of EFlags

    CFastaOstream(CNcbiOstream& out) : m_Out(out), m_Width(70), m_Flags(0) { }

    // Unspecified locations designate complete sequences
    void Write        (const CBioseq_Handle& handle,
                       const CSeq_loc* location = 0);
    void WriteTitle   (const CBioseq_Handle& handle);
    void WriteSequence(const CBioseq_Handle& handle,
                       const CSeq_loc* location = 0);

    // These versions set up a temporary object manager
    void Write(CSeq_entry& entry, const CSeq_loc* location = 0);
    void Write(CBioseq&    seq,   const CSeq_loc* location = 0);

    // Used only by Write(CSeq_entry, ...); permissive by default
    virtual bool SkipBioseq(const CBioseq& /* seq */) { return false; }

    // To adjust various parameters...
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

// public interface for coding region translation function

// uses CTrans_table in <objects/seqfeat/Genetic_code_table.hpp>
// for rapid translation from a given genetic code, allowing all
// of the iupac nucleotide ambiguity characters

class NCBI_XOBJUTIL_EXPORT CCdregion_translate
{
public:

    // translation coding region into ncbieaa protein sequence
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

    // return iupac sequence letters under feature location
    static void ReadSequenceByLocation (string& seq,
                                        const CBioseq_Handle& bsh,
                                        const CSeq_loc& loc);

};


class NCBI_XOBJUTIL_EXPORT CSeqTranslator
{
public:

    // translate a string using a specified genetic code
    // if the code is NULL, then the default genetic code is used
    static void Translate(const string& seq,
                          string& prot,
                          const CGenetic_code* code = NULL,
                          bool include_stop = true,
                          bool remove_trailing_X = false);

    // translate a seq-vector using a specified genetic code
    // if the code is NULL, then the default genetic code is used
    static void Translate(const CSeqVector& seq,
                          string& prot,
                          const CGenetic_code* code = NULL,
                          bool include_stop = true,
                          bool remove_trailing_X = false);

    // utility function: translate a given location on a sequence
    static void Translate(const CSeq_loc& loc,
                          const CBioseq_Handle& handle,
                          string& prot,
                          const CGenetic_code* code = NULL,
                          bool include_stop = true,
                          bool remove_trailing_X = false);
};



/// Location relative to a base Seq-loc: one (usually) or more ranges
/// of offsets.
// XXX - handle fuzz?
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



//============================================================================//
//                             Sequence Search                                //
//============================================================================//

// CSeqSearch
// ==========
//
// Search a nucleotide sequence for one or more patterns
//
//
//
//
//
//
class NCBI_XOBJUTIL_EXPORT CSeqSearch
{
public:

    
    // Holds information associated with a match, such as the name of the
    // restriction enzyme, location of cut site etc.
    class CMatchInfo
    {
    public:
        // Constructor:
        CMatchInfo(const string& name,
                   const string& pattern,
                   int cut_site,
                   int overhang,
                   ENa_strand strand):
            m_Name(name), m_Pattern(pattern), 
            m_CutSite(cut_site), m_Overhang(overhang), 
            m_Strand(strand)
        {}
            
        // Getters
        const string& GetName(void) const    { return m_Name; }
        const string& GetPattern(void) const { return m_Pattern; }
        int GetCutSite(void) const           { return m_CutSite; }
        int GetOverhang(void) const          { return m_Overhang; }
        ENa_strand GetStrand(void) const     { return m_Strand; }

    private:
        friend class CSeqSearch; 

        string m_Name;
        string m_Pattern;
        int m_CutSite;
        int m_Overhang;
        ENa_strand m_Strand;        
    };  // end of CMatchInfo


    // Client interface:
    // ==================
    // A class that uses the SeqSearch facility should implement the Client 
    // interface and register itself with the search utility to be notified 
    // of matches detection.
    class IClient
    {
    public:
        virtual void MatchFound(const CMatchInfo& match, int position) = 0;
    };


    // Constructors and Destructors:
    CSeqSearch(IClient *client = 0, bool allow_mismatch = false);
    ~CSeqSearch(void);

    // Add nucleotide pattern or restriction site to sequence search.
    // Uses ambiguity codes, e.g., R = A and G, H = A, C and T
    void AddNucleotidePattern(const string& name,
                              const string& pattern, 
                              int cut_site,
                              int overhang);

    // This is a low level search method.
    // The user is responsible for feeding each character in turn,
    // keep track of the position in the text and provide the length in case of
    // a circular topoloy.
    int Search(int current_state, char ch, int position, int length = INT_MAX);

    // Search an entire bioseq.
    void Search(const CBioseq_Handle& bsh);

    // Get / Set the Client.
    const IClient* GetClient() const { return m_Client; }
    void SetClient(IClient* client) { m_Client = client; }

private:

    // Member Functions:
    void InitializeMaps(void);


    void AddNucleotidePattern(const string& name,
                              const string& pattern, 
                              int cut_site,
                              int overhang,
                              ENa_strand strand);


    void ExpandPattern(const string& pattern,
                       string& temp,
                       int position,
                       int pat_len,
                       CMatchInfo& info);

    string ReverseComplement(const string& pattern) const;

    // Member Variables:

    static map<unsigned char, int>  sm_CharToEnum;
    static map<int, unsigned char>  sm_EnumToChar;
    static map<char, char>          sm_Complement;

    bool                 m_AllowOneMismatch;
    size_t               m_MaxPatLen;
    IClient*             m_Client;
    CTextFsm<CMatchInfo> m_Fsa;

}; // end of CSeqSearch




END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
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
