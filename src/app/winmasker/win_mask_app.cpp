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
#include <objmgr/reader_id1.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include "win_mask_app.hpp"
#include "win_mask_config.hpp"
#include "win_mask_reader.hpp"
#include "win_mask_fasta_reader.hpp"
#include "win_mask_writer.hpp"
#include "win_mask_gen_counts.hpp"
#include "win_mask_util.hpp"

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
    arg_desc->AddDefaultKey( "ustat", "unit_counts",
                             "file with unit counts"
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
    arg_desc->AddOptionalKey( "window", "window_size", "window size",
                              CArgDescriptions::eInteger );
#if 0
    arg_desc->AddDefaultKey( "wstep", "window_step", "window step",
                             CArgDescriptions::eInteger, "1" );
    arg_desc->AddDefaultKey( "ustep", "unit_step", "unit step",
                             CArgDescriptions::eInteger, "1" );
#endif
    arg_desc->AddOptionalKey( "t_extend", "T_extend", 
                              "window score above which it is allowed to extend masking",
                              CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey( "t_thres", "T_threshold",
                              "window score threshold used to trigger masking",
                              CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey( "t_high", "T_high",
                              "maximum useful unit score",
                              CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey( "t_low", "T_low",
                              "minimum useful unit score",
                              CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey( "set_t_high", "score_value",
                              "alternative high score for a unit if the"
                              "original unit score is more than highscore",
                              CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey( "set_t_low", "score_value",
                              "alternative low score for a unit if the"
                              "original unit score is lower than lowscore",
                              CArgDescriptions::eInteger );
#if 0
    arg_desc->AddDefaultKey( "ambig", "ambiguity_handler",
                             "the way to handle ambiguity characters",
                             CArgDescriptions::eString, "break" );
#endif
    arg_desc->AddDefaultKey( "oformat", "output_format",
                             "controls the format of the masker output",
                             CArgDescriptions::eString, "interval" );
    arg_desc->AddDefaultKey( "sformat", "unit_counts_format",
                             "controls the format of the file containing the unit counts",
                             CArgDescriptions::eString, "ascii" );
#if 0
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
#endif
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
    arg_desc->AddDefaultKey( "smem", "available_memory",
                             "memory available for masking stage in megabytes",
                             CArgDescriptions::eInteger, "512" );
    arg_desc->AddOptionalKey( "unit", "unit_length",
                              "number of bases in a unit",
                              CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey( "genome_size", "genome_size",
                              "total size of the genome",
                              CArgDescriptions::eInteger );
#if 0
    arg_desc->AddDefaultKey( "th", "thresholds",
                             "4 percentage values used to determine "
                             "masking thresholds (4 floating point numbers "
                             "separated by commas)", 
                             CArgDescriptions::eString, "90,99,99.5,99.8" );
#endif
    arg_desc->AddDefaultKey( "dust", "use_dust",
                             "combine window masking with dusting",
                             CArgDescriptions::eBoolean, "F" );
#if 0
    arg_desc->AddDefaultKey( "dust_window", "dust_window",
                             "window size for dusting",
                             CArgDescriptions::eInteger, "64" );
    arg_desc->AddDefaultKey( "dust_level", "dust_level",
                             "dust minimum level",
                             CArgDescriptions::eInteger, "20" );
    arg_desc->AddDefaultKey( "dust_linker", "dust_linker",
                             "link windows by this many basepairs",
                             CArgDescriptions::eInteger, "1" );
#endif
    arg_desc->AddDefaultKey( "exclude_ids", "exclude_id_list",
                             "file containing the list of ids to exclude from processing",
                             CArgDescriptions::eString, "" );
    arg_desc->AddDefaultKey( "ids", "id_list",
                             "file containing the list of ids to process",
                             CArgDescriptions::eString, "" );

    // Set some constraints on command line parameters
    arg_desc->SetConstraint( "window",
                             new CArgAllow_Integers( 1, kMax_Int ) );
#if 0
    arg_desc->SetConstraint( "wstep",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "ustep",
                             new CArgAllow_Integers( 1, 256 ) );
#endif
    arg_desc->SetConstraint( "t_extend",
                             new CArgAllow_Integers( 0, kMax_Int ) );
    arg_desc->SetConstraint( "t_thres",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "t_high",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "t_low",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "set_t_high",
                             new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "set_t_low",
                             new CArgAllow_Integers( 1, kMax_Int ) );
#if 0
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
#endif
    arg_desc->SetConstraint( "oformat",
                             (new CArgAllow_Strings())->Allow( "interval" )
                             ->Allow( "fasta" ) );
    arg_desc->SetConstraint( "sformat",
                             (new CArgAllow_Strings())
                             ->Allow( "ascii" )
                             ->Allow( "binary" )
                             ->Allow( "oascii" )
                             ->Allow( "obinary" ) );
#if 0
    arg_desc->SetConstraint( "trigger",
                             (new CArgAllow_Strings())->Allow( "mean" )
                             ->Allow( "min" ) );
    arg_desc->SetConstraint( "tmin_count",
                             new CArgAllow_Integers( 0, kMax_Int ) );
#endif
    arg_desc->SetConstraint( "mem", new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint( "unit", new CArgAllow_Integers( 1, 16 ) );

    // Parse the arguments according to descriptions.
    SetupArgDescriptions(arg_desc.release());
}

//-------------------------------------------------------------------------
int CWinMaskApplication::Run (void)
{
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    //CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eDefault);
    CGBDataLoader::RegisterInObjectManager(
        *om, new CId1Reader, CObjectManager::eDefault);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

#if 0
    if( GetArgs()["dbg"].AsBoolean() )
        SetDiagTrace( eDT_Enable );
#endif

    // Read and validate configuration values.
    CWinMaskConfig aConfig( GetArgs() );
    aConfig.Validate();

    if( aConfig.MakeCounts() )
    {
        {
            CWinMaskCountsGenerator cg( aConfig.Input(),
                                        aConfig.Output(),
                                        aConfig.SFormat(),
                                        aConfig.Th(),
                                        aConfig.Mem(),
                                        aConfig.UnitSize(),
                                        aConfig.GenomeSize(),
                                        aConfig.MinScore(),
                                        aConfig.MaxScore(),
                                        aConfig.CheckDup(),
                                        aConfig.FaList(),
                                        aConfig.Ids(),
                                        aConfig.ExcludeIds() );
            cg();
        }
        return 0;
    }

    CWinMaskReader & theReader = aConfig.Reader();
    CWinMaskWriter & theWriter = aConfig.Writer();
    CSeqMasker theMasker( aConfig.LStatName(),
                          aConfig.WindowSize(),
                          aConfig.WindowStep(),
                          aConfig.UnitStep(),
                          aConfig.Textend(),
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
    set< CSeq_id_Handle > ids( aConfig.Ids() );
    set< CSeq_id_Handle > exclude_ids( aConfig.ExcludeIds() );

    if( aConfig.UseDust() )
        duster = new CDustMasker( aConfig.DustWindow(),
                                  aConfig.DustLevel(),
                                  aConfig.DustLinker() );

    while( (aSeqEntry = theReader.GetNextSequence()).NotEmpty() )
    {
        CSeq_entry_Handle seh;
        try {
            seh = scope->GetSeq_entryHandle(*aSeqEntry);
        }
        catch (CException&) {
            seh = scope->AddTopLevelSeqEntry(*aSeqEntry);
        }

        Uint4 masked = 0;

        CBioseq_CI bs_iter(seh, CSeq_inst::eMol_na);
        for ( ;  bs_iter;  ++bs_iter) {
            CBioseq_Handle bsh = *bs_iter;
            if (bsh.GetBioseqLength() == 0) {
                continue;
            }

            if( CWinMaskUtil::consider( scope, bsh, ids, exclude_ids ) )
            {
                TSeqPos len = bsh.GetBioseqLength();
                total += len;
                _TRACE( "Sequence length " << len );
                CSeqVector data =
                    bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
                auto_ptr< CSeqMasker::TMaskList > mask_info( theMasker( data ) );

                if( duster != 0 ) // Dust and merge with mask_info
                {
                    auto_ptr< CSeqMasker::TMaskList > dust_info( (*duster)( data ) );
                    CSeqMasker::MergeMaskInfo( mask_info.get(), dust_info.get() );
                }

                theWriter.Print( bsh, *mask_info );

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
 * Revision 1.10  2005/05/02 14:27:46  morgulis
 * Implemented hash table based unit counts formats.
 *
 * Revision 1.9  2005/04/18 20:11:36  morgulis
 * Stage 1 can now take -t_high parameter.
 * Unit counts generated do not contain counts above T_high.
 *
 * Revision 1.8  2005/04/12 13:35:34  morgulis
 * Support for binary format of unit counts file.
 *
 * Revision 1.7  2005/03/28 21:33:26  morgulis
 * Added -sformat option to specify the output format for unit counts file.
 * Implemented framework allowing usage of different output formats for
 * unit counts. Rewrote the code generating the unit counts file using
 * that framework.
 *
 * Revision 1.6  2005/03/24 18:43:19  morgulis
 * Change to suppress an error message from GenBank data loader.
 *
 * Revision 1.5  2005/03/24 16:50:21  morgulis
 * -ids and -exclude-ids options can be applied in Stage 1 and Stage 2.
 *
 * Revision 1.4  2005/03/21 13:19:26  dicuccio
 * Updated API: use object manager functions to supply data, instead of passing
 * data as strings.
 *
 * Revision 1.3  2005/03/11 15:08:22  morgulis
 * 1. Made -window parameter optional and be default equal to unit_size + 4;
 * 2. Changed the name of -lstat parameter to -ustat.
 * 3. Added README file.
 *
 * Revision 1.2  2005/03/08 17:02:30  morgulis
 * Changed unit counts file to include precomputed threshold values.
 * Changed masking code to pick up threshold values from the units counts file.
 * Unit size is computed automatically from the genome length.
 * Added extra option for specifying genome length.
 * Removed all experimental command line options.
 * Fixed id strings in duplicate sequence checking code.
 *
 * Revision 1.1  2005/02/25 21:32:54  dicuccio
 * Rearranged winmasker files:
 * - move demo/winmasker to a separate app directory (src/app/winmasker)
 * - move win_mask_* to app directory
 *
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

