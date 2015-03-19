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
 *   CWinMaskConfig class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>

#include <algo/winmask/win_mask_config.hpp>
#include <objtools/seqmasks_io/mask_cmdline_args.hpp>
#include <objtools/seqmasks_io/mask_fasta_reader.hpp>
#include <objtools/seqmasks_io/mask_bdb_reader.hpp>
#include <objtools/seqmasks_io/mask_writer_int.hpp>
#include <objtools/seqmasks_io/mask_writer_fasta.hpp>
#include <objtools/seqmasks_io/mask_writer_seqloc.hpp>
#include <objtools/seqmasks_io/mask_writer_blastdb_maskinfo.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void CWinMaskConfig::AddWinMaskArgs(CArgDescriptions &arg_desc,
                                    EAppType type,
                                    bool determine_input)
{
    arg_desc.SetCurrentGroup("WindowMasker options");

    // Adding command line arguments descriptions
    if(type == eAny)
        arg_desc.AddOptionalKey( "ustat", "unit_counts",
                                 "file with unit counts",
                                 CArgDescriptions::eString);
    else if(type >= eGenerateMasks)
        arg_desc.AddKey( "ustat", "unit_counts",
                         "file with unit counts",
                         CArgDescriptions::eString);
    if(determine_input)
        arg_desc.AddDefaultKey( kInput, "input_file_name",
                                 "input file name "
                                 "(not optional if used with -mk_counts or -convert options)",
                                 CArgDescriptions::eInputFile, "-" );
    arg_desc.AddDefaultKey( kOutput, "output_file_name",
                             "output file name",
                             CArgDescriptions::eOutputFile, "-" );
    if(type == eAny || type == eComputeCounts){
        arg_desc.AddDefaultKey( "checkdup", "check_duplicates",
                                 "check for duplicate sequences",
                                 CArgDescriptions::eBoolean, "false" );
        if(determine_input)
            arg_desc.AddDefaultKey( "fa_list", "input_is_a_list",
                                     "indicates that -input represents a file containing "
                                     "a list of names of fasta files to process, one name "
                                     " per line", 
                                 CArgDescriptions::eBoolean, "false" );
        arg_desc.AddDefaultKey( "mem", "available_memory",
                                 "memory available for mk_counts option in megabytes",
                                 CArgDescriptions::eInteger, "1536" );
        arg_desc.AddDefaultKey( "meta", "info_string",
                                "add metadata to the counts file",
                                CArgDescriptions::eString, "" );
        arg_desc.AddOptionalKey( "unit", "unit_length",
                                  "number of bases in a unit",
                                  CArgDescriptions::eInteger );
        arg_desc.AddOptionalKey( "genome_size", "genome_size",
                                  "total size of the genome",
                                  CArgDescriptions::eInteger );
        arg_desc.SetConstraint( "mem", new CArgAllow_Integers( 1, kMax_Int ) );
        arg_desc.SetConstraint( "unit", new CArgAllow_Integers( 1, 16 ) );
    }
    if(type == eAny || type >= eGenerateMasks){
        arg_desc.AddOptionalKey( "window", "window_size", "window size",
                                  CArgDescriptions::eInteger );
        arg_desc.AddOptionalKey( "t_extend", "T_extend", 
                                  "window score above which it is allowed to extend masking",
                                  CArgDescriptions::eInteger );
        arg_desc.AddOptionalKey( "t_thres", "T_threshold",
                                  "window score threshold used to trigger masking",
                                  CArgDescriptions::eInteger );
        arg_desc.AddOptionalKey( "set_t_high", "score_value",
                                  "alternative high score for a unit if the"
                                  "original unit score is more than highscore",
                                  CArgDescriptions::eInteger );
        arg_desc.AddOptionalKey( "set_t_low", "score_value",
                                  "alternative low score for a unit if the"
                                  "original unit score is lower than lowscore",
                                  CArgDescriptions::eInteger );
        arg_desc.SetConstraint( "window",
                                 new CArgAllow_Integers( 1, kMax_Int ) );
        arg_desc.SetConstraint( "t_extend",
                                 new CArgAllow_Integers( 0, kMax_Int ) );
        arg_desc.SetConstraint( "t_thres",
                                 new CArgAllow_Integers( 1, kMax_Int ) );
        arg_desc.SetConstraint( "set_t_high",
                                 new CArgAllow_Integers( 1, kMax_Int ) );
        arg_desc.SetConstraint( "set_t_low",
                                 new CArgAllow_Integers( 1, kMax_Int ) );
        arg_desc.AddFlag      ( "parse_seqids",
                                 "Parse Seq-ids in FASTA input", true );
        arg_desc.AddDefaultKey( kOutputFormat, "output_format",
                                 "controls the format of the masker output (for masking stage only)",
                                 CArgDescriptions::eString, *kOutputFormats );
        CArgAllow_Strings* strings_allowed = new CArgAllow_Strings();
        for (size_t i = 0; i < kNumOutputFormats; i++) {
            strings_allowed->Allow(kOutputFormats[i]);
        }
        arg_desc.SetConstraint( kOutputFormat, strings_allowed );
    }
    if(type != eConvertCounts){
        arg_desc.AddOptionalKey( "t_high", "T_high",
                                  "maximum useful unit score",
                                  CArgDescriptions::eInteger );
        arg_desc.AddOptionalKey( "t_low", "T_low",
                                  "minimum useful unit score",
                                  CArgDescriptions::eInteger );
        arg_desc.SetConstraint( "t_high",
                                 new CArgAllow_Integers( 1, kMax_Int ) );
        arg_desc.SetConstraint( "t_low",
                                 new CArgAllow_Integers( 1, kMax_Int ) );
        arg_desc.AddDefaultKey( kInputFormat, "input_format",
                                "controls the format of the masker input",
                                CArgDescriptions::eString, *kInputFormats );
        arg_desc.AddDefaultKey( "exclude_ids", "exclude_id_list",
                                 "file containing the list of ids to exclude from processing",
                                 CArgDescriptions::eString, "" );
        arg_desc.AddDefaultKey( "ids", "id_list",
                                 "file containing the list of ids to process",
                                 CArgDescriptions::eString, "" );
        arg_desc.AddDefaultKey( "text_match", "text_match_ids",
                                 "match ids as strings",
                                 CArgDescriptions::eBoolean, "T" );
        CArgAllow_Strings* strings_allowed = new CArgAllow_Strings();
        for (size_t i = 0; i < kNumInputFormats; i++) {
            strings_allowed->Allow(kInputFormats[i]);
        }
        strings_allowed->Allow("seqids");
        arg_desc.SetConstraint( kInputFormat, strings_allowed );
    }
    if(type < eGenerateMasks){
        arg_desc.AddDefaultKey( "sformat", "unit_counts_format",
                                 "controls the format of the output file containing the unit counts "
                                 "(for counts generation and conversion only)",
                                 CArgDescriptions::eString, "ascii" );
        arg_desc.SetConstraint( "sformat",
                                 (new CArgAllow_Strings())
                                 ->Allow( "ascii" )
                                 ->Allow( "binary" )
                                 ->Allow( "oascii" )
                                 ->Allow( "obinary" ) );
        arg_desc.AddDefaultKey( "smem", "available_memory",
                                "target size of the output file containing the unit counts",
                                 CArgDescriptions::eInteger, "512" );
    }
    if(type == eAny || type >= eGenerateMasksWithDuster)
        arg_desc.AddDefaultKey( "dust", "use_dust",
                                 "combine window masking with dusting",
                                 CArgDescriptions::eBoolean, "F" );
        arg_desc.AddDefaultKey( "dust_level", "dust_level",
                                 "dust minimum level",
                                 CArgDescriptions::eInteger, "20" );

    if(type == eAny){
        arg_desc.AddFlag( "mk_counts", "generate frequency counts for a database" );
        arg_desc.AddFlag( "convert", "convert counts between different formats" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "outfmt" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "ustat" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "window" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "t_thres" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "t_extend" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "set_t_low" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "set_t_high" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "dust" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "dust_level" );
        arg_desc.CArgDescriptions::SetDependency( "mk_counts", CArgDescriptions::eExcludes, "convert" );

        arg_desc.CArgDescriptions::SetDependency( "ustat", CArgDescriptions::eExcludes, "checkdup" );
        if(determine_input)
            arg_desc.CArgDescriptions::SetDependency( "ustat", CArgDescriptions::eExcludes, "fa_list" );
        arg_desc.CArgDescriptions::SetDependency( "ustat", CArgDescriptions::eExcludes, "mem" );
        arg_desc.CArgDescriptions::SetDependency( "ustat", CArgDescriptions::eExcludes, "unit" );
        arg_desc.CArgDescriptions::SetDependency( "ustat", CArgDescriptions::eExcludes, "genome_size" );
        arg_desc.CArgDescriptions::SetDependency( "ustat", CArgDescriptions::eExcludes, "sformat" );
        arg_desc.CArgDescriptions::SetDependency( "ustat", CArgDescriptions::eExcludes, "smem" );
        arg_desc.CArgDescriptions::SetDependency( "ustat", CArgDescriptions::eExcludes, "convert" );

        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "checkdup" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "window" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "t_extend" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "t_thres" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "t_high" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "t_low" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "set_t_low" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "set_t_high" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "infmt" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "outfmt" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "parse_seqids" );
        if(determine_input)
            arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "fa_list" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "mem" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "unit" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "genome_size" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "dust" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "dust_level" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "exclude_ids" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "ids" );
        arg_desc.CArgDescriptions::SetDependency( "convert", CArgDescriptions::eExcludes, "text_match" );
    }
}

