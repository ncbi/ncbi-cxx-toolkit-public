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

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    typedef CTablePrinter::SEndOfCell CellEnd;
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
    CRef<CScope> m_pScope;
    CGapAnalysis m_gapAnalysis;
    CFastaReader::TFlags m_fFastaFlags;

    void x_ReadFileOrAccn(const string & sFileOrAccn);
    void x_PrintSummaryView( 
        CGapAnalysis::ESortGapLength eSort,
        CGapAnalysis::ESortDir eSortDir );
    void x_PrintSeqsForGapLengths(void);
    void x_PrintHistogram(Uint8 num_bins,
        CHistogramBinning::EHistAlgo eHistAlgo);
};

/////////////////////////////////////////////////////////////////////////////
// Constructor

CGapStatsApplication::CGapStatsApplication(void) :
    m_MolFilter(CSeq_inst::eMol_na),
        m_fFastaFlags(
            CFastaReader::fParseGaps |
            CFastaReader::fLetterGaps)
{
    CRef<CObjectManager> pObjMgr( CObjectManager::GetInstance() );
    CGBDataLoader::RegisterInObjectManager(*pObjMgr);
    m_pScope.Reset( new CScope(*pObjMgr) );
    m_pScope->AddDefaults();
}

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CGapStatsApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Gap analysis program");

    // Describe the expected command-line arguments

    arg_desc->SetCurrentGroup("BASIC");

    arg_desc->AddExtra(
        1, kMax_UInt,
        "The files or accessions to do gap analysis on.  "
        "ASN.1, XML, and FASTA are some of the supported file formats.",
        CArgDescriptions::eString );

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

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

/////////////////////////////////////////////////////////////////////////////
//  Run


int CGapStatsApplication::Run(void)
{
    // Get arguments
    const CArgs & args = GetArgs();

    // load variables set by args
    const string & sMol = args["mol"].AsString();
    if( sMol == "na" ) {
        m_MolFilter = CSeq_inst::eMol_na;
    } else if( sMol == "aa" ) {
        m_MolFilter = CSeq_inst::eMol_aa;
    } else if( sMol == "any" ) {
        m_MolFilter = CSeq_inst::eMol_not_set;
    } else {
        NCBI_USER_THROW_FMT("Unsupported mol: " << sMol);
    }

    const string & sAssumeMol = args["assume-mol"].AsString();
    if( sAssumeMol == "na" ) {
        m_fFastaFlags |= CFastaReader::fAssumeNuc;
    } else if( sAssumeMol == "aa" ) {
        m_fFastaFlags |= CFastaReader::fAssumeProt;
    } else {
        NCBI_USER_THROW_FMT("Unsupported assume-mol: " << sAssumeMol);
    }

    // load given data into m_gapAnalysis
    // (Note that extra-arg indexing is 1-based )
    for(size_t ii = 1; ii <= args.GetNExtra(); ++ii ) {
        x_ReadFileOrAccn(args[ii].AsString());
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
            NCBI_USER_THROW_FMT("Histogram algorithm not supported yet: " 
                << sHistAlgo );
        }

        x_PrintHistogram(num_bins, eHistAlgo);
    }

    return 0;
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
            NCBI_USER_THROW_FMT("This format is not yet supported: " 
                << format_guesser.GetFormatName(eFormat) );
        }

        if( eSerialDataFormat != eSerial_None ) {
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
                        NCBI_USER_THROW(
                            "Only Seq-submits with exactly one Seq-entry "
                            "inside are supported.");
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
                NCBI_USER_THROW_FMT("Unsupported object type for: " 
                    << sFileOrAccn);
            }
        }

        entry_h = m_pScope->AddTopLevelSeqEntry(*pSeqEntry);
    } else {
        // fall back on trying to load it as an accession
        CRef<CSeq_id> pSeqId( new CSeq_id(sFileOrAccn) );
        CBioseq_Handle bioseq_h = m_pScope->GetBioseqHandle(*pSeqId);
        entry_h = bioseq_h.GetParentEntry();
    }

    if( entry_h ) {

        m_gapAnalysis.AddSeqEntryGaps(entry_h, m_MolFilter);

        // conserve memory
        m_pScope->ResetDataAndHistory();
    }
}

