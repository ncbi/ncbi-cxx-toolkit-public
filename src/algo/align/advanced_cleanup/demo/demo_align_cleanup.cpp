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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 * Assumptions:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <util/stream_source.hpp>

#include <algo/align/advanced_cleanup/advanced_cleanup.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);


class CAlignCleanupApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


void CAlignCleanupApplication::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Assembly alignment cleanup program");

    CInputStreamSource::SetStandardInputArgs(*arg_desc);
    
    arg_desc->AddDefaultKey("ifmt", "InputFormat",
                            "Format for input file",
                            CArgDescriptions::eString,
                            "seq-align");
    arg_desc->SetConstraint("ifmt",
                            &(*new CArgAllow_Strings,
                              "seq-align", "seq-align-set", "seq-annot"));

    arg_desc->AddDefaultKey("query-type", "QueryType",
                            "Type of query sequences",
                            CArgDescriptions::eString,
                            "auto");
    arg_desc->SetConstraint("query-type",
                            &(*new CArgAllow_Strings,
                              "auto", "genomic", "rna", "protein"));

    CAdvancedAlignCleanup::SetupArgDescriptions(*arg_desc);

    arg_desc->AddDefaultKey
        ("splign-direction", "SplignDirection", "Query direction for splign",
         CArgDescriptions::eString, "sense");
    arg_desc->SetConstraint("splign-direction",
                           &(*new CArgAllow_Strings,
                             "sense", "antisense", "both"));

    arg_desc->AddFlag("with-best-placement",
                     "Invoke best placement algorithm on alignments.");


    arg_desc->AddDefaultKey("o", "Output",
                            "Output cleaned alignments",
                            CArgDescriptions::eOutputFile, "-");
    arg_desc->AddAlias("output", "o");

    arg_desc->AddFlag("t",
                       "Both input and output data streams are "
                       "ASN.1 text, not binary.");
    arg_desc->AddFlag("it", "Input data streams are ASN.1 text, not binary.");
    arg_desc->AddFlag("ot", "Output data streams are ASN.1 text, not binary.");

    // standard gpipe app arguments
    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CAlignCleanupApplication::Run(void)
{
    const CArgs& args = GetArgs();

    // setup object manager
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CDataLoadersUtil::SetupObjectManager(args, *om);

    // setup scope
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    // text or binary
    ESerialDataFormat input_fmt, output_fmt;
    if(args["t"]) {
        input_fmt = output_fmt = eSerial_AsnText;
    } else {
        input_fmt = args["it"] ? eSerial_AsnText : eSerial_AsnBinary;
        output_fmt = args["ot"] ? eSerial_AsnText : eSerial_AsnBinary;
    }

    CAdvancedAlignCleanup::EQueryType query_type = CAdvancedAlignCleanup::eInfer;
    if (args["query-type"].AsString() == "genomic") {
        query_type = CAdvancedAlignCleanup::eGenomic;
    } else if (args["query-type"].AsString() == "rna") {
        query_type = CAdvancedAlignCleanup::eRna;
    } else if (args["query-type"].AsString() == "protein") {
        query_type = CAdvancedAlignCleanup::eProtein;
    }

    CAdvancedAlignCleanup::ESplignDirRun splign_dir =
         CAdvancedAlignCleanup::eDirBoth;
    if (args["splign-direction"].AsString() == "sense") {
        splign_dir = CAdvancedAlignCleanup::eDirSense;
    } else if (args["splign-direction"].AsString() == "antisense") {
        splign_dir = CAdvancedAlignCleanup::eDirAntisense;
    }

    //
    // read alignments in, and collate by sequence pair
    //
    CSeq_align_set::Tdata input_aligns;
    CSeq_align_set::Tdata cleaned_aligns;
    for (CInputStreamSource stream_source(args); stream_source; ++stream_source)
    {
        unique_ptr<CObjectIStream> is(
            CObjectIStream::Open(input_fmt, *stream_source));
        while (!is->EndOfData()) {
            if (args["ifmt"].AsString() == "seq-align") {
                CRef<CSeq_align> align(new CSeq_align);
                *is >> *align;
                input_aligns.push_back(align);
            } else if (args["ifmt"].AsString() == "seq-align-set") {
                CRef<CSeq_align_set> align(new CSeq_align_set);
                *is >> *align;
                input_aligns.insert(input_aligns.end(),
                                    align->Get().begin(), align->Get().end());
            } else if (args["ifmt"].AsString() == "seq-annot") {
                CRef<CSeq_annot> input_annot(new CSeq_annot);
                *is >> *input_annot;
                input_aligns.insert(input_aligns.end(),
                                    input_annot->GetData().GetAlign().begin(),
                                    input_annot->GetData().GetAlign().end());
            }
        }
    }

    CAdvancedAlignCleanup cleanup;
    cleanup.SetParams(args);
    cleanup.SetScope(scope);
    cleanup.Cleanup(input_aligns, cleaned_aligns, query_type,
                    args["with-best-placement"], false, splign_dir);

    unique_ptr<CObjectOStream> os(
        CObjectOStream::Open(output_fmt, args["o"].AsOutputFile()));
    ITERATE (CSeq_align_set::Tdata, it, cleaned_aligns) {
        *os << **it;
    }

    return 0;
}


void 
CAlignCleanupApplication::Exit(void)
{
}


int 
main(int argc, const char* argv[])
{
    // Execute main application function
    return CAlignCleanupApplication().AppMain(argc, argv);
}
