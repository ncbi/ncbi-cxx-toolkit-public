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
 *   CDustMaskApplication class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>

#include <memory>

#include <corelib/ncbidbg.hpp>
#include <util/line_reader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/seqmasks_io/mask_fasta_reader.hpp>

#include "dust_mask_app.hpp"

// Filtering applications IO
#include <objtools/seqmasks_io/mask_cmdline_args.hpp>
#include <objtools/seqmasks_io/mask_reader.hpp>
#include <objtools/seqmasks_io/mask_writer.hpp>
#include <objtools/seqmasks_io/mask_fasta_reader.hpp>
#include <objtools/seqmasks_io/mask_bdb_reader.hpp>
#include <objtools/seqmasks_io/mask_writer_int.hpp>
#include <objtools/seqmasks_io/mask_writer_fasta.hpp>
#include <objtools/seqmasks_io/mask_writer_seqloc.hpp>
#include <objtools/seqmasks_io/mask_writer_blastdb_maskinfo.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//-------------------------------------------------------------------------
const char * const CDustMaskApplication::USAGE_LINE 
    = "Low complexity region masker based on Symmetric DUST algorithm";

//-------------------------------------------------------------------------
void CDustMaskApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion | fHideDryRun);
    auto_ptr< CArgDescriptions > arg_desc( new CArgDescriptions );
    arg_desc->SetUsageContext( GetArguments().GetProgramBasename(),
                               USAGE_LINE );
    arg_desc->AddDefaultKey( kInput, "input_file_name",
                             "input file name",
                             CArgDescriptions::eInputFile, "-" );
    arg_desc->AddDefaultKey( kOutput, "output_file_name",
                             "output file name",
                             CArgDescriptions::eOutputFile, "-");
    arg_desc->AddDefaultKey( "window", "window_size",
                             "DUST window length",
                             CArgDescriptions::eInteger, "64" );
    arg_desc->AddDefaultKey( "level", "level",
                             "DUST level (score threshold for subwindows)",
                             CArgDescriptions::eInteger, "20" );
    arg_desc->AddDefaultKey( "linker", "linker",
                             "DUST linker (how close masked intervals "
                             "should be to get merged together).",
                             CArgDescriptions::eInteger, "1" );
    arg_desc->AddDefaultKey( kInputFormat, "input_format",
                             "input format (possible values: fasta, blastdb)",
                             CArgDescriptions::eString, *kInputFormats );
    arg_desc->AddDefaultKey( kOutputFormat, "output_format",
                             "output format",
                             CArgDescriptions::eString, *kOutputFormats );
    arg_desc->AddFlag      ( "parse_seqids",
                             "Parse Seq-ids in FASTA input", true );
    CArgAllow_Strings* strings_allowed = new CArgAllow_Strings();
    for (size_t i = 0; i < kNumOutputFormats; i++) {
        strings_allowed->Allow(kOutputFormats[i]);
    }
    strings_allowed->Allow("acclist");
    arg_desc->SetConstraint( kOutputFormat, strings_allowed );

    SetupArgDescriptions( arg_desc.release() );
}

CMaskWriter*
CDustMaskApplication::x_GetWriter()
{
    const CArgs& args = GetArgs();
    const string& format(args[kOutputFormat].AsString());
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
    } else if (NStr::StartsWith(format, "maskinfo_asn1_bin")) {
        CNcbiOstream& output = args[kOutput].AsOutputFile(CArgValue::fBinary);
        retval = 
            new CMaskWriterBlastDbMaskInfo(output, format, 2,
                               eBlast_filter_program_dust,
                               BuildAlgorithmParametersString(args));
    } else if (NStr::StartsWith(format, "maskinfo_asn1")) {
        CNcbiOstream& output = args[kOutput].AsOutputFile();
        retval = 
            new CMaskWriterBlastDbMaskInfo(output, format, 2,
                               eBlast_filter_program_dust,
                               BuildAlgorithmParametersString(args));
    } else {

        throw runtime_error("Unknown output format");
    }
    return retval;
}

CMaskReader * CDustMaskApplication::x_GetReader()
{
    const CArgs & args( GetArgs() );
    const string & format( args[kInputFormat].AsString() );
    
    if( format == "fasta" ) {
        CNcbiIstream& input_stream  = GetArgs()[kInput].AsInputFile();
        return new CMaskFastaReader( 
                input_stream, true, args["parse_seqids"] );
    }
    else if( format == "blastdb" ) {
        return new CMaskBDBReader( args[kInput].AsString() );
    }
    else {
        throw runtime_error( "Unknown input format" );
    }

    return 0;
}

//-------------------------------------------------------------------------
int CDustMaskApplication::Run (void)
{
    // Set up the input and output streams.
    CNcbiOstream& output_stream = GetArgs()[kOutput].AsOutputFile();

    // Set up the object manager.
    CRef<CObjectManager> om(CObjectManager::GetInstance());

    // Set up the duster object.
    Uint4 level = GetArgs()["level"].AsInteger();
    duster_type::size_type window = GetArgs()["window"].AsInteger();
    duster_type::size_type linker = GetArgs()["linker"].AsInteger();
    duster_type duster( level, window, linker );

    // Now process each input sequence in a loop.
    CRef< CSeq_entry > aSeqEntry( 0 );
    auto_ptr<CMaskWriter> writer(x_GetWriter());
    CMaskReader * reader = x_GetReader();

    while( (aSeqEntry = reader->GetNextSequence()).NotEmpty() )
    {
        CScope scope( *om );
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry( *aSeqEntry );

        CBioseq_CI bs_iter(seh, CSeq_inst::eMol_na);

        for ( ;  bs_iter;  ++bs_iter) 
        {
            CBioseq_Handle bsh = *bs_iter;

            if (bsh.GetBioseqLength() == 0) 
                continue;

            CSeqVector data 
                = bsh.GetSeqVector( CBioseq_Handle::eCoding_Iupac );
            std::auto_ptr< duster_type::TMaskList > res = duster( data );
            if (res.get()) {
                writer->Print(bsh, *res, GetArgs()["parse_seqids"] );
            }
        }
    }

    output_stream << flush;
    return 0;
}

END_NCBI_SCOPE
