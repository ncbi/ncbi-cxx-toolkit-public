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

    arg_desc->AddDefaultPositional("image", "Image to crop",
                                   CArgDescriptions::eInputFile, "-",
                                   CArgDescriptions::fBinary);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CImageTestApp::Run(void)
{
    CArgs args = GetArgs();

    CNcbiIstream& istr = args["image"].AsInputFile();

    size_t width  = 0;
    size_t height = 0;
    size_t depth  = 0;
    CImageIO::EType type = CImageIO::eUnknown;

    if ( !CImageIO::ReadImageInfo(istr, &width, &height, &depth, &type) ) {
        NCBI_THROW(CException, eUnknown, "failed to read image info");
    }

    CNcbiOstrstream ostr;
    switch (type) {
    case CImageIO::ePng:    ostr << "PNG, ";    break;
    case CImageIO::eJpeg:   ostr << "JPEG, ";   break;
    case CImageIO::eGif:    ostr << "GIF, ";    break;
    case CImageIO::eTiff:   ostr << "TIFF, ";   break;
    case CImageIO::eBmp:    ostr << "BMP, ";    break;
    case CImageIO::eSgi:    ostr << "SGI, ";    break;
    case CImageIO::eRaw:    ostr << "RAW, ";    break;
    default:                ostr << "(unknown format), ";   break;
    }

    ostr << width << "x" << height;
    switch (depth) {
    case 1:
    case 2:     ostr << ", b&w"; break;
    case 3:     ostr << ", rgb"; break;
    case 4:     ostr << ", rgba"; break;
    default:    ostr << ", unknown format"; break;
    }
    LOG_POST(Error << string(CNcbiOstrstreamToString(ostr)));

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
    return CImageTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}




/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/12/04 13:54:46  dicuccio
 * Remove unneeded code
 *
 * Revision 1.1  2006/07/24 13:35:19  dicuccio
 * Added image_info app
 *
 * Revision 1.7  2006/06/21 13:17:30  dicuccio
 * Revert previous change; open stream in binary mode
 *
 * Revision 1.6  2006/06/21 13:05:02  dicuccio
 * Temporary fix: avoid using streams-based image retrieval without a specified
 * type
 *
 * Revision 1.5  2004/05/17 21:08:03  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.4  2003/12/20 17:49:25  dicuccio
 * Changed sub_image to use streams instead of file names.  Changed to us implicit
 * format guessing.
 *
 * Revision 1.3  2003/12/16 16:16:43  dicuccio
 * Added more options - raw files
 *
 * Revision 1.2  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.1  2003/06/03 15:17:14  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

