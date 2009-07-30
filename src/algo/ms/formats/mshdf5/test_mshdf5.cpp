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
* Author: Douglas Slotta
*
* File Description:
*   Utility to test the MsHdf5 class functions
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <algo/ms/formats/mshdf5/MsHdf5.hpp>

USING_SCOPE(ncbi);

class CHdf2mzXMLApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};

void CHdf2mzXMLApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Convert a set of mzXML files to hdf5");

    // Describe the expected command-line arguments
    arg_desc->AddPositional
        ("input", 
         "name of msHdf5 file to convert",
         CArgDescriptions::eInputFile);

    arg_desc->AddExtra
        (0, kMax_UInt,
         "Optional subset of mzXML files to extract",
         CArgDescriptions::eOutputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CHdf2mzXMLApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();
    
    //CNcbiIstream& inFile = args["in"].AsInputFile();
    string inFilename = args["input"].AsString();

    CRef<CMsHdf5> msHdf5(new CMsHdf5(inFilename, H5F_ACC_RDONLY));

    //for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
    //    string fileName = args[extra].AsString();
    //    cerr << "Reading " << fileName << " as SpectraSet " ;
    //}

    set<string> groups;
    msHdf5->getGroups(groups);    
    //groups.insert("DoesNotExist");

    cout << "Loading all precursor M/Z ratios" << endl;
    CTime start(CTime::eCurrent);
    
    int count = 0;
    //H5::Exception::dontPrint();
    ITERATE(set<string>, iGroup, groups) {
        cout << count << ".\t" << *iGroup << " - ";
        CMsHdf5::TSpecMap specMap;
        CMsHdf5::TSpecPrecursorMap specPreMap;
        try {
            msHdf5->getSpectraMap(*iGroup, specMap);
            msHdf5->getPrecursorMzs(*iGroup, specMap, specPreMap);
        }
        catch(H5::Exception & error) {
            cerr << error.getFuncName() << "\t" << error.getDetailMsg() << endl;
        }

        CTime current(CTime::eCurrent);
        cout << current.DiffSecond(start) << "s" << endl;
        start = current;
        count++;
    }
    // Exit successfully
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CHdf2mzXMLApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
