#ifndef OBJECTS_ALNMGR___ALNVEC__HPP
#define OBJECTS_ALNMGR___ALNVEC__HPP

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
 * Author:  Kamen Todorov, NCBI
 *
 * File Description:
 *   Access to the actual aligned residues
 *
 */


#include <objtools/alnmgr/alnmap.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


// forward declarations
class CScope;

class NCBI_XALNMGR_EXPORT CAlnVec : public CAlnMap
{
    typedef CAlnMap                         Tparent;
    typedef map<TNumrow, CBioseq_Handle>    TBioseqHandleCache;
    typedef map<TNumrow, CRef<CSeqVector> > TSeqVectorCache;

public:
    typedef CSeqVector::TResidue            TResidue;
    typedef vector<int>                     TResidueCount;

    // constructor
    CAlnVec(const CDense_seg& ds, CScope& scope);
    CAlnVec(const CDense_seg& ds, TNumrow anchor, CScope& scope);

    // destructor
    ~CAlnVec(void);

    CScope& GetScope(void) const;


    // GetSeqString methods:

    // raw seq string (in seq coords)
    string& GetSeqString   (string& buffer,
                            TNumrow row,
                            TSeqPos seq_from, TSeqPos seq_to)             const;
    string& GetSeqString   (string& buffer,
                            TNumrow row,
                            const CAlnMap::TRange& seq_rng)               const;
    string& GetSegSeqString(string& buffer, 
                            TNumrow row, 
                            TNumseg seg, TNumseg offset = 0)              const;
 
   // alignment (seq + gaps) string (in aln coords)
    string& GetAlnSeqString(string& buffer,
                            TNumrow row, 
                            const CAlnMap::TSignedRange& aln_rng)         const;

    // creates a vertical string of residues for a given aln pos
    // optionally, returns a distribution of residues
    // optionally, counts the gaps in this distribution
    string& GetColumnVector(string& buffer,
                            TSeqPos aln_pos,
                            TResidueCount * residue_count = 0,
                            bool gaps_in_count = false)                   const;

    // get the seq string for the whole alignment (seq + gaps)
    // optionally, get the inserts and screen limit coords
    string& GetWholeAlnSeqString(TNumrow       row,
                                 string&       buffer,
                                 TSeqPosList * insert_aln_starts = 0,
                                 TSeqPosList * insert_starts = 0,
                                 TSeqPosList * insert_lens = 0,
                                 unsigned int  scrn_width = 0,
                                 TSeqPosList * scrn_lefts = 0,
                                 TSeqPosList * scrn_rights = 0) const;

    const CBioseq_Handle& GetBioseqHandle(TNumrow row)                  const;
    TResidue              GetResidue     (TNumrow row, TSeqPos aln_pos) const;


    // gap character could be explicitely set otherwise taken from seqvector
    void     SetGapChar(TResidue gap_char);
    void     UnsetGapChar();
    bool     IsSetGapChar()                const;
    TResidue GetGapChar(TNumrow row)       const;

    // end character is ' ' by default
    void     SetEndChar(TResidue gap_char);
    void     UnsetEndChar();
    bool     IsSetEndChar()                const;
    TResidue GetEndChar()                  const;

    // functions for manipulating the consensus sequence
    CRef<CDense_seg> CreateConsensus(int& consensus_row) const;

    // utilities
    int CalculateScore          (TNumrow row1, TNumrow row2) const;
    int CalculatePercentIdentity(TSeqPos aln_pos)            const;

    // static utilities
    static void TranslateNAToAA(const string& na, string& aa);
    static int  CalculateScore (const string& s1, const string& s2,
                                bool s1_is_prot, bool s2_is_prot);
    
    // temporaries for conversion (see note below)
    static unsigned char FromIupac(unsigned char c);
    static unsigned char ToIupac  (unsigned char c);

protected:

