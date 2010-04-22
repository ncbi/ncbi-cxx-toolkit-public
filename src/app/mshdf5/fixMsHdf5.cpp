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
* Author: Douglas
*
* File Description:
*   Utility to convert a HDF5 file to one or more mzXML file(s)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/ms/formats/mzxml/mzXML__.hpp>

#include <algo/ms/formats/mshdf5/MsHdf5.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(H5);

class CFixMsHdf5Application : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
private:
    void checkInfo(CRef<CMsHdf5> msHdf5, const string& group);
    bool checkMap(CRef<CMsHdf5> msHdf5, const string& group);
};

void CFixMsHdf5Application::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Convert a set of mzXML files to hdf5");

    // Describe the expected command-line arguments
    arg_desc->AddExtra
        (1, kMax_UInt,
         "One or more msHdf5 files to fix",
         CArgDescriptions::eInputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

void CFixMsHdf5Application::checkInfo(CRef<CMsHdf5> msHdf5, const string& group) 
{
    cerr << "  checking /" << group << "/ms1info...";
    try{
        DataSet dataset = msHdf5->raw()->openDataSet("/" + group + "/ms1info");
        DataType datatype = dataset.getDataType();
        DataSpace dataspace = dataset.getSpace();
            
        hsize_t size[2];        // dataset dimensions
        dataspace.getSimpleExtentDims( size );
    
        char *buf = new char[size[0]*size[1]];
        dataset.read((void*)buf, datatype);

        int modified = 0;

        for (Uint4 i=0; i<size[0]; i++) {
            Uint4 p = i*size[1];
            string line(&buf[p]);
            size_t loc = NStr::Find(line, ">\n/>");
            if (loc != NPOS) {
                line = NStr::Replace(line, ">\n/>", "/>", loc);
                strcpy(&buf[p], line.c_str());
                modified++;
            }
        }
        if (modified > 0) {
            dataset.write(buf, datatype);
            cerr << "corrected " << modified << " lines" << endl;
        } else {
            cerr << "no correction needed" << endl;
        }
            
        delete [] buf;
    } catch (FileIException error) {
        cerr << "does not exist." << endl;
        return;
    }
}

bool CFixMsHdf5Application::checkMap(CRef<CMsHdf5> msHdf5, const string& group) 
{
    bool hasMS1 = false;
    bool hasParentScan = false;
    
    CMsHdf5::TSpecMap specMap;
    msHdf5->getSpectraMap(group, specMap);
    

    ITERATE(CMsHdf5::TSpecMap, iMapEntry, specMap) {
        if ((*iMapEntry).second.msLevel == 1) {
            hasMS1 = true;
        }
        if ((*iMapEntry).second.parentScan > 0) {
            hasParentScan = true;
        }
        if (hasMS1 && hasParentScan) {
            break;
        }
    }

    return (hasMS1 != hasParentScan);
}


int CFixMsHdf5Application::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();

    set<string> badMaps;
    for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
        string filename = args[extra].AsString();
                
        cerr << "Fixing " << filename << ":" << endl;

        CRef<CMsHdf5> msHdf5(new CMsHdf5(filename, H5F_ACC_RDWR));

        set<string> groups;    
        msHdf5->getGroups(groups);

        ITERATE(set<string>, iGroup, groups) {
            string group = *iGroup;
            checkInfo(msHdf5, group);
            if (checkMap(msHdf5, group)) {
                badMaps.insert(filename + "/" + group + "/map");
            }
        }
    }

    if (badMaps.size() > 0) {
        cerr << endl << endl << "List of incorrect maps:" << endl << endl;
        ITERATE(set<string>, iBadMap, badMaps) {
            cerr << "  " << *iBadMap << endl;
        }
    }
    
    // Exit successfully
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CFixMsHdf5Application().AppMain(argc, argv, 0, eDS_Default, 0);
}
