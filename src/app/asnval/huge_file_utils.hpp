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

string g_GetIdString(const objects::edit::CHugeAsnReader& reader);

struct SGlobalValidatorInfo;

class CHugeFileValidator {
public:
    struct SGlobalInfo {

        bool IsPatent = false;
        bool IsPDB    = false;
   
        bool NoBioSource    = true; 
        bool NoPubsFound    = true;
        bool NoCitSubsFound = true;
        set<int> pubSerialNumbers;
        set<int> conflictingSerialNumbers;

        void Clear() {
            bool IsPatent = false;
            bool IsPDB    = false;

            NoBioSource    = true;
            NoPubsFound    = true;
            NoCitSubsFound = true;
            pubSerialNumbers.clear();
            conflictingSerialNumbers.clear();
        }
    };

    using TGlobalInfo = SGlobalInfo;
    using TReader = objects::edit::CHugeAsnReader;
    using TBioseqInfo = TReader::TBioseqInfo;
    using TOptions = unsigned int;

    CHugeFileValidator(const TReader& reader, 
            TOptions options);
    ~CHugeFileValidator(){}

    void ReportMissingPubs(CRef<objects::CValidError>& pErrors) const;

    void ReportMissingCitSubs(CRef<objects::CValidError>& pErrors) const;

    void ReportCollidingSerialNumbers(const set<int>& collidingNumbers, 
            CRef<objects::CValidError>& pErrors) const;

    void ReportMissingBioSources(CRef<objects::CValidError>& pErrors) const;

    void ReportGlobalErrors(const TGlobalInfo& globalInfo,
            CRef<objects::CValidError>& pErrors) const;

private:
    bool x_HasRefSeqAccession() const; 
    string x_FindIdString() const;
    string x_GetIdString() const;

    mutable unique_ptr<string> m_pIdString;

    const TReader& m_Reader; 
    TOptions m_Options;
};


class CValidatorHugeAsnReader : public objects::edit::CHugeAsnReader {
public:
    CValidatorHugeAsnReader(CHugeFileValidator::TGlobalInfo& globalInfo) :
        m_GlobalInfo(globalInfo) {}
    virtual ~CValidatorHugeAsnReader(){}
    using TParent = objects::edit::CHugeAsnReader;

protected:
    void x_SetHooks(CObjectIStream& objStream, TContext& context) override;

private:
    CHugeFileValidator::TGlobalInfo& m_GlobalInfo;
};

END_NCBI_SCOPE

#endif
