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
 * Authors:  Frank Ludwig Justin Foley
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/readers/aln_error_reporter.hpp>
#include <objtools/readers/seqid_validate.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

void CSeqIdValidate::operator()(const CSeq_id& seqId,
        int lineNum,
        CAlnErrorReporter* pErrorReporter)
{

    if (!pErrorReporter) {
        return;
    }

    if (seqId.IsLocal() &&
        seqId.GetLocal().IsStr()) {
        const auto idString = seqId.GetLocal().GetStr();

        bool foundError = false;
        string description;
        if (idString.empty()) {
            description = "Empty local ID.";
            foundError = true;
        }
        else
        if (idString.size() > 50) {
            description = "Local ID \"" +
                          idString +
                          " \" exceeds 50 character limit.";
            foundError = true;
        }
        else
        if (CSeq_id::CheckLocalID(idString) & CSeq_id::fInvalidChar) {
            description = "Local ID \"" +
                          idString +
                          "\" contains invalid characters.";
            foundError = true;
        }

        if (foundError) {
            pErrorReporter->Error(
                    lineNum,
                    EAlnSubcode::eAlnSubcode_IllegalSequenceId,
                    description);
        }
    }
    // default implementation only checks local IDs
}



void CSeqIdValidate::operator()(const list<CRef<CSeq_id>>& ids,
        int lineNum,
        CAlnErrorReporter* pErrorReporter) {

    for (auto pSeqId : ids) {
        operator()(*pSeqId, lineNum, pErrorReporter);
    }
}


CFastaIdValidate::CFastaIdValidate(TFastaFlags flags) :
    m_Flags(flags) {}


void CFastaIdValidate::operator()(const TIds& ids,
        int lineNum,
        FReportError fReportError)
{
    if (ids.empty()) {
        return;
    }

    if (!(m_Flags&CFastaReader::fAssumeProt)) {
        CheckForExcessiveNucData(*ids.back(), lineNum, fReportError);
    }

    if (!(m_Flags&CFastaReader::fAssumeNuc)) {
        CheckForExcessiveProtData(*ids.back(), lineNum, fReportError);
    }


    for (const auto& pId : ids) {
        if (pId->IsLocal() &&
            !IsValidLocalID(*pId)) {
            const auto& idString = pId->GetSeqIdString();
            string msg = "'" + idString + "' is not a valid local ID";
            fReportError(eDiag_Error, lineNum, idString, CFastaIdValidate::eBadLocalID, msg);
        }
        CheckIDLength(*pId, lineNum, fReportError);
    }
}


void CFastaIdValidate::SetMaxLocalIDLength(size_t length)
{
    kMaxLocalIDLength = length;
}


void CFastaIdValidate::SetMaxGeneralTagLength(size_t length)
{
    kMaxGeneralTagLength = length;
}


void CFastaIdValidate::SetMaxAccessionLength(size_t length)
{
    kMaxAccessionLength = length;
}


static string s_GetIDLengthErrorString(int length,
        const string& idType,
        int maxAllowedLength,
        int lineNum)
{
    string err_message =
        "Near line " + NStr::NumericToString(lineNum) +
        + ", the " + idType + " is too long.  Its length is " + NStr::NumericToString(length)
        + " but the maximum allowed " + idType + " length is "+  NStr::NumericToString(maxAllowedLength)
        + ".  Please find and correct all " + idType + "s that are too long.";

    return err_message;
}


