/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>

#include "wgs_errors.hpp"
#include "wgs_utils.hpp"

USING_NCBI_SCOPE;

namespace wgsparse
{

struct SErrorSubcodes
{
    string m_error_str;
    map<int, string> m_sub_errors;
};

static map<int, SErrorSubcodes> ERROR_CODE_STR =
{
    { ERR_INPUT, { "INPUT",
        {
            { ERR_INPUT_NoInputFiles, "NoInputFiles" },
            { ERR_INPUT_NoMatchingInputData, "NoMatchingInputData" },
            { ERR_INPUT_NoInputAccession, "NoInputAccession" },
            { ERR_INPUT_NoOutputDir, "NoOutputDir" },
            { ERR_INPUT_IncorrectInputAccession, "IncorrectInputAccession" },
            { ERR_INPUT_CreateDirFail, "CreateDirFail" },
            { ERR_INPUT_FailedToLoadAsnModules, "FailedToLoadAsnModules" },
            { ERR_INPUT_BadSubmissionDate, "BadSubmissionDate" },
            { ERR_INPUT_InputFileArgsConflict, "InputFileArgsConflict" },
            { ERR_INPUT_NoInputNamesInFile, "NoInputNamesInFile" },
            { ERR_INPUT_IncorrectInputDataType, "IncorrectInputDataType" },
            { ERR_INPUT_AccsSaveWithNoAssemblyUpdate, "AccsSaveWithNoAssemblyUpdate" },
            { ERR_INPUT_CommandLineOptionsMisuse, "CommandLineOptionsMisuse" },
            { ERR_INPUT_IncorrectGenomeId, "IncorrectGenomeId" },
            { ERR_INPUT_WrongParsingMode, "WrongParsingMode" },
            { ERR_INPUT_ConflictingArguments, "ConflictingArguments" },
            { ERR_INPUT_IncorrectBioProjectId, "IncorrectBioProjectId" },
            { ERR_INPUT_BiomolNotSupported, "BiomolNotSupported" },
            { ERR_INPUT_IncorrectBiomolTypeSupplied, "IncorrectBiomolTypeSupplied" },
            { ERR_INPUT_DifferentBiomolsSupplied, "DifferentBiomolsSupplied" },
            { ERR_INPUT_UnusualBiomolValueUsed, "UnusualBiomolValueUsed" },
            { ERR_INPUT_IncorrectBioSampleId, "IncorrectBioSampleId" },
            { ERR_INPUT_NoLongerSupported, "NoLongerSupported" },
            { ERR_INPUT_DuplicatedInputFileNames, "DuplicatedInputFileNames" },
            { ERR_INPUT_InconsistentPubs, "InconsistentPubs" },
            { ERR_INPUT_MolTypeNotSupported, "MolTypeNotSupported" },
            { ERR_INPUT_IncorrectMolTypeSupplied, "IncorrectMolTypeSupplied" }
        }
    } },
    { ERR_MASTER, { "MASTER",
        {
            { ERR_MASTER_FailedToCreate, "FailedToCreate" },
            { ERR_MASTER_FileOpenFailed, "FileOpenFailed" },
            { ERR_MASTER_SeqEntryReadFailed, "SeqEntryReadFailed" },
            { ERR_MASTER_MissingComment, "MissingComment" },
            { ERR_MASTER_DifferentComments, "DifferentComments" },
            { ERR_MASTER_MissingSubmitBlock, "MissingSubmitBlock" },
            { ERR_MASTER_MissingContactInfo, "MissingContactInfo" },
            { ERR_MASTER_MissingCitSub, "MissingCitSub" },
            { ERR_MASTER_SubmitBlocksDiffer, "SubmitBlocksDiffer" },
            { ERR_MASTER_SeqSubmitReadFailed, "SeqSubmitReadFailed" },
            { ERR_MASTER_TaxLookupFailed, "TaxLookupFailed" },
            { ERR_MASTER_BioseqSetReadFailed, "BioseqSetReadFailed" },
            { ERR_MASTER_CannotGetAssemblyVersion, "CannotGetAssemblyVersion" },
            { ERR_MASTER_FailedToGetAccsRange, "FailedToGetAccsRange" },
            { ERR_MASTER_AccsRangeSaveFailed, "AccsRangeSaveFailed" },
            { ERR_MASTER_DifferentMasterComments, "DifferentMasterComments" },
            { ERR_MASTER_NoMasterBioSource, "NoMasterBioSource" },
            { ERR_MASTER_NoMasterCitSub, "NoMasterCitSub" },
            { ERR_MASTER_CannotReadFromFile, "CannotReadFromFile" },
            { ERR_MASTER_BioSourcesDiff, "BioSourcesDiff" }
        }
    } },
    { ERR_SUBMISSION, { "SUBMISSION",
        {
            { ERR_SUBMISSION_SegSetsAreNotAllowed, "SegSetsAreNotAllowed" },
            { ERR_SUBMISSION_MultBioseqsInOneSub, "MultBioseqsInOneSub" },
            { ERR_SUBMISSION_FailedToFillInByteStore, "FailedToFillInByteStore" },
            { ERR_SUBMISSION_NoPubsInSeqEntry, "NoPubsInSeqEntry" },
            { ERR_SUBMISSION_FailedToCopyPubdesc, "FailedToCopyPubdesc" },
            { ERR_SUBMISSION_MissingDbtag, "MissingDbtag" },
            { ERR_SUBMISSION_DbnamesDiffer, "DbnamesDiffer" },
            { ERR_SUBMISSION_IncorrectMolinfoTech, "IncorrectMolinfoTech" },
            { ERR_SUBMISSION_SeqAnnotInWrapperGBSet, "SeqAnnotInWrapperGBSet" },
            { ERR_SUBMISSION_FailedToCopyBioSource, "FailedToCopyBioSource" },
            { ERR_SUBMISSION_NoBioSourceInSeqEntry, "NoBioSourceInSeqEntry" },
            { ERR_SUBMISSION_DuplicatedObjectIds, "DuplicatedObjectIds" },
            { ERR_SUBMISSION_IncorrectAccession, "IncorrectAccession" },
            { ERR_SUBMISSION_TooManyAccessions, "TooManyAccessions" },
            { ERR_SUBMISSION_DuplicateAccessions, "DuplicateAccessions" },
            { ERR_SUBMISSION_MissingAccession, "MissingAccession" },
            { ERR_SUBMISSION_DifferentSeqIdTypes, "DifferentSeqIdTypes" },
            { ERR_SUBMISSION_MultGenIdsInOneBioseq, "MultGenIdsInOneBioseq" },
            { ERR_SUBMISSION_IncorrectDBName, "IncorrectDBName" },
            { ERR_SUBMISSION_NonStandardDbname, "NonStandardDbname" },
            { ERR_SUBMISSION_FailedToFixDbname, "FailedToFixDbname" },
            { ERR_SUBMISSION_MultipleBioSources, "MultipleBioSources" },
            { ERR_SUBMISSION_UpdateDateIsOld, "UpdateDateIsOld" },
            { ERR_SUBMISSION_CitSubsStayInUpdParts, "CitSubsStayInUpdParts" },
            { ERR_SUBMISSION_MultSeqIdsInScaffolds, "MultSeqIdsInScaffolds" },
            { ERR_SUBMISSION_DiffSeqIdsInScaffolds, "DiffSeqIdsInScaffolds" },
            { ERR_SUBMISSION_BadSeqIdInScaffolds, "BadSeqIdInScaffolds" },
            { ERR_SUBMISSION_IncorrectAccRange, "IncorrectAccRange" },
            { ERR_SUBMISSION_IncompleteTitle, "IncompleteTitle" },
            { ERR_SUBMISSION_CreateDateMissing, "CreateDateMissing" },
            { ERR_SUBMISSION_CreateDatesDiffer, "CreateDatesDiffer" },
            { ERR_SUBMISSION_UpdateDateMissing, "UpdateDateMissing" },
            { ERR_SUBMISSION_UpdateDatesDiffer, "UpdateDatesDiffer" },
            { ERR_SUBMISSION_FailedToRemoveStrComm, "FailedToRemoveStrComm" },
            { ERR_SUBMISSION_MissingCitSub, "MissingCitSub" },
            { ERR_SUBMISSION_DifferentCitSub, "DifferentCitSub" },
            { ERR_SUBMISSION_IncorrectMolinfoBiomol, "IncorrectMolinfoBiomol" },
            { ERR_SUBMISSION_IncorrectSeqInstMol, "IncorrectSeqInstMol" },
            { ERR_SUBMISSION_UnusualDescriptor, "UnusualDescriptor" },
            { ERR_SUBMISSION_UnexpectedDescriptor, "UnexpectedDescriptor" },
            { ERR_SUBMISSION_NonContiguousAccessions, "NonContiguousAccessions" },
            { ERR_SUBMISSION_MultipleAccPrefixes, "MultipleAccPrefixes" },
            { ERR_SUBMISSION_GeneralLocalIdsDiffer, "GeneralLocalIdsDiffer" },
            { ERR_SUBMISSION_IncorrectScfldPrefix, "IncorrectScfldPrefix" },
            { ERR_SUBMISSION_AccessionRangeIncreased, "AccessionRangeIncreased" },
            { ERR_SUBMISSION_MultipleScaffoldsRanges, "MultipleScaffoldsRanges" }
        }
    } },
    { ERR_PARSE, { "PARSE",
        {
            { ERR_PARSE_FileOpenFailed, "FileOpenFailed" },
            { ERR_PARSE_SeqSubmitReadFailed, "SeqSubmitReadFailed" },
            { ERR_PARSE_NucAccAssigned, "NucAccAssigned" },
            { ERR_PARSE_SecondariesAreNotAllowed, "SecondariesAreNotAllowed" },
            { ERR_PARSE_MultAccessions, "MultAccessions" },
            { ERR_PARSE_TaxLookupFailed, "TaxLookupFailed" },
            { ERR_PARSE_BadSeqHist, "BadSeqHist" },
            { ERR_PARSE_BioseqSetReadFailed, "BioseqSetReadFailed" },
            { ERR_PARSE_LostSeqLength, "LostSeqLength" },
            { ERR_PARSE_SeqEntryReadFailed, "SeqEntryReadFailed" },
            { ERR_PARSE_SecondariesMismatch, "SecondariesMismatch" },
            { ERR_PARSE_FoundSecondaryAccession, "FoundSecondaryAccession" }
        }
    } },
    { ERR_OUTPUT, { "OUTPUT",
        {
            { ERR_OUTPUT_WontOverrideFile, "WontOverrideFile" },
            { ERR_OUTPUT_CantOpenOutputFile, "CantOpenOutputFile" },
            { ERR_OUTPUT_CantOutputMasterBioseq, "CantOutputMasterBioseq" },
            { ERR_OUTPUT_MasterBioseqInFile, "MasterBioseqInFile" },
            { ERR_OUTPUT_ParsedSubToFileFailed, "ParsedSubToFileFailed" },
            { ERR_OUTPUT_CantOutputProcessedSub, "CantOutputProcessedSub" },
            { ERR_OUTPUT_ProcessedSubInFile, "ProcessedSubInFile" },
            { ERR_OUTPUT_CreateOutDirFailed, "CreateOutDirFailed" }
        }
    } },
    { ERR_SERVER, { "SERVER",
        {
            { ERR_SERVER_TaxServerDown, "TaxServerDown" },
            { ERR_SERVER_MedArchServerDown, "MedArchServerDown" },
            { ERR_SERVER_FailedToConnectToID, "FailedToConnectToID" },
            { ERR_SERVER_FailedToFetchMasterFromID, "FailedToFetchMasterFromID" }
        }
    } },
    { ERR_ORGANISM, { "ORGANISM",
        {
            { ERR_ORGANISM_TaxNameWasFound, "TaxNameWasFound" },
            { ERR_ORGANISM_TaxNameNotFound, "TaxNameNotFound" },
            { ERR_ORGANISM_TaxIdNotUnique, "TaxIdNotUnique" },
            { ERR_ORGANISM_TaxIdNotSpecLevel, "TaxIdNotSpecLevel" },
            { ERR_ORGANISM_UpdateOrganismDiff, "UpdateOrganismDiff" },
            { ERR_ORGANISM_MissingChromosome, "MissingChromosome" },
            { ERR_ORGANISM_EnvSampleSubSourceAdded, "EnvSampleSubSourceAdded" },
            { ERR_ORGANISM_MetagenomicSubSourceAdded, "MetagenomicSubSourceAdded" }
        }
    } },
    { ERR_SEQUENCE, { "SEQUENCE",
        {
            { ERR_SEQUENCE_BadScaffoldSequence, "BadScaffoldSequence" },
            { ERR_SEQUENCE_UnusualSeqLoc, "UnusualSeqLoc" },
            { ERR_SEQUENCE_BadScaffoldFarPointer, "BadScaffoldFarPointer" },
            { ERR_SEQUENCE_WGSsOnlyOnComposedSeq, "WGSsOnlyOnComposedSeq" }
        }
    } },
    { ERR_HTGS, { "HTGS",
        {
            { ERR_HTGS_FailedToConnectToHtgs, "FailedToConnectToHtgs" },
            { ERR_HTGS_IllegalCenterName, "IllegalCenterName" },
            { ERR_HTGS_SeqNameAlreadyInUse, "SeqNameAlreadyInUse" },
            { ERR_HTGS_ServerProblem, "ServerProblem" },
            { ERR_HTGS_FailedToGetAccessions, "FailedToGetAccessions" },
            { ERR_HTGS_FailedToAddAccession, "FailedToAddAccession" },
            { ERR_HTGS_AddedToHTGS, "AddedToHTGS" }
        }
    } },
    { ERR_GPID, { "GPID",
        {
            { ERR_GPID_Mismatch, "Mismatch" },
            { ERR_GPID_BioProject_Removed, "BioProject_Removed" },
            { ERR_GPID_NoLongerSupported, "NoLongerSupported" },
            { ERR_GPID_LegacyUserObjectFound, "LegacyUserObjectFound" }
        }
    } },
    { ERR_TPA, { "TPA",
        {
            { ERR_TPA_Keyword, "Keyword" },
            { ERR_TPA_InvalidKeyword, "InvalidKeyword" }
        }
    } },
    { ERR_BIOPROJECT, { "BIOPROJECT",
        {
            { ERR_BIOPROJECT_Mismatch, "Mismatch" },
            { ERR_BIOPROJECT_GPID_Removed, "GPID_Removed" },
            { ERR_BIOPROJECT_Missing, "Missing" }
        }
    } },
    { ERR_DBLINK, { "DBLINK",
        {
            { ERR_DBLINK_NotIdentical, "NotIdentical" },
            { ERR_DBLINK_Missing, "Missing" },
            { ERR_DBLINK_UnableToReadFromByteStore, "UnableToReadFromByteStore" },
            { ERR_DBLINK_LinkWithinMasterAbsent, "LinkWithinMasterAbsent" },
            { ERR_DBLINK_NewLinkTypeForMaster, "NewLinkTypeForMaster" },
            { ERR_DBLINK_NewLinkForMaster, "NewLinkForMaster" },
            { ERR_DBLINK_BioProjectMismatch, "BioProjectMismatch" },
            { ERR_DBLINK_GPID_Removed, "GPID_Removed" },
            { ERR_DBLINK_BioProjectMissing, "BioProjectMissing" },
            { ERR_DBLINK_BioSampleMismatch, "BioSampleMismatch" },
            { ERR_DBLINK_BioSampleMissing, "BioSampleMissing" },
            { ERR_DBLINK_SRAAccessionsMismatch, "SRAAccessionsMismatch" },
            { ERR_DBLINK_SRAAccessionsMissing, "SRAAccessionsMissing" },
            { ERR_DBLINK_MissingBioSample, "MissingBioSample" },
            { ERR_DBLINK_IncorrectBioSampleId, "IncorrectBioSampleId" }
        }
    } },
    { ERR_ACCESSION, { "ACCESSION",
        {
            { ERR_ACCESSION_ForcedAssemblyVersion, "ForcedAssemblyVersion" },
            { ERR_ACCESSION_DifferentLength, "DifferentLength" },
            { ERR_ACCESSION_NotContiguous, "NotContiguous" },
            { ERR_ACCESSION_WrongLength, "WrongLength" },
            { ERR_ACCESSION_RevertedAssemblyVersion, "RevertedAssemblyVersion" },
            { ERR_ACCESSION_AssemblyVersionSkipped, "AssemblyVersionSkipped" },
            { ERR_ACCESSION_AccessionSeqIdMismatch, "AccessionSeqIdMismatch" }
        }
    } },
    { ERR_REFERENCE, { "REFERENCE",
        {
            { ERR_REFERENCE_MasterHasMultipleCitArts, "MasterHasMultipleCitArts" },
            { ERR_REFERENCE_MasterMultiUnpublished, "MasterMultiUnpublished" },
            { ERR_REFERENCE_PubMedArticleDifferences, "PubMedArticleDifferences" },
            { ERR_REFERENCE_UnusualPubStatus, "UnusualPubStatus" }
        }
    } }
};

static string GetCodeStr(int err_code, int err_sub_code)
{
    string ret;

    const auto& err_category = ERROR_CODE_STR.find(err_code);
    if (err_category != ERROR_CODE_STR.end()) {
        ret = err_category->second.m_error_str;
        ret += '.';

        const auto& error_sub_code_str = err_category->second.m_sub_errors.find(err_sub_code);
        if (error_sub_code_str != err_category->second.m_sub_errors.end()) {
            ret += error_sub_code_str->second;
        }
        else {
            ret += ToStringLeadZeroes(err_sub_code, 3);
        }
    }
    else {
        ret = ToStringLeadZeroes(err_code, 3);
        ret += '.';
        ret += ToStringLeadZeroes(err_sub_code, 3);
    }

    return ret;
}

static CWgsParseDiagHandler s_diag_handler;
static CDiagHandler* s_old_handler;


void CWgsParseDiagHandler::Post(const SDiagMessage& mess)
{
    if (mess.m_Module == "wgsparse") {
        string sev = CNcbiDiag::SeverityName(mess.m_Severity);
        NStr::ToUpper(sev);

        CNcbiOstrstream str_os;

        str_os << "[wgsparse] " << sev << ": " << mess.m_Module << "[" << GetCodeStr(mess.m_ErrCode, mess.m_ErrSubCode) << "] " << mess.m_Buffer << '\n';

        string str = CNcbiOstrstreamToString(str_os);
        cerr.write(str.data(), str.size());
        cerr << NcbiFlush;
    }
    else {
        if (s_old_handler) {
            s_old_handler->Post(mess);
        }
    }
}

CWgsParseDiagHandler& CWgsParseDiagHandler::GetWgsParseDiagHandler()
{
    if (s_old_handler == nullptr) {
        s_old_handler = GetDiagHandler();
    }

    return s_diag_handler;
}

}
