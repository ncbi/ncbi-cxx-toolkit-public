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
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/aln_error_reporter.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objtools/readers/aln_formats.hpp>

BEGIN_NCBI_SCOPE

namespace objects {
    class CSeq_id;
}

class NCBI_XOBJREAD_EXPORT CAlnError
{
public:

    // error categories
    typedef enum {
        eAlnErr_Unknown = -1,
        eAlnErr_NoError = 0,
        eAlnErr_Fatal,
        eAlnErr_BadData,
        eAlnErr_BadFormat,
        eAlnErr_BadChar
    } EAlnErr;
    
    // constructor
    CAlnError(int category, int line_num, string id, string message);

    /// Copy constructor.
    CAlnError(const CAlnError& e);
    
    // destructor
    ~CAlnError() {}
    
    // accessors
    EAlnErr        GetCategory() const { return m_Category; }
    int            GetLineNum()  const { return m_LineNum; }
    const string&  GetID()       const { return m_ID; }
    const string&  GetMsg()      const { return m_Message; }

    /// @deprecated Use GetMsg() instead - to avoid conflict with Windows macro
    NCBI_DEPRECATED
    const string&  GetMessage()  const { return m_Message; }
    
private:
    EAlnErr m_Category;
    int     m_LineNum;
    string  m_ID;
    string  m_Message;
};

///
/// class CAlnReader supports importing a large variety of text-based
/// alignment formats into standard data structures.
///
class NCBI_XOBJREAD_EXPORT CAlnReader
{
public:
    // alphabets to try
    enum EAlphabet {
        eAlpha_Default,
        eAlpha_Nucleotide,
        eAlpha_Protein,
        eAlpha_Dna,
        eAlpha_Rna,
        eAlpha_Dna_no_ambiguity,
        eAlpha_Rna_no_ambiguity,
    };

    // This class is deprecated 
    class CAlnErrorContainer 
    {
    private:
        list<CAlnError> errors;
    public:
        size_t GetErrorCount(CAlnError::EAlnErr category) const
        {
            return 0;
        }

        void clear(void) {
        }

        void push_back(const CAlnError& error) {
        }

        size_t size(void) const {
            return 0;
        }

        typedef list<CAlnError> TErrors;
        typedef TErrors::const_iterator const_iterator;
        const_iterator begin(void) const { return errors.begin(); }
        const_iterator end(void)   const { return errors.end();   }
    };


    // error messages
    typedef CAlnErrorContainer TErrorList;

    static string
    GetAlphabetLetters(
        EAlphabet);

    using TLineInfo = objects::SLineInfo;
    using FValidateIds = 
        function<void(const list<CRef<objects::CSeq_id>>&,
                      int,
                      objects::CAlnErrorReporter*)>;

    using FIdValidate = 
        function<void(const objects::CSeq_id&,
                      int,
                      objects::CAlnErrorReporter*)>;


    // constructor
    // defaults to protein alphabet and A2M gap characters
    CAlnReader(CNcbiIstream& is, FValidateIds fIdValidate=nullptr);

    CAlnReader(CNcbiIstream& is, FIdValidate fSingleIdValidate);

    // destructor
    virtual ~CAlnReader(void);


    const string& GetAlphabet(void) const;
    void          SetAlphabet(const string& value);
    void          SetAlphabet(EAlphabet alpha);
    bool          IsAlphabet(EAlphabet alpha) const;

    const string& GetBeginningGap(void) const;
    void          SetBeginningGap(const string& value);

    const string& GetMiddleGap(void) const;
    void          SetMiddleGap(const string& value);

    const string& GetEndGap(void) const;
    void          SetEndGap(const string& value);

    bool GetUseNexusInfo() const {return m_UseNexusInfo; };
    void SetUseNexusInfo(bool useNexusInfo) { m_UseNexusInfo = useNexusInfo; };

    /// Convenience function for setting beginning, middle, and
    /// end gap to the same thing
    void          SetAllGap(const string& value);

    const string& GetMissing(void)                     const {return mSequenceInfo.Missing();};
    void          SetMissing(const string& value)            {mSequenceInfo.SetMissing(value);};

