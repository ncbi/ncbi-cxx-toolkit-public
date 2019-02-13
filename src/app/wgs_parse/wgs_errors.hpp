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

#ifndef WGS_ERRORS_H
#define WGS_ERRORS_H

#include <corelib/ncbidiag.hpp>

#define ERR_INPUT                              1
#define ERR_INPUT_NoInputFiles                 1
#define ERR_INPUT_NoMatchingInputData          2
#define ERR_INPUT_NoInputAccession             3
#define ERR_INPUT_NoOutputDir                  4
#define ERR_INPUT_IncorrectInputAccession      5
#define ERR_INPUT_CreateDirFail                6
#define ERR_INPUT_FailedToLoadAsnModules       7
#define ERR_INPUT_BadSubmissionDate            8
#define ERR_INPUT_InputFileArgsConflict        9
#define ERR_INPUT_NoInputNamesInFile           10
#define ERR_INPUT_IncorrectInputDataType       11
#define ERR_INPUT_AccsSaveWithNoAssemblyUpdate 12
#define ERR_INPUT_CommandLineOptionsMisuse     13
#define ERR_INPUT_IncorrectGenomeId            14
#define ERR_INPUT_WrongParsingMode             15
#define ERR_INPUT_ConflictingArguments         16
#define ERR_INPUT_IncorrectBioProjectId        17
#define ERR_INPUT_BiomolNotSupported           18
#define ERR_INPUT_IncorrectBiomolTypeSupplied  19
#define ERR_INPUT_DifferentBiomolsSupplied     20
#define ERR_INPUT_UnusualBiomolValueUsed       21
#define ERR_INPUT_IncorrectBioSampleId         22
#define ERR_INPUT_NoLongerSupported            23
#define ERR_INPUT_DuplicatedInputFileNames     24
#define ERR_INPUT_InconsistentPubs             25
#define ERR_INPUT_MolTypeNotSupported          26
#define ERR_INPUT_IncorrectMolTypeSupplied     27

#define ERR_MASTER                             2
#define ERR_MASTER_FailedToCreate              1
#define ERR_MASTER_FileOpenFailed              2
#define ERR_MASTER_SeqEntryReadFailed          3
#define ERR_MASTER_MissingComment              4
#define ERR_MASTER_DifferentComments           5
#define ERR_MASTER_MissingSubmitBlock          6
#define ERR_MASTER_MissingContactInfo          7
#define ERR_MASTER_MissingCitSub               8
#define ERR_MASTER_SubmitBlocksDiffer          9
#define ERR_MASTER_SeqSubmitReadFailed         10
#define ERR_MASTER_TaxLookupFailed             11
#define ERR_MASTER_BioseqSetReadFailed         12
#define ERR_MASTER_CannotGetAssemblyVersion    13
#define ERR_MASTER_FailedToGetAccsRange        14
#define ERR_MASTER_AccsRangeSaveFailed         15
#define ERR_MASTER_DifferentMasterComments     16
#define ERR_MASTER_NoMasterBioSource           17
#define ERR_MASTER_NoMasterCitSub              18
#define ERR_MASTER_CannotReadFromFile          19
#define ERR_MASTER_BioSourcesDiff              20

