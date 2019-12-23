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

#include "seqid_validate.hpp"
#include <objtools/readers/aln_error_reporter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

static void s_ValidateSingleID(const CSeq_id& seqId, 
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
        s_ValidateSingleID(*pSeqId, lineNum, pErrorReporter);
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


static bool s_ASCII_IsUnAmbigNuc(unsigned char c)
{
    switch( c ) {
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
        const CTempString& idString)
{
    size_t numNucChars = 0;
    for (size_t i = idString.size(); i>0; i--) {
        const auto ch = idString[i - 1];
        if (!s_ASCII_IsUnAmbigNuc(ch) && (ch != 'N')) {
            break;
        }
        ++numNucChars;
    }
    return numNucChars;
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
        const CTempString& idString) 
{
    TSeqPos numAaChars = 0;
    for (size_t i = idString.size(); i>0; i--) {
        const auto ch = idString[i - 1];
        if ( !(ch >= 'A' && ch <= 'Z')  &&
                !(ch >= 'a' && ch <= 'z') ) {
            break;
        }
            ++numAaChars;
    }
    return numAaChars;
}

END_SCOPE(objects)
END_NCBI_SCOPE
