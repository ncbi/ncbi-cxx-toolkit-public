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

#include <ncbi_pch.hpp>
#include "image_io_jpeg.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>
#include <util/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Util_Image

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

#ifdef HAVE_STDLIB_H
#  undef HAVE_STDLIB_H
#  define REDEFINE_HAVE_STDLIB_H
#endif

#include <jpeglib.h>

#if defined(REDEFINE_HAVE_STDLIB_H)  && !defined(HAVE_STDLIB_H)
#  define HAVE_STDLIB_H 1
#endif
}


BEGIN_NCBI_SCOPE

static const int sc_JpegBufLen = 4096;

struct SJpegErrorInfo
{
    SJpegErrorInfo()
        : has_error(false)
        {
        }

    string message;
    bool has_error;
};



static void s_JpegErrorHandler(j_common_ptr ptr)
{
    try {

        string msg("Error processing JPEG image: ");

        /// format the message
        char buffer[JMSG_LENGTH_MAX];
        (*ptr->err->format_message)(ptr, buffer);

        msg += buffer;

        LOG_POST_X(12, Error << msg);
#if JPEG_LIB_VERSION >= 62
        if (ptr->client_data) {
            SJpegErrorInfo* err_info = (SJpegErrorInfo*)ptr->client_data;
            if ( !err_info->message.empty() ) {
                err_info->message += "\n";
            }
            err_info->message += msg;
            err_info->has_error = true;
        }
#endif
    }
    catch (...) {
        LOG_POST_X(13, Error << "error processing error info");
    }
}


static void s_JpegOutputHandler(j_common_ptr ptr)
{
    string msg("JPEG message: ");

    /// format the message
    char buffer[JMSG_LENGTH_MAX];
    (*ptr->err->format_message)(ptr, buffer);

    msg += buffer;
    LOG_POST_X(14, Warning << msg);
}


//
// specialized internal structs used for reading/writing from streams
//

struct SJpegInput : jpeg_source_mgr // the input data elements
{
    // our buffer and stream
    CNcbiIstream* stream;
    JOCTET* buffer;
};


struct SJpegOutput : jpeg_destination_mgr // the input data elements
{
    // our buffer and stream
    CNcbiOstream* stream;
    JOCTET* buffer;
};


//
// JPEG stream i/o handlers handlers
//

// initialize reading on the stream
static void s_JpegReadInit(j_decompress_ptr cinfo)
{
    struct SJpegInput* sptr = (SJpegInput*)cinfo->src;

    // set up our buffer for the first read
    sptr->bytes_in_buffer = 0;
    sptr->next_input_byte = sptr->buffer;
}


// grab data from the stream
static boolean s_JpegReadBuffer(j_decompress_ptr cinfo)
{
    struct SJpegInput* sptr = (SJpegInput*)cinfo->src;
    if ( !*(sptr->stream) ) {
        return FALSE;
    }

    // read data from the stream
    sptr->stream->read(reinterpret_cast<char*>(sptr->buffer), sc_JpegBufLen);

    // reset our counts
    sptr->bytes_in_buffer = sptr->stream->gcount();
    sptr->next_input_byte = sptr->buffer;

    return sptr->bytes_in_buffer ? TRUE : FALSE;
}


// skip over some data in an input stream (rare operation)
static void s_JpegReadSkipData(j_decompress_ptr cinfo, long bytes)
{
    struct SJpegInput* sptr = (SJpegInput*)cinfo->src;

    if (bytes > 0) {
        while (*(sptr->stream)  &&
               bytes > (long)sptr->bytes_in_buffer) {
            bytes -= sptr->bytes_in_buffer;
            s_JpegReadBuffer(cinfo);
        }

        sptr->next_input_byte += (size_t) bytes;
        sptr->bytes_in_buffer -= (size_t) bytes;
    }
}


// finalize reading in a stream (no-op)
static void s_JpegReadTerminate(j_decompress_ptr)
{
    // don't need to do anything here
}


// initialize writing to a stream
static void s_JpegWriteInit(j_compress_ptr cinfo)
{
    struct SJpegOutput* sptr = (SJpegOutput*)cinfo->dest;

    // set up our buffer for the first read
    sptr->next_output_byte = sptr->buffer;
    sptr->free_in_buffer   = sc_JpegBufLen;
}


