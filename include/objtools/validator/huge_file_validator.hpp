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

#ifndef _HUGE_FILE_VALIDATOR_HPP_
#define _HUGE_FILE_VALIDATOR_HPP_

#include <objtools/edit/huge_asn_reader.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <objects/seq/MolInfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CValidError;
class IValidError;

BEGIN_SCOPE(validator)

struct SValidatorContext;

string NCBI_VALIDATOR_EXPORT g_GetIdString(const edit::CHugeAsnReader& reader);


class NCBI_VALIDATOR_EXPORT CHugeFileValidator {
public:
    struct SGlobalInfo {
        bool IsPatent = false;
        bool IsPDB    = false;

        bool NoBioSource    = true;
        bool NoPubsFound    = true;
        bool NoCitSubsFound = true;
        bool CurrIsGI = false;
        bool CurrTpaAssembly = false;
        int JustTpaAssembly = 0;
        int TpaAssemblyHist = 0;
        int TpaNoHistYesGI = 0;
        int CumulativeInferenceCount = 0;
        bool NotJustLocalOrGeneral = false;
        set<int> pubSerialNumbers;
        set<int> conflictingSerialNumbers;
        set<CMolInfo::TBiomol> biomols;
        unsigned int numMisplacedFeats {0};

        void Clear() {
            IsPatent = false;
            IsPDB    = false;

            NoBioSource    = true;
            NoPubsFound    = true;
            NoCitSubsFound = true;
            CurrIsGI = false;
            CurrTpaAssembly = false;
            JustTpaAssembly = 0;
            TpaAssemblyHist = 0;
            TpaNoHistYesGI = 0;
            CumulativeInferenceCount = 0;
            NotJustLocalOrGeneral = false;
            pubSerialNumbers.clear();
            conflictingSerialNumbers.clear();
            biomols.clear();
            numMisplacedFeats = 0;
        }
    };

    using TGlobalInfo = SGlobalInfo;
    using TReader =     edit::CHugeAsnReader;
    using TBioseqInfo = TReader::TBioseqInfo;
    using TOptions = unsigned int;

    CHugeFileValidator(const TReader& reader,
            TOptions options);
    ~CHugeFileValidator(){}


    bool IsInBlob(const CSeq_id& id) const;

    void UpdateValidatorContext(TGlobalInfo& globalInfo,
            SValidatorContext& context) const;

    void ReportGlobalErrors(const TGlobalInfo& globalInfo,
            IValidError& errors) const;

    void ReportPostErrors(const SValidatorContext& context, IValidError& errors) const;

    void PostprocessErrors(const TGlobalInfo& globalInfo,
            const string& genbankSetId,
            CRef<CValidError>& pErrors) const;

    static void RegisterReaderHooks(CObjectIStream& objStream, SGlobalInfo& m_GlobalInfo);

private:

    void x_ReportMissingPubs(IValidError& errors) const;

    void x_ReportMissingCitSubs(bool hasRefSeqAccession, IValidError& errors) const;

    void x_ReportCollidingSerialNumbers(const set<int>& collidingNumbers,
            IValidError& errors) const;

    void x_ReportMissingBioSources(IValidError& errors) const;

    void x_ReportConflictingBiomols(IValidError& errors) const;

    void x_PostMsg(EDiagSev severity,
                   EErrType errorType,
                   const string& message,
                   IValidError& errors) const;

    string x_GetIdString() const;

    string x_GetHugeSetLabel() const;

    mutable unique_ptr<string> m_pIdString;

    const TReader& m_Reader;
    TOptions m_Options;
};

string NCBI_VALIDATOR_EXPORT g_GetHugeSetIdString(const edit::CHugeAsnReader& reader);

void NCBI_VALIDATOR_EXPORT g_PostprocessErrors(const CHugeFileValidator::TGlobalInfo& globalInfo,
        const string& genbankSetId,
        CRef<CValidError>& pErrors);

void NCBI_VALIDATOR_EXPORT g_PostprocessErrors(const CHugeFileValidator::TGlobalInfo& globalInfo,
        const string& genbankSetId,
        list<CRef<CValidErrItem>>& errors);

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
