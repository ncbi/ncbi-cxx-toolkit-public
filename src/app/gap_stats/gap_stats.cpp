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
 * Authors:  Michael Kornbluh
 *
 * File Description:
 *   Compute gap statistics.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>

#include <algo/sequence/gap_analysis.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/readers/fasta.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/misc/sequence_util_macros.hpp>

#include <util/format_guess.hpp>
#include <util/table_printer.hpp>

#include <serial/objistr.hpp>

#include <misc/xmlwrapp/xmlwrapp.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    typedef CTablePrinter::SEndOfCell CellEnd;

    // some types used so often that we give them an abbreviation
    typedef CGapAnalysis GA;
    typedef CFastaReader FR;

    template<typename T1, typename T2>
    ostream & operator << (ostream & ostr, const pair<T1, T2> & a_pair )
    {
        ostr << '(' << a_pair.first << ", " << a_pair.second << ')';
        return ostr;
    }

    template<typename TMap>
    const typename TMap::mapped_type &
    find_attr_or_die(
        const TMap & a_map, const typename TMap::key_type & key )
    {
        typename TMap::const_iterator find_it =
            a_map.find(key);
        if( find_it == a_map.end() ) {
            NCBI_USER_THROW_FMT(
                "Could not find map key: " << key);
        }
        return find_it->second;
    }

    const CTempString 
    find_attrib_attr_or_die(
        const xml::attributes & attribs, const CTempString & key)
    {
        xml::attributes::const_iterator find_it = attribs.find(key.data());
        if( find_it == attribs.end() ) {
            NCBI_USER_THROW_FMT(
                "Could not find map key: " << key);
        }
        return find_it->get_value();
    }

    const CTempString 
    find_node_attr_or_die(
        const xml::node & node, const CTempString & key )
    {
        return find_attrib_attr_or_die(node.get_attributes(), key);
    }

    Uint8 to_uint8(const CTempString & str_of_num)
    {
        return NStr::StringToUInt8(str_of_num);
    }
    
    // translate gap types to string for ASCII output
    typedef SStaticPair<GA::EGapType, const char *> TGapTypeNameElem;
    static const TGapTypeNameElem sc_gaptypename_map[] = {
        { GA::eGapType_All,          "All Gaps"},
        { GA::eGapType_SeqGap,       "Seq Gaps"},
        { GA::eGapType_UnknownBases, "Unknown Bases Gaps"},
    };
    typedef CStaticArrayMap<
        GA::EGapType, const char *> TGapTypeNameMap;
    DEFINE_STATIC_ARRAY_MAP(
        TGapTypeNameMap, sc_gaptypename, sc_gaptypename_map);

    // we iterate through sc_gaptypename many times and we also
    // want to keep the same order each time because there are a
    // number of places that assume the same order each time.
    #define ITERATE_GAP_TYPES(iter_name) \
        ITERATE(TGapTypeNameMap, iter_name, sc_gaptypename)
    
    // map each flag value to an enum for CGapAnalysis
    struct SGapRelatedInfo {
        GA::EGapType gap_type;
        GA::TAddFlag gap_add_flag;
        FR::TFlags fasta_flag;
    };
    typedef SStaticPair<const char *, SGapRelatedInfo> TAddGapTypeElem;
    static const TAddGapTypeElem sc_addgaptypename_map[] = {
        { "all",
          { GA::eGapType_All,
            ( GA::fAddFlag_IncludeSeqGaps | GA::fAddFlag_IncludeUnknownBases),
            ( FR::fParseGaps | FR::fLetterGaps) } },
        { "seq-gaps",
          { GA::eGapType_SeqGap,
            GA::fAddFlag_IncludeSeqGaps, FR::fParseGaps } },
        { "unknown-bases",
          { GA::eGapType_UnknownBases,
            GA::fAddFlag_IncludeUnknownBases, FR::fLetterGaps } },
    };
    typedef CStaticArrayMap<
        const char *, SGapRelatedInfo, PCase_CStr> TAddGapTypeMap;
    DEFINE_STATIC_ARRAY_MAP(
        TAddGapTypeMap, sc_addgaptypename, sc_addgaptypename_map);
    
    /// Prints start_str when constructed and end_str
    /// when destroyed.  Example usage would be
    /// to print start and end tags of XML
    class CBeginEndStrToCoutRAII
    {
    public:
        CBeginEndStrToCoutRAII(
            const string & start_str, const string & end_str)
            : m_end_str(end_str)
        {
            cout << start_str << endl;
        }

        ~CBeginEndStrToCoutRAII()
        {
            cout << m_end_str << endl;
        }

    private:
        const string m_end_str;
    };

    /// Holds information about an issue that occurred that
    /// we need to output as a message.
    ///
    /// Normally this would be printed via WriteAsXML
    /// or WriteAsText, but
    /// this can be thrown to indicate that we're giving
    /// up on a given file or accn.
    struct SOutMessage : public std::runtime_error {
    public:
        const static string kErrorStr;
        const static string kFatalStr;

        SOutMessage(
            const string & file_or_accn,
            const string & level,
            const string & code,
            const string & text) :
                std::runtime_error(
                    x_CalcWhat(file_or_accn, level, code, text)),
                m_file_or_accn_basename(
                    x_CalcFileBaseNameOrAccn(file_or_accn)),
                m_level(level),
                m_code(code),
                m_text(text)
            { }

        // parent has no-throw dtor, so we must too
        ~SOutMessage() throw() { }

        const string m_file_or_accn_basename;
        const string m_level;
        const string m_code;
        const string m_text;

        void WriteAsXML(CNcbiOstream & out_strm) const;
        void WriteAsText(CNcbiOstream & out_strm) const;

    private:
        static string x_CalcFileBaseNameOrAccn(
            const string & file_or_accn);

        static string x_CalcWhat(
            const string & file_or_accn,
            const string & level,
            const string & code,
            const string & text);
    };

    const string SOutMessage::kErrorStr("ERROR");
    const string SOutMessage::kFatalStr("FATAL");

    void SOutMessage::WriteAsXML(CNcbiOstream & out_strm) const
    {
        xml::document expn_doc("message");
        xml::node & expn_root_node = expn_doc.get_root_node();

        xml::attributes & expn_root_attribs =
            expn_root_node.get_attributes();
        if( ! m_file_or_accn_basename.empty() ) {
            expn_root_attribs.insert(
                "file_or_accn", m_file_or_accn_basename.c_str());
        }
        expn_root_attribs.insert("severity", m_level.c_str());
        expn_root_attribs.insert("code", m_code.c_str());

        expn_root_node.set_content(m_text.c_str());

        expn_doc.save_to_stream(out_strm, xml::save_op_no_decl);
    }

    void SOutMessage::WriteAsText(CNcbiOstream & out_strm) const
    {
        out_strm << what() << endl;
    }

    string SOutMessage::x_CalcFileBaseNameOrAccn(
        const string & file_or_accn)
    {
        CFile maybe_file(file_or_accn);
        if( maybe_file.IsFile() ) {
            // if file, return basename
            return maybe_file.GetName();
        } else {
            // if accn, return as-is
            return file_or_accn;
        }
    }

    string SOutMessage::x_CalcWhat(
        const string & file_or_accn,
        const string & level,
        const string & code,
        const string & text)
    {
        // build answer here
        CNcbiOstrstream answer_str_strm;

        const string file_or_accn_basename =
            x_CalcFileBaseNameOrAccn(file_or_accn);

        answer_str_strm << level << ": [" << code << "] ";
        if( ! file_or_accn_basename.empty() ) {
            answer_str_strm << file_or_accn_basename << ": ";
        }
        answer_str_strm << text;

        return CNcbiOstrstreamToString(answer_str_strm);
    }

}

