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

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <util/strsearch.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_id;
class CSeq_loc_mix;
class CSeq_point;
class CPacked_seqpnt;
class CSeq_interval;
class CScope;
class CBioseq_Handle;
class CCdregion;
class CSeq_feat;

BEGIN_SCOPE(sequence)

struct CNotUnique : public runtime_error
{
    CNotUnique() : runtime_error("CSeq_ids do not refer to unique CBioseq") {}
};

struct CNoLength : public runtime_error
{
    CNoLength() : runtime_error("Unable to determine length") {}
};

// Containment relationships between CSeq_locs
enum ECompare {
    eNoOverlap = 0, // CSeq_locs do not overlap
    eContained,     // First CSeq_loc contained by second
    eContains,      // First CSeq_loc contains second
    eSame,          // CSeq_locs contain each other
    eOverlap        // CSeq_locs overlap
};


// Get sequence length if scope not null, else return max possible TSeqPos
TSeqPos GetLength(const CSeq_id& id, CScope* scope = 0);

// Get length of sequence represented by CSeq_loc, if possible
TSeqPos GetLength(const CSeq_loc& loc, CScope* scope = 0)
    THROWS((sequence::CNoLength));

// Get length of CSeq_loc_mix == sum (length of embedded CSeq_locs)
TSeqPos GetLength(const CSeq_loc_mix& mix, CScope* scope = 0)
    THROWS((sequence::CNoLength));

// Checks that point >= 0 and point < length of Bioseq
bool IsValid(const CSeq_point& pt, CScope* scope = 0);

// Checks that all points >=0 and < length of CBioseq. If scope is 0
// assumes length of CBioseq is max value of TSeqPos.
bool IsValid(const CPacked_seqpnt& pts, CScope* scope = 0);

// Checks from and to of CSeq_interval. If from < 0, from > to, or
// to >= length of CBioseq this is an interval for, returns false, else true.
bool IsValid(const CSeq_interval& interval, CScope* scope = 0);

// Determines if two CSeq_ids represent the same CBioseq
bool IsSameBioseq(const CSeq_id& id1, const CSeq_id& id2, CScope* scope = 0);

// Returns true if all embedded CSeq_ids represent the same CBioseq, else false
bool IsOneBioseq(const CSeq_loc& loc, CScope* scope = 0);

// If all CSeq_ids embedded in CSeq_loc refer to the same CBioseq, returns
// the first CSeq_id found, else throws exception CNotUnique()
const CSeq_id& GetId(const CSeq_loc& loc, CScope* scope = 0)
    THROWS((sequence::CNotUnique));

// Returns eNa_strand_unknown if multiple Bioseqs in loc
// Returns eNa_strand_other if multiple strands in same loc
// Returns eNa_strand_both if loc is a Whole
// Returns strand otherwise
ENa_strand GetStrand(const CSeq_loc& loc, CScope* scope = 0);

// If only one CBioseq is represented by CSeq_loc, returns the lowest residue
// position represented. If not null, scope is used to determine if two
// CSeq_ids represent the same CBioseq. Throws exception CNotUnique if
// CSeq_loc does not represent one CBioseq.
TSeqPos GetStart(const CSeq_loc& loc, CScope* scope = 0)
    THROWS((sequence::CNotUnique));

// If only one CBioseq is represented by CSeq_loc, returns the highest residue
// position represented. If not null, scope is used to determine if two
// CSeq_ids represent the same CBioseq. Throws exception CNotUnique if
// CSeq_loc does not represent one CBioseq.
TSeqPos GetStop(const CSeq_loc& loc, CScope* scope = 0)
    THROWS((sequence::CNotUnique));

// Returns the sequence::ECompare containment relationship between CSeq_locs
sequence::ECompare Compare(const CSeq_loc& loc1,
                           const CSeq_loc& loc2,
                           CScope* scope = 0);

// Get sequence's title (used in various flat-file formats.)
// This function is here rather than in CBioseq because it may need
// to inspect other sequences.  The reconstruct flag indicates that it
// should ignore any existing title Seqdesc.
enum EGetTitleFlags {
    fGetTitle_Reconstruct = 0x1, // ignore existing title Seqdesc.
    fGetTitle_Organism    = 0x2  // append [organism]
};
typedef int TGetTitleFlags;
string GetTitle(const CBioseq_Handle& hnd, TGetTitleFlags flags = 0);

// Change a CSeq_id to the one for the CBioseq that it represents
// that has the best rank or worst rank according on value of best.
// Just returns if scope == 0
void ChangeSeqId(CSeq_id* id, bool best, CScope* scope = 0);

