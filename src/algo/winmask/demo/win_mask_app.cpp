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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   CWinMaskApplication class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include "win_mask_app.hpp"
#include <algo/winmask/win_mask_config.hpp>
#include <algo/winmask/win_mask_reader.hpp>
#include <algo/winmask/win_mask_fasta_reader.hpp>
#include <algo/winmask/win_mask_writer.hpp>
#include <algo/winmask/win_mask_gen_counts.hpp>
#include <algo/winmask/win_mask_seq_title.hpp>

#include <algo/winmask/seq_masker.hpp>
#include <algo/winmask/dust_masker.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//-------------------------------------------------------------------------
const char * const 
CWinMaskApplication::USAGE_LINE = "Window based sequence masker";

//-------------------------------------------------------------------------
void CWinMaskApplication::Init(void)
{
    auto_ptr< CArgDescriptions > arg_desc( new CArgDescriptions );

    // Set the program description
    arg_desc->SetUsageContext( GetArguments().GetProgramBasename(),
                               USAGE_LINE );

    // Adding command line arguments descriptions
    arg_desc->AddDefaultKey( "lstat", "length_statistics_file",
                             "relative unit frequencies "
                             "(required if -mk_counts is false)",
                             CArgDescriptions::eString, "" );
    arg_desc->AddDefaultKey( "input", "input_file_name",
                             "input file name "
                             "(not optional if used with -mk_counts option)",
                             CArgDescriptions::eString, "" );
    arg_desc->AddDefaultKey( "output", "output_file_name",
                             "output file name",
                             CArgDescriptions::eString, "" );
    arg_desc->AddDefaultKey( "checkdup", "check_duplicates",
                             "check for duplicate sequences",
                             CArgDescriptions::eBoolean, "false" );
    arg_desc->AddDefaultKey( "window", "window_size", "window size",
                             CArgDescriptions::eInteger, "19" );
    arg_desc->AddDefaultKey( "wstep", "window_step", "window step",
                             CArgDescriptions::eInteger, "1" );
    arg_desc->AddDefaultKey( "ustep", "unit_step", "unit step",
                             CArgDescriptions::eInteger, "1" );
    arg_desc->AddDefaultKey( "xdrop", "X_drop", 
                             "value of X-drop parameter",
                             CArgDescriptions::eInteger, "50" );
    arg_desc->AddDefaultKey( "score", "score_threshold",
                             "window score threshold",
                             CArgDescriptions::eInteger, "100" );
    arg_desc->AddDefaultKey( "highscore", "max_score",
                             "maximum useful unit score",
                             CArgDescriptions::eInteger, "500" );
    arg_desc->AddOptionalKey( "lowscore", "min_score",
                              "minimum useful unit score",
                              CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey( "sethighscore", "score_value",
                              "alternative high score for a unit if the"
                              "original unit score is more than highscore",
                              CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey( "setlowscore", "score_value",
                              "alternative low score for a unit if the"
                              "original unit score is lower than lowscore",
                              CArgDescriptions::eInteger );
    arg_desc->AddDefaultKey( "ambig", "ambiguity_handler",
                             "the way to handle ambiguity characters",
                             CArgDescriptions::eString, "break" );
    arg_desc->AddDefaultKey( "oformat", "output_format",
                             "controls the format of the masker output",
                             CArgDescriptions::eString, "interval" );
    arg_desc->AddDefaultKey( "mpass", "merge_pass_flag",
                             "true if separate merging pass is needed",
                             CArgDescriptions::eBoolean, "false" );
    arg_desc->AddDefaultKey( "discontig", "discontiguous_units",
                             "true if using discontiguous units",
                             CArgDescriptions::eBoolean, "false" );
    arg_desc->AddDefaultKey( "mscore", "merge_cutoff_score",
                             "minimum average unit score triggering a merge",
                             CArgDescriptions::eInteger, "50" );
    arg_desc->AddDefaultKey( "mabs", "distance",
                             "absolute distance threshold for merging",
                             CArgDescriptions::eInteger, "8" );
    arg_desc->AddDefaultKey( "mmean", "distance",
                             "distance threshold for merging if average unit"
                             " score is high enough",
                             CArgDescriptions::eInteger, "50" );
    arg_desc->AddDefaultKey( "mustep", "merge_unit_step",
                             "unit step value used for interval merging",
                             CArgDescriptions::eInteger, "1" );
    arg_desc->AddDefaultKey( "trigger", "trigger_type",
                             "type of the event triggering masking",
                             CArgDescriptions::eString, "mean" );
    arg_desc->AddDefaultKey( "tmin_count", "unit_count",
                             "number of units to count with min trigger",
                             CArgDescriptions::eInteger, "0" );
    arg_desc->AddDefaultKey( "pattern", "base_mask", 
                             "which bases in a window to use as a discontinuous unit",
                             CArgDescriptions::eInteger, "0" );
    arg_desc->AddDefaultKey( "dbg", "debug_output",
                             "enable debug output",
                             CArgDescriptions::eBoolean, "false" );
    arg_desc->AddDefaultKey( "mk_counts", "generate_counts",
                             "generate frequency counts for a database",
                             CArgDescriptions::eBoolean, "false" );
    arg_desc->AddDefaultKey( "fa_list", "input_is_a_list",
                             "indicates that -input represents a file containing "
                             "a list of names of fasta files to process, one name "
                             " per line (can only be used with -mk_counts true)", 
                             CArgDescriptions::eBoolean, "false" );
    arg_desc->AddDefaultKey( "mem", "available_memory",
                             "memory available for mk_counts option in megabytes",
                             CArgDescriptions::eInteger, "1536" );
    arg_desc->AddDefaultKey( "unit", "unit_length",
                             "number of bases in a unit",
                             CArgDescriptions::eInteger, "15" );
    arg_desc->AddDefaultKey( "th", "thresholds",
                             "4 percentage values used to determine "
                             "masking thresholds (4 floating point numbers "
                             "separated by commas)", 
                             CArgDescriptions::eString, "90,99,99.5,99.8" );
    arg_desc->AddDefaultKey( "dust", "use_dust",
                             "combine window masking with dusting",
                             CArgDescriptions::eBoolean, "F" );
    arg_desc->AddDefaultKey( "dust_window", "dust_window",
                             "window size for dusting",
                             CArgDescriptions::eInteger, "64" );
    arg_desc->AddDefaultKey( "dust_level", "dust_level",
                             "dust minimum level",
                             CArgDescriptions::eInteger, "20" );
    arg_desc->AddDefaultKey( "dust_linker", "dust_linker",
                             "link windows by this many basepairs",
                             CArgDescriptions::eInteger, "1" );
    arg_desc->AddDefaultKey( "exclude_ids", "exclude_id_list",
                             "file containing the list of ids to exclude from processing",
                             CArgDescriptions::eString, "" );
    arg_desc->AddDefaultKey( "ids", "id_list",
                             "file containing the list of ids to process",
                             CArgDescriptions::eString, "" );

    // Set some constraints on command line parameters
    arg_desc->SetConstraint( "window",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "wstep",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "ustep",
                             new CArgAllow_Integers( 1, 256 ) );
    arg_desc->SetConstraint( "xdrop",
                             new CArgAllow_Integers( 0, kMax_Int ) );
    arg_desc->SetConstraint( "score",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "highscore",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "lowscore",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "sethighscore",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "setlowscore",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "mscore",
                             new CArgAllow_Integers( 0, kMax_Int ) );
    arg_desc->SetConstraint( "mabs",
                             new CArgAllow_Integers( 0, kMax_Int ) );
    arg_desc->SetConstraint( "mmean",
                             new CArgAllow_Integers( 0, kMax_Int ) );
    arg_desc->SetConstraint( "mustep",
                             new CArgAllow_Integers( 0, 256 ) );
    arg_desc->SetConstraint( "ambig",
                             (new CArgAllow_Strings())->Allow( "break" ) );
    arg_desc->SetConstraint( "oformat",
                             (new CArgAllow_Strings())->Allow( "interval" )
                             ->Allow( "fasta" ) );
    arg_desc->SetConstraint( "trigger",
                             (new CArgAllow_Strings())->Allow( "mean" )
                             ->Allow( "min" ) );
    arg_desc->SetConstraint( "tmin_count",
                             new CArgAllow_Integers( 0, kMax_Int ) );
    arg_desc->SetConstraint( "mem", new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "unit", new CArgAllow_Integers( 1, 16 ) );

    // Parse the arguments according to descriptions.
    SetupArgDescriptions(arg_desc.release());
}

//-------------------------------------------------------------------------
int CWinMaskApplication::Run (void)
{
    CRef<CObjectManager> om(CObjectManager::GetInstance());

    if( GetArgs()["dbg"].AsBoolean() )
        SetDiagTrace( eDT_Enable );

    // Read and validate configuration values.
    CWinMaskConfig aConfig( GetArgs() );
    aConfig.Validate();

    if( aConfig.MakeCounts() )
    {
        CWinMaskCountsGenerator cg( aConfig.Input(),
                                    aConfig.Output(),
                                    aConfig.Th(),
                                    aConfig.Mem(),
                                    aConfig.UnitSize(),
                                    aConfig.MinScore(),
                                    aConfig.MaxScore(),
                                    aConfig.HasMinScore(),
                                    aConfig.CheckDup(),
                                    aConfig.FaList() );
        cg();
        return 0;
    }

    CWinMaskReader & theReader = aConfig.Reader();
    CWinMaskWriter & theWriter = aConfig.Writer();
    CSeqMasker theMasker( aConfig.LStatName(),
                          aConfig.WindowSize(),
                          aConfig.WindowStep(),
                          aConfig.UnitStep(),
                          aConfig.XDrop(),
                          aConfig.CutoffScore(),
                          aConfig.MaxScore(),
                          aConfig.MinScore(),
                          aConfig.SetMaxScore(),
                          aConfig.SetMinScore(),
                          aConfig.MergePass(),
                          aConfig.MergeCutoffScore(),
                          aConfig.AbsMergeCutoffDist(),
                          aConfig.MeanMergeCutoffDist(),
                          aConfig.MergeUnitStep(),
                          aConfig.Trigger(),
                          aConfig.TMin_Count(),
                          aConfig.Discontig(),
                          aConfig.Pattern() );
    CRef< CSeq_entry > aSeqEntry( 0 );
    Uint4 total = 0, total_masked = 0;
    CDustMasker * duster( 0 );
    set< string > ids( aConfig.Ids() );
    set< string > exclude_ids( aConfig.ExcludeIds() );

    if( aConfig.UseDust() )
        duster = new CDustMasker( aConfig.DustWindow(),
                                  aConfig.DustLevel(),
                                  aConfig.DustLinker() );

    while( (aSeqEntry = theReader.GetNextSequence()).NotEmpty() )
    {
        Uint4 masked = 0;
        const CBioseq & bioseq = aSeqEntry->GetSeq();

        if(    bioseq.CanGetInst() 
               && bioseq.GetInst().CanGetLength()
               && bioseq.GetInst().CanGetSeq_data() )
        {
            CRef<CScope> scope(new CScope(*om));
            CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry( *aSeqEntry );
            bool process( true );
            string id( CWinMaskSeqTitle::GetId( seh, bioseq ) );

            if( !ids.empty() )
            {
                process = false;

                if( ids.find( id ) != ids.end() )
                    process = true;
            }

            if( !exclude_ids.empty() )
                if( exclude_ids.find( id ) != exclude_ids.end() )
                    process = false;

            if( process )
            {
                TSeqPos len = bioseq.GetInst().GetLength();
                total += len;
                _TRACE( "Sequence length " << len );
                const CSeq_data & seqdata = bioseq.GetInst().GetSeq_data();
                CRef< CSeq_data > dest( new CSeq_data );
                CSeqportUtil::Convert( seqdata, dest, CSeq_data::e_Iupacna, 
                                       0, len );
                const string & data = dest->GetIupacna().Get();
                auto_ptr< CSeqMasker::TMaskList > mask_info( theMasker( data ) );

                if( duster != 0 ) // Dust and merge with mask_info
                {
                    auto_ptr< CSeqMasker::TMaskList > dust_info( (*duster)( data ) );
                    CSeqMasker::MergeMaskInfo( mask_info.get(), dust_info.get() );
                }

                theWriter.Print( seh, bioseq, *mask_info );

                for( CSeqMasker::TMaskList::const_iterator i = mask_info->begin();
                     i != mask_info->end(); ++i )
                    masked += i->second - i->first + 1;

                total_masked += masked;
                _TRACE( "Number of positions masked: " << masked );
            }
        }
    }

    _TRACE( "Total number of positions: " << total );
    _TRACE( "Total number of positions masked: " << total_masked );
    return 0;
}

END_NCBI_SCOPE


/*
 * ========================================================================
 * $Log$
 * Revision 1.4  2005/02/25 21:09:18  morgulis
 * 1. Reduced the number of binary searches by the factor of 2 by locally
 *    caching some search results.
 * 2. Automatically compute -lowscore value if it is not specified on the
 *    command line during the counts generation pass.
 *
 * Revision 1.3  2005/02/14 12:14:36  dicuccio
 * CRef<> instead of auto_ptr<> for CObject-derived class
 *
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