/////////////////////////////////////////////////////////////////////////////
//  CGapStatsApplication::


class CGapStatsApplication : public CNcbiApplication
{
public:

    CGapStatsApplication(void);

    virtual void Init(void);
    virtual int  Run(void);
private:
    CSeq_inst::EMol m_MolFilter;
    GA m_gapAnalysis;
    GA::ESortGapLength m_eSort;
    GA::ESortDir m_eSortDir;

    typedef set<GA::EGapType> TGapTypeCont;
    TGapTypeCont m_IncludedGapTypes;
    bool x_IncludeGapType(GA::EGapType eGapType) const;
    GA::TAddFlag m_fGapAddFlags;
    FR::TFlags m_fFastaFlags;

    enum EOutFormat {
        eOutFormat_ASCIITable = 1,
        eOutFormat_XML,
    };
    EOutFormat m_eOutFormat;

    /// "Run" will catch all exceptions and try to do something reasonable,
    /// and calls RunNoCatch where the real work happens.
    ///
    /// (This is just to avoid having to surround an entire function with
    /// a try-catch block)
    int RunNoCatch(void);

    CRef<CScope> x_GetScope(void);
    static string x_GapNameToGapXMLNodeName(const CTempString & gap_name);

    typedef vector<GA::TGapLength> TGapLengthVec;
    /// Returns a vector of all possible gap lengths we've seen
    AutoPtr<TGapLengthVec> x_CalcAllGapLens(void) const;


    /// Reads and loads into m_gapAnalysis
    void x_ReadFileOrAccn(const string & sFileOrAccn);
    void x_PrintSummaryView(void);
    void x_PrintSeqsForGapLengths(void);
    void x_PrintHistogram(Uint8 num_bins,
        CHistogramBinning::EHistAlgo eHistAlgo);
    void x_PrintOutMessage(
        const SOutMessage &out_message, CNcbiOstream & out_strm) const;
};

/////////////////////////////////////////////////////////////////////////////
// Constructor