CWinMaskConfig::EAppType CWinMaskConfig::s_DetermineAppType(
          const CArgs & args, EAppType user_specified_type)
{
    EAppType type = user_specified_type;

    if(type == eAny){
        if(args["mk_counts"])
            type = eComputeCounts;
        else if(args["convert"])
            type = eConvertCounts;
        else if(args["ustat"])
            type = eGenerateMasksWithDuster;
        else
            NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                        "one of '-mk_counts', '-convert' or '-ustat <stat_file>' "
                        "must be specified" );
    }

    if(type == eGenerateMasksWithDuster && !args["dust"].AsBoolean())
        type = eGenerateMasks;

    return type;
}

CMaskWriter*
CWinMaskConfig::x_GetWriter(const CArgs& args)
{
    const string & format( args[kOutputFormat].AsString() );
    CMaskWriter* retval = NULL;

    if (format == "interval") {
        CNcbiOstream& output = args[kOutput].AsOutputFile();
        retval = new CMaskWriterInt(output);
    } else if (format == "fasta") {
        CNcbiOstream& output = args[kOutput].AsOutputFile();
        retval = new CMaskWriterFasta(output);
    } else if (NStr::StartsWith(format, "seqloc_asn1_binary")) {
        CNcbiOstream& output = args[kOutput].AsOutputFile(CArgValue::fBinary);
        retval = new CMaskWriterSeqLoc(output, format);
    } else if (NStr::StartsWith(format, "seqloc_")) {
        CNcbiOstream& output = args[kOutput].AsOutputFile();
        retval = new CMaskWriterSeqLoc(output, format);
    } else if (NStr::StartsWith(format, "maskinfo_asn1_binary")) {
        CNcbiOstream& output = args[kOutput].AsOutputFile(CArgValue::fBinary);
        retval = 
            new CMaskWriterBlastDbMaskInfo(output, format, 3,
                               eBlast_filter_program_windowmasker,
                               BuildAlgorithmParametersString(args));
    } else if (NStr::StartsWith(format, "maskinfo_")) {
        CNcbiOstream& output = args[kOutput].AsOutputFile();
        retval = 
            new CMaskWriterBlastDbMaskInfo(output, format, 3,
                               eBlast_filter_program_windowmasker,
                               BuildAlgorithmParametersString(args));
    } else {
        throw runtime_error("Unknown output format");
    }
    return retval;
}

