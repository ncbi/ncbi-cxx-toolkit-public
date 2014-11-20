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

#include <util/format_guess.hpp>
#include <util/table_printer.hpp>

#include <serial/objistr.hpp>

#include <misc/xmlwrapp/xmlwrapp.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    typedef CTablePrinter::SEndOfCell CellEnd;

    // this is for cases where the lack of a key in a map represents
    // a programming bug.
    Uint8 find_uint8_attr_or_die(
        const xml::attributes & attrs,
        const char * key)
    {
        xml::attributes::const_iterator find_iter = attrs.find(key);
        _ASSERT(find_iter != attrs.end());
        return NStr::StringToNumeric<Uint8>(find_iter->get_value());
    }

    /// Prints start_str when constructed and end_str
    /// when destroyed.  Example usage would be
    /// to print start and end tags of XML
    class CBeginEndStrRAII
    {
    public:
        CBeginEndStrRAII(string start_str, string end_str) : m_end_str(end_str)
        {
            cout << start_str << endl;
        }

        ~CBeginEndStrRAII()
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

    CGapStatsApplication(void) ;

    virtual void Init(void);
    virtual int  Run(void);
private:
    CSeq_inst::EMol m_MolFilter;
    CGapAnalysis m_gapAnalysis;
    CGapAnalysis::TAddFlag m_fGapAddFlags;
    CFastaReader::TFlags m_fFastaFlags;

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

    void x_ReadFileOrAccn(const string & sFileOrAccn);
    void x_PrintSummaryView( 
        CGapAnalysis::ESortGapLength eSort,
        CGapAnalysis::ESortDir eSortDir);
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
    m_fGapAddFlags(
        CGapAnalysis::fAddFlag_Default |
        CGapAnalysis::fAddFlag_IncludeUnknownBases),
    m_fFastaFlags(
        CFastaReader::fParseGaps |
        CFastaReader::fLetterGaps),
    m_eOutFormat(eOutFormat_ASCIITable)
{
    SetVersion(CVersionInfo(2, 0, 0));
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
        CArgDescriptions::eString );

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
    AutoPtr<CBeginEndStrRAII> pResultBeginEndStr;
    const string & sOutFormat = args["out-format"].AsString();
    if( "ascii-table" == sOutFormat ) {
        m_eOutFormat = eOutFormat_ASCIITable;
    } else if( "xml" == sOutFormat ) {
        m_eOutFormat = eOutFormat_XML;
        // outermost XML node to hold everything.  Use AutoPtr to be
        // sure the closing tag is printed at the end.
        pResultBeginEndStr.reset(
            new CBeginEndStrRAII("<result>", "</result>"));
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
    const CArgs & args = GetArgs();\

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
        m_fFastaFlags |= CFastaReader::fAssumeNuc;
    } else if( sAssumeMol == "aa" ) {
        m_fFastaFlags |= CFastaReader::fAssumeProt;
    } else {
        // shouldn't happen
        NCBI_USER_THROW_FMT("Unsupported assume-mol: " << sAssumeMol);
    }


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
    {
        CGapAnalysis::ESortGapLength eSort = 
            CGapAnalysis::eSortGapLength_Length;
        const string sSortOn = args["sort-on"].AsString();
        if( "length" == sSortOn ) {
            eSort = CGapAnalysis::eSortGapLength_Length;
        } else if( "num_seqs" == sSortOn ) {
            eSort = CGapAnalysis::eSortGapLength_NumSeqs;
        } else if( "num_gaps" == sSortOn ) {
            eSort = CGapAnalysis::eSortGapLength_NumGaps;
        } else {
            // shouldn't happen
            NCBI_USER_THROW_FMT("Unsupported sort-on: " << sSortOn);
        }

        CGapAnalysis::ESortDir eSortDir = ( 
            args["rev-sort"] ?
            CGapAnalysis::eSortDir_Descending : 
        CGapAnalysis::eSortDir_Ascending );
        x_PrintSummaryView(eSort, eSortDir);
    }

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
        } catch(const CException & ex) {
            // malformed seq-id
            throw SOutMessage(
                sFileOrAccn,
                SOutMessage::kErrorStr,
                "BAD_ACCESSION",
                FORMAT("Invalid accession"));
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

void CGapStatsApplication::x_PrintSummaryView(
    CGapAnalysis::ESortGapLength eSort,
    CGapAnalysis::ESortDir eSortDir)
{
    // turn the data into XML, then into whatever output format is
    // appropriate
    xml::document gap_info_doc("summary");
    xml::node & gap_info_root_node = gap_info_doc.get_root_node();

    xml::node gap_len_infos_node("gap_len_infos");

    AutoPtr<CGapAnalysis::TVectorGapLengthSummary> pGapLenSummary( 
        m_gapAnalysis.GetGapLengthSummary(eSort, eSortDir) );
    ITERATE( CGapAnalysis::TVectorGapLengthSummary, 
        summary_unit_it, *pGapLenSummary ) 
    {
        const CGapAnalysis::SOneGapLengthSummary & summary_unit =
            *summary_unit_it;
        const CGapAnalysis::TGapLength gap_len =
            summary_unit.gap_length;

        xml::node one_gap_len_info("one_gap_len_info");

        xml::attributes & one_gap_len_attributes =
            one_gap_len_info.get_attributes();
        one_gap_len_attributes.insert(
            "len", NStr::NumericToString(gap_len).c_str());
        one_gap_len_attributes.insert(
            "num_seqs", NStr::NumericToString(summary_unit.num_seqs).c_str());
        one_gap_len_attributes.insert(
            "num_gaps", NStr::NumericToString(summary_unit.num_gaps).c_str());

        gap_len_infos_node.insert(one_gap_len_info);
    }
    gap_info_root_node.insert(gap_len_infos_node);

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
        vecColInfos.AddCol("Num Seqs With Len", kDigitsInUint8, 
                           CTablePrinter::eJustify_Right);
        vecColInfos.AddCol("Num Gaps with Len", kDigitsInUint8, 
                           CTablePrinter::eJustify_Right);
        CTablePrinter table_printer(vecColInfos, cout);

        ITERATE(xml::node, one_gap_len_it, gap_len_infos_node) {
            const xml::node & one_gap_len_node = *one_gap_len_it;
            const xml::attributes & one_gap_len_info =
                one_gap_len_node.get_attributes();
            const CGapAnalysis::TGapLength gap_len =
                find_uint8_attr_or_die(one_gap_len_info, "len");
            const Uint8 num_seqs =
                find_uint8_attr_or_die(one_gap_len_info, "num_seqs");
            const Uint8 num_gaps =
                find_uint8_attr_or_die(one_gap_len_info, "num_gaps");

            if( 0 == gap_len ) {
                bAnyGapOfLenZero = true;
            }
            table_printer << gap_len << CellEnd();
            table_printer << num_seqs << CellEnd();
            table_printer << num_gaps << CellEnd();
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
    const CGapAnalysis::TMapGapLengthToSeqIds & len_to_id_map =
        m_gapAnalysis.GetGapLengthSeqIds();

    // turn into XML

    xml::document gap_seqs_doc("seqs_for_gap_lens");
    xml::node & gap_seqs_root_node = gap_seqs_doc.get_root_node();

    ITERATE(CGapAnalysis::TMapGapLengthToSeqIds, map_iter, len_to_id_map) {
        const CGapAnalysis::TGapLength iGapLength = map_iter->first;

        xml::node gap_len_seq_ids("gap_len_seq_ids");
        gap_len_seq_ids.get_attributes().insert(
            "len", NStr::NumericToString(iGapLength).c_str());

        const CGapAnalysis::TSetSeqIdConstRef & seq_ids = map_iter->second;
        ITERATE( CGapAnalysis::TSetSeqIdConstRef, seq_id_it, seq_ids ) {
            gap_len_seq_ids.insert(
                xml::node("seq_id", (*seq_id_it)->AsFastaString().c_str()));
        }

        gap_seqs_root_node.insert(gap_len_seq_ids);
    }

    // output
    if( m_eOutFormat == eOutFormat_XML ) {
        // trivial since already XML
        gap_seqs_doc.save_to_stream(cout, xml::save_op_no_decl);
    } else if ( m_eOutFormat == eOutFormat_ASCIITable ) {
        // convert XML to ASCII table

        cout << "SEQ-IDS FOR EACH GAP-LENGTH:" << endl;

        ITERATE(xml::node, gap_len_node_it, gap_seqs_root_node) {
            const CGapAnalysis::TGapLength iGapLength =
                find_uint8_attr_or_die(
                    gap_len_node_it->get_attributes(), "len");
            cout << "\tSeq-ids with a gap of length "
                 << iGapLength << ':' << endl;

            ITERATE(xml::node, seq_id_node_it, *gap_len_node_it) {
                cout << "\t\t" << seq_id_node_it->get_content() << endl;
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
    AutoPtr<CHistogramBinning::TListOfBins> pListOfBins(
        m_gapAnalysis.GetGapHistogram(num_bins, eHistAlgo) );

    // convert histogram into XML
    xml::document hist_doc("histogram");
    xml::node & hist_root_node = hist_doc.get_root_node();

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

        hist_root_node.insert(bin_node);
    }

    // output
    if( m_eOutFormat == eOutFormat_XML ) {
        // trivial since already XML
        hist_doc.save_to_stream(cout, xml::save_op_no_decl);
    } else if ( m_eOutFormat == eOutFormat_ASCIITable ) {
        // convert XML to ASCII table

        const size_t kDigitsInUint8 = numeric_limits<Uint8>::digits10;
        CTablePrinter::SColInfoVec vecColInfos;
        vecColInfos.AddCol("Range", 1 + 2*kDigitsInUint8);
        vecColInfos.AddCol("Number in Range", kDigitsInUint8,
                           CTablePrinter::eJustify_Right);
        CTablePrinter table_printer(vecColInfos, cout);

        cout << "HISTOGRAM:" << endl;

        ITERATE(xml::node, bin_node_it, hist_root_node) {
            const xml::attributes & bin_node_attrs =
                bin_node_it->get_attributes();

            const Uint8 start = find_uint8_attr_or_die(
                bin_node_attrs, "start_inclusive");
            const Uint8 end = find_uint8_attr_or_die(
                bin_node_attrs, "end_inclusive");
            const Uint8 num_appearances = find_uint8_attr_or_die(
                bin_node_attrs, "num_appearances");

            table_printer << start << '-' << end << CellEnd();
            table_printer << num_appearances << CellEnd();
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