    CSeqVector& x_GetSeqVector         (TNumrow row)       const;
    CSeqVector& x_GetConsensusSeqVector(void)              const;

    mutable CRef<CScope>            m_Scope;
    mutable TBioseqHandleCache      m_BioseqHandlesCache;
    mutable TSeqVectorCache         m_SeqVectorCache;

private:
    // Prohibit copy constructor and assignment operator
    CAlnVec(const CAlnVec&);
    CAlnVec& operator=(const CAlnVec&);

    TResidue m_GapChar;
    bool     m_set_GapChar;
    TResidue m_EndChar;
    bool     m_set_EndChar;
};



class NCBI_XALNMGR_EXPORT CAlnVecPrinter : public CAlnMapPrinter
{
public:
    /// Constructor
    CAlnVecPrinter(const CAlnVec& aln_vec,
                   CNcbiOstream&  out);


    /// which algorithm to choose
    enum EAlgorithm {
        eUseSeqString,         /// memory ineficient
        eUseAlnSeqString,      /// memory efficient, recommended for large alns
        eUseWholeAlnSeqString  /// memory ineficient, but very fast
    };

    /// Printing methods
    void PopsetStyle (int        scrn_width = 70,
                      EAlgorithm algorithm  = eUseAlnSeqString);

    void ClustalStyle(int        scrn_width = 50,
                      EAlgorithm algorithm  = eUseAlnSeqString);

private:
    void x_SetChars();
    void x_UnsetChars();

    const CAlnVec& m_AlnVec;

    typedef CSeqVector::TResidue            TResidue;

    bool     m_OrigSetGapChar;
    TResidue m_OrigGapChar;

    bool     m_OrigSetEndChar;
    TResidue m_OrigEndChar;
};



/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


inline
CScope& CAlnVec::GetScope(void) const
{
    return *m_Scope;
}


inline 
CSeqVector::TResidue CAlnVec::GetResidue(TNumrow row, TSeqPos aln_pos) const
{
    if (aln_pos > GetAlnStop()) {
        return (TResidue) 0; // out of range
    }
    TSegTypeFlags type = GetSegType(row, GetSeg(aln_pos));
    if (type & fSeq) {
        CSeqVector& seq_vec = x_GetSeqVector(row);
        TSignedSeqPos pos = GetSeqPosFromAlnPos(row, aln_pos);
        if (GetWidth(row) == 3) {
            string na_buff, aa_buff;
            if (IsPositiveStrand(row)) {
                seq_vec.GetSeqData(pos, pos + 3, na_buff);
            } else {
                TSeqPos size = seq_vec.size();
                seq_vec.GetSeqData(size - pos - 3, size - pos, na_buff);
            }
            TranslateNAToAA(na_buff, aa_buff);
            return aa_buff[0];
        } else {
            return seq_vec[IsPositiveStrand(row) ?
                          pos : seq_vec.size() - pos - 1];
        }
    } else {
        if (type & fNoSeqOnLeft  ||  type & fNoSeqOnRight) {
            return GetEndChar();
        } else {
            return GetGapChar(row);
        }
    }
}


inline
string& CAlnVec::GetSeqString(string& buffer,
                              TNumrow row,
                              TSeqPos seq_from, TSeqPos seq_to) const
{
    if (GetWidth(row) == 3) {
        string buff;
        buffer.erase();
        if (IsPositiveStrand(row)) {
            x_GetSeqVector(row).GetSeqData(seq_from, seq_to + 1, buff);
        } else {
            CSeqVector& seq_vec = x_GetSeqVector(row);
            TSeqPos size = seq_vec.size();
            seq_vec.GetSeqData(size - seq_to - 1, size - seq_from, buff);
        }
        TranslateNAToAA(buff, buffer);
    } else {
        if (IsPositiveStrand(row)) {
            x_GetSeqVector(row).GetSeqData(seq_from, seq_to + 1, buffer);
        } else {
            CSeqVector& seq_vec = x_GetSeqVector(row);
            TSeqPos size = seq_vec.size();
            seq_vec.GetSeqData(size - seq_to - 1, size - seq_from, buffer);
        }
    }
    return buffer;
}


