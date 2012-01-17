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
 *    CImageIOPng -- interface class for reading/writing PNG files
 */

#include <ncbi_pch.hpp>
#include "image_io_png.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>
#include <util/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   Util_Image


#ifdef HAVE_LIBPNG
//
//
// PNG SUPPORT
//
//

// libpng main header
#include <png.h>


BEGIN_NCBI_SCOPE


//
// internal message handler for PNG images
//
static void s_PngReadErrorHandler(png_structp /*png_ptr*/, png_const_charp msg)
{
    string str("Error reading PNG file: ");
    str += msg;
    NCBI_THROW(CImageException, eReadError, str);
}


//
// internal message handler for PNG images
//
static void s_PngWriteErrorHandler(png_structp /*png_ptr*/,png_const_charp msg)
{
    string str("Error writing PNG file: ");
    str += msg;
    NCBI_THROW(CImageException, eWriteError, str);
}


//
// internal handler: translate PNG warnings to NCBI warnings
//
static void s_PngWarningHandler(png_structp /*png_ptr*/, png_const_charp msg)
{
    LOG_POST_X(25, Warning << "Warning in PNG file: " << msg);
}


//
// initialize PNG reading
//
static void s_PngReadInit(png_structp& png_ptr,
                          png_infop&   info_ptr,
                          png_infop&   end_info_ptr)
{
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                     s_PngReadErrorHandler,
                                     s_PngWarningHandler);
    if ( !png_ptr ) {
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_read_struct() failed");
    }

    info_ptr = png_create_info_struct(png_ptr);
    if ( !info_ptr ) {
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_info_struct() failed");
    }

    end_info_ptr = png_create_info_struct(png_ptr);
    if ( !end_info_ptr ) {
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_info_struct() failed");
    }
}


//
// initialize PNG writing
//
static void s_PngWriteInit(png_structp& png_ptr,
                           png_infop&   info_ptr,
                           size_t width, size_t height, size_t depth,
                           CImageIO::ECompress compress)
{
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                      s_PngWriteErrorHandler,
                                      s_PngWarningHandler);
    if ( !png_ptr ) {
        NCBI_THROW(CImageException, eWriteError,
                   "CImageIOPng::WriteImage(): png_create_read_struct() failed");
    }

    info_ptr = png_create_info_struct(png_ptr);
    if ( !info_ptr ) {
        NCBI_THROW(CImageException, eWriteError,
                   "CImageIOPng::WriteImage(): png_create_info_struct() failed");
    }

    png_byte color_type = PNG_COLOR_TYPE_RGB;
    if (depth == 4) {
        color_type = PNG_COLOR_TYPE_RGBA;
    }
    png_set_IHDR(png_ptr, info_ptr,
                 width, height, 8, color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    // set our compression quality
    switch (compress) {
    case CImageIO::eCompress_None:
        png_set_compression_level(png_ptr, Z_NO_COMPRESSION);
        break;
    case CImageIO::eCompress_Low:
        png_set_compression_level(png_ptr, Z_BEST_SPEED);
        break;
    case CImageIO::eCompress_Medium:
        png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);
        break;
    case CImageIO::eCompress_High:
        png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
        break;
    default:
        LOG_POST_X(26, Error << "unknown compression type: " << (int)compress);
        break;
    }

}


