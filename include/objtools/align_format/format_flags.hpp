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
 */

#ifndef OBJTOOLS_ALIGN_FORMAT___FORMAT_FLAGS_HPP
#define OBJTOOLS_ALIGN_FORMAT___FORMAT_FLAGS_HPP

// Note: move this to corelib and define properly (see blastformat equivalent)
// #define NCBI_ALIGN_FORMAT_EXPORT

//Note: The following definitions have been refactored
//      from algo/blast/blastinput/cmdline_flags.hpp

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <string>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(align_format)

/* Formatting options */

/// Argument to select formatted output type
NCBI_ALIGN_FORMAT_EXPORT extern const string kArgOutputFormat;
/// Default value for formatted output type
NCBI_ALIGN_FORMAT_EXPORT extern const int kDfltArgOutputFormat;
/// Argument to specify whether the GIs should be shown in the deflines in the
/// traditional BLAST report
NCBI_ALIGN_FORMAT_EXPORT extern const string kArgShowGIs;
/// Default value for the "show GIs" formatter option
NCBI_ALIGN_FORMAT_EXPORT extern const bool kDfltArgShowGIs;
/// Argument to specify the number of one-line descriptions to show in the
/// traditional BLAST report
NCBI_ALIGN_FORMAT_EXPORT extern const string kArgNumDescriptions;
/// Default number of one-line descriptions to display in the traditional
/// BLAST report
NCBI_ALIGN_FORMAT_EXPORT extern const size_t kDfltArgNumDescriptions;
/// Argument to specify the number of alignments to show in the traditional 
/// BLAST report
NCBI_ALIGN_FORMAT_EXPORT extern const string kArgNumAlignments;
/// Default number of alignments to display in the traditional BLAST report
NCBI_ALIGN_FORMAT_EXPORT extern const size_t kDfltArgNumAlignments;
/// Argument to specify whether to create output as HTML or not
NCBI_ALIGN_FORMAT_EXPORT extern const string kArgProduceHtml;
/// Default value which specifies whether to create output as HTML or not
NCBI_ALIGN_FORMAT_EXPORT extern const bool kDfltArgProduceHtml;

/* Formatting options: tabular/comma-separated value output formats */

/// Default value for tabular and comma-separated value output formats
NCBI_ALIGN_FORMAT_EXPORT extern string kDfltArgTabularOutputFmt;
/// Tag/keyword which is equivalent to using kDfltArgTabularOutputFmt
NCBI_ALIGN_FORMAT_EXPORT extern const string kDfltArgTabularOutputFmtTag;

NCBI_ALIGN_FORMAT_EXPORT extern const size_t kDfltLineLength;

/// Enumeration for all fields that are supported in the tabular output
enum ETabularField {
    eQuerySeqId = 0,       ///< Query Seq-id(s)
    eQueryGi,              ///< Query gi
    eQueryAccession,       ///< Query accession
    eQueryAccessionVersion,///< Query accession.version
    eQueryLength,          ///< Query sequence length
    eSubjectSeqId,         ///< Subject Seq-id(s)
    eSubjectAllSeqIds,     ///< If multiple redundant sequences, all sets
                           /// of subject Seq-ids, separated by ';'
    eSubjectGi,            ///< Subject gi
    eSubjectAllGis,        ///< All subject gis
    eSubjectAccession,     ///< Subject accession 
    eSubjAccessionVersion, ///< Subject accession.version
    eSubjectAllAccessions, ///< All subject accessions, separated by ';'
    eSubjectLength,        ///< Subject sequence length
    eQueryStart,           ///< Start of alignment in query
    eQueryEnd,             ///< End of alignment in query
    eSubjectStart,         ///< Start of alignment in subject
    eSubjectEnd,           ///< End of alignment in subject
    eQuerySeq,             ///< Aligned part of query sequence
    eSubjectSeq,           ///< Aligned part of subject sequence
    eEvalue,               ///< Expect value
    eBitScore,             ///< Bit score
    eScore,                ///< Raw score
    eAlignmentLength,      ///< Alignment length
    ePercentIdentical,     ///< Percentage of identical matches
    eNumIdentical,         ///< Number of identical matches
    eMismatches,           ///< Number of mismatches
    ePositives,            ///< Number of positive-scoring matches
    eGapOpenings,          ///< Number of gap openings
    eGaps,                 ///< Total number of gaps
    ePercentPositives,     ///< Percentage of positive-scoring matches
    eFrames,               ///< Query and subject frames separated by a '/'
    eQueryFrame,           ///< Query frame
    eSubjFrame,            ///< Subject frame
    eBTOP,                 ///< BLAST traceback operations.
    eSubjectTaxIds,		   ///< Subject Tax ID
    eSubjectSciNames,	   ///< Subject Scientific Name
    eSubjectCommonNames,   ///< Subject Common Name
    eSubjectBlastNames,	   ///< Subject Blast Name
    eSubjectSuperKingdoms, ///< Subject Super Kingdom
    eSubjectTitle,		   ///< Only the first subject defline
    eSubjectAllTitles,	   ///< All subject deflines
    eSubjectStrand,        ///< Subject Strand
    eQueryCovSubject,      ///< Query Coverage per Subject
    eQueryCovSeqalign,     ///< Query Coverage per Seqalign
    eMaxTabularField       ///< Sentinel value
};

/// Structure to store the format specification strings, their description and
/// their corresponding enumeration
struct SFormatSpec {
    /// Format specification name
    string name;
    /// A description of what the above name represents
    string description;
    /// Enumeration that corresponds to this field
    ETabularField field;

    /// Constructor
    /// @param n format specification name [in]
    /// @param d format specification description [in]
    /// @param f enumeration value [in]
    SFormatSpec(string n, string d, ETabularField f)
        : name(n), description(d), field(f) {}
};

/// Array containing the supported output formats for tabular output.
NCBI_ALIGN_FORMAT_EXPORT extern const SFormatSpec sc_FormatSpecifiers[];
/// Number of elements in the sc_FormatSpecifiers array.
NCBI_ALIGN_FORMAT_EXPORT extern const size_t kNumTabularOutputFormatSpecifiers;

/// Returns a string documenting the available format specifiers
NCBI_ALIGN_FORMAT_EXPORT string DescribeTabularOutputFormatSpecifiers(bool is_igblast=false);

enum ESAMField {
    eSAM_QueryAsRefSeq = 0,  ///< Query as reference seq
    eSAM_SubjAsRefSeq,  ///< Query as reference seq
    eSAM_NCBIFastaID         ///< Use NCBI FASTA ID

};

struct SSAMFormatSpec {
    /// Format specification name
    string name;
    /// A description of what the above name represents
    string description;
    /// Enumeration that corresponds to this field
    ESAMField field;

    /// Constructor
    /// @param n format specification name [in]
    /// @param d format specification description [in]
    /// @param f enumeration value [in]
    SSAMFormatSpec(string n, string d, ESAMField f)
        : name(n), description(d), field(f) {}
};

/// Array containing the supported output formats for SAM output.
NCBI_ALIGN_FORMAT_EXPORT extern const SSAMFormatSpec sc_SAMFormatSpecifiers[];
/// Number of elements in the sc_SAMFormatSpecifiers array.
NCBI_ALIGN_FORMAT_EXPORT extern const size_t kNumSAMOutputFormatSpecifiers;

/// Returns a string documenting the available format specifiers
NCBI_ALIGN_FORMAT_EXPORT string DescribeSAMOutputFormatSpecifiers(bool is_vdb=false);

END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___FORMAT_FLAGS_HPP */