// fill the input buffer
static boolean s_JpegWriteBuffer(j_compress_ptr cinfo)
{
    struct SJpegOutput* sptr = (SJpegOutput*)cinfo->dest;

    // read data from the stream
    sptr->stream->write(reinterpret_cast<const char*>(sptr->buffer),
                        sc_JpegBufLen);

    // reset our counts
    sptr->next_output_byte = sptr->buffer;
    sptr->free_in_buffer  = sc_JpegBufLen;

    return TRUE;
}


// finalize our JPEG output
static void s_JpegWriteTerminate(j_compress_ptr cinfo)
{
    // don't need to do anything here
    struct SJpegOutput* sptr = (SJpegOutput*)cinfo->dest;
    size_t datacount = sc_JpegBufLen - sptr->free_in_buffer;

    // read data from the stream
    if (datacount > 0) {
        sptr->stream->write(reinterpret_cast<const char*>(sptr->buffer),
                            datacount);
    }
    sptr->stream->flush();
    if ( !*(sptr->stream) ) {
        LOG_POST(Error << "Error writing to JPEG stream");
    }
}


//
// set up our read structure to handle stream sources
// this is provided in place of jpeg_stdio_src()
//
static void s_JpegReadSetup(j_decompress_ptr cinfo,
                            CNcbiIstream& istr, JOCTET* buffer)
{
    _ASSERT(cinfo->src == NULL);
    struct SJpegInput* sptr = (SJpegInput*)calloc(1, sizeof(SJpegInput));
    cinfo->src = sptr;

    // allocate buffer and save our stream
    sptr->stream = &istr;
    sptr->buffer = buffer;

    // set our callbacks
    sptr->init_source       = s_JpegReadInit;
    sptr->fill_input_buffer = s_JpegReadBuffer;
    sptr->skip_input_data   = s_JpegReadSkipData;
    sptr->resync_to_restart = jpeg_resync_to_restart;
    sptr->term_source       = s_JpegReadTerminate;

    // make sure the structure knows we're just starting now
    sptr->bytes_in_buffer = 0;
    sptr->next_input_byte = buffer;
}



//
// set up our read structure to handle stream sources
// this is provided in place of jpeg_stdio_src()
//
static void s_JpegWriteSetup(j_compress_ptr cinfo,
                             CNcbiOstream& ostr, JOCTET* buffer)
{
    _ASSERT(cinfo->dest == NULL);
    struct SJpegOutput* sptr = (SJpegOutput*)calloc(1, sizeof(SJpegOutput));
    cinfo->dest = sptr;

    // allocate buffer and save our stream
    sptr->stream = &ostr;
    sptr->buffer = buffer;

    // set our callbacks
    sptr->init_destination    = s_JpegWriteInit;
    sptr->empty_output_buffer = s_JpegWriteBuffer;
    sptr->term_destination    = s_JpegWriteTerminate;

    // make sure the structure knows we're just starting now
    sptr->free_in_buffer = sc_JpegBufLen;
    sptr->next_output_byte = buffer;
}



//
// ReadImage()
// read a JPEG format image into memory, returning the image itself
// This version reads the entire image
//
CImage* CImageIOJpeg::ReadImage(CNcbiIstream& istr)
{
    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    CRef<CImage> image;

    // standard libjpeg stuff
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));

    SJpegErrorInfo jpeg_err_info;

    try {
        // open our file for reading
        // set up the standard error handler
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;
#if JPEG_LIB_VERSION >= 62
        cinfo.client_data = &jpeg_err_info;
#endif

        jpeg_create_decompress(&cinfo);
        if (jpeg_err_info.has_error) {
            throw runtime_error("error creating decompress info");
        }

        // set up our standard stream processor
        s_JpegReadSetup(&cinfo, istr, buf_ptr);
        if (jpeg_err_info.has_error) {
            throw runtime_error("error setting up input stream");
        }

        //jpeg_stdio_src(&cinfo, fp);
        jpeg_read_header(&cinfo, TRUE);
        if (jpeg_err_info.has_error) {
            throw runtime_error("error reading header");
        }

        // decompression parameters
        cinfo.dct_method = JDCT_FLOAT;
        jpeg_start_decompress(&cinfo);
        if (jpeg_err_info.has_error) {
            throw runtime_error("error starting decompression");
        }

        // allocate an image to hold our data
        image.Reset(new CImage(cinfo.output_width, cinfo.output_height, 3));
        _TRACE("JPEG image: "
               << cinfo.output_width << "x"
               << cinfo.output_height << "x"
               << cinfo.out_color_components);

        // we process the image 1 scanline at a time
        unsigned char *scanline[1];
        const size_t stride = cinfo.output_width * cinfo.out_color_components;
        switch (cinfo.out_color_components) {
        case 1:
            {{
                 unsigned char* data = image->SetData();
                 vector<unsigned char> scan_buf(stride);
                 scanline[0] = &scan_buf[0];
                 for (size_t i = 0;  i < image->GetHeight();  ++i) {
                     jpeg_read_scanlines(&cinfo, scanline, 1);
                     if (jpeg_err_info.has_error) {
                         throw runtime_error("error reading scanline " +
                                             NStr::NumericToString(i));
                     }

                     for (size_t j = 0;  j < stride;  ++j) {
                         *data++ = scanline[0][j];
                         *data++ = scanline[0][j];
                         *data++ = scanline[0][j];
                     }
                 }
             }}
            break;

        case 3:
            scanline[0] = image->SetData();
            for (size_t i = 0;  i < image->GetHeight();  ++i) {
                jpeg_read_scanlines(&cinfo, scanline, 1);
                if (jpeg_err_info.has_error) {
                    throw runtime_error("error reading scanline " +
                                        NStr::NumericToString(i));
                }
                scanline[0] += stride;
            }
            break;

        default:
            NCBI_THROW(CImageException, eReadError,
                       "CImageIOJpeg::ReadImage(): Unhandled color components");
        }

        ///
        /// sanity check:
        /// make sure we've processed enough scan lines
        ///
        if (cinfo.output_scanline != image->GetHeight()) {
            LOG_POST_X(15, Error << "Error: image is truncated: processed "
                           << cinfo.output_scanline << "/" << image->GetHeight()
                           << " scanlines");
        }

        //
        // standard clean-up
        // if we've gotten this far, the image is at least usable
        //
        jpeg_finish_decompress(&cinfo);
        if (jpeg_err_info.has_error) {
            LOG_POST_X(16, Error << "Error in finalizing image decompression: "
                           << jpeg_err_info.message);
            jpeg_err_info.has_error = false;
            jpeg_err_info.message.erase();
        }

        jpeg_destroy_decompress(&cinfo);
        if (jpeg_err_info.has_error) {
            LOG_POST_X(17, Error << "Error in finalizing image decompression: "
                           << jpeg_err_info.message);
            jpeg_err_info.message.erase();
        }
    }
    catch (CException&) {
        // clean up our mess
        jpeg_destroy_decompress(&cinfo);
        throw;
    }
    catch (std::exception& e) {
        // clean up our mess
        jpeg_destroy_decompress(&cinfo);
        LOG_POST_X(18, Error << "error reading JPEG image: " << e.what());
        NCBI_THROW(CImageException, eReadError,
                   "Error reading JPEG image");
    }
    catch (...) {
        // clean up our mess
        jpeg_destroy_decompress(&cinfo);
        throw;
    }
    return image.Release();
}