// Change each of the CSeq_ids embedded in a CSeq_loc to the best
// or worst CSeq_id accoring to the value of best. Just returns if
// scope == 0
void ChangeSeqLocId(CSeq_loc* loc, bool best, CScope* scope = 0);

enum ESeqLocCheck {
    eSeqLocCheck_ok,
    eSeqLocCheck_warning,
    eSeqLocCheck_error
};

// Checks that a CSeq_loc is all on one strand on one CBioseq. For embedded 
// points, checks that the point location is <= length of sequence of point. 
// For packed points, checks that all points are within length of sequence. 
// For intervals, ensures from <= to and interval is within length of sequence.
// If no mixed strands and lengths are valid, returns eSeqLocCheck_ok. If
// only mixed strands/CBioseq error, then returns eSeqLocCheck_warning. If 
// length error, then returns eSeqLocCheck_error.
ESeqLocCheck SeqLocCheck(const CSeq_loc& loc, CScope* scope);

// Returns true if the order of Seq_locs is bad, otherwise, false
bool BadSeqLocSortOrder
(const CBioseq&  seq,
 const CSeq_loc& loc,
 CScope*         scope);


enum ES2PFlags {
    fS2P_NoMerge  = 0x1, // don't merge adjacent intervals on the product
    fS2P_AllowTer = 0x2  // map the termination codon as a legal location
};
typedef int TS2PFlags; // binary OR of ES2PFlags
CRef<CSeq_loc> SourceToProduct(const CSeq_feat& feat,
                               const CSeq_loc& source_loc, TS2PFlags flags = 0,
                               CScope* scope = 0, int* frame = 0);

enum EP2SFlags {
    fP2S_Extend = 0x1  // if hitting ends, extend to include partial codons
};
typedef int TP2SFlags; // binary OR of ES2PFlags
CRef<CSeq_loc> ProductToSource(const CSeq_feat& feat, const CSeq_loc& prod_loc,
                               TP2SFlags flags = 0, CScope* scope = 0);

enum EOverlapType {
    eOverlap_Simple,          // any overlap of extremes
    eOverlap_Contained,       // contained within extremes
    eOverlap_Subset,          // is a subset of ranges
    eOverlap_CheckIntervals,  // is a subset with matching boundaries
    eOverlap_Interval         // at least one pair of intervals must overlap
};

// Check if the two locations have ovarlap of a given type
int TestForOverlap(const CSeq_loc& loc1, const CSeq_loc& loc2, EOverlapType type);

END_SCOPE(sequence)

// FASTA-format output; see also ReadFasta in <objects/seqset/Seq_entry.hpp>
class CFastaOstream {
public:
    enum EFlags {
        eAssembleParts   = 0x1,
        eInstantiateGaps = 0x2
    };
    typedef int TFlags; // binary OR of EFlags

    CFastaOstream(CNcbiOstream& out) : m_Out(out), m_Width(70), m_Flags(0) { }

    // Unspecified locations designate complete sequences
    void Write        (CBioseq_Handle& handle, const CSeq_loc* location = 0);
    void WriteTitle   (CBioseq_Handle& handle);
    void WriteSequence(CBioseq_Handle& handle, const CSeq_loc* location = 0);

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

class CCdregion_translate
{
public:
    // translation coding region into ncbieaa protein sequence
    static void TranslateCdregion (string& prot,
                                   CBioseq_Handle& bsh,
                                   const CSeq_loc& loc,
                                   const CCdregion& cdr,
                                   bool include_stop = true,
                                   bool remove_trailing_X = false);

    // return iupac sequence letters under feature location
    static void ReadSequenceByLocation (string& seq,
                                        CBioseq_Handle& bsh,
                                        const CSeq_loc& loc);

};



// Location relative to a base Seq-loc: one (usually) or more ranges
// of offsets
// XXX - handle fuzz?
struct SRelLoc
{
    enum EFlags {
        fNoMerge = 0x1 // don't merge adjacent intervals
    };
    typedef int TFlags; // binary OR of EFlags

    // Beware: treats locations corresponding to different sequences as
    // disjoint, even if one is actually a segment of the other. :-/
    SRelLoc(const CSeq_loc& parent, const CSeq_loc& child, CScope* scope = 0,
            TFlags flags = 0);
    CRef<CSeq_loc> Resolve(CScope* scope = 0, TFlags flags = 0) const;

    typedef CSeq_loc::TRange TRange;
    typedef vector<TRange>   TRanges;
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
class CSeqSearch
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
    int                  m_MaxPatLen;
    IClient*             m_Client;
    CTextFsm<CMatchInfo> m_Fsa;

}; // end of CSeqSearch




END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
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
