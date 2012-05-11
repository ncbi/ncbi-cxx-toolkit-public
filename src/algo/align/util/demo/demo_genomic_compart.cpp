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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/align/util/genomic_compart.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//  CTestCompartApplication::


class CTestCompartApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CTestCompartApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddDefaultKey("i", "InputFile",
                            "File of ASN text Seq-aligns",
                            CArgDescriptions::eInputFile,
                            "-");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "File of results",
                            CArgDescriptions::eOutputFile,
                            "-");

    arg_desc->AddFlag("allow-intersect",
                      "Permit compartments to contain intersecting alignments "
                      "provided the alignments are consistent");

    arg_desc->AddFlag("allow-intersect-query",
                      "Permit compartments to contain intersecting alignments "
                      "for the query only");

    arg_desc->AddFlag("allow-intersect-subject",
                      "Permit compartments to contain intersecting alignments "
                      "for the subject only");

    arg_desc->AddFlag("allow-intersect-both",
                      "Permit compartments to contain intersecting alignments "
                      "only if both query and subject intersect");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTestCompartApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    list< CRef<CSeq_align> > aligns;
    {{
         CNcbiIstream& istr = args["i"].AsInputFile();
         auto_ptr<CObjectIStream> is
             (CObjectIStream::Open(eSerial_AsnText, istr));
         while ( !is->EndOfData() ) {
             CRef<CSeq_align> align(new CSeq_align);
             *is >> *align;
             aligns.push_back(align);
         }
     }}

    CNcbiOstream& ostr = args["o"].AsOutputFile();

    TCompartOptions opts = fCompart_Defaults;
    if (args["allow-intersect"]) {
        opts |= fCompart_AllowIntersections;
    }
    if (args["allow-intersect-query"]) {
        opts |= fCompart_AllowIntersectionsQuery;
    }
    if (args["allow-intersect-subject"]) {
        opts |= fCompart_AllowIntersectionsSubject;
    }
    if (args["allow-intersect-both"]) {
        opts |= fCompart_AllowIntersectionsBoth;
    }

    list< CRef<CSeq_align_set> > compartments;
    FindCompartments(aligns, compartments, opts);

    LOG_POST(Error << "input alignments: " << aligns.size());
    LOG_POST(Error << "output compartments: " << compartments.size());

    size_t count = 0;
    NON_CONST_ITERATE (list< CRef<CSeq_align_set> >, i, compartments) {
        ++count;
        CSeq_align align;
        align.SetSegs().SetDisc(**i);

        TSeqRange r1 = align.GetSeqRange(0);
        TSeqRange r2 = align.GetSeqRange(1);
        CSeq_id_Handle id1 = CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
        CSeq_id_Handle id2 = CSeq_id_Handle::GetHandle(align.GetSeq_id(1));

        LOG_POST(Error << "compartment " << count << ":");
        LOG_POST(Error << "  query   = " << id1 << " (" << r1.GetFrom() + 1 << ".." << r1.GetTo() + 1 << ")");
        LOG_POST(Error << "  subject = " << id2 << " (" << r2.GetFrom() + 1 << ".." << r2.GetTo() + 1 << ")");
        LOG_POST(Error << "  alignments = " << (*i)->Get().size());

        string title("Compartment ");
        title += NStr::NumericToString(count);
        CSeq_annot annot;
        annot.SetNameDesc(title);
        annot.SetTitleDesc(title);
        annot.SetData().SetAlign() = (*i)->Get();
        ostr << MSerial_AsnText << annot;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CTestCompartApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestCompartApplication().AppMain(argc, argv);
}
