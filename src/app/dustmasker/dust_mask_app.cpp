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
static CRef< CSeq_entry > GetNextSequence( CNcbiIstream * input_stream )
{
    if( input_stream == 0 )
        return CRef< CSeq_entry >( 0 );

    CStreamLineReader line_reader( *input_stream );

    CFastaReader::TFlags flags = 
        CFastaReader::fAssumeNuc |
        CFastaReader::fForceType |
        CFastaReader::fOneSeq    |
        CFastaReader::fAllSeqIds;

    CFastaReader fasta_reader( line_reader, flags );
    CFastaReader fasta_reader_2( 
            line_reader, flags|CFastaReader::fNoParseID );

    while( !input_stream->eof() )
    {
        CRef< CSeq_entry > aSeqEntry( null );
        CT_POS_TYPE pos = input_stream->tellg();

        try{ 
            aSeqEntry = fasta_reader.ReadSet( 1 );
        }catch( ... ) {
            input_stream->seekg( pos );
            aSeqEntry = fasta_reader_2.ReadSet( 1 );
        }

        if(    aSeqEntry != 0
            && aSeqEntry->IsSeq()
            && aSeqEntry->GetSeq().IsNa() )
            return aSeqEntry;
    }

    return CRef< CSeq_entry >( 0 );
}

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
                             CArgDescriptions::eString, "" );
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
    arg_desc->AddDefaultKey( kOutputFormat, "output_format",
                             "output format",
                             CArgDescriptions::eString, *kOutputFormats );
    CArgAllow_Strings* strings_allowed = new CArgAllow_Strings();
    for (size_t i = 0; i < kNumOutputFormats; i++) {
        strings_allowed->Allow(kOutputFormats[i]);
    }
    strings_allowed->Allow("acclist");
    arg_desc->SetConstraint( kOutputFormat, strings_allowed );

    SetupArgDescriptions( arg_desc.release() );
}

#if 0
//-------------------------------------------------------------------------
void CDustMaskApplication::interval_out_handler( 
        CNcbiOstream * output_stream, 
        const objects::CBioseq_Handle & bsh, 
        const duster_type::TMaskList & res )
{
    if( output_stream != 0 ) {
        *output_stream << ">"
                       << CSeq_id::GetStringDescr( 
                               *bsh.GetCompleteBioseq(),
                               CSeq_id::eFormat_FastA )
                       << " " << sequence::GetTitle( bsh ) << "\n";

        for( it_type it = res.begin(); it != res.end(); ++it ) {
            *output_stream << it->first  << " - " 
                           << it->second << "\n";
        }
    }
}

//-------------------------------------------------------------------------
void CDustMaskApplication::acclist_out_handler( 
        CNcbiOstream * output_stream, 
        const objects::CBioseq_Handle & bsh, 
        const duster_type::TMaskList & res )
{
    if( output_stream != 0 ) {
        for( it_type it = res.begin(); it != res.end(); ++it ) {
            *output_stream << CSeq_id::GetStringDescr(
                    *bsh.GetCompleteBioseq(), CSeq_id::eFormat_FastA )
                           << "\t" << it->first 
                           << "\t" << it->second << "\n";
        }
    }
}

//-------------------------------------------------------------------------
static const Uint4 LINE_WIDTH = 60;

inline void CDustMaskApplication::write_normal( 
        CNcbiOstream * output_stream, const objects::CSeqVector & data, 
        TSeqPos & start, TSeqPos & stop )
{
    for( Uint4 count = start; count < stop; ++count ) {
        *output_stream << data[count];

        if( (count + 1)%LINE_WIDTH == 0 ) {
            *output_stream << "\n";
        }
    }
}

inline void CDustMaskApplication::write_lowerc( 
        CNcbiOstream * output_stream, const objects::CSeqVector & data,
        TSeqPos & start, TSeqPos & stop )
{
    for( Uint4 count = start; count < stop; ++count ) {
        *output_stream << (char)tolower( data[count] );

        if( (count + 1)%LINE_WIDTH == 0 ) {
            *output_stream << "\n";
        }
    }
}