inline
string& CAlnVec::GetSegSeqString(string& buffer,
                                 TNumrow row,
                                 TNumseg seg, int offset) const
{
    return GetSeqString(buffer, row,
                        GetStart(row, seg, offset),
                        GetStop (row, seg, offset));
}


inline
string& CAlnVec::GetSeqString(string& buffer,
                              TNumrow row,
                              const CAlnMap::TRange& seq_rng) const
{
    return GetSeqString(buffer, row,
                        seq_rng.GetFrom(),
                        seq_rng.GetTo());
}


inline
void CAlnVec::SetGapChar(TResidue gap_char)
{
    m_GapChar = gap_char;
    m_set_GapChar = true;
}

inline
void CAlnVec::UnsetGapChar()
{
    m_set_GapChar = false;
}

inline
bool CAlnVec::IsSetGapChar() const
{
    return m_set_GapChar;
}

inline
CSeqVector::TResidue CAlnVec::GetGapChar(TNumrow row) const
{
    if (IsSetGapChar()) {
        return m_GapChar;
    } else {
        return x_GetSeqVector(row).GetGapChar();
    }
}

inline
void CAlnVec::SetEndChar(TResidue end_char)
{
    m_EndChar = end_char;
    m_set_EndChar = true;
}

inline
void CAlnVec::UnsetEndChar()
{
    m_set_EndChar = false;
}

inline
bool CAlnVec::IsSetEndChar() const
{
    return m_set_EndChar;
}

inline
CSeqVector::TResidue CAlnVec::GetEndChar() const
{
    if (IsSetEndChar()) {
        return m_EndChar;
    } else {
        return ' ';
    }
}


//
// these are a temporary work-around
// CSeqportUtil contains routines for converting sequence data from one
// format to another, but it places a requirement on the data: it must in
// a CSeq_data class.  I can get this for my data, but it is a bit of work
// (much more work than calling CSeqVector::GetSeqdata(), which, if I use the
// internal sequence vector, is guaranteed to be in IUPAC notation)
//
inline
unsigned char CAlnVec::FromIupac(unsigned char c)
{
    switch (c)
    {
    case 'A': return 0x01;
    case 'C': return 0x02;
    case 'M': return 0x03;
    case 'G': return 0x04;
    case 'R': return 0x05;
    case 'S': return 0x06;
    case 'V': return 0x07;
    case 'T': return 0x08;
    case 'W': return 0x09;
    case 'Y': return 0x0a;
    case 'H': return 0x0b;
    case 'K': return 0x0c;
    case 'D': return 0x0d;
    case 'B': return 0x0e;
    case 'N': return 0x0f;
    }

    return 0x00;
}

inline unsigned char CAlnVec::ToIupac(unsigned char c)
{
    const char *data = "-ACMGRSVTWYHKDBN";
    return ((c < 16) ? data[c] : 0);
}


