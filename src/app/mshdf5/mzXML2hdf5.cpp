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
* Author:  Douglas Slotta
*
* File Description:
*   Utility to convert one or more mzXML file(s) to HDF5
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/bzip2.hpp>
#include <util/compress/stream.hpp>

#include <misc/xmlwrapp/errors.hpp>

#include <algo/ms/formats/mshdf5/MsHdf5.hpp>
#include "MzXmlReader.hpp"

USING_SCOPE(ncbi);

class CMzXML2hdf5Application : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);

private:
    
    void ProcessFiles(CDir::TEntries* entries);
    CRef<CMsHdf5> m_msHdf5;

    typedef map<string, xml::error_messages> TErrorMap;
    TErrorMap m_errors;
};

void CMzXML2hdf5Application::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Convert a set of mzXML files to hdf5");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey("out", "OutputFile",
                            "name of msHdf5 file to create",
                            CArgDescriptions::eOutputFile, "data.hdf5");
    //CArgDescriptions::eOutputFile, "data.hdf5", CArgDescriptions::fPreOpen);

    arg_desc->AddFlag("test", 
                      "Test all input files without creating an output file");

    arg_desc->AddExtra(1, kMax_UInt,
                       "One or more mzXML files to add to the new HDF5 file",
                       CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CMzXML2hdf5Application::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();
    
    string outFile = args["out"].AsString();

    if (args["test"]) {
        cout << "Testing mode, no hdf5 file will be created" << endl;
        m_msHdf5 = NULL;
    } else {
        m_msHdf5 = new CMsHdf5(outFile, H5F_ACC_TRUNC);
    }
    m_errors.clear();
    CDir::TEntries* files = new(CDir::TEntries);
    for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
        string filename = args[extra].AsString();
        files->push_back(new CDirEntry(filename));
    }
    ProcessFiles(files);
    delete files;

    CFile cOutFile(outFile);
    if (cOutFile.Exists()) {
        cOutFile.SetMode(ncbi::CDirEntry::fDefault,
                         ncbi::CDirEntry::fWrite | ncbi::CDirEntry::fRead,
                         ncbi::CDirEntry::fDefault,
                         ncbi::CDirEntry::fDefault);
    }

    if (!m_errors.empty()) {
        cout << endl << endl << "Files that failed to process: " << endl << endl;
        ITERATE(TErrorMap, it, m_errors) {
            //cout << "  " << (*it).first << ":\t" << (*it).second << endl;
            cout << (*it).first << endl << "-----------------" << endl;
            cout << (*it).second.print() << endl << endl;
        }
    }
    
    // Exit successfully
    return EXIT_SUCCESS;
}

void CMzXML2hdf5Application::ProcessFiles(CDir::TEntries* entries)
{
    ITERATE(CDir::TEntries, it, *entries) {
        if ((*it)->IsDir()) {
            cout << "Entering directory " << (*it)->GetPath() << endl;
            CDir::TEntries* sub_entries = CDir(**it).GetEntriesPtr(kEmptyStr, CDir::fIgnoreRecursive);
            ProcessFiles(sub_entries);
            delete sub_entries;
        } else {
            //cout << (*it)->GetPath() << endl;
            string fileName = (*it)->GetName();
            if (!(*it)->Exists() || 
                (NStr::FindNoCase(fileName, ".mzxml") == string::npos)) {
                continue;
            }
            cout << "Reading " << (*it)->GetPath() << " as SpectraSet ";

            istream *inStream = NULL;
            CCompressionIStream *inCompStream = NULL;
            CNcbiIfstream ifStream((*it)->GetPath().c_str());

            size_t startPos = fileName.rfind("/");
            if (startPos == string::npos) {
                startPos = 0; 
            } else {
                startPos++;
            }
            size_t pos = fileName.rfind(".");
            string suffix(fileName.substr(pos));
            string base;
            if (suffix == ".bz2") {
                base = fileName.substr(startPos,fileName.rfind(".", pos-1)-startPos);
                inCompStream = new CCompressionIStream(ifStream, new CBZip2StreamDecompressor(), CCompressionStream::fOwnProcessor);
                inStream = inCompStream;
            }
            else if (suffix == ".gz") {
                base = fileName.substr(startPos,fileName.rfind(".", pos-1)-startPos);
                inCompStream = new CCompressionIStream(ifStream, new CZipStreamDecompressor(CZipCompression::fGZip), CCompressionStream::fOwnProcessor);
                inStream = inCompStream;
            }
            else { // no compression
                base = fileName.substr(startPos,pos-startPos);
                inStream = &ifStream;
            }
            cout << base;

            MzXmlReader mzXmlParser(m_msHdf5);            
            if (m_msHdf5) m_msHdf5->newSpectraSet(base);  // Need to catch existing path exception
            if (!mzXmlParser.parse_stream(*inStream)) {
                //cout << "  Parse failed!  " << mzXmlParser.get_error_message() << endl;
                cout << " - unable to parse";
                //m_errors[(*it)->GetPath()] = mzXmlParser.get_error_message();
                m_errors[(*it)->GetPath()] = mzXmlParser.get_parser_messages();
            }

            if (inCompStream) {
                inCompStream->Finalize();
                delete inCompStream;
            }
            cout << endl;
        }
    }    

}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CMzXML2hdf5Application().AppMain(argc, argv, 0, eDS_Default, 0);
}
