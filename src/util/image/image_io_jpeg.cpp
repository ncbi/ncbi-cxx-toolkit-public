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
 *    CImageIOJpeg -- interface class for reading/writing JPEG files
 */

#include "image_io_jpeg.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>

#ifdef HAVE_LIBJPEG
//
//
// JPEG SUPPORT
//
//

#include <stdio.h>

// hack to get around duplicate definition of INT32
#ifdef NCBI_OS_MSWIN
#define XMD_H
#endif

// jpeglib include (not extern'ed already (!))
extern "C" {
#include <jpeglib.h>
}


BEGIN_NCBI_SCOPE


//
// ReadImage()
// read a JPEG format image into memory, returning the image itself
// This version reads the entire image
//
CImage* CImageIOJpeg::ReadImage(const string& file)
{
    // open our file for reading
    FILE * fp = fopen(file.c_str(), "rb");
    if ( !fp ) {
        string msg("CImageIOJpeg::ReadImage(): can't open file ");
        msg += file;
        msg += " for reading";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // satndard libjpeg stuff
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);

    // decompression parameters
    cinfo.dct_method = JDCT_FLOAT;
    jpeg_start_decompress(&cinfo);

    // allocate an image to hold our data
    CRef<CImage> image(new CImage(cinfo.output_width, cinfo.output_height,
                                  cinfo.out_color_components));

    // we process the image 1 scanline at a time
    unsigned char *data[1];
    data[0] = image->SetData();
    size_t stride = image->GetWidth() * image->GetDepth();
    for (size_t i = 0;  i < image->GetHeight();  ++i) {
        jpeg_read_scanlines(&cinfo, data, 1);
        data[0] += stride;
    }

    // standard clean-up
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);

    return image.Release();
}