//
// validate our input
//
static void s_PngReadValidate(png_structp png_ptr,
                              png_infop info_ptr,
                              size_t& width,
                              size_t& height,
                              size_t& depth,
                              size_t& x, size_t& y, size_t& w, size_t& h)
{
    // store and validate our image's parameters
    width        = info_ptr->width;
    height       = info_ptr->height;
    depth        = info_ptr->channels;
    png_byte color_type = info_ptr->color_type;
    png_byte bit_depth  = info_ptr->bit_depth;

    // we support only RGB and RGBA images
    if ( color_type != PNG_COLOR_TYPE_RGB  &&
         color_type != PNG_COLOR_TYPE_RGB_ALPHA ) {
        string msg("CImageIOPng::ReadImage(): unhandled color type: ");
        msg += NStr::NumericToString((int)color_type);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // ...and only with a bit depth of 8
    if (bit_depth != 8) {
        string msg("CImageIOPng::ReadImage(): unhandled bit depth: ");
        msg += NStr::NumericToString((int)bit_depth);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // this goes along with RGB or RGBA
    if (depth != 3  &&  depth != 4) {
        string msg("CImageIOPng::ReadImage(): unhandled image channels: ");
        msg += NStr::NumericToString((int)depth);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    if (x != (size_t)-1  &&  y != (size_t)-1  &&
        w != (size_t)-1  &&  h != (size_t)-1) {
        // further validation: make sure we're actually on the image
        if (x >= width  ||  y >= height) {
            string msg("CImageIOPng::ReadImage(): invalid starting position: ");
            msg += NStr::NumericToString(x);
            msg += ", ";
            msg += NStr::NumericToString(y);
            NCBI_THROW(CImageException, eReadError, msg);
        }

        // clamp our width and height to the image size
        if (x + w >= width) {
            w = width - x;
            LOG_POST_X(27, Warning
                     << "CImageIOPng::ReadImage(): clamped width to " << w);
        }

        if (y + h >= height) {
            h = height - y;
            LOG_POST_X(28, Warning
                     << "CImageIOPng::ReadImage(): clamped height to " << h);
        }
    }

    png_read_update_info(png_ptr, info_ptr);
}


//
// our local i/o handlers
//
static void s_PngRead(png_structp png_ptr, png_bytep data, png_size_t len)
{
    CNcbiIfstream* istr =
        reinterpret_cast<CNcbiIfstream*>(png_get_io_ptr(png_ptr));
    if (istr) {
        istr->read(reinterpret_cast<char*>(data), len);
    }
}


static void s_PngWrite(png_structp png_ptr, png_bytep data, png_size_t len)
{
    CNcbiOfstream* ostr =
        reinterpret_cast<CNcbiOfstream*>(png_get_io_ptr(png_ptr));
    if (ostr) {
        ostr->write(reinterpret_cast<char*>(data), len);
    }
}


static void s_PngFlush(png_structp png_ptr)
{
    CNcbiOfstream* ostr =
        reinterpret_cast<CNcbiOfstream*>(png_get_io_ptr(png_ptr));
    if (ostr) {
        ostr->flush();
    }
}


//
// finalize our structures
//
static void s_PngReadFinalize(png_structp& png_ptr,
                              png_infop&   info_ptr,
                              png_infop&   end_info_ptr)
{
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
}


static void s_PngWriteFinalize(png_structp& png_ptr,
                              png_infop&    info_ptr)
{
    png_destroy_write_struct(&png_ptr, &info_ptr);
}


//
// ReadImage()
// read a PNG format image into memory, returning the image itself
// This version reads the entire image
//
CImage* CImageIOPng::ReadImage(CNcbiIstream& istr)
{
    png_structp png_ptr  = NULL;
    png_infop   info_ptr = NULL;
    png_infop   end_ptr  = NULL;
    CRef<CImage> image;

    try {
        // create our PNG structures
        s_PngReadInit(png_ptr, info_ptr, end_ptr);

        // begin reading our image
        png_set_read_fn(png_ptr, &istr, s_PngRead);
        png_read_info(png_ptr, info_ptr);

        // store and validate our image's parameters
        size_t width  = 0;
        size_t height = 0;
        size_t depth  = 0;
        size_t x = (size_t)-1;
        size_t y = (size_t)-1;
        size_t w = (size_t)-1;
        size_t h = (size_t)-1;
        s_PngReadValidate(png_ptr, info_ptr,
                          width, height, depth,
                          x, y, w, h);

        // allocate our image and read, line by line
        image.Reset(new CImage(width, height, depth));
        unsigned char* row_ptr = image->SetData();
        for (size_t i = 0;  i < height;  ++i) {
            png_read_row(png_ptr, row_ptr, NULL);
            row_ptr += width * depth;
        }

        // read the end pointer
        png_read_end(png_ptr, end_ptr);

        // close and return
        s_PngReadFinalize(png_ptr, info_ptr, end_ptr);
    }
    catch (...) {
        // destroy everything
        s_PngReadFinalize(png_ptr, info_ptr, end_ptr);

        // rethrow
        throw;
    }

    return image.Release();
}


//
// ReadImage()
// read a PNG format image into memory, returning the image itself
// This version reads a subpart of an image
//
CImage* CImageIOPng::ReadImage(CNcbiIstream& istr,
                               size_t x, size_t y, size_t w, size_t h)
{
    png_structp png_ptr  = NULL;
    png_infop   info_ptr = NULL;
    png_infop   end_ptr  = NULL;
    CRef<CImage> image;

    try {
        // create our PNG structures
        s_PngReadInit(png_ptr, info_ptr, end_ptr);

        // begin reading our image
        png_set_read_fn(png_ptr, &istr, s_PngRead);
        png_read_info(png_ptr, info_ptr);

        // store and validate our image's parameters
        size_t width  = 0;
        size_t height = 0;
        size_t depth  = 0;
        s_PngReadValidate(png_ptr, info_ptr, width, height, depth,
                          x, y, w, h);

        // allocate our image
        image.Reset(new CImage(w, h, depth));
        unsigned char* to_data   = image->SetData();
        size_t         to_stride = image->GetWidth() * image->GetDepth();
        size_t         to_offs   = x * image->GetDepth();

        // allocate a scanline for this image
        vector<unsigned char> row(width * depth);
        unsigned char* row_ptr = &row[0];

        size_t i;

        // read up to our starting scan line
        for (i = 0;  i < y;  ++i) {
            png_read_row(png_ptr, row_ptr, NULL);
        }

        // read and convert the scanlines of interest
        for ( ;  i < y + h;  ++i) {
            png_read_row(png_ptr, row_ptr, NULL);
            memcpy(to_data, row_ptr + to_offs, to_stride);
            to_data += to_stride;
        }

        /**
        // read the rest of the image (do we need this?)
        for ( ;  i < height;  ++i) {
            png_read_row(png_ptr, row_ptr, NULL);
        }

        // read the end pointer
        png_read_end(png_ptr, end_ptr);
        **/

        // close and return
        s_PngReadFinalize(png_ptr, info_ptr, end_ptr);
    }
    catch (...) {
        // destroy everything
        s_PngReadFinalize(png_ptr, info_ptr, end_ptr);

        // rethrow
        throw;
    }

    return image.Release();
}


bool CImageIOPng::ReadImageInfo(CNcbiIstream& istr,
                                size_t* width, size_t* height, size_t* depth)
{
    png_structp png_ptr  = NULL;
    png_infop   info_ptr = NULL;
    png_infop   end_ptr  = NULL;

    try {
        // create our PNG structures
        s_PngReadInit(png_ptr, info_ptr, end_ptr);

        // begin reading our image
        png_set_read_fn(png_ptr, &istr, s_PngRead);
        png_read_info(png_ptr, info_ptr);

        // store and validate our image's parameters
        size_t sub_width  = 0;
        size_t sub_height = 0;
        size_t sub_depth  = 0;
        size_t x = (size_t)-1;
        size_t y = (size_t)-1;
        size_t w = (size_t)-1;
        size_t h = (size_t)-1;
        s_PngReadValidate(png_ptr, info_ptr, sub_width, sub_height, sub_depth,
                          x, y, w, h);
        if (width) {
            *width = sub_width;
        }
        if (height) {
            *height = sub_height;
        }
        if (depth) {
            *depth = sub_depth;
        }

        // close and return
        s_PngReadFinalize(png_ptr, info_ptr, end_ptr);
        return true;
    }
    catch (...) {
        // destroy everything
        s_PngReadFinalize(png_ptr, info_ptr, end_ptr);
    }

    return false;
}


//
// WriteImage()
// write an image to a file in PNG format
// This version writes the entire image
//
void CImageIOPng::WriteImage(const CImage& image, CNcbiOstream& ostr,
                             CImageIO::ECompress compress)
{
    // make sure we've got an image
    if ( !image.GetData() ) {
        NCBI_THROW(CImageException, eWriteError,
                   "CImageIOPng::WriteImage(): "
                   "attempt to write an empty image");
    }

    // validate our image - we need RGB or RGBA images
    if (image.GetDepth() != 3  &&  image.GetDepth() != 4) {
        string msg("CImageIOPng::WriteImage(): invalid image depth: ");
        msg += NStr::NumericToString(image.GetDepth());
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    png_structp png_ptr  = NULL;
    png_infop   info_ptr = NULL;

    try {
        // initialize png stuff
        s_PngWriteInit(png_ptr, info_ptr,
                       image.GetWidth(), image.GetHeight(), image.GetDepth(),
                       compress);

        // begin writing data
        png_set_write_fn(png_ptr, &ostr, s_PngWrite, s_PngFlush);
        png_write_info(png_ptr, info_ptr);

        // write our image, line-by-line
        unsigned char* row_ptr = const_cast<unsigned char*> (image.GetData());
        size_t width  = image.GetWidth();
        size_t height = image.GetHeight();
        size_t depth  = image.GetDepth();
        for (size_t i = 0;  i < height;  ++i) {
            png_write_row(png_ptr, row_ptr);
            row_ptr += width * depth;
        }

        // standard clean-up
        png_write_end(png_ptr, info_ptr);
        s_PngWriteFinalize(png_ptr, info_ptr);
    }
    catch (...) {
        s_PngWriteFinalize(png_ptr, info_ptr);
        throw;
    }
}


//
// WriteImage()
// write an image to a file in PNG format
// This version writes only a subpart of the image
//
void CImageIOPng::WriteImage(const CImage& image, CNcbiOstream& ostr,
                             size_t x, size_t y, size_t w, size_t h,
                             CImageIO::ECompress compress)
{
    // make sure we've got an image
    if ( !image.GetData() ) {
        NCBI_THROW(CImageException, eWriteError,
                   "CImageIOPng::WriteImage(): "
                   "attempt to write an empty image");
    }

    // validate our image - we need RGB or RGBA images
    if (image.GetDepth() != 3  &&  image.GetDepth() != 4) {
        string msg("CImageIOPng::WriteImage(): invalid image depth: ");
        msg += NStr::NumericToString(image.GetDepth());
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    png_structp png_ptr  = NULL;
    png_infop   info_ptr = NULL;

    try {
        // initialize png stuff
        s_PngWriteInit(png_ptr, info_ptr,
                       w, h, image.GetDepth(),
                       compress);

        // begin writing data
        png_set_write_fn(png_ptr, &ostr, s_PngWrite, s_PngFlush);
        png_write_info(png_ptr, info_ptr);

        // write our image
        // we plan to march through only part of our image
        // get a pointer to the start of our scan line
        //
        // NB: the const cast is necessary as png_write_row takes a non-const
        // pointer (go figure...)
        unsigned char* from_data = const_cast<unsigned char*>(image.GetData());
        from_data += (y * image.GetWidth() + x) * image.GetDepth();
        size_t from_stride = w * image.GetDepth();

        // march out h scan lines
        for (size_t i = 0;  i < h;  ++i) {
            png_write_row(png_ptr, from_data);
            from_data += from_stride;
        }


        // standard clean-up
        png_write_end(png_ptr, info_ptr);
        s_PngWriteFinalize(png_ptr, info_ptr);
    }
    catch (...) {
        s_PngWriteFinalize(png_ptr, info_ptr);
        throw;
    }
}


END_NCBI_SCOPE

#else // !HAVE_LIBPNG

//
// LIBPNG functionality not included - we stub out the various needed
// functions
//


BEGIN_NCBI_SCOPE


CImage* CImageIOPng::ReadImage(CNcbiIstream& /* file */)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::ReadImage(): PNG format not supported");
}


CImage* CImageIOPng::ReadImage(CNcbiIstream& /* file */,
                               size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::ReadImage(): PNG format not supported");
}


bool CImageIOPng::ReadImageInfo(CNcbiIstream& istr,
                                size_t* width, size_t* height, size_t* depth)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::ReadImageInfo(): PNG format not supported");
}


void CImageIOPng::WriteImage(const CImage& /* image */,
                             CNcbiOstream& /* file */,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::WriteImage(): PNG format not supported");
}


void CImageIOPng::WriteImage(const CImage& /* image */,
                             CNcbiOstream& /* file */,
                             size_t, size_t, size_t, size_t,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::WriteImage(): PNG format not supported");
}


END_NCBI_SCOPE


#endif  // HAVE_LIBPNG
