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
*   Utility to test the speed of MsHdf5 files
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

const double kMebiByte = 1024 * 1024;

class CMSHdf5SpeedApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);

    struct SpecLocation
    {
        SpecLocation(string grp, CMsHdf5::MSMapEntry mi)
            : group(grp), mapItem(mi) {}
        string group;
        CMsHdf5::MSMapEntry mapItem;
    };
};

void CMSHdf5SpeedApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Check the throughput for reading mshdf5 files");

    // Describe the expected command-line arguments
    arg_desc->AddPositional
        ("input", 
         "name of msHdf5 file to read",
         CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey
        ("size", "DefaultKey",
         "Size (in MiB) to read from file.",
         CArgDescriptions::eInteger, "5");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CMSHdf5SpeedApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();
    
    string inFilename = args["input"].AsString();

    CTime start(CTime::eCurrent);

    CRef<CMsHdf5> msHdf5(new CMsHdf5(inFilename, H5F_ACC_RDONLY));

    set<string> groups;
    msHdf5->getGroups(groups);    

    cout << "Discovering all spectra: ";
    vector<SpecLocation> spectra;
    
    //H5::Exception::dontPrint();
    ITERATE(set<string>, iGroup, groups) {
        CMsHdf5::TSpecMap specMap;
        try {
            msHdf5->getSpectraMap(*iGroup, specMap);
        }
        catch(H5::Exception & error) {
            cerr << error.getFuncName() << "\t" << error.getDetailMsg() << endl;
        }
        ITERATE(CMsHdf5::TSpecMap, iMapEntry, specMap) {
            SpecLocation sLoc(*iGroup, (*iMapEntry).second);
            spectra.push_back(sLoc);
        }
    }
    
    random_shuffle(spectra.begin(), spectra.end());

    CTime setupDone(CTime::eCurrent);
    double setupTime = setupDone.DiffNanoSecond(start) / kNanoSecondsPerSecond;
    
    cout << setupTime << " seconds." << endl;

    // output list
    Uint8 bytes = 0;
    Uint8 maxBytes = args["size"].AsInteger() * static_cast<Uint8>(kMebiByte);
    
    uint scan_count = 0;
    while (bytes < maxBytes) {
        ITERATE(vector<SpecLocation>, iSpecLoc, spectra) {
            string src((*iSpecLoc).group + ":" + 
                       NStr::IntToString((*iSpecLoc).mapItem.scan) + "\t" +
                       NStr::IntToString((*iSpecLoc).mapItem.idx));
            CMsHdf5::TSpectrum spectrum;
            string scanXML;
            string msLevel(NStr::IntToString((*iSpecLoc).mapItem.msLevel));
            msHdf5->getSpectrum(src, spectrum, scanXML, msLevel);
            
            bytes += scanXML.size() * sizeof(char);
            bytes += spectrum[0].size() * 2 * sizeof(float);

            scan_count++;

            if (bytes > maxBytes) break;
        }
    }
    
    double mBytes = bytes / kMebiByte;
    
    CTime finished(CTime::eCurrent);
    double tDiff = finished.DiffNanoSecond(setupDone) / kNanoSecondsPerSecond;
    
    cout << "Read " << scan_count << " scans,  " 
         << mBytes << " MiBs in " 
         << tDiff << " seconds."
         << "(" << (mBytes/tDiff) << " MiB/sec)" << endl;

    // Exit successfully
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CMSHdf5SpeedApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
