#ifndef FORMATGUESS__HPP
#define FORMATGUESS__HPP

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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Different "fuzzy-logic" methods to identify file formats.
 *
 */

#include <corelib/ncbistd.hpp>
#include <bitset>

BEGIN_NCBI_SCOPE

class CFormatGuessHints;


//////////////////////////////////////////////////////////////////
///
/// Class implements different ad-hoc unreliable file format
/// identifications.
///

class NCBI_XUTIL_EXPORT CFormatGuess
{
public:
    /// The formats are checked in the same order as declared here.
    enum EFormat {
        // WARNING! Never change numeric values of these enumerators!
        // E.g. these values are hard-coded in the Local Data Storage (LDS)
        // index databases.
        eUnknown             =  0, ///< unknown format
        eBinaryASN           =  1, ///< Binary ASN.1
        eRmo                 =  2, ///< RepeatMasker Output
        eGtf_POISENED        =  3, ///< Old and Dead GFF/GTF style annotations
        eGlimmer3            =  4, ///< Glimmer3 predictions
        eAgp                 =  5, ///< AGP format assembly, AgpRead
        eXml                 =  6, ///< XML
        eWiggle              =  7, ///< UCSC WIGGLE file format
        eBed                 =  8, ///< UCSC BED file format, CBedReader
        eBed15               =  9, ///< UCSC BED15 or microarray format
        eNewick              = 10, ///< Newick file
        eAlignment           = 11, ///< Text alignment
        eDistanceMatrix      = 12, ///< Distance matrix file
        eFlatFileSequence    = 13, ///< GenBank/GenPept/DDBJ/EMBL flat-file
                                   ///< sequence portion
        eFiveColFeatureTable = 14, ///< Five-column feature table
        eSnpMarkers          = 15, ///< SNP Marker flat file
        eFasta               = 16, ///< FASTA format sequence record, CFastaReader
        eTextASN             = 17, ///< Text ASN.1
        eTaxplot             = 18, ///< Taxplot file
        ePhrapAce            = 19, ///< Phrap ACE assembly file
        eTable               = 20, ///< Generic table
        eGtf                 = 21, ///< New GTF, CGtfReader
        eGff3                = 22, ///< GFF3, CGff3Reader
        eGff2                = 23, ///< GFF2, CGff2Reader, any GFF-like that doesn't fit the others
        eHgvs                = 24, ///< HGVS, CHgvsParser
        eGvf                 = 25, ///< GVF, CGvfReader
        eZip                 = 26, ///< zip compressed file
        eGZip                = 27, ///< GNU zip compressed file
        eBZip2               = 28, ///< bzip2 compressed file
        eLzo                 = 29, ///< lzo compressed file
        eSra                 = 30, ///< INSDC Sequence Read Archive file
        eBam                 = 31, ///< Binary alignment/map file
        eVcf                 = 32, ///< VCF, CVcfReader
        eUCSCRegion          = 33, ///< USCS Region file format
        /// Max value of EFormat
        eFormat_max
    };

    enum ESequenceType {
        eUndefined,
        eNucleotide,
        eProtein
    };

    enum EMode {
        eQuick,
        eThorough
    };

    enum ESTStrictness {
        eST_Lax,     ///< Implement historic behavior, risking false positives.
        eST_Default, ///< Be relatively strict, but still allow for typos.
        eST_Strict   ///< Require 100% encodability of printable non-digits.
    };

    enum EOnError {
        eDefault = 0,      ///< Return eUnknown
        eThrowOnBadSource, ///< Throw an exception if the data source (stream, file) can't be read
    };

    /// Hints for guessing formats. Two hint types can be used: preferred and
    /// disabled. Preferred are checked before any other formats. Disabled
    /// formats are not checked at all.
    class CFormatHints
    {
    public:
        typedef CFormatGuess::EFormat TFormat;

        CFormatHints(void) {}