void CFastaIdValidate::CheckIDLength(const CSeq_id& id, int lineNum, FReportError fReportError) const
{
    if (id.IsLocal()) {
        if (id.GetLocal().IsStr() &&
            id.GetLocal().GetStr().length() > kMaxLocalIDLength) {
            const auto& msg =
                s_GetIDLengthErrorString(id.GetLocal().GetStr().length(),
                        "local id",
                        kMaxLocalIDLength,
                        lineNum);
            fReportError(eDiag_Error, lineNum, id.GetSeqIdString(), CFastaIdValidate::eIDTooLong, msg);
        }
        return;
    }

    if (id.IsGeneral()) {
        if (id.GetGeneral().IsSetTag() &&
            id.GetGeneral().GetTag().IsStr()) {
            const auto length = id.GetGeneral().GetTag().GetStr().length();
            if (length > kMaxGeneralTagLength) {
                const auto& msg =
                    s_GetIDLengthErrorString(id.GetGeneral().GetTag().GetStr().length(),
                            "general id string",
                            kMaxGeneralTagLength,
                            lineNum);
                fReportError(eDiag_Error, lineNum, id.GetSeqIdString(), CFastaIdValidate::eIDTooLong, msg);
            }
        }
        return;
    }

    auto pTextId = id.GetTextseq_Id();
    if (pTextId && pTextId->IsSetAccession()) {
        const auto length = pTextId->GetAccession().length();
        if (length > kMaxAccessionLength) {
            const auto& msg =
                s_GetIDLengthErrorString(length,
                                "accession",
                                kMaxAccessionLength,
                                lineNum);
            fReportError(eDiag_Error, lineNum, id.GetSeqIdString(), CFastaIdValidate::eIDTooLong, msg);
        }
    }
}


bool CFastaIdValidate::IsValidLocalID(const CSeq_id& id) const
{
    if (id.IsLocal()) {
        if (id.GetLocal().IsId()) {
            return true;
        }
        if (id.GetLocal().IsStr()) {
            return IsValidLocalString(id.GetLocal().GetStr());
        }
    }
    return false;
}


bool CFastaIdValidate::IsValidLocalString(const CTempString& idString) const
{
    const CTempString& checkString =
        (m_Flags & CFastaReader::fQuickIDCheck) ?
        idString.substr(0,1) :
        idString;

    return !(CSeq_id::CheckLocalID(checkString)&CSeq_id::fInvalidChar);
}


void CFastaIdValidate::CheckForExcessiveNucData(
    const CSeq_id& id,
    int lineNum,
    FReportError fReportError
) const
{
    const auto& idString = id.GetSeqIdString();
    if (idString.length() > kWarnNumNucCharsAtEnd) {
        TSeqPos numNucChars = CountPossibleNucResidues(idString);
        if (numNucChars > kWarnNumNucCharsAtEnd) {
            const string err_message =
            "Fasta Reader: sequence id ends with " +
            NStr::NumericToString(numNucChars) +
            " valid nucleotide characters. " +
            " Was the sequence accidentally placed in the definition line?";

            auto severity = (numNucChars > kErrNumNucCharsAtEnd) ?
                            eDiag_Error :
                            eDiag_Warning;

            fReportError(severity, lineNum, idString, eUnexpectedNucResidues, err_message);
            return;
        }
    }
}


static bool s_IsPossibleNuc(unsigned char c)
{
    switch( c ) {
    case 'N':
    case 'A':
    case 'C':
    case 'G':
    case 'T':
    case 'a':
    case 'c':
    case 'g':
    case 't':
        return true;
    default:
        return false;
    }
}


size_t CFastaIdValidate::CountPossibleNucResidues(
        const string& idString)
{
    const auto first_it = rbegin(idString);
    const auto it = find_if_not(first_it, rend(idString), s_IsPossibleNuc);

    return static_cast<size_t>(distance(first_it, it));
}


void CFastaIdValidate::CheckForExcessiveProtData(
        const CSeq_id& id,
        int lineNum,
        FReportError fReportError) const
{
    const auto& idString = id.GetSeqIdString();

    // Check for Aa sequence
    if (idString.length() > kWarnNumAminoAcidCharsAtEnd) {
        const auto numAaChars = CountPossibleAminoAcids(idString);
        if (numAaChars > kWarnNumAminoAcidCharsAtEnd) {
            const string err_message =
            "Fasta Reader: sequence id ends with " +
            NStr::NumericToString(numAaChars) +
            " valid amino-acid characters. " +
            " Was the sequence accidentally placed in the definition line?";
            fReportError(eDiag_Warning, lineNum, idString, eUnexpectedAminoAcids, err_message);
        }
    }
}


size_t CFastaIdValidate::CountPossibleAminoAcids(
        const string& idString)
{
    const auto first_it = rbegin(idString);
    const auto it = find_if_not(first_it, rend(idString),
            [](char c) { return (c >= 'A' && c <= 'Z') ||
                                (c >= 'a' && c <= 'z'); });

    return static_cast<size_t>(distance(first_it, it));
}

END_SCOPE(objects)
END_NCBI_SCOPE
