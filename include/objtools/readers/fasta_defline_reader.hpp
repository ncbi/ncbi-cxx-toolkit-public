#ifndef FASTA_DEFLINE_READER_HPP
#define FASTA_DEFLINE_READER_HPP

#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_XOBJREAD_EXPORT CFastaDeflineReader : public CReaderBase {
    
public:

    using TFastaFlags = int;
    using TBaseFlags = CReaderBase::TReaderFlags;
    using TIgnoredProblems = vector<ILineError::EProblem>;

    struct SLineTextAndLoc {
        SLineTextAndLoc(
            const string& sLineText,
            TSeqPos iLineNum)
        : m_sLineText(sLineText),
          m_iLineNum(iLineNum) {}
        string m_sLineText;
        TSeqPos m_iLineNum;
    }; 

    using TSeqTitles = vector<SLineTextAndLoc>; // No reason this couldn't just be a map

    struct SDeflineParseInfo {
        TBaseFlags fBaseFlags;
        TFastaFlags fFastaFlags;
        size_t maxIdLength;
        size_t lineNumber;
    };

    static bool ParseIDs(
        const string& id_string,
        const SDeflineParseInfo& info,
        const TIgnoredProblems& ignoredErrors,
        list<CRef<CSeq_id>>& ids,
        ILineErrorListener* pMessageListener);

    static void ParseDefline(const string& defline,
        const SDeflineParseInfo& info,
        const TIgnoredProblems& ignoredErrors, 
        list<CRef<CSeq_id>>& ids,
        bool& hasRange,
        TSeqPos& rangeStart,
        TSeqPos& rangeEnd,
        TSeqTitles& seqTitles,
        ILineErrorListener* pMessageListener);

    static TSeqPos ParseRange(const string& s,
        TSeqPos& start,
        TSeqPos& end,
        ILineErrorListener* pMessageListener);

private:
    static bool x_IsValidLocalID(const CSeq_id& id, 
        TFastaFlags fasta_flags);

    static bool x_IsValidLocalID(const string& id_string,
        TFastaFlags fasta_flags);

    static void x_ConvertNumericToLocal(
        list<CRef<CSeq_id>>& ids);

    static void x_PostWarning(ILineErrorListener* pMessageListener, 
        const TSeqPos lineNumber,
        const string& errMessage, 
        const CObjReaderParseException::EErrCode errCode);

    static void x_PostError(ILineErrorListener* pMessageListener, 
        const TSeqPos lineNumber, 
        const string& errMessage,
        const CObjReaderParseException::EErrCode errCode);

    static bool x_ExcessiveSeqDataInTitle(const string& title, 
        TFastaFlags fasta_flags);

    static bool x_ExceedsMaxLength(const string& title, 
        TSeqPos max_length);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
