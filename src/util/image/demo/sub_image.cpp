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
 *    Test app -- tests various aspects of image conversion / manipulation
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbitime.hpp>

#include <util/image/image_io.hpp>

USING_NCBI_SCOPE;



class CSubImageApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};



/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CSubImageApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Image read/write test application");

    arg_desc->AddDefaultPositional("image", "Image to crop",
                                   CArgDescriptions::eInputFile, "-",
                                   CArgDescriptions::fBinary);

    arg_desc->AddOptionalKey("out", "OutFile",
                             "File name of resultant image",
                             CArgDescriptions::eString);

    arg_desc->AddKey("x", "PosX", "Crop X Position",
                     CArgDescriptions::eInteger);
    arg_desc->AddKey("y", "PosX", "Crop X Position",
                     CArgDescriptions::eInteger);
    arg_desc->AddKey("wd", "Width", "Crop Width",
                     CArgDescriptions::eInteger);
    arg_desc->AddKey("ht", "Height", "Crop Height",
                     CArgDescriptions::eInteger);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CSubImageApp::Run(void)
{
    const CArgs& args = GetArgs();

    CNcbiIstream& istr = args["image"].AsInputFile();
    int x = args["x"].AsInteger();
    int y = args["y"].AsInteger();
    int w = args["wd"].AsInteger();
    int h = args["ht"].AsInteger();

    CStopWatch sw;
    sw.Start();
    CRef<CImage> image(CImageIO::ReadSubImage(istr, x, y, w, h));
    double read_time = sw.Elapsed();

    if ( !image ) {
        LOG_POST(Error << "error: can't get subimage");
        return 1;
    }

    LOG_POST(Info << "read (" << x << ", " << y << ", " << x+w
             << ", " << y+h << ") in " << read_time << " seconds");

    if (args["out"]) {
        CImageIO::WriteImage(*image, args["out"].AsString());
        double write_time = sw.Elapsed();
        LOG_POST(Info << "wrote image in " << write_time - read_time
                 << " seconds");
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CSubImageApp::Exit(void)
{
    SetDiagStream(0);
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSubImageApp().AppMain(argc, argv);
}