        /// Mark the format as preferred.
        CFormatHints& AddPreferredFormat(TFormat fmt);
        /// Mark the format as disabled.
        CFormatHints& AddDisabledFormat(TFormat fmt);
        /// Disable all formats not marked as preferred
        CFormatHints& DisableAllNonpreferred(void);
        /// Remove format hint.
        void RemoveFormat(TFormat fmt);
        /// Remove all hints
        CFormatHints& Reset(void);

        /// Check if there are any hints are set at all.
        bool IsEmpty(void) const;
        /// Check if the format is listed as preferred.
        bool IsPreferred(TFormat fmt) const;
        /// Check if the format is listed as disabled.
        bool IsDisabled(TFormat fmt) const;

    private:
        typedef bitset<CFormatGuess::eFormat_max> THints;

        THints m_Preferred;
        THints m_Disabled;
    };

    /// Guess sequence type. Function calculates sequence alphabet and
    /// identifies if the source belongs to nucleotide or protein sequence
    static ESequenceType SequenceType(const char* str, unsigned length = 0,
                                      ESTStrictness strictness = eST_Default);

    static const char* GetFormatName(EFormat format);

    //  ----------------------------------------------------------------------
    //  "Stateless" interface:
    //  Useful for checking for all formats in one simple call.
    //  May go away; use object interface instead.
    //  ----------------------------------------------------------------------

    /// Guess file format
    static
    EFormat Format(const string& path, EOnError onerror = eDefault);

    /// Format prediction based on an input stream
    /// @note On completion, the function pushes whatever data it had to read
    ///       (in order to detect data format) back to the stream -- using
    ///       CStreamUtils::Stepback()
    static
    EFormat Format(CNcbiIstream& input, EOnError onerror = eDefault);


    //  ----------------------------------------------------------------------
    //  "Object" interface:
    //  Use when interested only in a limited number of formats, in excluding
    //  certain tests, a specific order in which formats are tested, ...
    //  ----------------------------------------------------------------------

    CFormatGuess();

    CFormatGuess(const string& fname);

    /// @note Data format detection methods GuessFormat() and TestFormat()
    ///       take care to push whatever data they read back to the stream
    ///       using CStreamUtils::Stepback()
    CFormatGuess(CNcbiIstream& input);

    ~CFormatGuess();

    NCBI_DEPRECATED EFormat GuessFormat(EMode);
    NCBI_DEPRECATED bool TestFormat(EFormat, EMode);

    /// @note If the instance of the class is built upon std::istream, then
    ///       on completion this function pushes whatever data it had to read
    ///       (in order to detect data format) back to the stream -- using
    ///       CStreamUtils::Stepback()
    EFormat GuessFormat(EOnError onerror = eDefault);


    /// @note If the instance of the class is built upon std::istream, then
    ///       on completion this function pushes whatever data it had to read
    ///       (in order to detect data format) back to the stream -- using
    ///       CStreamUtils::Stepback()
    bool TestFormat(EFormat, EOnError onerror = eDefault);

    /// Get format hints
    CFormatHints& GetFormatHints(void) { return m_Hints; }

protected:
    void Initialize();

    bool EnsureTestBuffer();
    bool EnsureStats();
    bool EnsureSplitLines();
	bool IsAllComment();
    bool IsAsciiText();

    bool TestFormatRepeatMasker(
        EMode );
    bool TestFormatPhrapAce(
        EMode );
    bool TestFormatGtf(
        EMode );
    bool TestFormatGvf(
		EMode );
    bool TestFormatGff3(
        EMode );
    bool TestFormatGff2(
        EMode );
    bool TestFormatGlimmer3(
        EMode );
    bool TestFormatAgp(
        EMode );
    bool TestFormatNewick(
        EMode );
    bool TestFormatXml(
        EMode );
    bool TestFormatAlignment(
        EMode );
    bool TestFormatBinaryAsn(
        EMode );
    bool TestFormatDistanceMatrix(
        EMode );
    bool TestFormatTaxplot(
        EMode );
    bool TestFormatFlatFileSequence(
        EMode );
    bool TestFormatFiveColFeatureTable(
        EMode );
    bool TestFormatTable(
        EMode );
    bool TestFormatFasta(
        EMode );
    bool TestFormatTextAsn(
        EMode );
    bool TestFormatSnpMarkers(
        EMode );
    bool TestFormatBed(
        EMode );
    bool TestFormatBed15(
        EMode );
    bool TestFormatWiggle(
        EMode );
    bool TestFormatHgvs(
        EMode );
    bool TestFormatZip(
        EMode );
    bool TestFormatGZip(
        EMode );
    bool TestFormatBZip2(
        EMode );
    bool TestFormatLzo(
        EMode );
    bool TestFormatSra(
        EMode );
    bool TestFormatBam(
        EMode mode );
    bool TestFormatVcf(
        EMode mode );

