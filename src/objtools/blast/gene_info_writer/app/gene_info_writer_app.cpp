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
 * Author:  Vahram Avagyan
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objtools/blast/gene_info_writer/gene_info_writer.hpp>

USING_NCBI_SCOPE;

//==========================================================================//

class CProcessFilesApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};

//==========================================================================//

void CProcessFilesApp::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
      "The program transforms the original Gene files into binary "
      "Gene Info files used by the Gene Info reader library.");

    // Input gene->accession file name
    arg_desc->AddDefaultKey ("gene2accession", "GeneToAccessionInputFile", 
        "The original \"Gene to Accession\" input file",
        CArgDescriptions::eString,
        "gene2accession");

    // Input gene info file name
    arg_desc->AddDefaultKey ("gene_info", "GeneInfoInputFile", 
        "The original \"Gene Info\" input file",
        CArgDescriptions::eString,
        "gene_info");

    // Input gene->PubMed file name
    arg_desc->AddDefaultKey ("gene2pubmed", "GeneToPubMedInputFile", 
        "The original \"Gene to PubMed\" input file",
        CArgDescriptions::eString,
        "gene2pubmed");

    // Path to the output directory
    arg_desc->AddDefaultKey ("out_dir", "OutputDirPath", 
        "Path to the output directory",
        CArgDescriptions::eString,
        "./");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CProcessFilesApp::Run(void)
{
    int nRetval = 0;
    try
    {
        // Process Gene info files

        CGeneFileWriter
            fileWriter(GetArgs()["gene2accession"].AsString(),
                       GetArgs()["gene_info"].AsString(),
                       GetArgs()["gene2pubmed"].AsString(),
                       GetArgs()["out_dir"].AsString());

        fileWriter.EnableMultipleGeneIdsForRNAGis(true);
        fileWriter.EnableMultipleGeneIdsForProteinGis(true);

        fileWriter.ProcessFiles(true);
    }
    catch (CException& e)
    {
        cerr << endl << "Gene file processing failed: "
             << e.what() << endl;
        nRetval = 1;
    }

    return nRetval;
}

void CProcessFilesApp::Exit(void)
{
    SetDiagStream(0);
}

//==========================================================================//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CProcessFilesApp().AppMain(argc, argv, 0, eDS_Default, "");
}