//
// ReadImage()
// read a JPEG format image into memory, returning the image itself
// This version reads only a subset of the image
//
CImage* CImageIOJpeg::ReadImage(CNcbiIstream& istr,
                                size_t x, size_t y, size_t w, size_t h)
{
    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    CRef<CImage> image;

    // standard libjpeg stuff
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));

    try {
        // open our file for reading
        // set up the standard error handler
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;

        jpeg_create_decompress(&cinfo);

        // set up our standard stream processor
        s_JpegReadSetup(&cinfo, istr, buf_ptr);

        //jpeg_stdio_src(&cinfo, fp);
        jpeg_read_header(&cinfo, TRUE);

        // decompression parameters
        cinfo.dct_method = JDCT_FLOAT;
        jpeg_start_decompress(&cinfo);

        // further validation: make sure we're actually on the image
        if (x >= cinfo.output_width  ||  y >= cinfo.output_height) {
            string msg("CImageIOJpeg::ReadImage(): invalid starting position: ");
            msg += NStr::NumericToString(x);
            msg += ", ";
            msg += NStr::NumericToString(y);
            msg += " (image dimensions: ";
            msg += NStr::NumericToString(cinfo.output_width);
            msg += ", ";
            msg += NStr::NumericToString(cinfo.output_height);
            msg += ")";
            NCBI_THROW(CImageException, eReadError, msg);
        }

        // clamp our width and height to the image size
        if (x + w >= cinfo.output_width) {
            w = cinfo.output_width - x;
            LOG_POST_X(19, Warning
                     << "CImageIOJpeg::ReadImage(): clamped width to " << w);
        }

        if (y + h >= cinfo.output_height) {
            h = cinfo.output_height - y;
            LOG_POST_X(20, Warning
                     << "CImageIOJpeg::ReadImage(): clamped height to " << h);
        }


        // allocate an image to store the subimage's data
        image.Reset(new CImage(w, h, 3));

        // we process the image 1 scanline at a time
        unsigned char *scanline[1];
        const size_t stride = cinfo.output_width * cinfo.out_color_components;
        vector<unsigned char> row(stride);
        scanline[0] = &row[0];

        size_t i;

        // start by skipping a number of scan lines
        for (i = 0;  i < y;  ++i) {
            jpeg_read_scanlines(&cinfo, scanline, 1);
        }

        // now read the actual data
        unsigned char* data = image->SetData();
        switch (cinfo.out_color_components) {
        case 1:
            for ( ;  i < y + h;  ++i) {
                jpeg_read_scanlines(&cinfo, scanline, 1);

                for (size_t j = x;  j < x + w;  ++j) {
                    *data++ = scanline[0][j];
                    *data++ = scanline[0][j];
                    *data++ = scanline[0][j];
                }
            }
            break;

        case 3:
            {{
                 const size_t offs      = x * image->GetDepth();
                 const size_t scan_w    = w * image->GetDepth();
                 const size_t to_stride = image->GetWidth() * image->GetDepth();
                 for ( ;  i < y + h;  ++i) {
                     jpeg_read_scanlines(&cinfo, scanline, 1);
                     memcpy(data, scanline[0] + offs, scan_w);
                     data += to_stride;
                 }
             }}
            break;

        default:
            NCBI_THROW(CImageException, eReadError,
                       "CImageIOJpeg::ReadImage(): Unhandled color components");
        }

        // standard clean-up
        jpeg_destroy_decompress(&cinfo);
    }
    catch (...) {

        // clean up our mess
        jpeg_destroy_decompress(&cinfo);

        throw;
    }
    return image.Release();
}


bool CImageIOJpeg::ReadImageInfo(CNcbiIstream& istr,
                                size_t* width, size_t* height, size_t* depth)
{
    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    // standard libjpeg stuff
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));

    try {
        // open our file for reading
        // set up the standard error handler
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;

        jpeg_create_decompress(&cinfo);

        // set up our standard stream processor
        s_JpegReadSetup(&cinfo, istr, buf_ptr);

        //jpeg_stdio_src(&cinfo, fp);
        int ret_val = jpeg_read_header(&cinfo, TRUE);
        if (ret_val != JPEG_HEADER_OK) {
            NCBI_THROW(CException, eUnknown,
                "invalid image header");
        }

        // decompression parameters
        cinfo.dct_method = JDCT_FLOAT;
        jpeg_start_decompress(&cinfo);

        if (width) {
            *width = cinfo.output_width;
        }
        if (height) {
            *height = cinfo.output_height;
        }
        if (depth) {
            *depth = cinfo.out_color_components;
        }

        // standard clean-up
        jpeg_destroy_decompress(&cinfo);

        return true;
    }
    catch (...) {
        // clean up our mess
        jpeg_destroy_decompress(&cinfo);
    }
    return false;
}