    const string& GetMatch(void)                       const {return mSequenceInfo.Match();};
    void          SetMatch(const string& value)              {mSequenceInfo.SetMatch(value);};


    /// Alternative & easy way to choose alphabet, etc.
    void SetFastaGap(EAlphabet alpha);
    void SetClustal (EAlphabet alpha);
    void SetPhylip  (EAlphabet alpha);
    void SetPaup    (EAlphabet alpha);


    /// Read the file
    /// This are the main functions.
    /// either would parse the alignment file and create the result data
    enum EReadFlags {
        fReadDefaults = 0,
        fGenerateLocalIDs = 1
    };
    typedef int TReadFlags; ///< binary OR of EReadFlags

    NCBI_DEPRECATED
    void Read(
        bool guess, 
        bool generate_local_ids=false,
        objects::ILineErrorListener* pErrorListener=nullptr);

    void Read(
        TReadFlags = fReadDefaults,
        objects::ILineErrorListener* pErrorListener=nullptr);

    /// Parsed result data accessors
    const vector<string>& GetIds(void)       const {return m_IdStrings;};
    const vector<string>& GetSeqs(void)      const {return m_Seqs;};
    NCBI_DEPRECATED const vector<string>& GetOrganisms(void) const {return m_Organisms;};
    const vector<string>& GetDeflines(void)  const {return m_Deflines;};
    const vector<TLineInfo>& GetDeflineInfo(void) const { return m_DeflineInfo; };
    int                   GetDim(void)       const {return m_Dim;};
    EAlignFormat GetLastAlignmentFileFormat(void) const;

    NCBI_DEPRECATED
    const TErrorList& GetErrorList(void)     const {return m_Errors;};
    
    using TFastaFlags = objects::CFastaDeflineReader::TFastaFlags;
    /// Create ASN.1 classes from the parsed alignment
    CRef<objects::CSeq_align> GetSeqAlign(TFastaFlags fasta_flags=0,
            objects::ILineErrorListener* pErrorListener=nullptr);
    CRef<objects::CSeq_entry> GetSeqEntry(TFastaFlags fasta_flags=objects::CFastaReader::fAddMods,
            objects::ILineErrorListener* pErrorListener=nullptr);

    /// Get a sequence's moltype, also considering the alphabet used to read it
    objects::CSeq_inst::EMol GetSequenceMolType(
        const string& alphabet,
        const string& seqData,
        objects::ILineErrorListener* pErrorListener=nullptr
        );

private:
    /// Prohibit copy constructor and assignment operator
    CAlnReader(const CAlnReader& value);
    CAlnReader& operator=(const CAlnReader& value);

    int x_GetGCD(const int a, const int b) const;

    bool x_IsReplicatedSequence(const char* sequence_data,
        int sequence_length,
        int repeat_interval) const;

    void x_VerifyAlignmentInfo(
        const ncbi::objects::SAlignmentFile&,
        TReadFlags readFlags);

    CRef<objects::CSeq_inst> x_GetSeqInst(objects::CSeq_inst::EMol mol,
            const string& seqData) const;

    objects::CSeq_inst::EMol x_GetSequenceMolType(
        const string& alphabet,
        const string& seqData,
        const string& seqId="", // Used in error message
        objects::ILineErrorListener* pErrorListener=nullptr);

    ncbi::objects::CSequenceInfo mSequenceInfo;


    /// Parsed result data (analogous to SAlignmentFile)
    /// Seqs are upper-case strings representing the sequences, with
    /// '-' for a gap.  Ids are ids read from file.  Organisms and
    /// Deflines may not be set, depending on the file.

    using TIdList = list<CRef<objects::CSeq_id>>;
    vector<string> m_IdStrings;
    vector<TIdList> m_Ids;
    vector<string> m_Seqs;
    vector<string> m_Organisms; // redundant
    vector<string> m_Deflines; // redundant
    vector<TLineInfo> m_DeflineInfo;
    FValidateIds m_fValidateIds=nullptr;
    EAlignFormat m_AlignFormat;