#define ERR_SUBMISSION                         3
#define ERR_SUBMISSION_SegSetsAreNotAllowed    1
#define ERR_SUBMISSION_MultBioseqsInOneSub     2
#define ERR_SUBMISSION_FailedToFillInByteStore 3
#define ERR_SUBMISSION_NoPubsInSeqEntry        4
#define ERR_SUBMISSION_FailedToCopyPubdesc     5
#define ERR_SUBMISSION_MissingDbtag            6
#define ERR_SUBMISSION_DbnamesDiffer           7
#define ERR_SUBMISSION_IncorrectMolinfoTech    8
#define ERR_SUBMISSION_SeqAnnotInWrapperGBSet  9
#define ERR_SUBMISSION_FailedToCopyBioSource   10
#define ERR_SUBMISSION_NoBioSourceInSeqEntry   11
#define ERR_SUBMISSION_DuplicatedObjectIds     12
#define ERR_SUBMISSION_IncorrectAccession      13
#define ERR_SUBMISSION_TooManyAccessions       14
#define ERR_SUBMISSION_DuplicateAccessions     15
#define ERR_SUBMISSION_MissingAccession        16
#define ERR_SUBMISSION_DifferentSeqIdTypes     17
#define ERR_SUBMISSION_MultGenIdsInOneBioseq   18
#define ERR_SUBMISSION_IncorrectDBName         19
#define ERR_SUBMISSION_NonStandardDbname       20
#define ERR_SUBMISSION_FailedToFixDbname       21
#define ERR_SUBMISSION_MultipleBioSources      22
#define ERR_SUBMISSION_UpdateDateIsOld         23
#define ERR_SUBMISSION_CitSubsStayInUpdParts   24
#define ERR_SUBMISSION_MultSeqIdsInScaffolds   25
#define ERR_SUBMISSION_DiffSeqIdsInScaffolds   26
#define ERR_SUBMISSION_BadSeqIdInScaffolds     27
#define ERR_SUBMISSION_IncorrectAccRange       28
#define ERR_SUBMISSION_IncompleteTitle         29
#define ERR_SUBMISSION_CreateDateMissing       30
#define ERR_SUBMISSION_CreateDatesDiffer       31
#define ERR_SUBMISSION_UpdateDateMissing       32
#define ERR_SUBMISSION_UpdateDatesDiffer       33
#define ERR_SUBMISSION_FailedToRemoveStrComm   34
#define ERR_SUBMISSION_MissingCitSub           35
#define ERR_SUBMISSION_DifferentCitSub         36
#define ERR_SUBMISSION_IncorrectMolinfoBiomol  37
#define ERR_SUBMISSION_IncorrectSeqInstMol     38
#define ERR_SUBMISSION_UnusualDescriptor       39
#define ERR_SUBMISSION_UnexpectedDescriptor    40
#define ERR_SUBMISSION_NonContiguousAccessions 41
#define ERR_SUBMISSION_MultipleAccPrefixes     42
#define ERR_SUBMISSION_GeneralLocalIdsDiffer   43
#define ERR_SUBMISSION_IncorrectScfldPrefix    44
#define ERR_SUBMISSION_AccessionRangeIncreased 45
#define ERR_SUBMISSION_MultipleScaffoldsRanges 46

#define ERR_PARSE                              4
#define ERR_PARSE_FileOpenFailed               1
#define ERR_PARSE_SeqSubmitReadFailed          2
#define ERR_PARSE_NucAccAssigned               3
#define ERR_PARSE_SecondariesAreNotAllowed     4
#define ERR_PARSE_MultAccessions               5
#define ERR_PARSE_TaxLookupFailed              6
#define ERR_PARSE_BadSeqHist                   7
#define ERR_PARSE_BioseqSetReadFailed          8
#define ERR_PARSE_LostSeqLength                9
#define ERR_PARSE_SeqEntryReadFailed           10
#define ERR_PARSE_SecondariesMismatch          11
#define ERR_PARSE_FoundSecondaryAccession      12

#define ERR_OUTPUT                             5
#define ERR_OUTPUT_WontOverrideFile            1
#define ERR_OUTPUT_CantOpenOutputFile          2
#define ERR_OUTPUT_CantOutputMasterBioseq      3
#define ERR_OUTPUT_MasterBioseqInFile          4
#define ERR_OUTPUT_ParsedSubToFileFailed       5
#define ERR_OUTPUT_CantOutputProcessedSub      6
#define ERR_OUTPUT_ProcessedSubInFile          7
#define ERR_OUTPUT_CreateOutDirFailed          8

#define ERR_SERVER                             6
#define ERR_SERVER_TaxServerDown               1
#define ERR_SERVER_MedArchServerDown           2
#define ERR_SERVER_FailedToConnectToID         3
#define ERR_SERVER_FailedToFetchMasterFromID   4

