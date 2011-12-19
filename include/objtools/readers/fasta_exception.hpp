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

#include <objtools/readers/reader_exception.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_id;

class CBadResiduesException : public CObjReaderException
{
public:
    enum EErrCode {
        eBadResidues
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
            case eBadResidues:    return "eBadResidues";
            default:              return CException::GetErrCodeString();
        }
    }

    struct SBadResiduePositions {
        SBadResiduePositions(void)
            : m_LineNo(-1) { }

        SBadResiduePositions( 
            CConstRef<CSeq_id> seqId,
            const vector<TSeqPos> & badIndexes,
            int lineNo )
            : m_SeqId(seqId), m_BadIndexes(badIndexes), m_LineNo(lineNo) { }

        SBadResiduePositions( CConstRef<CSeq_id> seqId, TSeqPos badIndex, int lineNo )
            : m_SeqId(seqId), m_LineNo(lineNo)
        {
            m_BadIndexes.push_back(badIndex);
        }

        CConstRef<CSeq_id> m_SeqId;
        vector<TSeqPos> m_BadIndexes;
        int m_LineNo;
    };

    virtual void ReportExtra(ostream& out) const;

    CBadResiduesException(const CDiagCompileInfo& info,
        const CException* prev_exception,
        EErrCode err_code, const string& message,
        const SBadResiduePositions& badResiduePositions, 
        EDiagSev severity = eDiag_Error) THROWS_NONE
        : CObjReaderException(info, prev_exception,
        (CObjReaderException::EErrCode) CException::eInvalid,
        message), m_BadResiduePositions(badResiduePositions)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CBadResiduesException, CObjReaderException);

public:
    // Returns the bad residues found, which might not be complete
    // if we bailed out early.
    const SBadResiduePositions& GetBadResiduePositions(void) const THROWS_NONE
    {
        return m_BadResiduePositions;
    }

private:
    SBadResiduePositions m_BadResiduePositions;

    static void x_ConvertBadIndexesToString(
        CNcbiOstream & out,
        const vector<TSeqPos> &badIndexes, 
        unsigned int maxRanges );
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_READERS___FASTA_EXCEPTION__HPP */