//
// WriteImage()
// write an image to a file in JPEG format
// This version writes the entire image
//
void CImageIOJpeg::WriteImage(const CImage& image, CNcbiOstream& ostr,
                              CImageIO::ECompress compress)
{
    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    struct jpeg_compress_struct cinfo = {};
    struct jpeg_error_mgr jerr = {};

    try {
        // standard libjpeg setup
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;

        jpeg_create_compress(&cinfo);
        //jpeg_stdio_dest(&cinfo, fp);
        s_JpegWriteSetup(&cinfo, ostr, buf_ptr);

        cinfo.image_width      = (JDIMENSION)image.GetWidth();
        cinfo.image_height     = (JDIMENSION)image.GetHeight();
        cinfo.input_components = (int)image.GetDepth();
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
            LOG_POST_X(21, Error << "unknown compression type: " << (int)compress);
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
    }
    catch (...) {
        jpeg_destroy_compress(&cinfo);
        throw;
    }
}


//
// WriteImage()
// write an image to a file in JPEG format
// This version writes only a subpart of the image
//
void CImageIOJpeg::WriteImage(const CImage& image, CNcbiOstream& ostr,
                              size_t x, size_t y, size_t w, size_t h,
                              CImageIO::ECompress compress)
{
    // validate our image position as inside the image we've been passed
    if (x >= image.GetWidth()  ||  y >= image.GetHeight()) {
        string msg("CImageIOJpeg::WriteImage(): invalid image position: ");
        msg += NStr::NumericToString(x);
        msg += ", ";
        msg += NStr::NumericToString(y);
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // clamp our width and height
    if (x + w >= image.GetWidth()) {
        w = image.GetWidth() - x;
        LOG_POST_X(22, Warning
                 << "CImageIOJpeg::WriteImage(): clamped width to " << w);
    }

    if (y + h >= image.GetHeight()) {
        h = image.GetHeight() - y;
        LOG_POST_X(23, Warning
                 << "CImageIOJpeg::WriteImage(): clamped height to " << h);
    }

    vector<JOCTET> buffer(sc_JpegBufLen);
    JOCTET* buf_ptr = &buffer[0];

    struct jpeg_compress_struct cinfo = {};
    struct jpeg_error_mgr jerr = {};

    try {
        // standard libjpeg setup
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->error_exit = s_JpegErrorHandler;
        cinfo.err->output_message = s_JpegOutputHandler;

        jpeg_create_compress(&cinfo);
        //jpeg_stdio_dest(&cinfo, fp);
        s_JpegWriteSetup(&cinfo, ostr, buf_ptr);

        cinfo.image_width      = (JDIMENSION)w;
        cinfo.image_height     = (JDIMENSION)h;
        cinfo.input_components = (int)image.GetDepth();
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
            LOG_POST_X(24, Error << "unknown compression type: " << (int)compress);
            break;
        }
        jpeg_set_quality(&cinfo, quality, TRUE);

        // begin compression
        jpeg_start_compress(&cinfo, true);

        // process our data on a line-by-line basis
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
    }
    catch (...) {
        jpeg_destroy_compress(&cinfo);
        throw;
    }
}


END_NCBI_SCOPE


#else // !HAVE_LIBJPEG

//
// JPEGLIB functionality not included - we stub out the various needed
// functions
//


BEGIN_NCBI_SCOPE


CImage* CImageIOJpeg::ReadImage(CNcbiIstream& /* file */)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::ReadImage(): JPEG format not supported");
}


CImage* CImageIOJpeg::ReadImage(CNcbiIstream& /* file */,
                                size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::ReadImage(): JPEG format not supported");
}


bool CImageIOJpeg::ReadImageInfo(CNcbiIstream& istr,
                                size_t* width, size_t* height, size_t* depth)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::ReadImageInfo(): JPEG format not supported");
}


void CImageIOJpeg::WriteImage(const CImage& /* image */,
                              CNcbiOstream& /* file */,
                              CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::WriteImage(): JPEG format not supported");
}


void CImageIOJpeg::WriteImage(const CImage& /* image */,
                              CNcbiOstream& /* file */,
                              size_t, size_t, size_t, size_t,
                              CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOJpeg::WriteImage(): JPEG format not supported");
}


END_NCBI_SCOPE


#endif  // HAVE_LIBJPEG