    bool IsInputRepeatMaskerWithoutHeader();
    bool IsInputRepeatMaskerWithHeader();

    static bool IsLineFlatFileSequence(
        const std::string& );
    static bool IsSampleNewick(
        const std::string& );
    static bool IsLabelNewick(
        const std::string& );
    static bool IsLineAgp(
        const std::string& );
    static bool IsLineGlimmer3(
        const std::string& );
    static bool IsLineGtf(
        const std::string& );
    static bool IsLineGvf(
		const std::string& );
	static bool IsLineGff3(
		const std::string& );
	static bool IsLineGff2(
		const std::string& );
	static bool IsLinePhrapId(
        const std::string& );
    static bool IsLineRmo(
        const std::string& );
    static bool IsAsnComment(
        const vector<string>& );
	static bool IsLineHgvs(
		const std::string& );
	
private:
    static bool x_TestInput( CNcbiIstream& input, EOnError onerror );

    bool x_TestFormat(EFormat format, EMode mode);

    // to test for a table we check each of the most common delimiter combitions,
    // ' ' ' \t' '\t' ',' '|'
    bool x_TestTableDelimiter(const string& delims);

    // data:
    static const char* const sm_FormatNames[eFormat_max];
protected:
    static int s_CheckOrder[];
    
    static const streamsize s_iTestBufferSize = 4096;

    CNcbiIstream& m_Stream;
    bool m_bOwnsStream;
    char* m_pTestBuffer;
    streamsize m_iTestDataSize;

    bool m_bStatsAreValid;
    bool m_bSplitDone;
    unsigned int m_iStatsCountData;
    unsigned int m_iStatsCountAlNumChars;
    unsigned int m_iStatsCountDnaChars;
    unsigned int m_iStatsCountAaChars;
    unsigned int m_iStatsCountBraces;
    std::list<std::string> m_TestLines;
    CFormatHints m_Hints;
};


inline CFormatGuess::CFormatHints&
CFormatGuess::CFormatHints::AddPreferredFormat(TFormat fmt)
{
    m_Disabled.reset(fmt);
    m_Preferred.set(fmt);
    return *this;
}


inline CFormatGuess::CFormatHints&
CFormatGuess::CFormatHints::AddDisabledFormat(TFormat fmt)
{
    m_Preferred.reset(fmt);
    m_Disabled.set(fmt);
    return *this;
}

inline CFormatGuess::CFormatHints&
CFormatGuess::CFormatHints::DisableAllNonpreferred(void)
{
    m_Disabled = ~m_Preferred;
    return *this;
}

inline void CFormatGuess::CFormatHints::RemoveFormat(TFormat fmt)
{
    m_Disabled.reset(fmt);
    m_Preferred.reset(fmt);
}

inline CFormatGuess::CFormatHints&
CFormatGuess::CFormatHints::Reset(void)
{
    m_Preferred.reset();
    m_Disabled.reset();
    return *this;
}

inline bool CFormatGuess::CFormatHints::IsEmpty(void) const
{
    return m_Preferred.count() == 0  &&  m_Disabled.count() == 0;
}

inline bool CFormatGuess::CFormatHints::IsPreferred(TFormat fmt) const
{
    return m_Preferred.test(fmt);
}

inline bool CFormatGuess::CFormatHints::IsDisabled(TFormat fmt) const
{
    return m_Disabled.test(fmt);
}

END_NCBI_SCOPE

#endif
