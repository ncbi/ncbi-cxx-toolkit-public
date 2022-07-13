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
 * Author:  Justin Foley
 *
 * File Description:
 *
 */

#ifndef _HUGE_FILE_UTILS_HPP_
#define _HUGE_FILE_UTILS_HPP_

#include <objtools/edit/huge_asn_reader.hpp>
#include <objects/valerr/ValidErrItem.hpp>

BEGIN_NCBI_SCOPE

namespace objects {
    class CValidError;
}

using TBioseqInfo = objects::edit::CHugeAsnReader::TBioseqInfo;


bool g_ReportMissingCitSub(const TBioseqInfo& info, 
        const objects::edit::CHugeAsnReader& reader,
        bool IsRefSeq);

bool g_ReportMissingPubs(const TBioseqInfo& info,
        const objects::edit::CHugeAsnReader& reader);

void g_PostErr(EDiagSev severity,
        objects::EErrType errorType,
        const string& message,
        const string& idString,
        objects::CValidError& errRepository);

bool g_HasRefSeqAccession(const objects::edit::CHugeAsnReader& reader);

bool g_IsCuratedRefSeq(const TBioseqInfo& info);

string g_GetIdString(const TBioseqInfo& info, int* version);

string g_GetIdString(const objects::edit::CHugeAsnReader& reader);

void g_ReportCollidingSerialNumbers(const objects::edit::CHugeAsnReader& reader,
        const string& idString,
        objects::CValidError& errRepository);


class CHugeFileValidator {
public:
    using TReader = objects::edit::CHugeAsnReader;
    using TBioseqInfo = TReader::TBioseqInfo;
    using TOptions = unsigned int;

    CHugeFileValidator(const TReader& reader, 
            const objects::validator::SValidatorContext& context,
            TOptions options);
    ~CHugeFileValidator(){}

    void ReportMissingPubs(CRef<objects::CValidError>& errors) const;

    void ReportMissingCitSubs(CRef<objects::CValidError>& errors) const;

    void ReportCollidingSerialNumbers(CRef<objects::CValidError>& errors) const;

private:
    bool x_HasRefSeqAccession() const; 
    string x_FindIdString() const;
    string x_GetIdString() const;

    mutable unique_ptr<string> m_pIdString;

    const TReader& m_Reader; // Is this what I want?
    const objects::validator::SValidatorContext& m_Context;
    TOptions m_Options;
};

END_NCBI_SCOPE

#endif