#define ERR_ORGANISM                           7
#define ERR_ORGANISM_TaxNameWasFound           1
#define ERR_ORGANISM_TaxNameNotFound           2
#define ERR_ORGANISM_TaxIdNotUnique            3
#define ERR_ORGANISM_TaxIdNotSpecLevel         4
#define ERR_ORGANISM_UpdateOrganismDiff        5
#define ERR_ORGANISM_MissingChromosome         6
#define ERR_ORGANISM_EnvSampleSubSourceAdded   7
#define ERR_ORGANISM_MetagenomicSubSourceAdded 8

#define ERR_SEQUENCE                           8
#define ERR_SEQUENCE_BadScaffoldSequence       1
#define ERR_SEQUENCE_UnusualSeqLoc             2
#define ERR_SEQUENCE_BadScaffoldFarPointer     3
#define ERR_SEQUENCE_WGSsOnlyOnComposedSeq     4

#define ERR_HTGS                               9
#define ERR_HTGS_FailedToConnectToHtgs         1
#define ERR_HTGS_IllegalCenterName             2
#define ERR_HTGS_SeqNameAlreadyInUse           3
#define ERR_HTGS_ServerProblem                 4
#define ERR_HTGS_FailedToGetAccessions         5
#define ERR_HTGS_FailedToAddAccession          6
#define ERR_HTGS_AddedToHTGS                   7

#define ERR_GPID                               10
#define ERR_GPID_Mismatch                      1
#define ERR_GPID_BioProject_Removed            2
#define ERR_GPID_NoLongerSupported             3
#define ERR_GPID_LegacyUserObjectFound         4

#define ERR_TPA                                11
#define ERR_TPA_Keyword                        1
#define ERR_TPA_InvalidKeyword                 2

#define ERR_BIOPROJECT                         12
#define ERR_BIOPROJECT_Mismatch                1
#define ERR_BIOPROJECT_GPID_Removed            2
#define ERR_BIOPROJECT_Missing                 3

#define ERR_DBLINK                             13
#define ERR_DBLINK_NotIdentical                1
#define ERR_DBLINK_Missing                     2
#define ERR_DBLINK_UnableToReadFromByteStore   3
#define ERR_DBLINK_LinkWithinMasterAbsent      4
#define ERR_DBLINK_NewLinkTypeForMaster        5
#define ERR_DBLINK_NewLinkForMaster            6
#define ERR_DBLINK_BioProjectMismatch          7
#define ERR_DBLINK_GPID_Removed                8
#define ERR_DBLINK_BioProjectMissing           9
#define ERR_DBLINK_BioSampleMismatch           10
#define ERR_DBLINK_BioSampleMissing            11
#define ERR_DBLINK_SRAAccessionsMismatch       12
#define ERR_DBLINK_SRAAccessionsMissing        13
#define ERR_DBLINK_MissingBioSample            14
#define ERR_DBLINK_IncorrectBioSampleId        15

#define ERR_ACCESSION                          14
#define ERR_ACCESSION_ForcedAssemblyVersion    1
#define ERR_ACCESSION_DifferentLength          2
#define ERR_ACCESSION_NotContiguous            3
#define ERR_ACCESSION_WrongLength              4
#define ERR_ACCESSION_RevertedAssemblyVersion  5
#define ERR_ACCESSION_AssemblyVersionSkipped   6
#define ERR_ACCESSION_AccessionSeqIdMismatch   7

#define ERR_REFERENCE                          15
#define ERR_REFERENCE_MasterHasMultipleCitArts 1
#define ERR_REFERENCE_MasterMultiUnpublished   2
#define ERR_REFERENCE_PubMedArticleDifferences 3
#define ERR_REFERENCE_UnusualPubStatus         4

namespace wgsparse
{

class CWgsParseDiagHandler : public ncbi::CDiagHandler
{
public:
    CWgsParseDiagHandler();
    ~CWgsParseDiagHandler();

    CWgsParseDiagHandler(const CWgsParseDiagHandler&) = delete;
    CWgsParseDiagHandler& operator=(const CWgsParseDiagHandler&) = delete;

    virtual void Post(const ncbi::SDiagMessage& mess);
};

}

#endif // WGS_ERRORS_H