CGapStatsApplication::CGapStatsApplication(void) :
    m_MolFilter(CSeq_inst::eMol_na),
    // default to all unless user adds the gap types manually
    m_fGapAddFlags(
        // all by default
        GA::fAddFlag_All),
    m_fFastaFlags(
        // all by default
        FR::fParseGaps |
        FR::fLetterGaps),
    m_eOutFormat(eOutFormat_ASCIITable)
{
    int build_num =
#ifdef NCBI_PRODUCTION_VER
    NCBI_PRODUCTION_VER
#else
    0
#endif
    ;

    m_IncludedGapTypes.insert(GA::eGapType_All);

    SetVersion(CVersionInfo(2,1,build_num));
}

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CGapStatsApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Gap analysis program", false);

    // Describe the expected command-line arguments

    arg_desc->SetCurrentGroup("BASIC");

    arg_desc->AddExtra(
        1, kMax_UInt,
        "The files or accessions to do gap analysis on.  "
        "ASN.1, XML, and FASTA are some of the supported file formats.",
        CArgDescriptions::eString);

    // TODO consider removing and always showing all??
    arg_desc->AddOptionalKey(
        "add-gap-type",
        "GapTypes",
        "This indicates which types of gaps we look at.  If none specified, "
        "all types will be shown.",
        CArgDescriptions::eString,
        CArgDescriptions::fAllowMultiple);
    AutoPtr<CArgAllow_Strings> add_gap_types_allow_strings(
        new CArgAllow_Strings);
    ITERATE(TAddGapTypeMap, add_gap_type_it, sc_addgaptypename) {
        const string add_gap_type_key(add_gap_type_it->first);
        add_gap_types_allow_strings->Allow(add_gap_type_key);
    }
    arg_desc->SetConstraint(
        "add-gap-type", add_gap_types_allow_strings.release());

    arg_desc->AddDefaultKey(
        "out-format", "Format",
        "Specifies how the results should be printed.",
        CArgDescriptions::eString,
        "ascii-table");
    arg_desc->SetConstraint("out-format", &(*new CArgAllow_Strings,
        "ascii-table", "xml"));

    arg_desc->AddDefaultKey("sort-on", "HowToSortResults",
        "Specify the order of the summary. length sorts on the gap length. "
        "num_seqs sorts on the number of sequences "
        "each gap length appears on. "
        "num_gaps sorts on the total number of times a gap "
        "of each gap length appears. ",
        CArgDescriptions::eString,
        "length");
    arg_desc->SetConstraint("sort-on", &(*new CArgAllow_Strings,
        "length", "num_seqs", "num_gaps"));
    
    arg_desc->AddFlag("rev-sort",
        "Set this to reverse the sorting order.");

    arg_desc->AddFlag("show-seqs-for-gap-lengths",
        "This will show the sequences that each gap size has.  It "
        "is not affected by the sorting options.");

    arg_desc->AddDefaultKey("mol", "MolTypesToLookAt",
        "Specify whether you just want to look at sequences which are "
        "nucleic acids (na), amino acids (aa), or any.",
        CArgDescriptions::eString,
        "na" );
    arg_desc->SetConstraint("mol", &(*new CArgAllow_Strings,
        "na", "aa", "any"));

    arg_desc->AddDefaultKey("assume-mol", "AssumedMolType",
        "If unable to determine mol of sequence from ASN.1, "
        "FASTA mods, etc. this specifies what mol we assume it is.",
        CArgDescriptions::eString,
        "na" );
    arg_desc->SetConstraint("assume-mol", &(*new CArgAllow_Strings,
        "na", "aa"));

    arg_desc->SetCurrentGroup("HISTOGRAM");

    arg_desc->AddFlag("show-hist",
        "Set this flag to see a histogram of gap data");

    arg_desc->AddDefaultKey("hist-bins", "NumBins",
        "Set the number of histogram bins to aim for (not guaranteed "
        "to be that exact number).  Default is 0, which means to "
        "automatically pick a reasonable number of bins",
        CArgDescriptions::eInt8,
        "0" );
    arg_desc->SetConstraint("hist-bins", 
        new CArgAllow_Int8s(0, kMax_I8) );
    arg_desc->SetDependency(
        "hist-bins", CArgDescriptions::eRequires, "show-hist" );

    arg_desc->AddDefaultKey("hist-algo", "HistogramAlgorithm",
        "Set this if you want the histogram binner to try to "
        "use a different binning algorithm.  The default should "
        "be fine for most people.",
        CArgDescriptions::eString,
        "cluster" );
    arg_desc->SetConstraint("hist-algo", &(*new CArgAllow_Strings,
        "cluster", "even_bins"));
    arg_desc->SetDependency(
        "hist-algo", CArgDescriptions::eRequires, "show-hist" );

    arg_desc->SetCurrentGroup("DEBUGGING");

    arg_desc->AddFlag("trigger-internal-error",
        "Since this program should never trigger an "
        "internal error (hopefully), this flag causes one "
        "to happen for testing purposes");

    // Setup arg.descriptions for this application
    arg_desc->SetCurrentGroup(kEmptyStr);
    SetupArgDescriptions(arg_desc.release());
}

/////////////////////////////////////////////////////////////////////////////
//  Run


int CGapStatsApplication::Run(void)
{
    // Get arguments
    const CArgs & args = GetArgs();

    // must check out-format arg _first_ because it may set up start and end
    // strings which must occur.
    AutoPtr<CBeginEndStrToCoutRAII> pResultBeginEndStr;
    const string & sOutFormat = args["out-format"].AsString();
    if( "ascii-table" == sOutFormat ) {
        m_eOutFormat = eOutFormat_ASCIITable;
    } else if( "xml" == sOutFormat ) {
        m_eOutFormat = eOutFormat_XML;
        // outermost XML node to hold everything.  Use AutoPtr to be
        // sure the closing tag is printed at the end.
        pResultBeginEndStr.reset(
            new CBeginEndStrToCoutRAII("<result>", "</result>"));
    } else {
        _TROUBLE;
    }

    // of course, this is only used if there was an exception
    AutoPtr<SOutMessage> p_out_message;

    try {
        // almost all work happens in RunNoCatch
        return RunNoCatch();
    } catch (const std::exception & ex ) {
        p_out_message.reset(
            new SOutMessage(
                kEmptyStr, SOutMessage::kFatalStr,
                "INTERNAL_ERROR", ex.what()));
    } catch(...) {
        p_out_message.reset(
            new SOutMessage(
                kEmptyStr, SOutMessage::kFatalStr,
                "INTERNAL_ERROR", "(---UNKNOWN INTERNAL ERROR TYPE---)"));
    }

    // if we're here, there was a fatal exception, which we now output
    _ASSERT(p_out_message);
    x_PrintOutMessage(*p_out_message, cout);

    // failure
    return 1;
}

