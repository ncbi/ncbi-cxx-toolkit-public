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

#include "image_io_png.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>

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
// ReadImage()
// read a PNG format image into memory, returning the image itself
// This version reads the entire image
//
CImage* CImageIOPng::ReadImage(const string& file)
{
    // open our file for reading
    FILE* fp = fopen(file.c_str(), "rb");
    if ( !fp ) {
        string msg("CImageIOPng::ReadImage(): can't open file ");
        msg += file;
        msg += " for reading";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // initialize png stuff
    png_structp png_ptr =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if ( !png_ptr ) {
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_read_struct() failed");
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if ( !info_ptr ) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_info_struct() failed");
    }

    png_infop end_info_ptr = png_create_info_struct(png_ptr);
    if ( !end_info_ptr ) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_info_struct() failed");
    }

    // begin reading our image
    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    // store and validate our image's parameters
    size_t width           = info_ptr->width;
    size_t height          = info_ptr->height;
    size_t depth           = info_ptr->channels;
    png_byte color_type = info_ptr->color_type;
    png_byte bit_depth  = info_ptr->bit_depth;

    // we support only RGB and RGBA images
    if ( color_type != PNG_COLOR_TYPE_RGB  &&
         color_type != PNG_COLOR_TYPE_RGB_ALPHA ) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
        fclose(fp);

        string msg("CImageIOPng::ReadImage(): unhandled color type: ");
        msg += NStr::IntToString((int)color_type);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // ...and only with a bit depth of 8
    if (bit_depth != 8) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
        fclose(fp);

        string msg("CImageIOPng::ReadImage(): unhandled bit depth: ");
        msg += NStr::IntToString((int)bit_depth);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // this goes along with RGB or RGBA
    if (depth != 3  &&  depth != 4) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
        fclose(fp);

        string msg("CImageIOPng::ReadImage(): unhandled image channels: ");
        msg += NStr::IntToString((int)depth);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    png_read_update_info(png_ptr, info_ptr);

    // allocate our image and read, line by line
    CRef<CImage> image(new CImage(width, height, depth));
    unsigned char* row_ptr = image->SetData();
    for (size_t i = 0;  i < height;  ++i) {
        png_read_row(png_ptr, row_ptr, NULL);
        row_ptr += width * depth;
    }

    // close and return
    png_read_end(png_ptr, end_info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
    fclose(fp);

    return image.Release();
}


//
// ReadImage()
// read a PNG format image into memory, returning the image itself
// This version reads a subpart of an image
//
CImage* CImageIOPng::ReadImage(const string& file,
                               size_t x, size_t y, size_t w, size_t h)
{
    // open our file for reading
    FILE* fp = fopen(file.c_str(), "rb");
    if ( !fp ) {
        string msg("CImageIOPng::ReadImage(): can't open file ");
        msg += file;
        msg += " for reading";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // initialize png stuff
    png_structp png_ptr =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if ( !png_ptr ) {
        fclose(fp);
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_read_struct() failed");
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if ( !info_ptr ) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_info_struct() failed");
    }

    png_infop end_info_ptr = png_create_info_struct(png_ptr);
    if ( !end_info_ptr ) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_info_struct() failed");
    }

    // begin reading
    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    // store and validate our image's parameters
    size_t width           = info_ptr->width;
    size_t height          = info_ptr->height;
    size_t depth           = info_ptr->channels;
    png_byte color_type = info_ptr->color_type;
    png_byte bit_depth  = info_ptr->bit_depth;

    // validate against CImage supprt
    // we only deal with RGB or RGBA images, 8 bits per pixel
    if ( color_type != PNG_COLOR_TYPE_RGB  &&
         color_type != PNG_COLOR_TYPE_RGB_ALPHA ) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
        fclose(fp);

        string msg("CImageIOPng::ReadImage(): unhandled color type: ");
        msg += NStr::IntToString((int)color_type);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    if (bit_depth != 8) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
        fclose(fp);

        string msg("CImageIOPng::ReadImage(): unhandled bit depth: ");
        msg += NStr::IntToString((int)bit_depth);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    if (depth != 3  &&  depth != 4) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
        fclose(fp);

        string msg("CImageIOPng::ReadImage(): unhandled image channels: ");
        msg += NStr::IntToString((int)depth);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // further validation: make sure we're actually on the image
    if (x >= width  ||  y >= height) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
        fclose(fp);

        string msg("CImageIOJpeg::ReadImage(): invalid starting position: ");
        msg += NStr::IntToString(x);
        msg += ", ";
        msg += NStr::IntToString(y);
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // clamp our width and height to the image size
    if (x + w >= width) {
        w = width - x;
        LOG_POST(Warning
                 << "CImageIOPng::ReadImage(): clamped width to " << w);
    }

    if (y + h >= height) {
        h = height - y;
        LOG_POST(Warning
                 << "CImageIOPng::ReadImage(): clamped height to " << h);
    }

    png_read_update_info(png_ptr, info_ptr);

    // allocate our image
    CRef<CImage> image(new CImage(w, h, depth));
    unsigned char* to_data = image->SetData();
    size_t to_stride = image->GetWidth() * image->GetDepth();

    // allocate a scanline for this image
    AutoPtr<unsigned char, ArrayDeleter<unsigned char> > row
        (new unsigned char[width * depth]);
    unsigned char* row_ptr = row.get();

    size_t i;

    // read up to our starting scan line
    for (i = 0;  i < y;  ++i) {
        png_read_row(png_ptr, row_ptr, NULL);
    }

    // read and convert the scanlines of interest
    for ( ;  i < y + h;  ++i) {
        png_read_row(png_ptr, row_ptr, NULL);
        memcpy(to_data, row_ptr, to_stride);
        to_data += to_stride;
    }

    // read the rest of the image (do we need this?)
    for ( ;  i < height;  ++i) {
        png_read_row(png_ptr, row_ptr, NULL);
    }

    // close up shop
    png_read_end(png_ptr, end_info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
    fclose(fp);

    return image.Release();
}


//
// WriteImage()
// write an image to a file in PNG format
// This version writes the entire image
//
void CImageIOPng::WriteImage(const CImage& image, const string& file,
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
        msg += NStr::IntToString(image.GetDepth());
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // open our file for writing
    FILE* fp = fopen(file.c_str(), "wb");
    if ( !fp ) {
        string msg("CImageIOPng::WriteImage(): can't open file ");
        msg += file;
        msg += " for reading";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // initialize png stuff
    png_structp png_ptr =
        png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if ( !png_ptr ) {
        fclose(fp);

        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_read_struct() failed");
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if ( !info_ptr ) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);

        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_info_struct() failed");
    }

    // begin writing data
    png_init_io(png_ptr, fp);

    png_byte color_type = PNG_COLOR_TYPE_RGB;
    if (image.GetDepth() == 4) {
        color_type = PNG_COLOR_TYPE_RGBA;
    }
    png_set_IHDR(png_ptr, info_ptr,
                 image.GetWidth(), image.GetHeight(), 8, color_type,
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
        LOG_POST(Error << "unknown compression type: " << (int)compress);
        break;
    }

    // now, write!
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
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}


//
// WriteImage()
// write an image to a file in PNG format
// This version writes only a subpart of the image
//
void CImageIOPng::WriteImage(const CImage& image, const string& file,
                             size_t x, size_t y, size_t w, size_t h,
                             CImageIO::ECompress compress)
{
    // make sure we've got an image
    if ( !image.GetData() ) {
        NCBI_THROW(CImageException, eWriteError,
                   "CImageIOPng::WriteImage(): "
                   "attempt to write an empty image");
    }

    // validate our position as inside the image
    if (x >= image.GetWidth()  ||  y >= image.GetHeight()) {
        string msg("CImageIOPng::WriteImage(): invalid image position: ");
        msg += NStr::IntToString(x);
        msg += ", ";
        msg += NStr::IntToString(y);
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // clamp our width and height
    if (x+w >= image.GetWidth()) {
        w = image.GetWidth() - x;
        LOG_POST(Warning
                 << "CImageIOPng::WriteImage(): clamped width to " << w);
    }

    if (y+h >= image.GetHeight()) {
        h = image.GetHeight() - y;
        LOG_POST(Warning
                 << "CImageIOPng::WriteImage(): clamped height to " << h);
    }

    // open our file
    FILE* fp = fopen(file.c_str(), "wb");
    if ( !fp ) {
        string msg("CImageIOPng::WriteImage(): can't open file ");
        msg += file;
        msg += " for writing";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // initialize png stuff
    png_structp png_ptr =
        png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if ( !png_ptr ) {
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_read_struct() failed");
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if ( !info_ptr ) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        NCBI_THROW(CImageException, eReadError,
                   "CImageIOPng::ReadImage(): png_create_info_struct() failed");
    }

    // begin writing
    png_init_io(png_ptr, fp);

    png_byte color_type = PNG_COLOR_TYPE_RGB;
    if (image.GetDepth() == 4) {
        color_type = PNG_COLOR_TYPE_RGBA;
    }
    png_set_IHDR(png_ptr, info_ptr,
                 w, h, 8, color_type,
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
        LOG_POST(Error << "unknown compression type: " << (int)compress);
        break;
    }

    // now, write!
    png_write_info(png_ptr, info_ptr);

    // we plan to march through only part of our image
    // get a pointer to the start of our scan line
    unsigned char* from_data = const_cast<unsigned char*> (image.GetData());
    from_data += (y * image.GetWidth() + x) * image.GetDepth();
    size_t from_stride = image.GetWidth() * image.GetDepth();

    // march out h scan lines
    for (size_t i = 0;  i < h;  ++i) {
        png_write_row(png_ptr, from_data);
        from_data += from_stride;
    }

    // close up and return
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}


END_NCBI_SCOPE

#else // !HAVE_LIBPNG

//
// LIBPNG functionality not included - we stub out the various needed
// functions
//


BEGIN_NCBI_SCOPE


CImage* CImageIOPng::ReadImage(const string& file)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::ReadImage(): PNG format not supported");
}


CImage* CImageIOPng::ReadImage(const string& file,
                               size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::ReadImage(): PNG format not supported");
}


void CImageIOPng::WriteImage(const CImage& image, const string& file,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::WriteImage(): PNG format not supported");
}


void CImageIOPng::WriteImage(const CImage& image, const string& file,
                             size_t, size_t, size_t, size_t,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOPng::WriteImage(): PNG format not supported");
}


END_NCBI_SCOPE


#endif  // HAVE_LIBPNG

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

