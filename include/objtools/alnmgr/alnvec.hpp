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
    // NB: buffer will be resized to GetNumRows()
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

    // Sequence coding. If not set (default), Iupac[na/aa] coding is used.
    // If set to a value conflicting with the sequence type
    typedef CSeq_data::E_Choice TCoding;
    TCoding GetNaCoding(void) const { return m_NaCoding; }
    TCoding GetAaCoding(void) const { return m_AaCoding; }
    void SetNaCoding(TCoding coding) { m_NaCoding = coding; }
    void SetAaCoding(TCoding coding) { m_AaCoding = coding; }

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

    // genetic code
    enum EConstants {
        kDefaultGenCode = 1
    };
    void     SetGenCode(int gen_code, 
                        TNumrow row = -1);
    void     UnsetGenCode();
    bool     IsSetGenCode()                const;
    int      GetGenCode(TNumrow row)       const;

    // Functions for obtaining a consensus sequence
    // These versions add the consensus Bioseq to the scope.
    CRef<CDense_seg> CreateConsensus(int& consensus_row) const;
    CRef<CDense_seg> CreateConsensus(int& consensus_row,
                                     const CSeq_id& consensus_id) const;
    // This version returns the consensus Bioseq (in a parameter)
    // without adding it to the scope.
    CRef<CDense_seg> CreateConsensus(int& consensus_row,
                                     CBioseq& consensus_seq,
     	                             const CSeq_id& consensus_id) const;

    // utilities
    int CalculateScore          (TNumrow row1, TNumrow row2) const;
    int CalculatePercentIdentity(TSeqPos aln_pos)            const;

    // static utilities
    static void TranslateNAToAA(const string& na, string& aa,
                                int gen_code = kDefaultGenCode);
    //                          gen_code per 
    //                          http://www.ncbi.nlm.nih.gov/collab/FT/#7.5.5

    static int  CalculateScore (const string& s1, const string& s2,
                                bool s1_is_prot, bool s2_is_prot,
                                int gen_code1 = kDefaultGenCode,
                                int gen_code2 = kDefaultGenCode);
    
    // temporaries for conversion (see note below)
    static unsigned char FromIupac(unsigned char c);
    static unsigned char ToIupac  (unsigned char c);

protected:

    CSeqVector& x_GetSeqVector         (TNumrow row)       const;
    CSeqVector& x_GetConsensusSeqVector(void)              const;

    void CreateConsensus(vector<string>& consens) const;
    void RetrieveSegmentSequences(size_t segment, vector<string>& segs) const;

    mutable CRef<CScope>            m_Scope;
    mutable TBioseqHandleCache      m_BioseqHandlesCache;
    mutable TSeqVectorCache         m_SeqVectorCache;

private:
    // Prohibit copy constructor and assignment operator
    CAlnVec(const CAlnVec&);
    CAlnVec& operator=(const CAlnVec&);

    TResidue    m_GapChar;
    bool        m_set_GapChar;
    TResidue    m_EndChar;
    bool        m_set_EndChar;
    vector<int> m_GenCodes;
    TCoding     m_NaCoding;
    TCoding     m_AaCoding;
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
            TranslateNAToAA(na_buff, aa_buff, GetGenCode(row));
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
        TranslateNAToAA(buff, buffer, GetGenCode(row));
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

inline
void CAlnVec::SetGenCode(int gen_code, TNumrow row)
{
    if (row == -1) {
        if (IsSetGenCode()) {
            UnsetGenCode();
        }
        m_GenCodes.resize(GetNumRows(), gen_code);
    } else {
        if ( !IsSetGenCode() ) {
            m_GenCodes.resize(GetNumRows(), kDefaultGenCode);
        }
        m_GenCodes[row] = gen_code;
    }
}

inline
void CAlnVec::UnsetGenCode()
{
    m_GenCodes.clear();
}

inline
bool CAlnVec::IsSetGenCode() const
{
    return !m_GenCodes.empty();
}

inline
int CAlnVec::GetGenCode(TNumrow row) const
{
    if (IsSetGenCode()) {
        return m_GenCodes[row];
    } else {
        return kDefaultGenCode;
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

#endif  /* OBJECTS_ALNMGR___ALNVEC__HPP */