///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.35  2005/03/16 19:31:21  todorov
 * Added independent (from CAlnVec) default end & gap characters for the printers.
 *
 * Revision 1.34  2005/03/15 17:44:24  todorov
 * Added a printer class
 *
 * Revision 1.33  2004/03/30 16:37:43  todorov
 * +GetColumnVector comments
 *
 * Revision 1.32  2003/12/22 19:14:13  todorov
 * Only left constructors that accept CScope&
 *
 * Revision 1.31  2003/12/22 18:30:38  todorov
 * ObjMgr is no longer created internally. Scope should be passed as a reference in the ctor
 *
 * Revision 1.30  2003/12/18 19:48:28  todorov
 * + GetColumnVector & CalculatePercentIdentity
 *
 * Revision 1.29  2003/12/17 17:01:07  todorov
 * Added translated GetResidue
 *
 * Revision 1.28  2003/12/10 17:17:40  todorov
 * CalcScore now const
 *
 * Revision 1.27  2003/10/15 21:18:57  yazhuk
 * Made TResidue declaration public
 *
 * Revision 1.26  2003/09/10 15:28:11  todorov
 * improved the Get{,Seg,Aln}SeqString comments
 *
 * Revision 1.25  2003/08/29 18:17:50  dicuccio
 * Changed API for creating consensus - CreateConsensus() now returns the new
 * dense-seg, rather than altering the existing alignment manager
 *
 * Revision 1.24  2003/08/25 16:35:06  todorov
 * exposed GetWidth
 *
 * Revision 1.23  2003/08/20 14:35:14  todorov
 * Support for NA2AA Densegs
 *
 * Revision 1.22  2003/07/23 20:40:54  todorov
 * +aln_starts for the inserts in GetWhole...
 *
 * Revision 1.21  2003/07/17 22:46:09  todorov
 * +GetWholeAlnSeqString
 *
 * Revision 1.20  2003/06/02 16:01:38  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.19  2003/02/11 21:32:37  todorov
 * fMinGap optional merging algorithm
 *
 * Revision 1.18  2003/01/27 22:30:24  todorov
 * Attune to seq_vector interface change
 *
 * Revision 1.17  2003/01/23 21:30:50  todorov
 * Removing the original, inefficient GetXXXString methods
 *
 * Revision 1.16  2003/01/23 16:31:56  todorov
 * Added calc score methods
 *
 * Revision 1.15  2003/01/22 22:54:04  todorov
 * Removed an obsolete function
 *
 * Revision 1.14  2003/01/17 18:17:29  todorov
 * Added a better-performing set of GetXXXString methods
 *
 * Revision 1.13  2003/01/16 20:46:30  todorov
 * Added Gap/EndChar set flags
 *
 * Revision 1.12  2002/12/26 12:38:08  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.11  2002/10/21 19:15:12  todorov
 * added GetAlnSeqString
 *
 * Revision 1.10  2002/10/04 17:31:36  todorov
 * Added gap char and end char
 *
 * Revision 1.9  2002/09/27 17:01:16  todorov
 * changed order of params for GetSeqPosFrom{Seq,Aln}Pos
 *
 * Revision 1.8  2002/09/25 19:34:15  todorov
 * "un-inlined" x_GetSeqVector
 *
 * Revision 1.7  2002/09/25 18:16:26  dicuccio
 * Reworked computation of consensus sequence - this is now stored directly
 * in the underlying CDense_seg
 * Added exception class; currently used only on access of non-existent
 * consensus.
 *
 * Revision 1.6  2002/09/23 18:22:45  vakatov
 * Workaround an older MSVC++ compiler bug.
 * Get rid of "signed/unsigned" comp.warning, and some code prettification.
 *
 * Revision 1.5  2002/09/19 18:22:28  todorov
 * New function name for GetSegSeqString to avoid confusion
 *
 * Revision 1.4  2002/09/19 18:19:18  todorov
 * fixed unsigned to signed return type for GetConsensusSeq{Start,Stop}
 *
 * Revision 1.3  2002/09/05 19:31:18  dicuccio
 * - added ability to reference a consensus sequence for a given alignment
 * - added caching of CSeqVector (big performance win)
 * - many small bugs fixed
 *
 * Revision 1.2  2002/08/29 18:40:53  dicuccio
 * added caching mechanism for CSeqVector - this greatly improves speed in
 * accessing sequence data.
 *
 * Revision 1.1  2002/08/23 14:43:50  ucko
 * Add the new C++ alignment manager to the public tree (thanks, Kamen!)
 *
 *
 * ===========================================================================
 */

#endif  /* OBJECTS_ALNMGR___ALNVEC__HPP */
