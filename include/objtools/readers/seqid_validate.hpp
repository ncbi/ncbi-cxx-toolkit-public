#ifndef _SEQID_VALIDATE_HPP_
#define _SEQID_VALIDATE_HPP_

/*
 * $Id$
 *
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
 * Authors: Frank Ludwig
 *
 */
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_id;
struct SLineInfo;
class CAlnErrorReporter;

//  ----------------------------------------------------------------------------
class CSeqIdValidate
//  ----------------------------------------------------------------------------
{
public:
    virtual ~CSeqIdValidate(void) = default;


    virtual void operator()(const CSeq_id& seqId,
            int lineNum,
            CAlnErrorReporter* pErrorReporter);

    virtual void operator()(const list<CRef<CSeq_id>>& seqIds,
            int lineNum,
            CAlnErrorReporter* pErrorReporter);
};

//  ----------------------------------------------------------------------------
class CFastaIdValidate
//  ----------------------------------------------------------------------------
{
public:
    using TFastaFlags = long;

    CFastaIdValidate(TFastaFlags flags);

    virtual ~CFastaIdValidate() = default;

    enum EErrCode {
        eUnexpectedNucResidues,
        eUnexpectedAminoAcids,
        eIDTooLong,
        eBadLocalID,
        eOther
    };

    using FReportError =
        function<void(EDiagSev severity,
                      int lineNum,
                      const string& idString,
                      EErrCode errCode,
                      const string& msg)>;

    using TIds = list<CRef<CSeq_id>>;

    virtual void operator()(
            const TIds& ids,
            int lineNum,
            FReportError fReportError);

    void SetMaxLocalIDLength(size_t length);
    void SetMaxGeneralTagLength(size_t length);
    void SetMaxAccessionLength(size_t length);
protected:
    void CheckIDLength(const CSeq_id& id,
                       int lineNum,
                       FReportError fReportError) const;

    bool IsValidLocalID(const CSeq_id& id) const;

    virtual bool IsValidLocalString(const CTempString& idString) const;

    void CheckForExcessiveNucData(
            const CSeq_id& id,
            int lineNum,
            FReportError fReportError) const;

    static size_t CountPossibleNucResidues(
            const string& idString);

    void CheckForExcessiveProtData(
            const CSeq_id& id,
            int lineNum,
            FReportError fReportError) const;

    static size_t CountPossibleAminoAcids(
            const string& idString);

protected:
    size_t  kWarnNumNucCharsAtEnd = 20;
    size_t  kErrNumNucCharsAtEnd = 25;
    size_t  kWarnNumAminoAcidCharsAtEnd = 50;
    size_t  kMaxLocalIDLength    = CSeq_id::kMaxLocalIDLength;
    size_t  kMaxGeneralTagLength = CSeq_id::kMaxGeneralTagLength;
    size_t  kMaxAccessionLength  = CSeq_id::kMaxAccessionLength;

    TFastaFlags m_Flags;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _SEQID_VALIDATE_HPP_
