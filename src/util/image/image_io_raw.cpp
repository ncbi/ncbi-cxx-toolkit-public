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
 *    CImageIORaw -- interface class for reading/writing Windows RAW files
 */

#include <ncbi_pch.hpp>
#include "image_io_raw.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>

BEGIN_NCBI_SCOPE


// header signature
static const char* sc_Header = "RAW\0";


CImage* CImageIORaw::ReadImage(CNcbiIstream& istr)
{
    // read header
    char header[4];
    istr.read(reinterpret_cast<char*>(&header), 4);

    // read dimensions
    size_t width;
    size_t height;
    size_t depth;

    istr.read(reinterpret_cast<char*>(&width),  sizeof(size_t));
    istr.read(reinterpret_cast<char*>(&height), sizeof(size_t));
    istr.read(reinterpret_cast<char*>(&depth),  sizeof(size_t));

    CRef<CImage> image(new CImage(width, height, depth));
    if ( !image ) {
        NCBI_THROW(CImageException, eReadError,
                   "CImageIORaw::ReadImage(): failed to allocate image");
    }

    istr.read(reinterpret_cast<char*>(image->SetData()),
                width * height * depth);
    return image.Release();
}


CImage* CImageIORaw::ReadImage(CNcbiIstream& istr,
                               size_t x, size_t y, size_t w, size_t h)
{
    // read header
    char header[4];
    istr.read(reinterpret_cast<char*>(&header), 4);

    // read dimensions
    size_t width;
    size_t height;
    size_t depth;

    istr.read(reinterpret_cast<char*>(&width),  sizeof(size_t));
    istr.read(reinterpret_cast<char*>(&height), sizeof(size_t));
    istr.read(reinterpret_cast<char*>(&depth),  sizeof(size_t));

    // create our sub-image
    CRef<CImage> image(new CImage(w, h, depth));
    if ( !image ) {
        NCBI_THROW(CImageException, eReadError,
                   "CImageIORaw::ReadImage(): failed to allocate image");
    }

    // calculate the bytes per line for our sub-image anf dor the input image
    const size_t input_bpl  = width * depth;
    const size_t output_bpl = w * depth;

    // start reading
    unsigned char* data = image->SetData();
    istr.seekg(input_bpl * y + x * depth, ios::beg);
    for (size_t i = 0;  i < h;  ++i, data += output_bpl) {
        istr.read(reinterpret_cast<char*>(data), output_bpl);
        istr.seekg(input_bpl - output_bpl, ios::cur);
    }
    return image.Release();
}


void CImageIORaw::WriteImage(const CImage& image, CNcbiOstream& ostr,
                             CImageIO::ECompress)
{
    // write the header
    ostr.write(reinterpret_cast<const char*>(sc_Header), 4);

    // write dimensions
    size_t width  = image.GetWidth();
    size_t height = image.GetHeight();
    size_t depth  = image.GetDepth();

    ostr.write(reinterpret_cast<const char*>(&width),  sizeof(size_t));
    ostr.write(reinterpret_cast<const char*>(&height), sizeof(size_t));
    ostr.write(reinterpret_cast<const char*>(&depth),  sizeof(size_t));

    // write the image data
    ostr.write(reinterpret_cast<const char*>(image.GetData()),
                 width * height * depth);
}


void CImageIORaw::WriteImage(const CImage& image, CNcbiOstream& ostr,
                             size_t /* x */, size_t y,
                             size_t width, size_t height,
                             CImageIO::ECompress)
{
    // write the header
    ostr.write(reinterpret_cast<const char*>(sc_Header), 4);

    // write dimensions
    size_t depth  = image.GetDepth();

    ostr.write(reinterpret_cast<const char*>(&width),  sizeof(size_t));
    ostr.write(reinterpret_cast<const char*>(&height), sizeof(size_t));
    ostr.write(reinterpret_cast<const char*>(&depth),  sizeof(size_t));

    // calculate the bytes per line for our sub-image anf dor the input image
    const size_t input_bpl  = image.GetWidth() * depth;
    const size_t output_bpl = width * depth;

    // write the image data
    const unsigned char* data = image.GetData();
    data += input_bpl * y;
    for (size_t i = 0;  i < height;  ++i, data += input_bpl) {
        ostr.write(reinterpret_cast<const char*>(data), output_bpl);
    }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/02/01 21:47:15  grichenk
 * Fixed warnings
 *
 * Revision 1.2  2004/05/17 21:07:58  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.1  2003/12/16 15:48:11  dicuccio
 * Added support for RAW image files
 *
 * Revision 1.2  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

