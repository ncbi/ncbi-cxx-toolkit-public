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


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_id;
class CSeq_loc;
class CSeq_loc_mix;
class CSeq_point;
class CPacked_seqpnt;
class CSeq_interval;
class CScope;
class CBioseq_Handle;

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

// Checks that a CSeq_loc is all on one strand. For embedded points, checks
// that the point location is <= length of sequence of point. For packed
// points, checks that all points are within length of sequence. For
// intervals, ensures from <= to and interval is within length of sequence.
ESeqLocCheck SeqLocCheck(const CSeq_loc& loc, CScope* scope);

// Returns true if the order of Seq_locs is bad, otherwise, false
bool BadSeqLocSortOrder
(const CBioseq&  seq,
 const CSeq_loc& loc,
 CScope*         scope);


END_SCOPE(sequence)

// FASTA-format output
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


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
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
