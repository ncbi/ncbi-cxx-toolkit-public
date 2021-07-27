#ifndef FASTA_READER_UTILS_HPP
#define FASTA_READER_UTILS_HPP

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
* Authors:  Justin Foley
*
*/

#include <corelib/ncbicntr.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/seqid_validate.hpp>
#include <atomic>
#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJREAD_EXPORT CFastaDeflineReader : public CReaderBase {

public:

    using TFastaFlags = int;
    using TBaseFlags = CReaderBase::TReaderFlags;
    using TIgnoredProblems = vector<ILineError::EProblem>;
    using TIds = list<CRef<CSeq_id>>;

    static size_t s_MaxLocalIDLength;
    static size_t s_MaxGeneralTagLength;
    static size_t s_MaxAccessionLength;

    struct SLineTextAndLoc {
        SLineTextAndLoc(
            const string& sLineText,
            TSeqPos iLineNum)
        : m_sLineText(sLineText),
          m_iLineNum(iLineNum) {}
        string m_sLineText;
        TSeqPos m_iLineNum;
    };

    using TSeqTitles = vector<SLineTextAndLoc>;

    struct SDeflineParseInfo {
        TBaseFlags fBaseFlags=0;
        TFastaFlags fFastaFlags=0;
        TSeqPos maxIdLength=0; // If maxIdLength is zero, the code uses the
                               // default values specified in CSeq_id
        TSeqPos lineNumber=0;
    };

    using TInfo = SDeflineParseInfo;

    struct SDeflineData {
        TIds ids;
        bool has_range;
        TSeqPos range_start;
        TSeqPos range_end;
        TSeqTitles titles;
    };

    static void ParseDefline(const CTempString& defline,
        const SDeflineParseInfo& info,
        const TIgnoredProblems& ignoredErrors,
        TIds& ids,
        bool& hasRange,
        TSeqPos& rangeStart,
        TSeqPos& rangeEnd,
        TSeqTitles& seqTitles,
        ILineErrorListener* pMessageListener);

    using FIdCheck = function<void(const TIds& id,
                                   const TInfo& info,
                                   ILineErrorListener* listener)>;

    using FIdValidate = function<void(const TIds& id,
                                      int lineNum,
                                      CFastaIdValidate::FReportError* fReportError)>;

    static void ParseDefline(const CTempString& defline,
        const SDeflineParseInfo& info,
        SDeflineData& data,
        ILineErrorListener* pMessageListener);

    static void ParseDefline(const CTempString& defline,
        const SDeflineParseInfo& info,
        SDeflineData& data,
        ILineErrorListener* pMessageListener,
        FIdCheck fn_id_check);

    static TSeqPos ParseRange(const CTempString& s,
        TSeqPos& start,
        TSeqPos& end,
        ILineErrorListener* pMessageListener);

private:
    static void x_ProcessIDs(
        const CTempString& id_string,
        const SDeflineParseInfo& info,
        TIds& ids,
        ILineErrorListener* pMessageListener,
        FIdCheck fn_id_check);

    static void x_ConvertNumericToLocal(
        TIds& ids);
};


class NCBI_XOBJREAD_EXPORT CSeqIdCheck
{
public:
    using TInfo = CFastaDeflineReader::SDeflineParseInfo;
    using TIds = CFastaDeflineReader::TIds;

    void operator()(const TIds& ids,
                    const TInfo& info,
                    ILineErrorListener* listener);

    using FReportError NCBI_STD_DEPRECATED("") =
        function<void (const string& msg, EDiagSev severity)>;
};


class NCBI_XOBJREAD_EXPORT CSeqIdGenerator : public CObject
{
public:
    typedef int TCount;
    CSeqIdGenerator(TCount count = 1,
                    const string& prefix = kEmptyStr,
                    const string& suffix = kEmptyStr) :
        m_Prefix(prefix),
        m_Suffix(suffix),
        m_Counter(count)
        {}

    CRef<CSeq_id> GenerateID(const string& defline, bool advance);
    CRef<CSeq_id> GenerateID(bool advance);
    /// Equivalent to GenerateID(false)
    CRef<CSeq_id> GenerateID(void) const;

    const string& GetPrefix (void) const    { return m_Prefix;  }
    TCount GetCounter(void) const           { return m_Counter; }
    const string& GetSuffix (void) const    { return m_Suffix;  }

    void SetPrefix (const string& prefix)   { m_Prefix  = prefix; }
    void SetCounter(TCount count)           { m_Counter = count;  }
    void SetSuffix (const string& suffix)   { m_Suffix  = suffix; }

protected:
    string m_Prefix;
    string m_Suffix;
    std::atomic<TCount> m_Counter;
};


class NCBI_XOBJREAD_EXPORT CFastaIdHandler : public CObject
{
public:
    CFastaIdHandler(void) : mp_IdGenerator(new CSeqIdGenerator()) {}

    void SetGenerator(CSeqIdGenerator& generator) {
        mp_IdGenerator.Reset(&generator);
    }

    CSeqIdGenerator& SetGenerator(void) {
        return *mp_IdGenerator;
    }

    const CSeqIdGenerator& GetGenerator(void) const {
        return *mp_IdGenerator;
    }

    virtual CRef<CSeq_id> GenerateID(bool unique_id);

    virtual CRef<CSeq_id> GenerateID(const string& defline, bool unique_id=true);


    void ClearIdCache(void) {
        m_PreviousIdHandles.clear();
    }

    bool CacheIdHandle(CSeq_id_Handle idh) {
        return m_PreviousIdHandles.insert(idh).second;
    }

protected:

    bool x_IsUniqueIdHandle(CSeq_id_Handle idh) {
        return (m_PreviousIdHandles.find(idh) == m_PreviousIdHandles.end());
    }


    using TIdHandleCache = set<CSeq_id_Handle>;
    CRef<CSeqIdGenerator> mp_IdGenerator;
    TIdHandleCache m_PreviousIdHandles;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif
