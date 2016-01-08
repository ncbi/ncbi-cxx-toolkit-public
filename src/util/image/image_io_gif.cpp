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
 *    CImageIOGif -- interface class for reading/writing CompuServ GIF files
 */

//
// we include gif_lib.h first because of a conflict with windows.h
// (DrawText() is both a giflib function and a Win32 GDI function)
//
#include <ncbi_pch.hpp>
#include <ncbiconf.h>
#ifdef HAVE_LIBGIF
// alas, poor giflib... it isn't extern'ed
extern "C" {
#  include <gif_lib.h>

    /// !@#$%^ libunfig mis-spelled the prototype in their header,
    /// so we must add it here
    GifFileType *EGifOpen(void *userPtr, OutputFunc writeFunc);

};
#endif

#include "image_io_gif.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>
#include <util/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   Util_Image

#ifdef HAVE_LIBGIF

//
//
// LIBGIF support
//
//

BEGIN_NCBI_SCOPE



static int s_GifRead(GifFileType* file, GifByteType* data, int len)
{
    CNcbiIstream* istr = reinterpret_cast<CNcbiIstream*>(file->UserData);
    if (istr) {
        istr->read(reinterpret_cast<char*>(data), len);
        return (int)istr->gcount();
    }
    return -1;
}


static int s_GifWrite(GifFileType* file, const GifByteType* data, int len)
{
    CNcbiOstream* ostr = reinterpret_cast<CNcbiOstream*>(file->UserData);
    if (ostr) {
        ostr->write(reinterpret_cast<const char*>(data), len);
        if ( *ostr ) {
            return len;
        }
    }
    return -1;
}


//
// ReadImage()
// read an entire GIF image into memory.  This will read only the first image
// in an image set.
//
CImage* CImageIOGif::ReadImage(CNcbiIstream& istr)
{
    GifFileType* fp = NULL;
    CRef<CImage> image;

    try {
        // open our file for reading
        fp = DGifOpen(&istr, s_GifRead);
        if ( !fp ) {
            NCBI_THROW(CImageException, eReadError,
                       "CImageIOGif::ReadImage(): "
                       "cannot open file for reading");
        }

        // allocate an image
        image.Reset(new CImage(fp->SWidth, fp->SHeight, 3));
        memset(image->SetData(), fp->SBackGroundColor,
               image->GetWidth() * image->GetHeight() * image->GetDepth());

        // we also allocate a single row
        // this row is a color indexed row, and will be decoded row-by-row into the
        // image
        vector<unsigned char> row_data(image->GetWidth());
        unsigned char* row_ptr = &row_data[0];

        bool done = false;
        while ( !done ) {
            // determine what sort of record type we have
            // these can be image, extension, or termination
            GifRecordType type;
            if (DGifGetRecordType(fp, &type) == GIF_ERROR) {
                NCBI_THROW(CImageException, eReadError,
                    "CImageIOGif::ReadImage(): error reading file");
            }

            switch (type) {
            case IMAGE_DESC_RECORD_TYPE:
                //
                // we only support the first image in a gif
                //
                if (DGifGetImageDesc(fp) == GIF_ERROR) {
                    NCBI_THROW(CImageException, eReadError,
                        "CImageIOGif::ReadImage(): error reading file");
                }

                if (fp->Image.Interlace) {
                    // interlaced images are a bit more complex
                    size_t row = fp->Image.Top;
                    size_t col = fp->Image.Left;
                    size_t wid = fp->Image.Width;
                    size_t ht  = fp->Image.Height;

                    static int interlaced_offs[4] = { 0, 4, 2, 1 };
                    static int interlaced_jump[4] = { 8, 8, 4, 2 };
                    for (size_t i = 0;  i < 4;  ++i) {
                        for (size_t j = row + interlaced_offs[i];
                             j < row + ht;  j += interlaced_jump[i]) {
                            x_ReadLine(fp, row_ptr);
                            x_UnpackData(fp, row_ptr,
                                         image->SetData() +
                                         (j * wid + col) * image->GetDepth());
                        }
                    }
                } else {
                    size_t col = fp->Image.Left;
                    size_t wid = fp->Image.Width;
                    size_t ht  = fp->Image.Height;

                    for (size_t i = 0;  i < ht;  ++i) {
                        x_ReadLine(fp, row_ptr);
                        x_UnpackData(fp, row_ptr,
                                     image->SetData() +
                                     (i * wid + col) * image->GetDepth());
                    }
                }
                break;

            case EXTENSION_RECORD_TYPE:
                {{
                     int ext_code;
                     GifByteType* extension;

                     // we ignore extension blocks
                     if (DGifGetExtension(fp, &ext_code, &extension) == GIF_ERROR) {
                         NCBI_THROW(CImageException, eReadError,
                                    "CImageIOGif::ReadImage(): "
                                    "error reading file");
                     }
                     while (extension != NULL) {
                         if (DGifGetExtensionNext(fp, &extension) == GIF_OK) {
                             continue;
                         }

                         NCBI_THROW(CImageException, eReadError,
                                    "CImageIOGif::ReadImage(): "
                                    "error reading file");
                     }
                 }}
                break;

            default:
                // terminate record - break our of our while()
                done = true;
                break;
            }
        }

        // close up and exit
        DGifCloseFile(fp);
    }
    catch (...) {
        DGifCloseFile(fp);
        fp = NULL;
        throw;
    }

    return image.Release();
}


//
// ReadImage
// this version returns a sub-image from the desired image.
//
CImage* CImageIOGif::ReadImage(CNcbiIstream& istr,
                               size_t x, size_t y, size_t w, size_t h)
{
    // we use a brain-dead implementation here - this can be done in a more
    // memory-efficient manner...
    CRef<CImage> image(ReadImage(istr));
    return image->GetSubImage(x, y, w, h);
}


bool CImageIOGif::ReadImageInfo(CNcbiIstream& istr,
                                size_t* width, size_t* height, size_t* depth)
{
    GifFileType* fp = NULL;
    try {
        // open our file for reading
        fp = DGifOpen(&istr, s_GifRead);
        if ( !fp ) {
            NCBI_THROW(CImageException, eReadError,
                       "CImageIOGif::ReadImageInfo(): "
                       "cannot open file for reading");
        }

        // allocate an image
        if (width) {
            *width = fp->SWidth;
        }
        if (height) {
            *height = fp->SHeight;
        }
        if (depth) {
            *depth = 3;
        }

        // close up and exit
        DGifCloseFile(fp);
        return true;
    }
    catch (...) {
        DGifCloseFile(fp);
        fp = NULL;
    }

    return false;
}

//
// WriteImage()
// this writes out a GIF image.
//
void CImageIOGif::WriteImage(const CImage& image, CNcbiOstream& ostr,
                             CImageIO::ECompress)
{
    if ( !image.GetData() ) {
        NCBI_THROW(CImageException, eWriteError,
                   "CImageIOGif::WriteImage(): "
                   "cannot write empty image to file");
    }

    ColorMapObject* cmap = NULL;
    GifFileType* fp = NULL;

    try {
        // first, we need to split our image into red/green/blue channels
        // we do this to get proper GIF quantization
        size_t size = image.GetWidth() * image.GetHeight();
        vector<unsigned char> red  (size);
        vector<unsigned char> green(size);
        vector<unsigned char> blue (size);

        unsigned char* red_ptr   = &red[0];
        unsigned char* green_ptr = &green[0];
        unsigned char* blue_ptr  = &blue[0];
        const unsigned char* from_data = image.GetData();
        const unsigned char* end_data  = image.GetData() + size * image.GetDepth();

        switch (image.GetDepth()) {
        case 3:
            {{
                 for ( ;  from_data != end_data;  ) {
                     *red_ptr++   = *from_data++;
                     *green_ptr++ = *from_data++;
                     *blue_ptr++  = *from_data++;
                 }
             }}
            break;

        case 4:
            {{
                 LOG_POST_X(10, Warning <<
                                "CImageIOGif::WriteImage(): "
                                "ignoring alpha channel");
                 for ( ;  from_data != end_data;  ) {
                     *red_ptr++   = *from_data++;
                     *green_ptr++ = *from_data++;
                     *blue_ptr++  = *from_data++;

                     // alpha channel ignored - should we use this to compute a
                     // scaled rgb?
                     ++from_data;
                 }
             }}
            break;

        default:
            NCBI_THROW(CImageException, eWriteError,
                       "CImageIOGif::WriteImage(): unsupported image depth");
        }

        // reset the color channel pointers!
        red_ptr   = &red[0];
        green_ptr = &green[0];
        blue_ptr  = &blue[0];

        // now, create a GIF color map object
        int cmap_size = 256;
        cmap = MakeMapObject(cmap_size, NULL);
        if ( !cmap ) {
            NCBI_THROW(CImageException, eWriteError,
                       "CImageIOGif::WriteImage(): failed to allocate color map");
        }

        // we also allocate a strip of data to hold the indexed colors
        vector<unsigned char> qdata(size);
        unsigned char* qdata_ptr = &qdata[0];

        // quantize our colors
        if (QuantizeBuffer((unsigned int)image.GetWidth(),
                           (unsigned int)image.GetHeight(), &cmap_size,
                           red_ptr, green_ptr, blue_ptr,
                           qdata_ptr, cmap->Colors) == GIF_ERROR) {
            free(cmap);
            NCBI_THROW(CImageException, eWriteError,
                       "CImageIOGif::WriteImage(): failed to quantize image");
        }

        //
        // we are now ready to write our file
        //

        // open our file
        fp = EGifOpen(&ostr, s_GifWrite);
        if ( !fp ) {
            NCBI_THROW(CImageException, eWriteError,
                       "CImageIOGif::WriteImage(): failed to open file");
        }

        // write the GIF screen description
        if (EGifPutScreenDesc(fp, (int)image.GetWidth(), (int)image.GetHeight(),
                              8, 0, cmap) == GIF_ERROR) {
            NCBI_THROW(CImageException, eWriteError,
                "CImageIOGif::WriteImage(): failed to write GIF screen "
                "description");
        }

        // write the GIF image description
        if (EGifPutImageDesc(fp, 0, 0, (int)image.GetWidth(), (int)image.GetHeight(),
                             false, NULL) == GIF_ERROR) {
            NCBI_THROW(CImageException, eWriteError,
                       "CImageIOGif::WriteImage(): failed to write GIF image "
                       "description");
        }

        // put our data
        for (size_t i = 0;  i < image.GetHeight();  ++i) {
            if (EGifPutLine(fp, qdata_ptr, (int)image.GetWidth()) == GIF_ERROR) {
                string msg("CImageIOGif::WriteImage(): error writing line ");
                msg += NStr::NumericToString(i);
                NCBI_THROW(CImageException, eWriteError, msg);
            }

            qdata_ptr += image.GetWidth();
        }

        // clean-up and close
        if (EGifCloseFile(fp) == GIF_ERROR) {
            fp = NULL;
            NCBI_THROW(CImageException, eWriteError,
                       "CImageIOGif::WriteImage(): error closing file");
        }

        free(cmap);
    }
    catch (...) {
        if (fp) {
            if (EGifCloseFile(fp) == GIF_ERROR) {
                LOG_POST_X(11, Error
                    << "CImageIOGif::WriteImage(): error closing file");
            }
            fp = NULL;
        }

        if (cmap) {
            free(cmap);
            cmap = NULL;
        }

        throw;
    }
}


//
// WriteImage()
// this writes out a sub-image as a GIF file.  We use a brain-dead
// implementation currently - subset the image and write it out, rather than
// simply quantizing just the sub-image's data
//
void CImageIOGif::WriteImage(const CImage& image, CNcbiOstream& ostr,
                             size_t x, size_t y, size_t w, size_t h,
                             CImageIO::ECompress compress)
{
    CRef<CImage> subimage(image.GetSubImage(x, y, w, h));
    WriteImage(*subimage, ostr, compress);
}


//
// x_UnpackData()
// this function de-indexes a GIF image's color table, expanding the values
// into RGB tuples
//
void CImageIOGif::x_UnpackData(GifFileType* fp,
                               const unsigned char* from_data,
                               unsigned char* to_data)
{
    struct ColorMapObject* cmap =
        (fp->Image.ColorMap ? fp->Image.ColorMap : fp->SColorMap);

    for (int i = 0;  i < fp->Image.Width;  ++i) {
        *to_data++ = cmap->Colors[ from_data[i] ].Red;
        *to_data++ = cmap->Colors[ from_data[i] ].Green;
        *to_data++ = cmap->Colors[ from_data[i] ].Blue;
    }
}


//
// x_ReadLine()
// read a single line from a GIF file
//
void CImageIOGif::x_ReadLine(GifFileType* fp, unsigned char* data)
{
    if ( DGifGetLine(fp, data, fp->Image.Width) == GIF_ERROR) {
        DGifCloseFile(fp);
        string msg("CImageIOGif::ReadImage(): error reading file");
        NCBI_THROW(CImageException, eReadError, msg);
    }
}


END_NCBI_SCOPE



#else   // HAVE_LIBGIF

//
// LIBGIF not supported
//

BEGIN_NCBI_SCOPE

CImage* CImageIOGif::ReadImage(CNcbiIstream&)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::ReadImage(): GIF format not supported");
}


CImage* CImageIOGif::ReadImage(CNcbiIstream&,
                               size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::ReadImage(): GIF format not supported");
}


bool CImageIOGif::ReadImageInfo(CNcbiIstream&,
                                size_t*, size_t*, size_t*)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::ReadImageInfo(): GIF format not supported");
}

void CImageIOGif::WriteImage(const CImage&, CNcbiOstream&,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::WriteImage(): GIF format not supported");
}


void CImageIOGif::WriteImage(const CImage&, CNcbiOstream&,
                             size_t, size_t, size_t, size_t,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::WriteImage(): GIF format not supported");
}


END_NCBI_SCOPE

#endif  // !HAVE_LIBGIF