int CGapStatsApplication::RunNoCatch(void)
{
    // Get arguments
    const CArgs & args = GetArgs();

    // if requested, trigger an internal error for testing
    // purposes.
    if( args["trigger-internal-error"] ) {
        throw std::runtime_error(
            "This runtime_error was specifically requested "
            "via a program parameter");
    }

    // load variables set by args
    const string & sMol = args["mol"].AsString();
    if( sMol == "na" ) {
        m_MolFilter = CSeq_inst::eMol_na;
    } else if( sMol == "aa" ) {
        m_MolFilter = CSeq_inst::eMol_aa;
    } else if( sMol == "any" ) {
        m_MolFilter = CSeq_inst::eMol_not_set;
    } else {
        // shouldn't happen
        NCBI_USER_THROW_FMT("Unsupported mol: " << sMol);
    }

    const string & sAssumeMol = args["assume-mol"].AsString();
    if( sAssumeMol == "na" ) {
        m_fFastaFlags |= FR::fAssumeNuc;
    } else if( sAssumeMol == "aa" ) {
        m_fFastaFlags |= FR::fAssumeProt;
    } else {
        // shouldn't happen
        NCBI_USER_THROW_FMT("Unsupported assume-mol: " << sAssumeMol);
    }

    // if add-gap-type's specified, use those instead of defaulting to "all"
    if( args["add-gap-type"].HasValue() ) {
        m_IncludedGapTypes.clear();
        m_fGapAddFlags = 0;
        m_fFastaFlags = 0;

        const CArgValue::TStringArray & add_gap_types_chosen =
            args["add-gap-type"].GetStringList();
        ITERATE(CArgValue::TStringArray, add_gap_type_choice_it,
                add_gap_types_chosen)
        {
            const string add_gap_type_choice = *add_gap_type_choice_it;
            const SGapRelatedInfo & gap_related_flags =
                find_attr_or_die(
                    sc_addgaptypename, add_gap_type_choice.c_str());
            m_IncludedGapTypes.insert(gap_related_flags.gap_type);
            m_fGapAddFlags |= gap_related_flags.gap_add_flag;
            m_fFastaFlags |= gap_related_flags.fasta_flag;
        }
    }
    _ASSERT( ! m_IncludedGapTypes.empty() );
    _ASSERT(m_fGapAddFlags != 0);
    _ASSERT(m_fFastaFlags != 0);

    m_eSort = GA::eSortGapLength_Length;
    const string sSortOn = args["sort-on"].AsString();
    if( "length" == sSortOn ) {
        m_eSort = GA::eSortGapLength_Length;
    } else if( "num_seqs" == sSortOn ) {
        m_eSort = GA::eSortGapLength_NumSeqs;
    } else if( "num_gaps" == sSortOn ) {
        m_eSort = GA::eSortGapLength_NumGaps;
    } else {
        // shouldn't happen
        NCBI_USER_THROW_FMT("Unsupported sort-on: " << sSortOn);
    }

    m_eSortDir = ( 
        args["rev-sort"] ?
        GA::eSortDir_Descending : 
        GA::eSortDir_Ascending );
    
    // load given data into m_gapAnalysis
    // (Note that extra-arg indexing is 1-based )
    for(size_t ii = 1; ii <= args.GetNExtra(); ++ii ) {
        const string sFileOrAccn = args[ii].AsString();
        try {
            x_ReadFileOrAccn(sFileOrAccn);
        } catch(const SOutMessage & out_message) {
            // a thrown SOutMessage indicates we give up on this file_or_accn.
            // (Note that a non-thrown SOutMessage would just be printed
            // but would not halt processing of the file_or_accn)
            x_PrintOutMessage(out_message, cout);
        } catch(...) {
            // Unexpected exceptions make us give up without processing
            // further files-or-accns.
            
            // print a message in case higher-up catch clauses are
            // unable to determine the file-or-accn under which this
            // occurred
            SOutMessage out_message(
                sFileOrAccn, SOutMessage::kFatalStr,
                "INTERNAL_ERROR", "Unexpected exception");
            x_PrintOutMessage(out_message, cout);

            throw;
        }
    }

    // summary view is always shown
    x_PrintSummaryView();

    if( args["show-seqs-for-gap-lengths"] ) {
        x_PrintSeqsForGapLengths();
    }

    if( args["show-hist"] ) {
        const Uint8 num_bins = args["hist-bins"].AsInt8();
        const string & sHistAlgo = args["hist-algo"].AsString();
        CHistogramBinning::EHistAlgo eHistAlgo =
            CHistogramBinning::eHistAlgo_Default;
        if( "cluster" == sHistAlgo ) {
            eHistAlgo = CHistogramBinning::eHistAlgo_IdentifyClusters;
        } else if( "even_bins" == sHistAlgo ) {
            eHistAlgo = CHistogramBinning::eHistAlgo_TryForSameNumDataInEachBin;
        } else {
            // shouldn't happen
            NCBI_USER_THROW_FMT("Histogram algorithm not supported yet: " 
                << sHistAlgo );
        }

        x_PrintHistogram(num_bins, eHistAlgo);
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//  x_GetScope

CRef<CScope>
CGapStatsApplication::x_GetScope(void)
{
    DEFINE_STATIC_FAST_MUTEX(s_scope_mtx);
    CFastMutexGuard guard(s_scope_mtx);

    static CRef<CScope> s_scope;
    if( ! s_scope ) {
        // set up singleton scope
        CRef<CObjectManager> pObjMgr( CObjectManager::GetInstance() );
        CGBDataLoader::RegisterInObjectManager(*pObjMgr);
        s_scope.Reset( new CScope(*pObjMgr) );
        s_scope->AddDefaults();
    }

    return s_scope;
}

string CGapStatsApplication::x_GapNameToGapXMLNodeName(
    const CTempString & gap_name)
{
    string answer = gap_name;
    NStr::ReplaceInPlace(answer, " ", "_");
    NStr::ToLower(answer);
    return answer;
}

bool
CGapStatsApplication::x_IncludeGapType(GA::EGapType eGapType) const
{
    if( m_IncludedGapTypes.find(eGapType) != m_IncludedGapTypes.end() ) {
        return true;
    } else {
        return false;
    }
}

/////////////////////////////////////////////////////////////////////////////
//  x_CalcAllGapLens

AutoPtr<CGapStatsApplication::TGapLengthVec>
CGapStatsApplication::x_CalcAllGapLens(void) const
{
    AutoPtr<TGapLengthVec> all_gap_lengths_list(new TGapLengthVec);
    {
        AutoPtr<GA::TVectorGapLengthSummary> pGapLenSummary( 
            m_gapAnalysis.GetGapLengthSummary(
                GA::eGapType_All, m_eSort, m_eSortDir) );
        ITERATE( GA::TVectorGapLengthSummary,
                 summary_unit_it, *pGapLenSummary )
        {
            all_gap_lengths_list->push_back((*summary_unit_it)->gap_length);
        }
        sort(BEGIN_COMMA_END(*all_gap_lengths_list));
        // make sure unique
        _ASSERT(
            unique(BEGIN_COMMA_END(*all_gap_lengths_list))
            == all_gap_lengths_list->end());
    }

    return all_gap_lengths_list;
}

/////////////////////////////////////////////////////////////////////////////
//  x_ReadFileOrAccn

void CGapStatsApplication::x_ReadFileOrAccn(const string & sFileOrAccn)
{
    CSeq_entry_Handle entry_h;

    // if file exists, load from that
    if( CDirEntry(sFileOrAccn).IsFile() ) {

        // auto-detect format and object type
        CNcbiIfstream in_file(sFileOrAccn.c_str(), ios::in | ios::binary );

        ESerialDataFormat eSerialDataFormat = eSerial_None;

        CRef<CSeq_entry> pSeqEntry;

        CFormatGuess format_guesser(in_file);
        CFormatGuess::EFormat eFormat = format_guesser.GuessFormat();
        switch(eFormat) {
        case CFormatGuess::eBinaryASN:
            eSerialDataFormat = eSerial_AsnBinary;
            break;
        case CFormatGuess::eTextASN:
            eSerialDataFormat = eSerial_AsnText;
            break;
        case CFormatGuess::eXml:
            eSerialDataFormat = eSerial_Xml;
            break;
        case CFormatGuess::eFasta: {
            CFastaReader fasta_reader(in_file, 
                                      m_fFastaFlags );
            pSeqEntry = fasta_reader.ReadSet();
            break;
        }
        default:

            throw SOutMessage(
                sFileOrAccn,
                SOutMessage::kErrorStr,
                "UNSUPPORTED_FORMAT",
                FORMAT("This format is not yet supported: "
                       << format_guesser.GetFormatName(eFormat)));
        }

        _ASSERT(eSerialDataFormat != eSerial_None ||
                eFormat == CFormatGuess::eFasta);
        if( ! pSeqEntry ) {
            // try to parse as Seq-submit
            in_file.seekg(0);
            CRef<CSeq_submit> pSeqSubmit( new CSeq_submit );
            try {
                in_file >> MSerial_Format(eSerialDataFormat) 
                        >> *pSeqSubmit;

                if( ! pSeqSubmit->IsEntrys() ||
                    pSeqSubmit->GetData().GetEntrys().size() != 1 ) 
                {
                    throw SOutMessage(
                        sFileOrAccn,
                        SOutMessage::kErrorStr,
                        "SEQ_SUBMIT_MULTIPLE_SEQ_ENTRIES",
                        FORMAT(
                            "Only Seq-submits with exactly "
                            "one Seq-entry "
                            "inside are supported."));
                }
                pSeqEntry = *pSeqSubmit->SetData().SetEntrys().begin();
            } catch(...) {
                // keep going and try to parse another way
            }
        } 
            
        if( ! pSeqEntry ) {
            // try to parse as CSeq_entry
            try {
                in_file.seekg(0);
                CRef<CSeq_entry> pNewSeqEntry( new CSeq_entry );
                in_file >> MSerial_Format(eSerialDataFormat) 
                        >> *pNewSeqEntry;
                pSeqEntry = pNewSeqEntry;
            } catch(...) {
                // keep going and try to parse another way
            }
        }
            
        if( ! pSeqEntry ) {
            // try to parse as CBioseq
            try {
                in_file.seekg(0);
                CRef<CBioseq> pBioseq( new CBioseq );
                in_file >> MSerial_Format(eSerialDataFormat) 
                        >> *pBioseq;
                pSeqEntry.Reset( new CSeq_entry );
                pSeqEntry->SetSeq( *pBioseq );
            } catch(...) {
            }
        } 
            
        if( ! pSeqEntry ) {
            throw SOutMessage(
                sFileOrAccn,
                SOutMessage::kErrorStr,
                "INVALID_FORMAT_OR_BAD_OBJ_TYPE",
                FORMAT("Invalid ASN.1 or unsupported object type"));
        }

        entry_h = x_GetScope()->AddTopLevelSeqEntry(*pSeqEntry);
    } else {

        // fall back on trying to load it as an accession
        CRef<CSeq_id> pSeqId;
        try {
            pSeqId.Reset( new CSeq_id(sFileOrAccn) );
        } catch(const CSeqIdException & ex) {
            // malformed seq-id
            throw SOutMessage(
                sFileOrAccn,
                SOutMessage::kErrorStr,
                "BAD_ACCESSION",
                FORMAT(ex.what()));
        }
        
        CBioseq_Handle bioseq_h = x_GetScope()->GetBioseqHandle(*pSeqId);
        if( ! bioseq_h ) {
            throw SOutMessage(
                sFileOrAccn,
                SOutMessage::kErrorStr,
                "ACCESSION_NOT_FOUND",
                FORMAT("Accession could not be found"));
        }
        entry_h = bioseq_h.GetParentEntry();
    }

    _ASSERT(entry_h);

    m_gapAnalysis.AddSeqEntryGaps(
        entry_h,
        m_MolFilter,
        CBioseq_CI::eLevel_All,
        m_fGapAddFlags);

    // conserve memory
    x_GetScope()->ResetDataAndHistory();
}

/////////////////////////////////////////////////////////////////////////////
//  x_PrintSummaryView

void CGapStatsApplication::x_PrintSummaryView(void)
{
    // turn the data into XML, then into whatever output format is
    // appropriate
    xml::document gap_info_doc("summary");
    xml::node & gap_info_root_node = gap_info_doc.get_root_node();

    xml::node gap_len_infos_node("gap_len_infos");

    // map pair of (gap-len, gap-type) to GA::SOneGapLengthSummary for
    // all gap types.
    typedef pair<GA::TGapLength, GA::EGapType>
        TGapLenTypeKey;
    typedef map<TGapLenTypeKey, CConstRef<GA::SOneGapLengthSummary> >
        TGapLenTypeToSummaryMap;
    TGapLenTypeToSummaryMap gap_len_type_to_summary_map;

    // loop loads gap_len_type_to_summary_map
    ITERATE_GAP_TYPES(gap_type_name_it) {
        const GA::EGapType eGapType = gap_type_name_it->first;

        if( ! x_IncludeGapType(eGapType) ) {
            continue;
        }

        AutoPtr<GA::TVectorGapLengthSummary> p_gap_len_summary =
            m_gapAnalysis.GetGapLengthSummary(eGapType, m_eSort, m_eSortDir);
        ITERATE(GA::TVectorGapLengthSummary, gap_summary_it,
                *p_gap_len_summary )
        {
            CConstRef<GA::SOneGapLengthSummary> p_one_summary =
                      *gap_summary_it;
            TGapLenTypeKey gap_len_type(p_one_summary->gap_length, eGapType);
            pair<TGapLenTypeToSummaryMap::iterator, bool> insert_ret =
                gap_len_type_to_summary_map.insert(
                    make_pair(
                        gap_len_type,
                        p_one_summary));
            // there shouldn't be dups
            _ASSERT( insert_ret.second );
        }
    }

    //  use eGapType_All to determine all possible gap lengths
    typedef vector<GA::TGapLength> TGapLengthVec;
    AutoPtr<TGapLengthVec> p_all_gap_lengths_list = x_CalcAllGapLens();
    // for convenience
    TGapLengthVec & all_gap_lengths_list = *p_all_gap_lengths_list;

    // each iteration creates an XML node for one gap length
    // with all the relevant info inside
    ITERATE( TGapLengthVec, gap_length_it, all_gap_lengths_list ) {

        const GA::TGapLength gap_len = *gap_length_it;
        
        xml::node one_gap_len_info("one_gap_len_info");
        xml::attributes & one_gap_len_attributes =
            one_gap_len_info.get_attributes();

        one_gap_len_attributes.insert(
            "len", NStr::NumericToString(gap_len).c_str());

        // get information about each kind of gap for the gap length
        // set by the loop above
        ITERATE_GAP_TYPES(gap_type_name_it) {
            const GA::EGapType eGapType = gap_type_name_it->first;
            const CTempString pchGapName = gap_type_name_it->second;

            if( ! x_IncludeGapType(eGapType) ) {
                continue;
            }


            // get the info for this gap type
            CConstRef<GA::SOneGapLengthSummary> p_one_summary;
            {
                TGapLenTypeToSummaryMap::const_iterator find_it =
                    gap_len_type_to_summary_map.find(
                        make_pair(gap_len, eGapType));
                if( find_it != gap_len_type_to_summary_map.end() ) {
                    p_one_summary = find_it->second;
                } else {
                    p_one_summary.Reset(
                        new GA::SOneGapLengthSummary(
                            // make sure no one uses that first arg
                            numeric_limits<GA::TGapLength>::max(),
                            0, 0));
                }
            }
            _ASSERT(p_one_summary);

            // XML for info about just this gap type
            // (convert gap name to reasonable XML node name)
            xml::node gap_type_info(
                x_GapNameToGapXMLNodeName(pchGapName).c_str());
            xml::attributes & gap_type_info_attributes =
                gap_type_info.get_attributes();
            gap_type_info_attributes.insert(
                    "num_seqs",
                    NStr::NumericToString(p_one_summary->num_seqs).c_str());
            gap_type_info_attributes.insert(
                "num_gaps",
                NStr::NumericToString(p_one_summary->num_gaps).c_str());

            one_gap_len_info.insert(gap_type_info);
        }

        gap_len_infos_node.insert(one_gap_len_info);
    }
    gap_info_root_node.insert(gap_len_infos_node);

    // output to summary cout
    if( m_eOutFormat == eOutFormat_XML ) {
        // XML case is trivial since we've already formed it
        gap_info_doc.save_to_stream(cout, xml::save_op_no_decl);
    } else if( m_eOutFormat == eOutFormat_ASCIITable) {
        // turn XML into an ASCII table
        cout << "SUMMARY:" << endl;

        bool bAnyGapOfLenZero = false;

        const size_t kDigitsInUint8 = numeric_limits<Uint8>::digits10;
        CTablePrinter::SColInfoVec vecColInfos;
        vecColInfos.AddCol("Gap Length", kDigitsInUint8, 
                           CTablePrinter::eJustify_Right);

        ITERATE_GAP_TYPES(gap_type_name_it) {
            const GA::EGapType eGapType = gap_type_name_it->first;
            string pchGapName = gap_type_name_it->second;

            if( ! x_IncludeGapType(eGapType) ) {
                continue;
            }

            vecColInfos.AddCol(
                "Num Seqs With Len For " + pchGapName, kDigitsInUint8, 
                CTablePrinter::eJustify_Right);
            vecColInfos.AddCol(
                "Num with Len for " + pchGapName, kDigitsInUint8, 
                CTablePrinter::eJustify_Right);
        }

        CTablePrinter table_printer(vecColInfos, cout);

        ITERATE(xml::node, one_gap_len_it, gap_len_infos_node) {
            const xml::node & one_gap_len_node = *one_gap_len_it;
            const xml::attributes & one_gap_len_info =
                one_gap_len_node.get_attributes();

            const GA::TGapLength gap_len =
                to_uint8(find_attrib_attr_or_die(one_gap_len_info, "len"));
            table_printer << gap_len << CellEnd();
            if( 0 == gap_len ) {
                bAnyGapOfLenZero = true;
            }

            // children of one_gap_len_info represent the info for
            // each gap type
            ITERATE( xml::node, child_it, one_gap_len_node ) {
                const xml::node & gap_len_summary_node = *child_it;
                const Uint8 num_seqs = to_uint8(
                    find_node_attr_or_die(
                        gap_len_summary_node, "num_seqs"));
                table_printer << num_seqs << CellEnd();

                const Uint8 num_gaps = to_uint8(find_node_attr_or_die(
                    gap_len_summary_node, "num_gaps"));
                table_printer << num_gaps << CellEnd();
            }
        }
        cout << endl;

        // print a note if any gaps are of length 0, which
        // means unknown
        if( bAnyGapOfLenZero ) {
            cout << "* Note: Gap of length zero means "
                 << "'completely unknown length'." << endl;
        }
    } else {
        _TROUBLE;
    }
}

/////////////////////////////////////////////////////////////////////////////
//  x_PrintSeqsForGapLengths

void CGapStatsApplication::x_PrintSeqsForGapLengths(void)
{
    AutoPtr<TGapLengthVec> p_all_gap_lengths_list = x_CalcAllGapLens();
    
    // turn into XML

    xml::document gap_seqs_doc("seqs_for_gap_lens");
    xml::node & gap_seqs_root_node = gap_seqs_doc.get_root_node();

    // each loop iteration handles one gap length (all gap types)
    ITERATE(TGapLengthVec, all_gap_lens_it, *p_all_gap_lengths_list ) {
        const GA::TGapLength gap_len = *all_gap_lens_it;

        xml::node gap_seqs_one_len_node(xml::node("gap_length_info"));
        gap_seqs_one_len_node.get_attributes().insert(
            "len", NStr::NumericToString(gap_len).c_str());

        ITERATE_GAP_TYPES(gap_type_name_it) {
            const GA::EGapType eGapType = gap_type_name_it->first;
            const char * pchGapName = gap_type_name_it->second;

            if( ! x_IncludeGapType(eGapType) ) {
                continue;
            }

            xml::node gap_seqs_one_len_and_gap_type("gap_type_seq_ids");
            gap_seqs_one_len_and_gap_type.get_attributes().insert(
                "gap_type", pchGapName);

            const GA::TMapGapLengthToSeqIds & map_len_to_seq_ids =
                m_gapAnalysis.GetGapLengthSeqIds(eGapType);

            GA::TMapGapLengthToSeqIds::const_iterator find_seq_ids_it =
                map_len_to_seq_ids.find(gap_len);

            // add a node for each seq_id for this gap type
            if( find_seq_ids_it != map_len_to_seq_ids.end() ) {
                const GA::TSetSeqIdConstRef & set_seq_id_const_ref =
                    find_seq_ids_it->second;
                ITERATE(
                    GA::TSetSeqIdConstRef, seq_id_ref_it, set_seq_id_const_ref)
                {
                    xml::node one_seq_node("seq_info");
                    one_seq_node.get_attributes().insert(
                        "seq_id", (*seq_id_ref_it)->AsFastaString().c_str());

                    gap_seqs_one_len_and_gap_type.push_back(one_seq_node);
                }
            }

            gap_seqs_one_len_node.push_back(gap_seqs_one_len_and_gap_type);
        }

        gap_seqs_root_node.push_back(gap_seqs_one_len_node);
    }

    // output
    if( m_eOutFormat == eOutFormat_XML ) {
        // TODO: give example output
        
        // trivial since already XML
        gap_seqs_doc.save_to_stream(cout, xml::save_op_no_decl);
    } else if ( m_eOutFormat == eOutFormat_ASCIITable ) {
        // convert XML to ASCII table

        // example output:
        //   SEQ-IDS FOR EACH GAP-LENGTH:
        //           Seq-ids with a gap of length 10:
        //                   Seq gaps:
        //                           lcl|scaffold17
        //                           lcl|scaffold33
        //                           lcl|scaffold35
        //                           lcl|scaffold37
        //                   Run of Unknown Bases:
        //                           lcl|scaffold40
        //                           lcl|scaffold41
        //                           lcl|scaffold43
        //                           lcl|scaffold5
        //                           lcl|scaffold6
        //           Seq-ids with a gap of length 68:
        //                   Seq gaps:
        //                           lcl|scaffold6
        //           Seq-ids with a gap of length 72:
        //                   Seq gaps:
        //                           lcl|scaffold43
        //                           lcl|scaffold88
        //                   Run of Unknown Bases:
        //                           lcl|scaffold88

        cout << "SEQ-IDS FOR EACH GAP-LENGTH:" << endl;

        ITERATE(xml::node, gap_len_node_it, gap_seqs_root_node) {
            const GA::TGapLength iGapLength = 
                to_uint8(find_node_attr_or_die(
                     *gap_len_node_it, "len"));
            cout << "\tSeq-ids with a gap of length "
                 << iGapLength << ':' << endl;

            ITERATE(xml::node, gap_type_seq_ids_it, *gap_len_node_it) {
                const CTempString pchGapName = find_node_attr_or_die(
                    *gap_type_seq_ids_it, "gap_type");
                cout << "\t\t" << pchGapName << ":" << endl;

                ITERATE(xml::node, seq_info_it, *gap_type_seq_ids_it) {
                    cout << "\t\t\t"
                         << find_node_attr_or_die(*seq_info_it, "seq_id")
                         << endl;
                }
            }
        }
        cout << endl;

    } else {
        _TROUBLE;
    }
}

/////////////////////////////////////////////////////////////////////////////
//  x_PrintHistogram

void CGapStatsApplication::x_PrintHistogram(
    Uint8 num_bins,
    CHistogramBinning::EHistAlgo eHistAlgo)
{
    // convert histograms into XML
    xml::document hist_doc("histogram_list");
    xml::node & hist_root_node = hist_doc.get_root_node();

    // build the histogram for each gap type
    ITERATE_GAP_TYPES(gap_type_it) {
        const GA::EGapType eGapType = gap_type_it->first;
        const char * pchGapName = gap_type_it->second;

        if( ! x_IncludeGapType(eGapType) ) {
            continue;
        }

        xml::node histogram_node("histogram");
        xml::attributes & histogram_node_attrs =
            histogram_node.get_attributes();
        histogram_node_attrs.insert("gap_type", pchGapName);

        AutoPtr<CHistogramBinning::TListOfBins> pListOfBins(
            m_gapAnalysis.GetGapHistogram(eGapType, num_bins, eHistAlgo));

        // load each histogram bin into the histogram_node
        ITERATE( CHistogramBinning::TListOfBins, bin_iter, *pListOfBins ) {
            const CHistogramBinning::SBin & bin = *bin_iter;

            xml::node bin_node("bin");
            xml::attributes & bin_node_attrs =
                bin_node.get_attributes();
            bin_node_attrs.insert(
                "start_inclusive",
                NStr::NumericToString(bin.first_number).c_str());
            bin_node_attrs.insert(
                "end_inclusive",
                NStr::NumericToString(bin.last_number).c_str());
            bin_node_attrs.insert(
                "num_appearances",
                NStr::NumericToString(bin.total_appearances).c_str());

            histogram_node.insert(bin_node);
        }

        hist_root_node.insert(histogram_node);
    }

    // output
    if( m_eOutFormat == eOutFormat_XML ) {
        // trivial since already XML
        hist_doc.save_to_stream(cout, xml::save_op_no_decl);
    } else if ( m_eOutFormat == eOutFormat_ASCIITable ) {
        // convert XML to ASCII table

        const size_t kDigitsInUint8 = numeric_limits<Uint8>::digits10;

        // a histogram for each gap type
        ITERATE_GAP_TYPES(gap_type_it) {
            const char * pchGapName = gap_type_it->second;

            CTablePrinter::SColInfoVec vecColInfos;
            vecColInfos.AddCol("Range", 1 + 2*kDigitsInUint8);
            vecColInfos.AddCol("Number in Range", kDigitsInUint8,
                               CTablePrinter::eJustify_Right);
            CTablePrinter table_printer(vecColInfos, cout);

            cout << "HISTOGRAM FOR " << pchGapName << ":" << endl;

            ITERATE(xml::node, bin_node_it, hist_root_node) {
                const xml::attributes & bin_node_attrs =
                    bin_node_it->get_attributes();

                const Uint8 start = to_uint8(find_attrib_attr_or_die(
                    bin_node_attrs, "start_inclusive"));
                const Uint8 end = to_uint8(find_attrib_attr_or_die(
                    bin_node_attrs, "end_inclusive"));
                const Uint8 num_appearances = to_uint8(find_attrib_attr_or_die(
                    bin_node_attrs, "num_appearances"));

                table_printer << start << '-' << end << CellEnd();
                table_printer << num_appearances << CellEnd();
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
/// x_PrintOutMessage

void CGapStatsApplication::x_PrintOutMessage(
    const SOutMessage &out_message, CNcbiOstream & out_strm) const
{
    if( m_eOutFormat == eOutFormat_XML ) {
        // yes, cout not cerr because everything should go to cout
        out_message.WriteAsXML(out_strm);
    } else if ( m_eOutFormat == eOutFormat_ASCIITable ) {
        // yes, cout not cerr because everything should go to cout
        out_message.WriteAsText(out_strm);
    } else {
        _TROUBLE;
    }
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CGapStatsApplication().AppMain(argc, argv);
}
