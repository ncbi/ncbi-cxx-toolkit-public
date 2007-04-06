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
* Author:  Aaron Ucko
*
* File Description:
*   test code for CScope::GetTitle
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objmgr/util/sequence.hpp>

#include <test/test_assert.h>  /* This header must go last */


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);

class CTitleTester : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CTitleTester::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Show a sequence's title", false);

    arg_desc->AddKey("gi", "SeqEntryID", "GI id of the Seq-Entry to examine",
                     CArgDescriptions::eInteger);
    arg_desc->AddFlag("reconstruct", "Reconstruct title");
    arg_desc->AddFlag("accession", "Prepend accession");
    arg_desc->AddFlag("organism", "Append organism name");

    SetupArgDescriptions(arg_desc.release());
}


int CTitleTester::Run(void)
{
    const CArgs&   args = GetArgs();
    CObjectManager objmgr;
    CScope         scope(objmgr);
    CSeq_id        id;
    
    id.SetGi(args["gi"].AsInteger());

    objmgr.RegisterDataLoader(*(new CGBDataLoader), CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle handle = scope.GetBioseqHandle(id);
    TGetTitleFlags flags  = 0;
    if (args["reconstruct"]) {
        flags |= fGetTitle_Reconstruct;
    }
    if (args["organism"]) {
        flags |= fGetTitle_Organism;
    }
    NcbiCout << GetTitle(handle, flags) << NcbiEndl;
    return 0;
}


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return CTitleTester().AppMain(argc, argv);
}
