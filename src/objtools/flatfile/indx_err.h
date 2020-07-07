/* indx_err.h
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
 * File Name:  indx_err.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Defines errors.
 *
 */

#ifndef __MODULE_indx_err__
#define __MODULE_indx_err__

#define ERR_FORMAT                         1,0
#define ERR_FORMAT_NonAsciiChar            1,1
#define ERR_FORMAT_MissingEnd              1,2
#define ERR_FORMAT_MissingField            1,3
#define ERR_FORMAT_LocusLinePosition       1,4
#define ERR_FORMAT_DirSubMode              1,5
#define ERR_FORMAT_LineTypeOrder           1,6
#define ERR_FORMAT_Multiple_NI             1,8
#define ERR_FORMAT_ContigInSegset          1,9
#define ERR_FORMAT_Multiple_SV             1,10
#define ERR_FORMAT_MissingNIDLine          1,11
#define ERR_FORMAT_IncorrectNIDLine        1,12
#define ERR_FORMAT_UnexpectedEnd           1,13
#define ERR_FORMAT_XMLMissingStartTag      1,14
#define ERR_FORMAT_XMLMissingEndTag        1,15
#define ERR_FORMAT_XMLFormatError          1,16
#define ERR_FORMAT_XMLInvalidINSDInterval  1,17
#define ERR_FORMAT_IncorrectMGALine        1,18
#define ERR_FORMAT_IllegalCAGEMoltype      1,19
#define ERR_FORMAT_BadlyFormattedIDLine    1,20
#define ERR_FORMAT_InvalidIDlineMolType    1,21

#define ERR_ENTRY                          2,0
#define ERR_ENTRY_Skipped                  2,1
#define ERR_ENTRY_Begin                    2,3
#define ERR_ENTRY_InvalidLineType          2,6

#define ERR_ACCESSION                      3,0
#define ERR_ACCESSION_BadAccessNum         3,2
#define ERR_ACCESSION_NoAccessNum          3,3
#define ERR_ACCESSION_WGSProjectAccIsPri   3,7
#define ERR_ACCESSION_2ndAccPrefixMismatch 3,11
#define ERR_ACCESSION_Invalid2ndAccRange   3,12

#define ERR_LOCUS                          4,0
#define ERR_LOCUS_BadLocusName             4,7
#define ERR_LOCUS_NoLocusName              4,8

#define ERR_SEGMENT                        5,0
#define ERR_SEGMENT_BadLocusName           5,3
#define ERR_SEGMENT_IncompSeg              5,4

#define ERR_VERSION                        6,0
#define ERR_VERSION_MissingVerNum          6,1
#define ERR_VERSION_NonDigitVerNum         6,2
#define ERR_VERSION_AccessionsDontMatch    6,3
#define ERR_VERSION_BadVersionLine         6,4
#define ERR_VERSION_IncorrectGIInVersion   6,5
#define ERR_VERSION_NonDigitGI             6,6
#define ERR_VERSION_InvalidVersion         6,8

#define ERR_REFERENCE                      8,0
#define ERR_REFERENCE_IllegalDate          8,1

#define ERR_SEQID                          10,0
#define ERR_SEQID_NoSeqId                  10,1

#define ERR_ORGANISM                       11,0
#define ERR_ORGANISM_Multiple              11,1

#define ERR_INPUT                          12,0
#define ERR_INPUT_CannotReadEntry          12,1

#define ERR_TPA                            13,0
#define ERR_TPA_TpaSpansMissing            13,1

#define ERR_DATE                           14,0
#define ERR_DATE_IllegalDate               14,1

#define ERR_KEYWORD                        15,0
#define ERR_KEYWORD_InvalidTPATier         15,1
#define ERR_KEYWORD_UnexpectedTPA          15,2
#define ERR_KEYWORD_MissingTPAKeywords     15,3
#define ERR_KEYWORD_MissingTPATier         15,4
#define ERR_KEYWORD_ConflictingTPATiers    15,5
#define ERR_KEYWORD_MissingTSAKeywords     15,6
#define ERR_KEYWORD_MissingMGAKeywords     15,7
#define ERR_KEYWORD_ConflictingMGAKeywords 15,8
#define ERR_KEYWORD_MissingTLSKeywords     15,9

#define ERR_TSA                            16,0
#define ERR_TSA_TsaSpansMissing            16,1

#define ERR_QSCORE                         17,0
#define ERR_QSCORE_RedundantScores         17,1
#define ERR_QSCORE_NoSequenceRecord        17,2
#define ERR_QSCORE_NoScoreDataFound        17,3

#endif
