/* flat2err.h
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
 * File Name:  flat2err.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

#ifndef __MODULE_flat2asn__
#define __MODULE_flat2asn__

#define ERR_FORMAT                                     1,0
#define ERR_FORMAT_MissingEnd                          1,2
#define ERR_FORMAT_LineTypeOrder                       1,6
#define ERR_FORMAT_MissingSequenceData                 1,7
#define ERR_FORMAT_ContigWithSequenceData              1,8
#define ERR_FORMAT_MissingContigFeature                1,9
#define ERR_FORMAT_MissingSourceFeature                1,10
#define ERR_FORMAT_MultipleCopyright                   1,11
#define ERR_FORMAT_MissingCopyright                    1,12
#define ERR_FORMAT_MultiplePatRefs                     1,13
#define ERR_FORMAT_DuplicateCrossRef                   1,14
#define ERR_FORMAT_InvalidMolType                      1,15
#define ERR_FORMAT_Unknown                             1,16
#define ERR_FORMAT_UnexpectedData                      1,17
#define ERR_FORMAT_InvalidECNumber                     1,18
#define ERR_FORMAT_LongECNumber                        1,19
#define ERR_FORMAT_UnusualECNumber                     1,20
#define ERR_FORMAT_UnknownDetermineField               1,21
#define ERR_FORMAT_UnknownGeneField                    1,22
#define ERR_FORMAT_ExcessGeneFields                    1,23
#define ERR_FORMAT_MissingGeneName                     1,24
#define ERR_FORMAT_InvalidPDBCrossRef                  1,25
#define ERR_FORMAT_MixedPDBXrefs                       1,26
#define ERR_FORMAT_IncorrectPROJECT                    1,27
#define ERR_FORMAT_Date                                1,28
#define ERR_FORMAT_ECNumberNotPresent                  1,29
#define ERR_FORMAT_NoProteinNameCategory               1,30
#define ERR_FORMAT_MultipleRecName                     1,31
#define ERR_FORMAT_MissingRecName                      1,32
#define ERR_FORMAT_SwissProtHasSubName                 1,33
#define ERR_FORMAT_MissingFullRecName                  1,34
#define ERR_FORMAT_IncorrectDBLINK                     1,35
#define ERR_FORMAT_AssemblyGapWithoutContig            1,37
#define ERR_FORMAT_ContigVersusAssemblyGapMissmatch    1,38
#define ERR_FORMAT_WrongBioProjectPrefix               1,39
#define ERR_FORMAT_InvalidBioProjectAcc                1,40

#define ERR_DATACLASS                                  2,0
#define ERR_DATACLASS_UnKnownClass                     2,1

#define ERR_ENTRY                                      3,0
#define ERR_ENTRY_ParsingComplete                      3,2
#define ERR_ENTRY_Skipped                              3,6
#define ERR_ENTRY_Repeated                             3,7
#define ERR_ENTRY_LongSequence                         3,8
#define ERR_ENTRY_Parsed                               3,10
#define ERR_ENTRY_ParsingSetup                         3,11
#define ERR_ENTRY_GBBlock_not_Empty                    3,12
#define ERR_ENTRY_LongHTGSSequence                     3,13
#define ERR_ENTRY_NumKeywordBlk                        3,14
#define ERR_ENTRY_Dropped                              3,15
#define ERR_ENTRY_TSALacksStructuredComment            3,16
#define ERR_ENTRY_TSALacksBioProjectLink               3,17
#define ERR_ENTRY_TLSLacksStructuredComment            3,18
#define ERR_ENTRY_TLSLacksBioProjectLink               3,19

#define ERR_COMMENT                                    4,0
#define ERR_COMMENT_NCBI_gi_in                         4,1
#define ERR_COMMENT_InvalidStructuredComment           4,2
#define ERR_COMMENT_SameStructuredCommentTags          4,3
#define ERR_COMMENT_StructuredCommentLacksDelim        4,4

#define ERR_DATE                                       5,0
#define ERR_DATE_NumKeywordBlk                         5,1
#define ERR_DATE_IllegalDate                           5,2

#define ERR_QUALIFIER                                  6,0
#define ERR_QUALIFIER_MissingTerminalDoubleQuote       6,1
#define ERR_QUALIFIER_UnbalancedQuotes                 6,2
#define ERR_QUALIFIER_EmbeddedQual                     6,3
#define ERR_QUALIFIER_EmptyQual                        6,4
#define ERR_QUALIFIER_ShouldNotHaveValue               6,5
#define ERR_QUALIFIER_DbxrefIncorrect                  6,6
#define ERR_QUALIFIER_DbxrefShouldBeNumeric            6,7
#define ERR_QUALIFIER_DbxrefUnknownDBName              6,8
#define ERR_QUALIFIER_DbxrefWrongType                  6,9
#define ERR_QUALIFIER_DuplicateRemoved                 6,10
#define ERR_QUALIFIER_MultRptUnitComma                 6,11
#define ERR_QUALIFIER_IllegalCompareQualifier          6,12
#define ERR_QUALIFIER_InvalidEvidence                  6,13
#define ERR_QUALIFIER_InvalidException                 6,14
#define ERR_QUALIFIER_ObsoleteRptUnit                  6,15
#define ERR_QUALIFIER_InvalidRptUnitRange              6,16
#define ERR_QUALIFIER_InvalidPCRprimer                 6,17
#define ERR_QUALIFIER_MissingPCRprimerSeq              6,18
#define ERR_QUALIFIER_PCRprimerEmbeddedComma           6,19
#define ERR_QUALIFIER_Conflict                         6,20
#define ERR_QUALIFIER_InvalidArtificialLoc             6,21
#define ERR_QUALIFIER_MissingGapType                   6,22
#define ERR_QUALIFIER_MissingLinkageEvidence           6,23
#define ERR_QUALIFIER_InvalidGapTypeForLinkageEvidence 6,24
#define ERR_QUALIFIER_InvalidGapType                   6,25
#define ERR_QUALIFIER_InvalidLinkageEvidence           6,26
#define ERR_QUALIFIER_MultiplePseudoGeneQuals          6,27
#define ERR_QUALIFIER_InvalidPseudoGeneValue           6,28
#define ERR_QUALIFIER_OldPseudoWithPseudoGene          6,29
#define ERR_QUALIFIER_AntiCodonLacksSequence           6,30
#define ERR_QUALIFIER_UnexpectedGapTypeForHTG          6,31
#define ERR_QUALIFIER_LinkageShouldBeUnspecified       6,32
#define ERR_QUALIFIER_LinkageShouldNotBeUnspecified    6,33
#define ERR_QUALIFIER_InvalidRegulatoryClass           6,34
#define ERR_QUALIFIER_MissingRegulatoryClass           6,35
#define ERR_QUALIFIER_MultipleRegulatoryClass          6,36
#define ERR_QUALIFIER_NoNoteForOtherRegulatory         6,37
#define ERR_QUALIFIER_NoRefForCiteQual                 6,38

#define ERR_SEQUENCE                                   7,0
#define ERR_SEQUENCE_UnknownBaseHTG3                   7,1
#define ERR_SEQUENCE_SeqLenNotEq                       7,2
#define ERR_SEQUENCE_BadResidue                        7,3
#define ERR_SEQUENCE_BadData                           7,4
#define ERR_SEQUENCE_HTGWithoutGaps                    7,5
#define ERR_SEQUENCE_HTGPossibleShortGap               7,6
#define ERR_SEQUENCE_NumKeywordBlk                     7,7
#define ERR_SEQUENCE_HTGPhaseZeroHasGap                7,8
#define ERR_SEQUENCE_TooShort                          7,9
#define ERR_SEQUENCE_AllNs                             7,10
#define ERR_SEQUENCE_TooShortIsPatent                  7,11
#define ERR_SEQUENCE_HasManyComponents                 7,12
#define ERR_SEQUENCE_MultipleWGSProjects               7,13

#define ERR_SEGMENT                                    8,0
#define ERR_SEGMENT_MissSegEntry                       8,1
#define ERR_SEGMENT_DiffMolType                        8,2
#define ERR_SEGMENT_PubMatch                           8,5
#define ERR_SEGMENT_OnlyOneMember                      8,6
#define ERR_SEGMENT_Rejected                           8,7
#define ERR_SEGMENT_GPIDMissingOrNonUnique             8,8
#define ERR_SEGMENT_DBLinkMissingOrNonUnique           8,9

#define ERR_ACCESSION                                  9,0
#define ERR_ACCESSION_CannotGetDivForSecondary         9,1
#define ERR_ACCESSION_InvalidAccessNum                 9,5
#define ERR_ACCESSION_WGSWithNonWGS_Sec                9,6
#define ERR_ACCESSION_WGSMasterAsSecondary             9,7
#define ERR_ACCESSION_UnusualWGS_Secondary             9,8
#define ERR_ACCESSION_ScfldHasWGSContigSec             9,9
#define ERR_ACCESSION_WGSPrefixMismatch                9,10 

#define ERR_LOCUS                                      10,0
#define ERR_LOCUS_WrongTopology                        10,2
#define ERR_LOCUS_NonViralRNAMoltype                   10,9

#define ERR_ORGANISM                                   11,0
#define ERR_ORGANISM_NoOrganism                        11,1
#define ERR_ORGANISM_HybridOrganism                    11,2
#define ERR_ORGANISM_Unclassified                      11,3
#define ERR_ORGANISM_MissParen                         11,4
#define ERR_ORGANISM_UnknownReplace                    11,5
#define ERR_ORGANISM_NoSourceFeatMatch                 11,6
#define ERR_ORGANISM_UnclassifiedLineage               11,7
#define ERR_ORGANISM_TaxIdNotUnique                    11,10
#define ERR_ORGANISM_TaxNameNotFound                   11,11
#define ERR_ORGANISM_TaxIdNotSpecLevel                 11,12
#define ERR_ORGANISM_NewSynonym                        11,13
#define ERR_ORGANISM_NoFormalName                      11,14
#define ERR_ORGANISM_Empty                             11,15
#define ERR_ORGANISM_Diff                              11,16
#define ERR_ORGANISM_SynOrgNameNotSYNdivision          11,17
#define ERR_ORGANISM_LineageLacksMetagenome            11,18
#define ERR_ORGANISM_OrgNameLacksMetagenome            11,19

#define ERR_KEYWORD                                    12,0
#define ERR_KEYWORD_MultipleHTGPhases                  12,1
#define ERR_KEYWORD_ESTSubstring                       12,2
#define ERR_KEYWORD_STSSubstring                       12,3
#define ERR_KEYWORD_GSSSubstring                       12,4
#define ERR_KEYWORD_ConflictingKeywords                12,5
#define ERR_KEYWORD_ShouldNotBeTPA                     12,6
#define ERR_KEYWORD_MissingTPA                         12,7
#define ERR_KEYWORD_IllegalForCON                      12,8
#define ERR_KEYWORD_ShouldNotBeCAGE                    12,9
#define ERR_KEYWORD_MissingCAGE                        12,10
#define ERR_KEYWORD_NoGeneExpressionKeywords           12,11
#define ERR_KEYWORD_ENV_NoMatchingQualifier            12,12
#define ERR_KEYWORD_ShouldNotBeTSA                     12,13
#define ERR_KEYWORD_MissingTSA                         12,14
#define ERR_KEYWORD_HTGPlusENV                         12,15
#define ERR_KEYWORD_ShouldNotBeTLS                     12,16
#define ERR_KEYWORD_MissingTLS                         12,17

#define ERR_DIVISION                                   13,0
#define ERR_DIVISION_UnknownDivCode                    13,1
#define ERR_DIVISION_MappedtoEST                       13,2
#define ERR_DIVISION_MappedtoPAT                       13,3
#define ERR_DIVISION_MappedtoSTS                       13,4
#define ERR_DIVISION_Mismatch                          13,5
#define ERR_DIVISION_MissingESTKeywords                13,6
#define ERR_DIVISION_MissingSTSKeywords                13,7
#define ERR_DIVISION_MissingPatentRef                  13,8
#define ERR_DIVISION_PATHasESTKeywords                 13,9
#define ERR_DIVISION_PATHasSTSKeywords                 13,10
#define ERR_DIVISION_PATHasCDSFeature                  13,11
#define ERR_DIVISION_STSHasCDSFeature                  13,12
#define ERR_DIVISION_NotMappedtoSTS                    13,13
#define ERR_DIVISION_ESTHasSTSKeywords                 13,14
#define ERR_DIVISION_ESTHasCDSFeature                  13,15
#define ERR_DIVISION_NotMappedtoEST                    13,16
#define ERR_DIVISION_ShouldBeHTG                       13,17
#define ERR_DIVISION_MissingGSSKeywords                13,18
#define ERR_DIVISION_GSSHasCDSFeature                  13,19
#define ERR_DIVISION_NotMappedtoGSS                    13,20
#define ERR_DIVISION_MappedtoGSS                       13,21
#define ERR_DIVISION_PATHasGSSKeywords                 13,22
#define ERR_DIVISION_LongESTSequence                   13,23
#define ERR_DIVISION_LongSTSSequence                   13,24
#define ERR_DIVISION_LongGSSSequence                   13,25
#define ERR_DIVISION_GBBlockDivision                   13,26
#define ERR_DIVISION_MappedtoCON                       13,27
#define ERR_DIVISION_MissingHTGKeywords                13,28
#define ERR_DIVISION_ShouldNotBeHTG                    13,29
#define ERR_DIVISION_ConDivInSegset                    13,30
#define ERR_DIVISION_ConDivLacksContig                 13,31
#define ERR_DIVISION_MissingHTCKeyword                 13,32
#define ERR_DIVISION_InvalidHTCKeyword                 13,33
#define ERR_DIVISION_HTCWrongMolType                   13,34
#define ERR_DIVISION_ShouldBePAT                       13,35
#define ERR_DIVISION_BadTPADivcode                     13,36
#define ERR_DIVISION_ShouldBeENV                       13,37
#define ERR_DIVISION_TGNnotTransgenic                  13,38
#define ERR_DIVISION_TransgenicNotSYN_TGN              13,39
#define ERR_DIVISION_BadTSADivcode                     13,40
#define ERR_DIVISION_NotPatentedSeqId                  13,41

#define ERR_DEFINITION                                 15,0
#define ERR_DEFINITION_HTGNotInProgress                15,1
#define ERR_DEFINITION_DifferingRnaTokens              15,2
#define ERR_DEFINITION_HTGShouldBeComplete             15,3
#define ERR_DEFINITION_ShouldNotBeTPA                  15,4
#define ERR_DEFINITION_MissingTPA                      15,5
#define ERR_DEFINITION_ShouldNotBeTSA                  15,6
#define ERR_DEFINITION_MissingTSA                      15,7
#define ERR_DEFINITION_ShouldNotBeTLS                  15,8
#define ERR_DEFINITION_MissingTLS                      15,9

#define ERR_REFERENCE                                  16,0
#define ERR_REFERENCE_IllegPageRange                   16,3
#define ERR_REFERENCE_UnkRefRcToken                    16,4
#define ERR_REFERENCE_UnkRefSubType                    16,5
#define ERR_REFERENCE_IllegalFormat                    16,6
#define ERR_REFERENCE_IllegalAuthorName                16,7
#define ERR_REFERENCE_YearEquZero                      16,8
#define ERR_REFERENCE_IllegalDate                      16,9
#define ERR_REFERENCE_Patent                           16,10
#define ERR_REFERENCE_Thesis                           16,12
#define ERR_REFERENCE_Book                             16,14
#define ERR_REFERENCE_NoContactInfo                    16,15
#define ERR_REFERENCE_Illegalreference                 16,16
#define ERR_REFERENCE_Fail_to_parse                    16,17
#define ERR_REFERENCE_No_references                    16,18
#define ERR_REFERENCE_InvalidInPress                   16,22
#define ERR_REFERENCE_EtAlInAuthors                    16,24
#define ERR_REFERENCE_UnusualPageNumber                16,25
#define ERR_REFERENCE_LargePageRange                   16,26
#define ERR_REFERENCE_InvertPageRange                  16,27
#define ERR_REFERENCE_SingleTokenPageRange             16,28
#define ERR_REFERENCE_MissingBookPages                 16,29
#define ERR_REFERENCE_MissingBookAuthors               16,30
#define ERR_REFERENCE_DateCheck                        16,31
#define ERR_REFERENCE_GsdbRefDropped                   16,32
#define ERR_REFERENCE_UnusualBookFormat                16,33
#define ERR_REFERENCE_ImpendingYear                    16,34
#define ERR_REFERENCE_YearPrecedes1950                 16,35
#define ERR_REFERENCE_YearPrecedes1900                 16,36
#define ERR_REFERENCE_NumKeywordBlk                    16,37
#define ERR_REFERENCE_UnparsableLocation               16,38
#define ERR_REFERENCE_LongAuthorName                   16,39
#define ERR_REFERENCE_MissingAuthors                   16,40
#define ERR_REFERENCE_InvalidPmid                      16,41
#define ERR_REFERENCE_InvalidMuid                      16,42
#define ERR_REFERENCE_CitArtLacksPmid                  16,43
#define ERR_REFERENCE_DifferentPmids                   16,44
#define ERR_REFERENCE_MuidPmidMissMatch                16,45
#define ERR_REFERENCE_MultipleIdentifiers              16,46
#define ERR_REFERENCE_MuidIgnored                      16,47
#define ERR_REFERENCE_PmidIgnored                      16,48
#define ERR_REFERENCE_ArticleIdDiscarded               16,49
#define ERR_REFERENCE_UnusualPubStatus                 16,50

#define ERR_FEATURE                                    17,0
#define ERR_FEATURE_MultFocusedFeats                   17,1
#define ERR_FEATURE_ExpectEmptyComment                 17,6
#define ERR_FEATURE_DiscardData                        17,7
#define ERR_FEATURE_InValidEndPoint                    17,9
#define ERR_FEATURE_MissManQual                        17,10
#define ERR_FEATURE_NoFeatData                         17,11
#define ERR_FEATURE_NoFragment                         17,12
#define ERR_FEATURE_NotSeqEndPoint                     17,13
#define ERR_FEATURE_OldNonExp                          17,15
#define ERR_FEATURE_PartialNoNonTer                    17,16
#define ERR_FEATURE_Pos                                17,17
#define ERR_FEATURE_TooManyInitMet                     17,20
#define ERR_FEATURE_UnEqualEndPoint                    17,22
#define ERR_FEATURE_UnknownFeatKey                     17,23
#define ERR_FEATURE_UnknownQualSpelling                17,24
#define ERR_FEATURE_LocationParsing                    17,30
#define ERR_FEATURE_FeatureKeyReplaced                 17,32
#define ERR_FEATURE_Dropped                            17,33
#define ERR_FEATURE_UnknownDBName                      17,36
#define ERR_FEATURE_Duplicated                         17,37
#define ERR_FEATURE_NoSource                           17,38
#define ERR_FEATURE_MultipleSource                     17,39
#define ERR_FEATURE_ObsoleteFeature                    17,40
#define ERR_FEATURE_UnparsableLocation                 17,41
#define ERR_FEATURE_BadAnticodonLoc                    17,42
#define ERR_FEATURE_CDSNotFound                        17,43
#define ERR_FEATURE_CannotMapDnaLocToAALoc             17,44
#define ERR_FEATURE_BadLocation                        17,45
#define ERR_FEATURE_BadOrgRefFeatOnBackbone            17,46
#define ERR_FEATURE_DuplicateRemoved                   17,47
#define ERR_FEATURE_FourBaseAntiCodon                  17,48
#define ERR_FEATURE_StrangeAntiCodonSize               17,49
#define ERR_FEATURE_MultipleLocusTags                  17,50
#define ERR_FEATURE_InconsistentLocusTagAndGene        17,51
#define ERR_FEATURE_MultipleOperonQuals                17,52
#define ERR_FEATURE_MissingOperonQual                  17,53
#define ERR_FEATURE_OperonQualsNotUnique               17,54
#define ERR_FEATURE_InvalidOperonQual                  17,55
#define ERR_FEATURE_OperonLocationMisMatch             17,56
#define ERR_FEATURE_ObsoleteDbXref                     17,57
#define ERR_FEATURE_EmptyOldLocusTag                   17,58
#define ERR_FEATURE_RedundantOldLocusTag               17,59
#define ERR_FEATURE_OldLocusTagWithoutNew              17,60
#define ERR_FEATURE_MatchingOldNewLocusTag             17,61
#define ERR_FEATURE_InvalidGapLocation                 17,62
#define ERR_FEATURE_OverlappingGaps                    17,63
#define ERR_FEATURE_ContiguousGaps                     17,64
#define ERR_FEATURE_NsAbutGap                          17,65
#define ERR_FEATURE_AllNsBetweenGaps                   17,66
#define ERR_FEATURE_InvalidGapSequence                 17,67
#define ERR_FEATURE_RequiredQualifierMissing           17,68
#define ERR_FEATURE_IllegalEstimatedLength             17,69
#define ERR_FEATURE_GapSizeEstLengthMissMatch          17,70
#define ERR_FEATURE_UnknownGapNot100                   17,71
#define ERR_FEATURE_MoreThanOneCAGEFeat                17,72
#define ERR_FEATURE_Invalid_INIT_MET                   17,73
#define ERR_FEATURE_INIT_MET_insert                    17,74
#define ERR_FEATURE_MissingInitMet                     17,75
#define ERR_FEATURE_ncRNA_class                        17,76
#define ERR_FEATURE_InvalidSatelliteType               17,77
#define ERR_FEATURE_NoSatelliteClassOrIdentifier       17,78
#define ERR_FEATURE_PartialNoNonTerNonCons             17,79
#define ERR_FEATURE_AssemblyGapAndLegacyGap            17,80
#define ERR_FEATURE_InvalidAssemblyGapLocation         17,81
#define ERR_FEATURE_InvalidQualifier                   17,82
#define ERR_FEATURE_MultipleGenesDifferentLocusTags    17,83
#define ERR_FEATURE_InvalidAnticodonPos                17,84
#define ERR_FEATURE_InconsistentPseudogene             17,85
#define ERR_FEATURE_MultipleWBGeneXrefs                17,86
#define ERR_FEATURE_FinishedHTGHasAssemblyGap          17,87
#define ERR_FEATURE_MultipleOldLocusTags               17,88

#define ERR_LOCATION                                   18,0
#define ERR_LOCATION_FailedCheck                       18,1
#define ERR_LOCATION_MixedStrand                       18,2
#define ERR_LOCATION_PeptideFeatOutOfFrame             18,3
#define ERR_LOCATION_AccessionNotTPA                   18,4
#define ERR_LOCATION_ContigHasNull                     18,5
#define ERR_LOCATION_TransSpliceMixedStrand            18,6
#define ERR_LOCATION_AccessionNotTSA                   18,7
#define ERR_LOCATION_SeqIdProblem                      18,8
#define ERR_LOCATION_TpaAndNonTpa                      18,9
#define ERR_LOCATION_CrossDatabaseFeatLoc              18,10
#define ERR_LOCATION_RefersToExternalRecord            18,11
#define ERR_LOCATION_NCBIRefersToExternalRecord        18,12
#define ERR_LOCATION_ContigAndScaffold                 18,13
#define ERR_LOCATION_AccessionNotTLS                   18,14

#define ERR_GENENAME                                   19,0
#define ERR_GENENAME_IllegalGeneName                   19,1
#define ERR_GENENAME_DELineGeneName                    19,2

#define ERR_BIOSEQSETCLASS                             20,0
#define ERR_BIOSEQSETCLASS_NewClass                    20,1

#define ERR_CDREGION                                   21,0
#define ERR_CDREGION_InternalStopCodonFound            21,2
#define ERR_CDREGION_NoProteinSeq                      21,6
#define ERR_CDREGION_TerminalStopCodonMissing          21,7
#define ERR_CDREGION_TranslationDiff                   21,8
#define ERR_CDREGION_TranslationsAgree                 21,9
#define ERR_CDREGION_IllegalStart                      21,10
#define ERR_CDREGION_GeneticCodeDiff                   21,11
#define ERR_CDREGION_UnevenLocation                    21,12
#define ERR_CDREGION_ShortProtein                      21,13
#define ERR_CDREGION_GeneticCodeAssumed                21,14
#define ERR_CDREGION_NoTranslationCompare              21,15
#define ERR_CDREGION_TranslationAdded                  21,16
#define ERR_CDREGION_InvalidGcodeTable                 21,17
#define ERR_CDREGION_ConvertToImpFeat                  21,18
#define ERR_CDREGION_BadLocForTranslation              21,19
#define ERR_CDREGION_LocationLength                    21,20
#define ERR_CDREGION_TranslationOverride               21,21
#define ERR_CDREGION_InvalidDb_xref                    21,24
#define ERR_CDREGION_Multiple_PID                      21,28
#define ERR_CDREGION_TooBad                            21,29
#define ERR_CDREGION_MissingProteinId                  21,30
#define ERR_CDREGION_MissingProteinVersion             21,31
#define ERR_CDREGION_IncorrectProteinVersion           21,32
#define ERR_CDREGION_IncorrectProteinAccession         21,33
#define ERR_CDREGION_MissingCodonStart                 21,34
#define ERR_CDREGION_MissingTranslation                21,35
#define ERR_CDREGION_PseudoWithTranslation             21,36
#define ERR_CDREGION_UnexpectedProteinId               21,37
#define ERR_CDREGION_NCBI_gi_in                        21,38
#define ERR_CDREGION_StopCodonOnly                     21,39
#define ERR_CDREGION_StopCodonBadInterval              21,40
#define ERR_CDREGION_ProteinLenDiff                    21,41
#define ERR_CDREGION_SuppliedProteinUsed               21,42
#define ERR_CDREGION_IllegalException                  21,43
#define ERR_CDREGION_BadTermStopException              21,44
#define ERR_CDREGION_BadCodonQualFormat                21,45
#define ERR_CDREGION_InvalidCodonQual                  21,46
#define ERR_CDREGION_CodonQualifierUsed                21,47
#define ERR_CDREGION_UnneededCodonQual                 21,48

#define ERR_GENEREF                                    22,0
#define ERR_GENEREF_GeneIntervalOverlap                22,1
#define ERR_GENEREF_NoUniqMaploc                       22,2
#define ERR_GENEREF_BothStrands                        22,3
#define ERR_GENEREF_CircularHeuristicFit               22,4
#define ERR_GENEREF_CircularHeuristicDoesNotFit        22,5

#define ERR_PROTREF                                    23,0
#define ERR_PROTREF_NoNameForProtein                   23,1

#define ERR_SEQID                                      24,0
#define ERR_SEQID_NoSeqId                              24,1

#define ERR_SERVER                                     26,0
#define ERR_SERVER_NotUsed                             26,1
#define ERR_SERVER_Failed                              26,2
#define ERR_SERVER_NoLineageFromTaxon                  26,3
#define ERR_SERVER_GcFromSuppliedLineage               26,6
#define ERR_SERVER_TaxNameWasFound                     26,7
#define ERR_SERVER_TaxServerDown                       26,8
#define ERR_SERVER_NoTaxLookup                         26,9
#define ERR_SERVER_NoPubMedLookup                      26,10

#define ERR_SPROT                                      28,0
#define ERR_SPROT_DRLine                               28,1
#define ERR_SPROT_PELine                               28,2
#define ERR_SPROT_DRLineCrossDBProtein                 28,3

#define ERR_SOURCE                                     29,0
#define ERR_SOURCE_InvalidCountry                      29,1
#define ERR_SOURCE_OrganelleQualMultToks               29,2
#define ERR_SOURCE_OrganelleIllegalClass               29,3
#define ERR_SOURCE_GenomicViralRnaAssumed              29,4
#define ERR_SOURCE_UnclassifiedViralRna                29,5
#define ERR_SOURCE_LineageImpliesGenomicViralRna       29,6
#define ERR_SOURCE_InvalidDbXref                       29,7
#define ERR_SOURCE_FeatureMissing                      29,8
#define ERR_SOURCE_InvalidLocation                     29,9
#define ERR_SOURCE_BadLocation                         29,10
#define ERR_SOURCE_NoOrganismQual                      29,11
#define ERR_SOURCE_IncompleteCoverage                  29,12
#define ERR_SOURCE_ExcessSpanning                      29,13
#define ERR_SOURCE_FocusQualNotNeeded                  29,14
#define ERR_SOURCE_MultipleOrganismWithFocus           29,15
#define ERR_SOURCE_FocusQualMissing                    29,16
#define ERR_SOURCE_MultiOrgOverlap                     29,17
#define ERR_SOURCE_UnusualLocation                     29,18
#define ERR_SOURCE_OrganismIncomplete                  29,19
#define ERR_SOURCE_UnwantedQualifiers                  29,20
#define ERR_SOURCE_ManySourceFeats                     29,21
#define ERR_SOURCE_MissingSourceFeatureForDescr        29,22
#define ERR_SOURCE_FocusAndTransposonNotAllowed        29,23
#define ERR_SOURCE_FocusQualNotFullLength              29,24
#define ERR_SOURCE_UnusualOrgName                      29,25
#define ERR_SOURCE_QualUnknown                         29,26
#define ERR_SOURCE_QualDiffValues                      29,27
#define ERR_SOURCE_IllegalQual                         29,28
#define ERR_SOURCE_NotFound                            29,29
#define ERR_SOURCE_GeneticCode                         29,30
#define ERR_SOURCE_TransgenicTooShort                  29,31
#define ERR_SOURCE_FocusAndTransgenicQuals             29,32
#define ERR_SOURCE_MultipleTransgenicQuals             29,33
#define ERR_SOURCE_ExcessCoverage                      29,34
#define ERR_SOURCE_TransSingleOrgName                  29,35
#define ERR_SOURCE_PartialLocation                     29,37
#define ERR_SOURCE_PartialQualifier                    29,38
#define ERR_SOURCE_SingleSourceTooShort                29,39
#define ERR_SOURCE_InconsistentMolType                 29,40
#define ERR_SOURCE_MultipleMolTypes                    29,41
#define ERR_SOURCE_InvalidMolType                      29,42
#define ERR_SOURCE_MolTypesDisagree                    29,43
#define ERR_SOURCE_MolTypeSeqTypeConflict              29,44
#define ERR_SOURCE_MissingMolType                      29,45
#define ERR_SOURCE_UnknownOXType                       29,46
#define ERR_SOURCE_InvalidNcbiTaxID                    29,47
#define ERR_SOURCE_NoNcbiTaxIDLookup                   29,48
#define ERR_SOURCE_NcbiTaxIDLookupFailure              29,49
#define ERR_SOURCE_ConflictingGenomes                  29,50
#define ERR_SOURCE_OrgNameVsTaxIDMissMatch             29,52
#define ERR_SOURCE_InconsistentEnvSampQual             29,53
#define ERR_SOURCE_MissingEnvSampQual                  29,54
#define ERR_SOURCE_MissingPlasmidName                  29,55
#define ERR_SOURCE_UnknownOHType                       29,56
#define ERR_SOURCE_IncorrectOHLine                     29,57
#define ERR_SOURCE_HostNameVsTaxIDMissMatch            29,58
#define ERR_SOURCE_ObsoleteDbXref                      29,59
#define ERR_SOURCE_InvalidCollectionDate               29,60
#define ERR_SOURCE_FormerCountry                       29,61
#define ERR_SOURCE_MultipleSubmitterSeqids             29,62
#define ERR_SOURCE_DifferentSubmitterSeqids            29,63
#define ERR_SOURCE_LackingSubmitterSeqids              29,64
#define ERR_SOURCE_SubmitterSeqidNotAllowed            29,65
#define ERR_SOURCE_SubmitterSeqidDropped               29,66
#define ERR_SOURCE_SubmitterSeqidIgnored               29,67

#define ERR_QSCORE                                     30,0
#define ERR_QSCORE_MissingByteStore                    30,1
#define ERR_QSCORE_NonLiteralDelta                     30,2
#define ERR_QSCORE_UnknownDelta                        30,3
#define ERR_QSCORE_EmptyLiteral                        30,4
#define ERR_QSCORE_ZeroLengthLiteral                   30,5
#define ERR_QSCORE_MemAlloc                            30,6
#define ERR_QSCORE_NonZeroInGap                        30,7
#define ERR_QSCORE_InvalidArgs                         30,8
#define ERR_QSCORE_BadBioseqLen                        30,9
#define ERR_QSCORE_BadBioseqId                         30,10
#define ERR_QSCORE_BadQscoreRead                       30,11
#define ERR_QSCORE_BadDefline                          30,12
#define ERR_QSCORE_NoAccession                         30,13
#define ERR_QSCORE_NoSeqVer                            30,14
#define ERR_QSCORE_NoTitle                             30,15
#define ERR_QSCORE_BadLength                           30,16
#define ERR_QSCORE_BadMinMax                           30,17
#define ERR_QSCORE_BadScoreLine                        30,18
#define ERR_QSCORE_ScoresVsLen                         30,19
#define ERR_QSCORE_ScoresVsBspLen                      30,20
#define ERR_QSCORE_BadMax                              30,21
#define ERR_QSCORE_BadMin                              30,22
#define ERR_QSCORE_BadTitle                            30,23
#define ERR_QSCORE_OutOfScores                         30,24
#define ERR_QSCORE_NonByteGraph                        30,25
#define ERR_QSCORE_FailedToParse                       30,26
#define ERR_QSCORE_DoubleSlash                         30,27

#define ERR_TITLE                                      31,0
#define ERR_TITLE_NumKeywordBlk                        31,1

#define ERR_SUMMARY                                    32,0
#define ERR_SUMMARY_NumKeywordBlk                      32,1

#define ERR_TPA                                        33,0
#define ERR_TPA_InvalidPrimarySpan                     33,1
#define ERR_TPA_InvalidPrimarySeqId                    33,2
#define ERR_TPA_InvalidPrimaryBlock                    33,3
#define ERR_TPA_IncompleteCoverage                     33,4
#define ERR_TPA_SpanLengthDiff                         33,5
#define ERR_TPA_SpanDiffOver300bp                      33,6
#define ERR_TPA_TpaSpansMissing                        33,7

#define ERR_DRXREF                                     34,0
#define ERR_DRXREF_UnknownDBname                       34,1
#define ERR_DRXREF_InvalidBioSample                    34,2
#define ERR_DRXREF_DuplicatedBioSamples                34,3
#define ERR_DRXREF_InvalidSRA                          34,4
#define ERR_DRXREF_DuplicatedSRA                       34,5

#define ERR_TSA                                        35,0
#define ERR_TSA_InvalidPrimarySpan                     35,1
#define ERR_TSA_InvalidPrimarySeqId                    35,2
#define ERR_TSA_InvalidPrimaryBlock                    35,3
#define ERR_TSA_IncompleteCoverage                     35,4
#define ERR_TSA_SpanLengthDiff                         35,5
#define ERR_TSA_SpanDiffOver300bp                      35,6
#define ERR_TSA_UnexpectedPrimaryAccession             35,7

#define ERR_DBLINK                                     36,0
#define ERR_DBLINK_InvalidIdentifier                   36,1
#define ERR_DBLINK_DuplicateIdentifierRemoved          36,2

#endif