//
// ReadImage()
// read a JPEG format image into memory, returning the image itself
// This version reads only a subset of the image
//
CImage* CImageIOJpeg::ReadImage(const string& file,
                                size_t x, size_t y, size_t w, size_t h)
{
    // open our image for reading
    FILE* fp = fopen(file.c_str(), "rb");
    if ( !fp ) {
        string msg("CImageIOJpeg::ReadImage(): can't open file ");
        msg += file;
        msg += " for reading";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // standard libjpeg setup
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    // further validation: make sure we're actually on the image
    if (x >= cinfo.output_width  ||  y >= cinfo.output_height) {
        // don't forget to clean up our local structures
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(fp);

        string msg("CImageIOJpeg::ReadImage(): invalid starting position: ");
        msg += NStr::IntToString(x);
        msg += ", ";
        msg += NStr::IntToString(y);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // clamp our width and height to the image size
    if (x + w >= cinfo.output_width) {
        w = cinfo.output_width - x;
        LOG_POST(Warning
                 << "CImageIOJpeg::ReadImage(): clamped width to " << w);
    }

    if (y + h >= cinfo.output_height) {
        h = cinfo.output_height - y;
        LOG_POST(Warning
                 << "CImageIOJpeg::ReadImage(): clamped height to " << h);
    }

    // allocate an image to store the subimage's data
    CRef<CImage> image(new CImage(w, h, cinfo.out_color_components));

    // also allocate a single scanline
    // we plan to process the input file on a line-by-line basis
    AutoPtr<unsigned char, ArrayDeleter<unsigned char> > row_data
        (new unsigned char[cinfo.output_width * image->GetDepth()]);
    unsigned char *data[1];
    data[0] = row_data.get();
    size_t i;

    // start by skipping a number of scan lines
    for (i = 0;  i < y;  ++i) {
        jpeg_read_scanlines(&cinfo, data, 1);
    }

    // read the chunk of interest
    unsigned char* to_data = image->SetData();
    size_t to_stride  = image->GetWidth() * image->GetDepth();
    size_t offs       = x * image->GetDepth();
    size_t scan_width = w * image->GetDepth();
    for (;  i < y + h;  ++i) {
        jpeg_read_scanlines(&cinfo, data, 1);
        memcpy(to_data, data[0] + offs, scan_width);
        to_data += to_stride;
    }

    // finish reading the remainder of the scan lines (do we need this?)
    for (;  i < cinfo.output_height;  ++i) {
        jpeg_read_scanlines(&cinfo, data, 1);
    }

    // standard clean-up
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);

    return image.Release();
}


//
// WriteImage()
// write an image to a file in JPEG format
// This version writes the entire image
//
void CImageIOJpeg::WriteImage(const CImage& image, const string& file,
                              CImageIO::ECompress compress)
{
    // open our file for writing
    FILE* fp = fopen(file.c_str(), "wb");
    if ( !fp ) {
        string msg("CImageIOJpeg::WriteImage(): can't open file ");
        msg += file;
        msg += " for writing";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // standard libjpeg setup
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width      = image.GetWidth();
    cinfo.image_height     = image.GetHeight();
    cinfo.input_components = image.GetDepth();
    cinfo.in_color_space   = JCS_RGB;
    jpeg_set_defaults(&cinfo);

    // set our compression quality
    int quality = 70;
    switch (compress) {
    case CImageIO::eCompress_None:
        quality = 100;
        break;
    case CImageIO::eCompress_Low:
        quality = 90;
        break;
    case CImageIO::eCompress_Medium:
        quality = 70;
        break;
    case CImageIO::eCompress_High:
        quality = 40;
        break;
    default:
        LOG_POST(Error << "unknown compression type: " << (int)compress);
        break;
    }
    jpeg_set_quality(&cinfo, quality, TRUE);

    // begin compression
    jpeg_start_compress(&cinfo, true);

    // process our data on a line-by-line basis
    JSAMPROW data[1];
    data[0] =
        const_cast<JSAMPLE*> (static_cast<const JSAMPLE*> (image.GetData()));
    for (size_t i = 0;  i < image.GetHeight();  ++i) {
        jpeg_write_scanlines(&cinfo, data, 1);
        data[0] += image.GetWidth() * image.GetDepth();
    }

    // standard clean-up
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);
}


//
// WriteImage()
// write an image to a file in JPEG format
// This version writes only a subpart of the image
//
void CImageIOJpeg::WriteImage(const CImage& image, const string& file,
                              size_t x, size_t y, size_t w, size_t h,
                              CImageIO::ECompress compress)
{
    // validate our image position as inside the image we've been passed
    if (x >= image.GetWidth()  ||  y >= image.GetHeight()) {
        string msg("CImageIOJpeg::WriteImage(): invalid image position: ");
        msg += NStr::IntToString(x);
        msg += ", ";
        msg += NStr::IntToString(y);
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // clamp our width and height
    if (x+w >= image.GetWidth()) {
        w = image.GetWidth() - x;
        LOG_POST(Warning
                 << "CImageIOJpeg::WriteImage(): clamped width to " << w);
    }

    if (y+h >= image.GetHeight()) {
        h = image.GetHeight() - y;
        LOG_POST(Warning
                 << "CImageIOJpeg::WriteImage(): clamped height to " << h);
    }

    // open our file
    FILE* fp = fopen(file.c_str(), "wb");
    if ( !fp ) {
        string msg("CImageIOJpeg::WriteImage(): can't open file ");
        msg += file;
        msg += " for writing";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // standard libjpeg stuff
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width      = w;
    cinfo.image_height     = h;
    cinfo.input_components = image.GetDepth();
    cinfo.in_color_space   = JCS_RGB;
    jpeg_set_defaults(&cinfo);

    // set our compression quality
    int quality = 70;
    switch (compress) {
    case CImageIO::eCompress_None:
        quality = 100;
        break;
    case CImageIO::eCompress_Low:
        quality = 90;
        break;
    case CImageIO::eCompress_Medium:
        quality = 70;
        break;
    case CImageIO::eCompress_High:
        quality = 40;
        break;
    default:
        LOG_POST(Error << "unknown compression type: " << (int)compress);
        break;
    }
    jpeg_set_quality(&cinfo, quality, TRUE);

    // begin compression
    jpeg_start_compress(&cinfo, true);

    // we will process a single scanline at a time
    const unsigned char* data_start =
        image.GetData() + (y * image.GetWidth() + x) * image.GetDepth();
    size_t data_stride = image.GetWidth() * image.GetDepth();

    JSAMPROW data[1];
    data[0] = const_cast<JSAMPLE*> (static_cast<const JSAMPLE*> (data_start));
    for (size_t i = 0;  i < h;  ++i) {
        jpeg_write_scanlines(&cinfo, data, 1);
        data[0] += data_stride;
    }

    // standard clean-up
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);
}


END_NCBI_SCOPE


#else // !HAVE_LIBJPEG

//
// JPEGLIB functionality not included - we stub out the various needed
// functions
//


BEGIN_NCBI_SCOPE


CImage* CImageIOJpeg::ReadImage(const string& file)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::ReadImage(): JPEG format not supported");
}


CImage* CImageIOJpeg::ReadImage(const string& file,
                                size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::ReadImage(): JPEG format not supported");
}


void CImageIOJpeg::WriteImage(const CImage& image, const string& file,
                              CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::WriteImage(): JPEG format not supported");
}


void CImageIOJpeg::WriteImage(const CImage& image, const string& file,
                              size_t, size_t, size_t, size_t,
                              CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::WriteImage(): JPEG format not supported");
}


END_NCBI_SCOPE


#endif  // HAVE_LIBJPEG


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.3  2003/10/02 15:37:33  ivanov
 * Get rid of compilation warnings
 *
 * Revision 1.2  2003/06/16 19:20:02  dicuccio
 * Added guard for libjpeg on Win32
 *
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

