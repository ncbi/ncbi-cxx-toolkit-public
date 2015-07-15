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
 * Author:  Christiam Camacho
 *
 */

/** @file segmasker.cpp
 * SEG filtering application
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_loc.hpp>

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

// Object manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <algo/segmask/segmask.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(objects);
#endif /* SKIP_DOXYGEN_PROCESSING */

/////////////////////////////////////////////////////////////////////////////
//  SegMaskerApplication::


class SegMaskerApplication : public CNcbiApplication
{
public:
    /// Application constructor
    SegMaskerApplication() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(1, 0, 0);
        SetFullVersion(version);
    }

private:
    /** @inheritDoc */
    virtual void Init(void);
    /** @inheritDoc */
    virtual int  Run(void);
    /** @inheritDoc */
    virtual void Exit(void);

    /// Retrieves the sequence reader interface for the application
    CMaskReader* x_GetReader();
    /// Retrieves the output writer interface for the application
    CMaskWriter* x_GetWriter();

    /// Contains the description of this application
    static const char * const USAGE_LINE;
};

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments

const char * const SegMaskerApplication::USAGE_LINE 
    = "Low complexity region masker based on the SEG algorithm";

void SegMaskerApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion | fHideDryRun);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              USAGE_LINE);

    arg_desc->SetCurrentGroup("Input/output options");
    arg_desc->AddDefaultKey(kInput, "input_file_name",
                            "input file name",
                            CArgDescriptions::eInputFile, "-");
    arg_desc->AddDefaultKey(kOutput, "output_file_name",
                            "output file name",
                            CArgDescriptions::eOutputFile, "-");
    arg_desc->AddDefaultKey(kInputFormat, "input_format",
                            "controls the format of the masker input",
                            CArgDescriptions::eString, *kInputFormats);
    CArgAllow_Strings* strings_allowed = new CArgAllow_Strings();
    for (size_t i = 0; i < kNumInputFormats; i++) {
        strings_allowed->Allow(kInputFormats[i]);
    }
    arg_desc->SetConstraint(kInputFormat, strings_allowed);
    arg_desc->AddFlag      ( "parse_seqids",
                             "Parse Seq-ids in FASTA input", true );

    arg_desc->AddDefaultKey(kOutputFormat, "output_format",
                            "controls the format of the masker output",
                            CArgDescriptions::eString, *kOutputFormats);
    strings_allowed = new CArgAllow_Strings();
    for (size_t i = 0; i < kNumOutputFormats; i++) {
        strings_allowed->Allow(kOutputFormats[i]);
    }
    arg_desc->SetConstraint(kOutputFormat, strings_allowed);

    arg_desc->SetCurrentGroup("SEG algorithm options");
    arg_desc->AddDefaultKey("window", "integer_value", "SEG window",
                            CArgDescriptions::eInteger,
                            NStr::IntToString(kSegWindow));
    arg_desc->AddDefaultKey("locut", "float_value", "SEG locut",
                            CArgDescriptions::eDouble,
                            NStr::DoubleToString(kSegLocut));
    arg_desc->AddDefaultKey("hicut", "float_value", "SEG hicut",
                            CArgDescriptions::eDouble,
                            NStr::DoubleToString(kSegHicut));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

CMaskReader*
SegMaskerApplication::x_GetReader()
{
    const CArgs& args = GetArgs();
    const string& format(args[kInputFormat].AsString());
    CMaskReader* retval = NULL;

    if (format == "fasta") {
        CNcbiIstream& input = args[kInput].AsInputFile();
        retval = new CMaskFastaReader(input, false, args["parse_seqids"]);
    } else if (format == "blastdb") {
        retval = new CMaskBDBReader(args[kInput].AsString(), false);
    } else {
        _ASSERT("Unknown input format" == 0);
    }
    return retval;
}

CMaskWriter*
SegMaskerApplication::x_GetWriter()
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
            new CMaskWriterBlastDbMaskInfo(output, format, 1,
                               eBlast_filter_program_seg,
                               BuildAlgorithmParametersString(args));
    } else if (NStr::StartsWith(format, "maskinfo_")) {
        CNcbiOstream& output = args[kOutput].AsOutputFile();
        retval = 
            new CMaskWriterBlastDbMaskInfo(output, format, 1,
                               eBlast_filter_program_seg,
                               BuildAlgorithmParametersString(args));
    } else {
        throw runtime_error("Unknown output format");
    }
    return retval;
}

/////////////////////////////////////////////////////////////////////////////
//  Run demo


int SegMaskerApplication::Run(void)
{
    int retval = 0;
    const CArgs& args = GetArgs();

    try {

        CRef<CObjectManager> objmgr(CObjectManager::GetInstance());

        CSegMasker masker(args["window"].AsInteger(),
                          args["locut"].AsDouble(),
                          args["hicut"].AsDouble());

        CRef<CSeq_entry> seq_entry;
        auto_ptr<CMaskReader> reader(x_GetReader());
        auto_ptr<CMaskWriter> writer(x_GetWriter());

        while ( (seq_entry = reader->GetNextSequence()).NotEmpty() ) {

        	// Allow skipping of oid
        	if(seq_entry->Which() == CSeq_entry::e_not_set)
        		continue;

            CScope scope(*objmgr);
            CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*seq_entry);
            CBioseq_Handle bioseq_handle = seh.GetSeq();
            CSeqVector sequence_data = 
                bioseq_handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
            auto_ptr<CSegMasker::TMaskList> masks(masker(sequence_data));
            writer->Print(bioseq_handle, *masks, GetArgs()["parse_seqids"]);
            // writer->Print(bioseq_handle, *masks);

        }

    } catch (const CException& e) {
        cerr << e.what() << endl;
        retval = 1;
    }

    return retval;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void SegMaskerApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    // Execute main application function
    return SegMaskerApplication().AppMain(argc, argv, 0, eDS_Default, "");
}
#endif /* SKIP_DOXYGEN_PROCESSING */

