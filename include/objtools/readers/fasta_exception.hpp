#ifndef OBJTOOLS_READERS___FASTA_EXCEPTION__HPP
#define OBJTOOLS_READERS___FASTA_EXCEPTION__HPP

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
* Authors:  Michael Kornbluh, NCBI
*
* File Description:
*   Exceptions for CFastaReader.
*/

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_id;

class CBadResiduesException : public CException
{
public:
    enum EErrCode {
        eBadResidues,
        eInvalid = -1
    };

    EErrCode GetErrCode(void) const
    {
        return typeid(*this) == typeid(CBadResiduesException) ?
            (EErrCode) this->x_GetErrCode() :
            (EErrCode) CException::eInvalid;
    }

    virtual const char* GetErrCodeString(void) const override
    {
        switch (GetErrCode()) {
            case eBadResidues:    return "eBadResidues";
            default:              return CException::GetErrCodeString();
        }
    }

    struct SBadResiduePositions {
        // map lines to the index of bad residues on that line
        typedef map<int, vector<TSeqPos> > TBadIndexMap;

        SBadResiduePositions() = default;
        SBadResiduePositions(const SBadResiduePositions&) = default;

        // convenience ctor for when all bad indexes are on the same line
        SBadResiduePositions(
            CConstRef<CSeq_id> seqId,
            const vector<TSeqPos> & badIndexesOnLine,
            int lineNum )
            : m_SeqId(seqId)
        {
            if( ! badIndexesOnLine.empty() ) {
                m_BadIndexMap[lineNum] = badIndexesOnLine;
            }
        }

        // if we're trying to assemble a CBadResiduesException from multiple errors,
        // we can use this.  This assumes that the newly added positions do NOT
        // repeat any already in this CBadResiduesException.
        void AddBadIndexMap(const TBadIndexMap & additionalBadIndexMap);

        void ConvertBadIndexesToString(
            CNcbiOstream & out,
            unsigned int maxRanges = 1000 ) const;

        CConstRef<CSeq_id> m_SeqId;
        TBadIndexMap m_BadIndexMap;
    };

    void ReportExtra(ostream& out) const override;
    CBadResiduesException(
        const CDiagCompileInfo &info,
        const CException* prev_exception,
        EErrCode err_code,
        const string& message,
        const SBadResiduePositions& badResiduePositions,
        EDiagSev severity = eDiag_Error)
        : CException(info, prev_exception, (CException::EErrCode) err_code, message, severity, 0),
            m_BadResiduePositions(badResiduePositions)
    {}

    CBadResiduesException() = delete;
    CBadResiduesException(const CBadResiduesException&) = default;

    // Returns the bad residues found, which might not be complete
    // if we bailed out early.
    const SBadResiduePositions& GetBadResiduePositions(void) const THROWS_NONE
    {
        return m_BadResiduePositions;
    }

    bool empty(void) const THROWS_NONE
    {
        return m_BadResiduePositions.m_BadIndexMap.empty();
    }

protected:
    const CException* x_Clone(void) const override
    {
        return new CBadResiduesException(*this);
    }

private:
    SBadResiduePositions m_BadResiduePositions;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_READERS___FASTA_EXCEPTION__HPP */
