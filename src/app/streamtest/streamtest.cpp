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

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objectio.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#include "process.hpp"
#include "process_defline.hpp"
#include "processor.hpp"
#include "processor_releasefile.hpp"
#include "processor_seqset.hpp"

//  ============================================================================
class CStreamTestApp : public CNcbiApplication
//  ============================================================================
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    CBioseqProcess* GetProcess(
        const CArgs& );
    CBioseqProcessor* GetProcessor(
        const CArgs& );
};


//  ============================================================================
void CStreamTestApp::Init()
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext( GetArguments().GetProgramBasename(),
        "Object Manager Stream Test" );

    arg_desc->AddFlag( "id-handle",
        "Cache seq-ids using CSeq_id_Handle (applies to mode=raw only)" );

    arg_desc->AddFlag( "fresh-scope",
        "Use a fresh scope each time (for mode={get-title|seqdesc-ci} only)" );

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

    arg_desc->AddKey( "test", 
        "TestCase",
        "Mode for generation",
        CArgDescriptions::eString );

    arg_desc->AddFlag( "batch",
        "Process genbank release file" );

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

    SetupArgDescriptions(arg_desc.release());
}


//  ============================================================================
int CStreamTestApp::Run(void)
//  ============================================================================
{
    const CArgs& args = GetArgs();

    CBioseqProcess* pProcess = 0;
    CBioseqProcessor* pProcessor = 0;

    pProcess = GetProcess( args );
    pProcessor = GetProcessor( args );
    if ( pProcess == 0 || pProcessor == 0 ) {
        LOG_POST( Error << "Not yet implemented!" );
        return 1;
    }
    
    pProcessor->Run( pProcess );

    delete pProcessor;
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
CBioseqProcess*
CStreamTestApp::GetProcess(
    const CArgs& args )
//  ============================================================================
{
    string testcase = args["test"].AsString();

    CBioseqProcess* pProcess = 0;
    if ( testcase == "null"  ) {
        pProcess = new CBioseqProcess;
    }
    if ( testcase == "defline" ) {
        pProcess = new CDeflineProcess;
    }
    if ( 0 != pProcess ) {
        pProcess->Initialize( args );
    }
    return pProcess;
}

//  ============================================================================
CBioseqProcessor*
CStreamTestApp::GetProcessor(
    const CArgs& args )
//  ============================================================================
{
    CBioseqProcessor* pProcessor = 0;
    if ( args["batch"] ) {
        pProcessor = new CReleaseFileProcessor;
    }
    else {
        pProcessor = new CSeqSetProcessor;
    }
    if ( pProcessor ) {
        pProcessor->Initialize( args );
    }
    return pProcessor;
}


//  ============================================================================
int main(int argc, const char* argv[])
//  ============================================================================
{
    // Execute main application function
    return CStreamTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
