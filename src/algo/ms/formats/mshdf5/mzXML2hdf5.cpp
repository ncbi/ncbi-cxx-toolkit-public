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

#include <algo/ms/formats/mshdf5/MsHdf5.hpp>
#include "CompressedInputSource.hpp"
#include "MzXmlReader.hpp"

#include <xercesc/util/XMLString.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/framework/StdInInputSource.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(xercesc);

class CMzXML2hdf5Application : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};

void CMzXML2hdf5Application::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Convert a set of mzXML files to hdf5");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("out", "OutputFile",
         "name of msHdf5 file to create",
         CArgDescriptions::eOutputFile, "data.hdf5", CArgDescriptions::fPreOpen);

    arg_desc->AddExtra
        (1, kMax_UInt,
         "One or more mzXML files to add to the new HDF5 file",
         CArgDescriptions::eInputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CMzXML2hdf5Application::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();
    
    //CNcbiIstream& inFile = args["in"].AsInputFile();
    string outFile = args["out"].AsString();

    CRef<CMsHdf5> msHdf5(new CMsHdf5(outFile, H5F_ACC_TRUNC));

    MzXmlReader::initialize();

    auto_ptr<SAX2XMLReader> parser(XMLReaderFactory::createXMLReader());
    parser->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);
    parser->setFeature(XMLUni::fgXercesSchema, false);
    parser->setFeature(XMLUni::fgSAX2CoreValidation, false);
    parser->setFeature(XMLUni::fgXercesSchemaFullChecking, false);

    auto_ptr<MzXmlReader> xmlReader(new MzXmlReader("UTF8", msHdf5));
    parser->setContentHandler(xmlReader.get());
    parser->setErrorHandler(xmlReader.get());
    parser->setEntityResolver(xmlReader.get());

    for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
        string fileName = args[extra].AsString();
        cerr << "Reading " << fileName << " as SpectraSet " ;
        try {
            auto_ptr<InputSource> input;
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
                input.reset(new CompressedInputSource(fileName, BZIP2_COMPRESSED));
                base = fileName.substr(startPos,fileName.rfind(".", pos-1)-startPos);
            }
            else if (suffix == ".gz") {
                input.reset(new CompressedInputSource(fileName, GZIP_COMPRESSED));
                base = fileName.substr(startPos,fileName.rfind(".", pos-1)-startPos);
            }
            else { // no compression
                if (fileName.empty() || fileName == "-") {
                    // Should never be here!
                    input.reset(new StdInInputSource);
                }
                else {
                    XMLCh * xmlFile(XMLString::transcode(fileName.c_str()));
                    input.reset(new LocalFileInputSource(xmlFile));
                    XMLString::release(&xmlFile);
                    base = fileName.substr(startPos,pos-startPos);
                }
            }
            cerr << base << endl;
            
            msHdf5->newSpectraSet(base);  // Need to catch existing path exception
            parser->parse(*input);
        }
        catch (const XMLException & toCatch) {
            char * message(XMLString::transcode(toCatch.getMessage()));
            cerr << "XML Error: " << message << "\n";
            XMLString::release(&message);
            return EXIT_FAILURE; // Exit unsuccessfully
        }
        catch (const SAXParseException & toCatch) {
            char * message(XMLString::transcode(toCatch.getMessage()));
            cerr << "SAX Parsing Error at line #" << toCatch.getLineNumber()
                 << ": " << message << "\n";
            XMLString::release(&message);
            return EXIT_FAILURE; // Exit unsuccessfully
        }
        catch (const SAXException & toCatch) {
            char * message(XMLString::transcode(toCatch.getMessage()));
            cerr << "SAX Error: " << message << "\n";
            XMLString::release(&message);
            return EXIT_FAILURE; // Exit unsuccessfully
        }
        catch (std::exception & ex) {
            cerr << "Error: " << ex.what() << "\n";
            return EXIT_FAILURE; // Exit unsuccessfully
        }
        catch (...) {
            cerr << "Unexpected exception, aborting.\n";
            return EXIT_FAILURE; // Exit unsuccessfully
        }
    }

    CFile(outFile).SetMode(ncbi::CDirEntry::fDefault,
                           ncbi::CDirEntry::fWrite | ncbi::CDirEntry::fRead,
                           ncbi::CDirEntry::fDefault,
                           ncbi::CDirEntry::fDefault);

    // Exit successfully
    return EXIT_SUCCESS;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CMzXML2hdf5Application().AppMain(argc, argv, 0, eDS_Default, 0);
}