//-------------------------------------------------------------------------
void CDustMaskApplication::fasta_out_handler( 
        CNcbiOstream * output_stream, 
        const objects::CBioseq_Handle & bsh, 
        const duster_type::TMaskList & res )
{
    if( output_stream != 0 ) {
        *output_stream << ">"
                       << CSeq_id::GetStringDescr( 
                               *bsh.GetCompleteBioseq(),
                               CSeq_id::eFormat_FastA )
                       << " " << sequence::GetTitle( bsh ) << "\n";
        CSeqVector data 
            = bsh.GetSeqVector( CBioseq_Handle::eCoding_Iupac );
        typedef CSeqVector::const_iterator citer_type;
        TSeqPos start = 0, stop;

        for( it_type it = res.begin(); it != res.end(); ++it ) {
            stop = it->first;
            write_normal( output_stream, data, start, stop );
            start = stop; stop = it->second + 1;
            write_lowerc( output_stream, data, start, stop );
            start = stop;
        }

        stop = data.size();
        write_normal( output_stream, data, start, stop );

        if( stop%LINE_WIDTH != 0 ) {
            *output_stream << endl;
        }else {
            *output_stream << flush;
        }
    }
}
#endif

CMaskWriter*
CDustMaskApplication::x_GetWriter()
{
    const CArgs& args = GetArgs();
    const string& format(args[kOutputFormat].AsString());
    CNcbiOstream& output = args[kOutput].AsOutputFile();
    CMaskWriter* retval = NULL;

    if (format == "interval") {
        retval = new CMaskWriterInt(output);
    } else if (format == "fasta") {
        retval = new CMaskWriterFasta(output);
    } else if (NStr::StartsWith(format, "seqloc_")) {
        retval = new CMaskWriterSeqLoc(output, format);
    } else if (NStr::StartsWith(format, "maskinfo_")) {
        retval = 
            new CMaskWriterBlastDbMaskInfo(output, format, 2,
                               eBlast_filter_program_dust,
                               BuildAlgorithmParametersString(args));
    } else {
        throw runtime_error("Unknown output format");
    }
    return retval;
}

//-------------------------------------------------------------------------
int CDustMaskApplication::Run (void)
{
    // Set up the input and output streams.
    auto_ptr< CNcbiIstream > input_stream_ptr;
    auto_ptr< CNcbiOstream > output_stream_ptr;
    CNcbiIstream * input_stream = NULL;
    CNcbiOstream * output_stream = NULL;
    
    if( GetArgs()[kInput].AsString().empty() )
        input_stream = &cin;
    else
    {
        input_stream_ptr.reset(
            new CNcbiIfstream( GetArgs()[kInput].AsString().c_str() ) );
        input_stream = input_stream_ptr.get();
    }

    if( GetArgs()[kOutput].AsString().empty() )
        output_stream = &cout;
    else
    {
        output_stream_ptr.reset(
            new CNcbiOfstream( GetArgs()[kOutput].AsString().c_str() ) );
        output_stream = output_stream_ptr.get();
    }

    // Set up the object manager.
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    // CGBDataLoader::RegisterInObjectManager(
    //    *om, "id2", CObjectManager::eDefault);

    // Set up the duster object.
    Uint4 level = GetArgs()["level"].AsInteger();
    duster_type::size_type window = GetArgs()["window"].AsInteger();
    duster_type::size_type linker = GetArgs()["linker"].AsInteger();
    duster_type duster( level, window, linker );

    // Now process each input sequence in a loop.
    CRef< CSeq_entry > aSeqEntry( 0 );
    auto_ptr<CMaskWriter> writer(x_GetWriter());

    while( (aSeqEntry = GetNextSequence( input_stream )).NotEmpty() )
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
                writer->Print(bsh, *res);
            }

            NcbiCerr << "." << flush;
        }
    }

    *output_stream << flush;
    NcbiCerr << endl;
    return 0;
}

END_NCBI_SCOPE
