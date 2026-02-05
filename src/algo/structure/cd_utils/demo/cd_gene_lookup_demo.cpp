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
* Authors:  Eyal Mozes
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <algo/structure/cd_utils/cuGiLookup.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(cd_utils);

class GiLookupDemoApp : public ncbi::CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};

void GiLookupDemoApp::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                        "Test CDGiLookup class");

    arg_desc->AddKey("bioseqs", "Bioseqs",
                     "Set of bioseqs to transform",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey("aligns", "Alignments",
                     "Set of alignments to transform",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey("obioseqs", "BioseqsOutput",
                     "Transformed bioseqs",
                     CArgDescriptions::eOutputFile);

    arg_desc->AddKey("oaligns", "AlignmentsOutput",
                     "Transformed alignments",
                     CArgDescriptions::eOutputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int GiLookupDemoApp::Run(void)
{
    const CArgs& args = GetArgs();
    vector< CRef< CBioseq > > bioseqs;
    unique_ptr<CObjectIStream> is(
        CObjectIStream::Open(eSerial_AsnText, args["bioseqs"].AsInputFile()));
    while (!is->EndOfData()) {
        CRef<CBioseq> bioseq(new CBioseq);
        *is >> *bioseq;
        bioseqs.push_back(bioseq);
    }

    CDGiLookup lookup;
    lookup.InsertGis(bioseqs);

    unique_ptr<CObjectOStream> os(
        CObjectOStream::Open(eSerial_AsnText, args["obioseqs"].AsOutputFile()));
    for  (const CRef<CBioseq> &bioseq : bioseqs) {
        *os << *bioseq;
    }

    is.reset(
        CObjectIStream::Open(eSerial_AsnText, args["aligns"].AsInputFile()));
    os.reset(
        CObjectOStream::Open(eSerial_AsnText, args["oaligns"].AsOutputFile()));
    while (!is->EndOfData()) {
        CSeq_align_set align_set;
        *is >> align_set;
        lookup.InsertGis(align_set);
        *os << align_set;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    SetDiagStream(&NcbiCerr); // send all diagnostic messages to cerr
    SetDiagPostLevel(eDiag_Info);   // show all messages

    GiLookupDemoApp app;
    return app.AppMain(argc, argv, NULL, eDS_Default, NULL);    // don't use config file
}
