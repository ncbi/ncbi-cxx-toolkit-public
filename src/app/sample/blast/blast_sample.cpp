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
 * File Description:
 *   Demo of using the CBl2Seq class to compare two sequences using the BLAST
 *   algorithm
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_loc.hpp>

// Object Manager includes
#include <objtools/simple/simple_om.hpp>

// BLAST includes
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/sseqloc.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBlastSampleApplication::


class CBlastSampleApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    enum ESeqType {
        eQuery,
        eSubject
    };
    blast::SSeqLoc* x_CreateSSeqLoc(ESeqType st);
};


blast::SSeqLoc*
CBlastSampleApplication::x_CreateSSeqLoc(CBlastSampleApplication::ESeqType st)
{
    // Get the gi
    int gi;
    if (st == eQuery) {
        gi = GetArgs()["query"].AsInteger();
    } else {
        gi = GetArgs()["subject"].AsInteger();
    }

    CRef<CSeq_loc> seqloc(new CSeq_loc);
    seqloc->SetWhole().SetGi(gi);

    // Setup the scope
    CRef<CScope> scope(CSimpleOM::NewScope());

    return new blast::SSeqLoc(seqloc, scope);
}

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CBlastSampleApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideVersion);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
      "CBl2Seq demo to compare 2 sequences using BLAST");

    // Program type
    arg_desc->AddKey("program", "p", "Type of BLAST program",
            CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("program", &(*new CArgAllow_Strings, 
                "blastp", "blastn", "blastx", "tblastn", "tblastx"));

    // Identifier for the query sequence
    arg_desc->AddKey("query", "QuerySequenceID", 
                     "GI of the query sequence",
                     CArgDescriptions::eInteger);

    // Identifier for the subject sequence
    arg_desc->AddKey("subject", "SubjectSequenceID", 
                     "GI of the subject sequence",
                     CArgDescriptions::eInteger);

    // Output file
    arg_desc->AddDefaultKey("out", "OutputFile", 
        "File name for writing the Seq-align results in ASN.1 form",
        CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run demo


int CBlastSampleApplication::Run(void)
{
    int retval = 0;

    try {

        // Obtain the query, subject, and program from command line
        // arguments...
        auto_ptr<blast::SSeqLoc> query(x_CreateSSeqLoc(eQuery));
        auto_ptr<blast::SSeqLoc> subject(x_CreateSSeqLoc(eSubject));
        blast::EProgram program =
            blast::ProgramNameToEnum(GetArgs()["program"].AsString());

        /// ... and BLAST the sequences!
        blast::CBl2Seq blaster(*query, *subject, program);
        blast::TSeqAlignVector alignments = blaster.Run();

        /// Display the alignments in text ASN.1
        CNcbiOstream& out = GetArgs()["out"].AsOutputFile();
        for (int i = 0; i < (int) alignments.size(); ++i)
            out << MSerial_AsnText << *alignments[i];

    } catch (const CException& e) {
        cerr << e.what() << endl;
        retval = 1;
    }

    return retval;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBlastSampleApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBlastSampleApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