    /// Other internal data
    CNcbiIstream&             m_IS;
    bool                      m_ReadDone;
    bool                      m_ReadSucceeded;
    int                       m_Dim;
    CRef<objects::CSeq_align> m_Aln;
    CRef<objects::CSeq_entry> m_Entry;
    vector<string>            m_SeqVec; 
    vector<TSeqPos>           m_SeqLen; 
    TErrorList                m_Errors;
    bool                      m_UseNexusInfo;

    /// characters have different contexts, depending on 
    /// whether they are before the first non-gap character,
    /// after the last non-gap character, or between the
    /// first and last non-gap character. This must be
    /// precalculated before gap characters can be converted.
    typedef pair<TSeqPos, TSeqPos> TAlignMiddleInterval;
    typedef vector<TAlignMiddleInterval> TAlignMiddles;
    TAlignMiddles m_MiddleSections;
    void x_CalculateMiddleSections();
    typedef objects::CDense_seg::TDim TNumrow;
    bool x_IsGap(TNumrow row, TSeqPos pos, const string& residue);
    void x_AssignDensegIds(
        TFastaFlags fasta_flags,
        objects::CDense_seg& denseg);

    void x_ParseAndValidateSeqIds(
            const TLineInfo& seqIdInfo,
            TReadFlags flags,
            TIdList& ids);

    void x_AddMods(const TLineInfo& defline_info, 
            objects::CBioseq& bioseq,
            objects::ILineErrorListener* pErrorListener);

    void x_AddTitle(const string& defline, objects::CBioseq& bioseq);

protected:
    virtual CRef<objects::CSeq_id> GenerateID(const string& fasta_defline, 
        const TSeqPos& line_number,
        TFastaFlags fasta_flags);

    using SLineTextAndLoc   = objects::CFastaDeflineReader::SLineTextAndLoc;
    using TSeqTitles        = objects::CFastaDeflineReader::TSeqTitles;
    using SDeflineParseInfo = objects::CFastaDeflineReader::SDeflineParseInfo;
    using TIgnoredProblems  = objects::CFastaDeflineReader::TIgnoredProblems;

    NCBI_DEPRECATED
    void ParseDefline(const string& defline,
            const SDeflineParseInfo& info,
            const TIgnoredProblems& ignoredErrors,
            list<CRef<objects::CSeq_id>>& ids,
            bool& hasRange,
            TSeqPos& rangeStart,
            TSeqPos& rangeEnd,
            TSeqTitles& seqTitles,
            objects::ILineErrorListener* pMessageListener);

protected:
    objects::CFastaIdHandler m_FastaIdHandler;
};



///////////////////////////////////////////////////////////////////////
//
//  Inline Methods
//

inline
const string& CAlnReader::GetAlphabet(void) const
{
    return mSequenceInfo.Alphabet();
}


inline
void CAlnReader::SetAlphabet(const string& value)
{
    mSequenceInfo.SetAlphabet(value);
}


inline
const string& CAlnReader::GetBeginningGap(void) const
{
    return mSequenceInfo.BeginningGap();
}


inline
void CAlnReader::SetBeginningGap(const string& value)
{
    mSequenceInfo.SetBeginningGap(value);
}


inline
const string& CAlnReader::GetMiddleGap(void) const
{
    return mSequenceInfo.MiddleGap();
}


inline
void CAlnReader::SetMiddleGap(const string& value)
{
    mSequenceInfo.SetMiddleGap(value);
}


inline
const string& CAlnReader::GetEndGap(void) const
{
    return mSequenceInfo.EndGap();
}
    
inline
void CAlnReader::SetEndGap(const string& value)
{
    mSequenceInfo.SetEndGap(value);
}


inline
void CAlnReader::SetAlphabet(EAlphabet alpha)
{
    SetAlphabet(GetAlphabetLetters(alpha));
}


inline 
bool CAlnReader::IsAlphabet(EAlphabet alpha) const
{
    return (GetAlphabet() == GetAlphabetLetters(alpha));
}


inline
void CAlnReader::SetAllGap(const string& value)
{
    mSequenceInfo.SetBeginningGap(value).SetMiddleGap(value).SetEndGap(value);
}

inline
EAlignFormat CAlnReader::GetLastAlignmentFileFormat() const
{
    return m_AlignFormat;
}


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___ALN_READER__HPP
