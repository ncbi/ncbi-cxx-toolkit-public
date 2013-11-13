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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objectio.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/gb_release_file.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <objects/valerr/ValidError.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/validator/validator.hpp>
#include <objtools/writers/agp_write.hpp>

#include <algo/align/prosplign/prosplign.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);
USING_SCOPE(validator);

#include "process.hpp"
#include "process_scoped.hpp"
#include "process_null.hpp"
#include "process_agpwrite.hpp"
#include "process_cleanup.hpp"
#include "process_defline.hpp"
#include "process_eutils.hpp"
#include "process_fasta.hpp"
#include "process_gene_overlap.hpp"
#include "process_macrotest.hpp"
#include "process_prosplign.hpp"
#include "process_seqvector.hpp"
#include "process_title.hpp"
#include "process_validate.hpp"
#include "presenter.hpp"
#include "presenter_releasefile.hpp"
#include "presenter_seqset.hpp"

//  ============================================================================
class CStreamTestApp : public CNcbiApplication
//  ============================================================================
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    CSeqEntryProcess* GetProcess(
        const CArgs& );
    CSeqEntryPresenter* GetPresenter(
        const CArgs& );
};


//  ============================================================================
void CStreamTestApp::Init()
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext( GetArguments().GetProgramBasename(),
        "Object Manager Stream Test" );

    /*
     arg_desc->AddFlag( "id-handle",
        "Cache seq-ids using CSeq_id_Handle (applies to mode=raw only)" );

    arg_desc->AddFlag( "fresh-scope",
        "Use a fresh scope each time (for mode={get-title|seqdesc-ci} only)" );
    */

    //  ------------------------------------------------------------------------
    //  New set of parameters:
    //  ------------------------------------------------------------------------
    arg_desc->AddDefaultKey( "i", 
        "InputFile",
        "Bioseq-set to test",
        CArgDescriptions::eInputFile,
        "-",
        CArgDescriptions::fBinary);

    arg_desc->AddDefaultKey( "o", 
        "OutputFile",
        "Formatted data",
        CArgDescriptions::eOutputFile,
        "-");

    arg_desc->AddFlag( "binary",
        "Input is binary ASN" );

    arg_desc->AddFlag( "compressed",
        "Input is compressed" );

    arg_desc->AddFlag( "cleanup",
        "BasicCleanup each record" );

    arg_desc->AddDefaultKey( "skip", 
        "Skip",
        "Skip certain record types",
        CArgDescriptions::eString,
        "" );

    arg_desc->AddKey( "test", 
        "TestCase",
        "Mode for generation",
        CArgDescriptions::eString );
     arg_desc->SetConstraint( "test", &(*new CArgAllow_Strings,
                                        "null",
                                        "agpwrite",
                                        "cleanup",
                                        "defline",
                                        "deprecated-title",
                                        "eutils",
                                        "fasta",
                                        "gene-overlap",
                                        "gpipe-defline",
                                        "macrotest",
                                        "prosplign",
                                        "seqvector",
                                        "unindexed-defline",
                                        "validate"));
    
    arg_desc->AddDefaultKey( "options", 
        "Options",
        "Test-specific options. E.g. 'map' for agpwrite tests "
        "the comp id mapper ",
        CArgDescriptions::eString,
        "" );

    arg_desc->AddFlag( "batch",
        "Process genbank release file" );

    arg_desc->AddFlag("gbload",
        "Use GenBank data loader");

    arg_desc->AddFlag( "rf", 
        "Generate final report" );

    arg_desc->AddDefaultKey( "ri", 
        "ReportInterval", 
        "Generate report after every N objects processed",
        CArgDescriptions::eInteger,
        "0" );

    arg_desc->AddDefaultKey( "count",
        "RepCount",
        "Repeat process N times",
        CArgDescriptions::eInteger,
        "1" );

    arg_desc->AddDefaultKey( "journal", 
        "JournalName",
        "Test Journal Lookup",
        CArgDescriptions::eString,
        "" );

    SetupArgDescriptions(arg_desc.release());
}


//  ============================================================================
int CStreamTestApp::Run(void)
//  ============================================================================
{
    const CArgs& args = GetArgs();

    CSeqEntryProcess* pProcess = 0;
    CSeqEntryPresenter* pPresenter = 0;

    pProcess = GetProcess( args );
    pPresenter = GetPresenter( args );
    if ( pProcess == 0 || pPresenter == 0 ) {
        LOG_POST( Error << "Not yet implemented!" );
        return 1;
    }

    pPresenter->Initialize( args );

    pProcess->ProcessInitialize( args );
    pPresenter->Run( pProcess );
    pProcess->ProcessFinalize();

    pPresenter->Finalize( args );

    delete pPresenter;
    delete pProcess;
    return 0;
}


//  ============================================================================
void CStreamTestApp::Exit(void)
//  ============================================================================
{
    SetDiagStream(0);
}

//  ============================================================================
CSeqEntryProcess*
CStreamTestApp::GetProcess(
    const CArgs& args )
//  ============================================================================
{
    string testcase = args["test"].AsString();

    CSeqEntryProcess* pProcess = 0;
    if ( testcase == "null" ) {
        pProcess = new CNullProcess;
    }
    if ( testcase == "agpwrite" ) {
        pProcess = new CAgpwriteProcess( args["options"].AsString() );
    }
    if ( testcase == "cleanup" ) {
        pProcess = new CCleanupProcess;
    }
    if ( testcase == "defline" ) {
        pProcess = new CDeflineProcess (true);
    }
    if ( testcase == "deprecated-title" ) {
        pProcess = new CTitleProcess;
    }
    if ( testcase == "eutils" ) {
        pProcess = new CEUtilsProcess;
    }
    if ( testcase == "fasta" ) {
        pProcess = new CFastaProcess;
    }
    if ( testcase == "gene-overlap" ) {
        pProcess = new CGeneOverlapProcess;
    }
    if ( testcase == "gpipe-defline" ) {
        pProcess = new CDeflineProcess (true, true);
    }
    if ( testcase == "macrotest" ) {
        pProcess = new CMacroTestProcess;
    }
    if ( testcase == "prosplign" ) {
        pProcess = new CProsplignProcess;
    }
    if ( testcase == "seqvector" ) {
        pProcess = new CSeqVectorProcess;
    }
    if ( testcase == "unindexed-defline" ) {
        pProcess = new CDeflineProcess;
    }
    if ( testcase == "validate" ) {
        pProcess = new CValidateProcess;
    }
    return pProcess;
}

//  ============================================================================
CSeqEntryPresenter*
CStreamTestApp::GetPresenter(
    const CArgs& args )
//  ============================================================================
{
    CSeqEntryPresenter* pPresenter = 0;
    if ( args["batch"] ) {
        pPresenter = new CReleaseFilePresenter;
    }
    else {
        pPresenter = new CSeqSetPresenter;
    }
    return pPresenter;
}


//  ============================================================================
int main(int argc, const char* argv[])
//  ============================================================================
{
    // Execute main application function
    return CStreamTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
