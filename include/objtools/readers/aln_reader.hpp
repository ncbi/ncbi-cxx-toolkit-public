#ifndef OBJTOOLS_READERS___ALN_READER__HPP
#define OBJTOOLS_READERS___ALN_READER__HPP

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
 * Authors:  Josh Cherry
 *
 * File Description:  C++ wrappers for alignment file reading
 *
 */

#include <corelib/ncbistd.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE


///
/// class CAlnReader supports importing a large variety of text-based
/// alignment formats into standard data structures.
///

class NCBI_XOBJREAD_EXPORT CAlnReader
{
public:

    // alphabets to try
    enum EAlphabet {
        eAlpha_Nucleotide,
        eAlpha_Protein
    };

    // constructor
    CAlnReader(CNcbiIstream& is) : m_IS(is), m_ReadDone(false) {};

    // destructor
    ~CAlnReader(void);



    /// Sequence data accessors and modifiers:

    const string& GetAlphabet(void) const;
    string&       SetAlphabet(void);
    void          SetAlphabet(const string& value);
    void          SetAlphabet(EAlphabet alpha);

    const string& GetBeginningGap(void) const;
    string&       SetBeginningGap(void);
    void          SetBeginningGap(const string& value);

    const string& GetMiddleGap(void) const;
    string&       SetMiddleGap(void);
    void          SetMiddleGap(const string& value);

    const string& GetEndGap(void) const;
    string&       SetEndGap(void);
    void          SetEndGap(const string& value);

    /// Convenience function for setting beginning, middle, and
    /// end gap to the same thing
    void          SetAllGap(const string& value);

    const string& GetMissing(void)                     const {return m_Missing;};
    string&       SetMissing(void)                           {return m_Missing;};
    void          SetMissing(const string& value)            {m_Missing = value;};

    const string& GetMatch(void)                       const {return m_Match;};
    string&       SetMatch(void)                             {return m_Match;};
    void          SetMatch(const string& value)              {m_Match = value;};


    /// Alternative & easy way to choose alphabet, etc.
    void SetFastaGap(EAlphabet alpha);
    void SetClustal (EAlphabet alpha);
    void SetPhylip  (EAlphabet alpha);
    void SetPaup    (EAlphabet alpha);


    /// Read the file
    /// This is the main function
    /// that would parse the alignment file and create the result data
    void Read();



    /// Parsed result data accessors
    const vector<string>& GetIds(void)       const {return m_Ids;};
    const vector<string>& GetSeqs(void)      const {return m_Seqs;};
    const vector<string>& GetOrganisms(void) const {return m_Organisms;};
    const vector<string>& GetDeflines(void)  const {return m_Deflines;};
    int                   GetDim(void)       const {return m_Dim;};

    /// Create ASN.1 classes from the parsed alignment
    CRef<objects::CSeq_align> GetSeqAlign(void);
    CRef<objects::CSeq_entry> GetSeqEntry(void);


private:

    /// Prohibit copy constructor and assignment operator
    CAlnReader(const CAlnReader& value);
    CAlnReader& operator=(const CAlnReader& value);



    /// A bunch of strings listing characters with various
    /// meanings in an alignment file.
    /// Analogous to SSequenceInfo.
    string m_Alphabet;
    string m_BeginningGap;
    string m_MiddleGap;
    string m_EndGap;
    string m_Missing;
    string m_Match;


    /// Parsed result data (analogous to SAlignmentFile)
    /// Seqs are upper-case strings representing the sequences, with
    /// '-' for a gap.  Ids are ids read from file.  Organisms and
    /// Deflines may not be set, depending on the file.
    vector<string> m_Ids;
    vector<string> m_Seqs;
    vector<string> m_Organisms;
    vector<string> m_Deflines;


    /// Other internal data
    CNcbiIstream&             m_IS;
    bool                      m_ReadDone;
    int                       m_Dim;
    CRef<objects::CSeq_align> m_Aln;
    CRef<objects::CSeq_entry> m_Entry;
    vector<string>            m_SeqVec; 
    vector<TSeqPos>           m_SeqLen; 
};



///////////////////////////////////////////////////////////////////////
//
//  Inline Methods
//

inline
const string& CAlnReader::GetAlphabet(void) const
{
    return m_Alphabet;
}


inline
string& CAlnReader::SetAlphabet(void)
{
    return m_Alphabet;
}


inline
void CAlnReader::SetAlphabet(const string& value)
{
    m_Alphabet = value;
}


inline
const string& CAlnReader::GetBeginningGap(void) const
{
    return m_BeginningGap;
}


inline
string& CAlnReader::SetBeginningGap(void)
{
    return m_BeginningGap;
}


inline
void CAlnReader::SetBeginningGap(const string& value)
{
    m_BeginningGap = value;
}


inline
const string& CAlnReader::GetMiddleGap(void) const
{
    return m_MiddleGap;
}


inline
string& CAlnReader::SetMiddleGap(void)
{
    return m_MiddleGap;
}


inline
void CAlnReader::SetMiddleGap(const string& value)
{
    m_MiddleGap = value;
}


inline
const string& CAlnReader::GetEndGap(void) const
{
    return m_EndGap;
}
    
inline
string& CAlnReader::SetEndGap(void)
{
    return m_EndGap;
}


inline
void CAlnReader::SetEndGap(const string& value)
{
    m_EndGap = value;
}


inline
void CAlnReader::SetAlphabet(EAlphabet alpha)
{
    switch (alpha) {
    case eAlpha_Nucleotide:
        // Nucleotide alphabet: IUPAC plus 'x'
        SetAlphabet("ABCDGHKMNRSTUVWXYabcdghkmnrstuvwxy");
        break;

    case eAlpha_Protein:
        SetAlphabet("ABCDEFGHIKLMNPQRSTUVWXYZabcdefghiklmnpqrstuvwxyz");
        break;
    }
}


inline
void CAlnReader::SetAllGap(const string& value)
{
    m_BeginningGap = m_MiddleGap = m_EndGap = value;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___ALN_READER__HPP

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/04/26 17:28:12  dicuccio
 * Added Fasta + Gap API
 *
 * Revision 1.2  2004/03/01 15:26:32  dicuccio
 * Code clean-up.  Added enum for standard alphabets.  Added new APIs to set
 * standard parameters for other alignment types (implemented with unclear details
 * currently).  Added better exception handling.
 *
 * Revision 1.1  2004/02/19 16:55:27  todorov
 * File moved from util/creaders and renamed to aln_reader
 *
 * Revision 1.2  2004/02/18 22:29:17  todorov
 * Converted to single class. Added methods for creating Seq-align and Seq-entry. A working version, but still need to polish: seq-ids, na/aa recognition, etc.
 *
 * Revision 1.1  2004/02/09 16:02:36  jcherry
 * Initial versionnnn
 *
 * ===========================================================================
 */