//----------------------------------------------------------------------------
CWinMaskConfig::CWinMaskConfig( const CArgs & args, EAppType type, bool determine_input )
    : app_type(s_DetermineAppType(args, type)),
      is( app_type >= eGenerateMasks && args[kInputFormat].AsString() != "blastdb"
                                     && determine_input ? 
          ( !(args[kInput].AsString() == "-")
            ? new CNcbiIfstream( args[kInput].AsString().c_str() ) 
            : static_cast<CNcbiIstream*>(&NcbiCin) ) : NULL ), reader( NULL ), writer( NULL ),
      lstat_name( app_type >= eGenerateMasks ? args["ustat"].AsString() : "" ),
      textend( app_type >= eGenerateMasks && args["t_extend"] ? args["t_extend"].AsInteger() : 0 ), 
      cutoff_score( app_type >= eGenerateMasks && args["t_thres"] ? args["t_thres"].AsInteger() : 0 ),
      max_score( app_type != eConvertCounts && args["t_high"] ? args["t_high"].AsInteger() : 0 ),
      min_score( app_type != eConvertCounts && args["t_low"] ? args["t_low"].AsInteger() : 0 ),
      window_size( app_type >= eGenerateMasks && args["window"] ? args["window"].AsInteger() : 0 ),
      merge_pass( false ),
      merge_cutoff_score( 50 ),
      abs_merge_cutoff_dist( 8 ),
      mean_merge_cutoff_dist( 50 ),
      trigger( "mean" ),
      tmin_count( 0 ),
      discontig( false ),
      pattern( 0 ),
      window_step( 1 ),
      unit_step( 1 ),
      merge_unit_step( 1 ),
      fa_list( app_type == eComputeCounts && determine_input ? args["fa_list"].AsBoolean() : false ),
      mem( app_type == eComputeCounts ? args["mem"].AsInteger() : 0 ),
      unit_size( app_type == eComputeCounts && args["unit"] ? args["unit"].AsInteger() : 0 ),
      genome_size( app_type == eComputeCounts && args["genome_size"] ? args["genome_size"].AsInt8() : 0 ),
      input( determine_input ? args[kInput].AsString() : ""),
      output( args[kOutput].AsString() ),
      th( "90,99,99.5,99.8" ),
      dust_window( 64 ),
      dust_level( app_type == eGenerateMasksWithDuster ? args["dust_level"].AsInteger() : 0 ),
      dust_linker( 1 ),
      checkdup( app_type == eComputeCounts ? args["checkdup"].AsBoolean() : false ),
      sformat( app_type < eGenerateMasks ? args["sformat"].AsString() : "" ),
      smem( app_type < eGenerateMasks ? args["smem"].AsInteger() : 0 ),
      ids( 0 ), exclude_ids( 0 ),
      use_ba( app_type != eConvertCounts ),
      text_match( app_type != eConvertCounts && args["text_match"].AsBoolean() )
{
    if (args.Exist("meta")  &&  args["meta"]) {
        metadata = args["meta"].AsString();
    }
    _TRACE( "Entering CWinMaskConfig::CWinMaskConfig()" );

    if(app_type == eConvertCounts)
        return;

    iformatstr = args[kInputFormat].AsString();

    if( app_type == eComputeCounts) {
        text_match = true;
    } else {
        if( is && !*is )
        {
            NCBI_THROW( CWinMaskConfigException,
                        eInputOpenFail,
                        args[kInput].AsString() );
        }

        if(determine_input && iformatstr != "seqids"){
            if( iformatstr == "fasta" )
                reader = new CMaskFastaReader( *is, true, args["parse_seqids"] );
            else if( iformatstr == "blastdb" )
                reader = new CMaskBDBReader( args[kInput].AsString() );

            if( !reader )
            {
                NCBI_THROW( CWinMaskConfigException,
                            eReaderAllocFail, "" );
            }
        }

        writer = x_GetWriter(args);

        set_max_score = args["set_t_high"]  ? args["set_t_high"].AsInteger()
                                            : 0;
        set_min_score = args["set_t_low"]   ? args["set_t_low"].AsInteger()
                                            : 0;
    }

    string ids_file_name( args["ids"].AsString() );
    string exclude_ids_file_name( args["exclude_ids"].AsString() );

    if(    !ids_file_name.empty()
        && !exclude_ids_file_name.empty() )
    {
        NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                    "only one of -ids or -exclude_ids can be specified" );
    }

    if( !ids_file_name.empty() ) {
        if( text_match ) {
            ids = new CIdSet_TextMatch;
        }else {
            if( iformatstr == "blastdb" ) 
                ids = new CIdSet_SeqId;
            else
                NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                        "-text_match false can be used only with "
                        + string(kInputFormat) + " blastdb" );
        }

        FillIdList( ids_file_name, *ids );
    }

    if( !exclude_ids_file_name.empty() ) {
        if( text_match ) {
            exclude_ids = new CIdSet_TextMatch;
        }else {
            if( iformatstr == "blastdb" ) 
                exclude_ids = new CIdSet_SeqId;
            else
                NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                        "-text_match false can be used only with "
                        + string(kInputFormat) + " blastdb" );
        }

        FillIdList( exclude_ids_file_name, *exclude_ids );
    }

    _TRACE( "Leaving CWinMaskConfig::CWinMaskConfig" );
}

CWinMaskConfig::~CWinMaskConfig()
{
    if ( reader ) {
        delete reader;
    }
    if ( writer ) {
        delete writer;
    }
}

CMaskReader & CWinMaskConfig::Reader() {
    if ( !reader ) {
        NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
            "User options caused reader not to be created; can't get reader" );
    }
    return *reader;
 }

//----------------------------------------------------------------------------
void CWinMaskConfig::FillIdList( const string & file_name, 
                                 CIdSet & id_list )
{
    CNcbiIfstream file( file_name.c_str() );
    string line;

    while( NcbiGetlineEOL( file, line ) ) {
        if( !line.empty() )
        {
            string::size_type stop( line.find_first_of( " \t" ) );
            string::size_type start( line[0] == '>' ? 1 : 0 );
            string id_str = line.substr( start, stop - start );
            id_list.insert( id_str );
        }
    }
}

//----------------------------------------------------------------------------
const char * CWinMaskConfigException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
    case eInputOpenFail: 

        return "can not open input stream";

    case eReaderAllocFail:

        return "can not allocate fasta sequence reader";

    case eInconsistentOptions:

        return "inconsistent program options";

    default: 

        return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
