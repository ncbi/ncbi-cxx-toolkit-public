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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *    CConvImageApp: Convert an image from one format to another
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbitime.hpp>

#include <util/image/image_io.hpp>
#include <util/image/image_util.hpp>

USING_NCBI_SCOPE;



class CConvImageApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};



/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CConvImageApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Image read/write test application");

    arg_desc->AddKey("in", "InputImage", "Image to convert",
                     CArgDescriptions::eString);
    arg_desc->AddKey("out", "OutputImage", "Resulting image",
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("fmt", "OutputFormat",
                             "Format of the new image",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("fmt",
                            &(*new CArgAllow_Strings(),
                              "jpeg", "tiff", "png", "bmp",
                              "sgi", "raw", "gif"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CConvImageApp::Run(void)
{
    const CArgs& args = GetArgs();

    string in_file  = args["in"].AsString();
    string out_file = args["out"].AsString();

    CStopWatch sw;
    sw.Start();
    CRef<CImage> image(CImageIO::ReadImage(in_file));
    double read_time = sw.Elapsed();

    if ( !image ) {
        LOG_POST(Error << "error: can't read image from " << in_file);
        return 1;
    }

    LOG_POST(Info << "read " << in_file << " ("
             << image->GetWidth() << ", " << image->GetHeight()
             << ") in " << read_time << " seconds");

    CImageIO::EType fmt = CImageIO::GetTypeFromFileName(out_file);
    if (args["fmt"].HasValue()) {
        string str = args["fmt"].AsString();
        if (str == "jpeg") {
            fmt = CImageIO::eJpeg;
        } else if (str == "png") {
            fmt = CImageIO::ePng;
        } else if (str == "tiff") {
            fmt = CImageIO::eTiff;
        } else if (str == "bmp") {
            fmt = CImageIO::eBmp;
        } else if (str == "raw") {
            fmt = CImageIO::eRaw;
        } else if (str == "sgi") {
            fmt = CImageIO::eSgi;
        } else if (str == "gif") {
            fmt = CImageIO::eGif;
        }
    }

    CImageIO::WriteImage(*image, out_file, fmt);
    double write_time = sw.Elapsed();
    LOG_POST(Info << "wrote image in " << write_time - read_time
             << " seconds");

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CConvImageApp::Exit(void)
{
    SetDiagStream(0);
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CConvImageApp().AppMain(argc, argv);
}
