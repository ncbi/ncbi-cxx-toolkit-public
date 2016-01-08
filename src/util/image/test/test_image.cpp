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
 *    Test app -- tests various aspects of image conversion and manipulation
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



class CImageTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};



/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CImageTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Image read/write test application");

    arg_desc->AddDefaultKey("base", "ImageBase",
                            "Base of image name",
                            CArgDescriptions::eString, "test");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CImageTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    //
    // test image conversions
    //
    string base = args["base"].AsString();

    const int max_fmts = 8;
    CImageIO::EType fmts[max_fmts] = {
        CImageIO::eBmp,
        CImageIO::eJpeg,
        CImageIO::ePng,
        CImageIO::eGif,
        CImageIO::eSgi,
        CImageIO::eTiff,
        CImageIO::eXpm,
        CImageIO::eRaw
    };

    const char* extensions[max_fmts] = {
        ".bmp",
        ".jpg",
        ".png",
        ".gif",
        ".sgi",
        ".tiff",
        ".xpm",
        ".raw"
    };

    vector<string> image_names;
    for (int i = 0;  i < max_fmts;  ++i) {
        image_names.push_back(base + extensions[i]);
    }

    cout << "testing image read/write..." << endl;
    ITERATE (vector<string>, iter, image_names) {
        cout << "image: " << *iter << endl;
        CRef<CImage> image(CImageIO::ReadImage(*iter));
        if ( !image ) {
            continue;
        }

        cout << "  width = " << image->GetWidth()
            << " height = " << image->GetHeight()
            << " depth = " << image->GetDepth()
            << endl;

        image->SetDepth(3);

        // write in a multitude of formats
        for (int i = 0;  i < max_fmts;  ++i) {
            string fname (*iter + extensions[i]);
            cout << "trying to write: " << fname << endl;
            if (CImageIO::WriteImage(*image, fname, fmts[i])) {
                cout << "wrote to file " << fname << endl;
            }
        }
    }

    //
    // test image subsetting
    //

    cout << "testing image subsetting..." << endl;
    ITERATE (vector<string>, iter, image_names) {
        CRef<CImage> image(CImageIO::ReadImage(*iter));
        if ( !image ) {
            continue;
        }

        int width  = (int)image->GetWidth() / 4;
        int height = (int)image->GetHeight() / 4;
        int x      = (int)image->GetWidth() / 3;
        int y      = (int)image->GetHeight() / 3;

        CRef<CImage> subimage(image->GetSubImage(x, y, width, height));
        subimage->SetDepth(4);
        if (CImageIO::WriteImage(*subimage, *iter + ".sub-4.png",
                                 CImageIO::ePng)) {
            cout << "wrote subset (" << x << ", " << y << ", "
                << x+width << ", " << y+height << ") to file "
                << *iter + ".sub.png"
                << endl;
        }

        subimage->SetDepth(3);
        if (CImageIO::WriteImage(*subimage, *iter + ".sub-3.jpg",
                                 CImageIO::ePng)) {
            cout << "wrote subset (" << x << ", " << y << ", "
                << x+width << ", " << y+height << ") to file "
                << *iter + ".sub.jpg"
                << endl;
        }
    }

    //
    // test ReadSubImage()
    //
    
    cout << "testing ReadSubImage()..." << endl;
    CRef<CImage> image(CImageIO::ReadImage(base + ".jpeg"));
    if (image) {
        int width  = (int)image->GetWidth();
        int height = (int)image->GetHeight();

        int x = width / 3;
        int y = height / 3;
        int w = width / 4;
        int h = height / 4;

        CStopWatch sw;
        sw.Start();
        CRef<CImage> subimage(CImageIO::ReadSubImage(base + ".jpg",
                                                     x, y, w, h));
        double e = sw.Elapsed();
        cout << "done, read (" << x << ", " << y << ", " << x+w
            << ", " << y+h << ") in " << e << "seconds" << endl;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CImageTestApp::Exit(void)
{
    SetDiagStream(0);
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CImageTestApp().AppMain(argc, argv);
}