/////////////////////////////////////////////////////////////////////////////
//  x_PrintSummaryView

void CGapStatsApplication::x_PrintSummaryView(
    CGapAnalysis::ESortGapLength eSort,
    CGapAnalysis::ESortDir eSortDir)
{


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

    AutoPtr<CGapAnalysis::TVectorGapLengthSummary> pGapLenSummary( 
        m_gapAnalysis.GetGapLengthSummary(eSort, eSortDir) );
    ITERATE( CGapAnalysis::TVectorGapLengthSummary, 
        summary_unit_it, *pGapLenSummary ) 
    {
        const CGapAnalysis::SOneGapLengthSummary & summary_unit =
            *summary_unit_it;
        const CGapAnalysis::TGapLength gap_len =
            summary_unit.gap_length;
        if( 0 == gap_len ) {
            bAnyGapOfLenZero = true;
        }
        table_printer << gap_len << CellEnd();
        table_printer << summary_unit.num_seqs << CellEnd();
        table_printer << summary_unit.num_gaps << CellEnd();
    }
    cout << endl;

    // print a note if any gaps are of length 0, which
    // means unknown
    if( bAnyGapOfLenZero ) {
        cout << "* Note: Gap of length zero means 'completely unknown length'." << endl;
    }
}

/////////////////////////////////////////////////////////////////////////////
//  x_PrintSeqsForGapLengths

void CGapStatsApplication::x_PrintSeqsForGapLengths(void)
{
    const CGapAnalysis::TMapGapLengthToSeqIds & len_to_id_map =
        m_gapAnalysis.GetGapLengthSeqIds();

    cout << "SEQ-IDS FOR EACH GAP-LENGTH:" << endl;
    ITERATE(CGapAnalysis::TMapGapLengthToSeqIds, map_iter, len_to_id_map) {
        const CGapAnalysis::TGapLength iGapLength = map_iter->first;
        cout << "\tSeq-ids with a gap of length " << iGapLength << ':' << endl;
        const CGapAnalysis::TSetSeqIdConstRef & seq_ids = map_iter->second;
        ITERATE( CGapAnalysis::TSetSeqIdConstRef, seq_id_it, seq_ids ) {
            cout << "\t\t" << (*seq_id_it)->AsFastaString() << endl;
        }
    }
    cout << endl;
}

/////////////////////////////////////////////////////////////////////////////
//  x_PrintSeqsForGapLengths

void CGapStatsApplication::x_PrintHistogram(
    Uint8 num_bins,
    CHistogramBinning::EHistAlgo eHistAlgo)
{
    AutoPtr<CHistogramBinning::TListOfBins> pListOfBins(
        m_gapAnalysis.GetGapHistogram(num_bins, eHistAlgo) );

    const size_t kDigitsInUint8 = numeric_limits<Uint8>::digits10;
    CTablePrinter::SColInfoVec vecColInfos;
    vecColInfos.AddCol("Range", 1 + 2*kDigitsInUint8);
    vecColInfos.AddCol("Number in Range", kDigitsInUint8,
        CTablePrinter::eJustify_Right);
    CTablePrinter table_printer(vecColInfos, cout);

    cout << "HISTOGRAM:" << endl;
    ITERATE( CHistogramBinning::TListOfBins, bin_iter, *pListOfBins ) {
        const CHistogramBinning::SBin & bin = *bin_iter;
        table_printer << bin.first_number 
                      << '-' << bin.last_number << CellEnd();
        table_printer << bin.total_appearances << CellEnd();
    }
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CGapStatsApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}

