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
#include <ncbiconf.h>
#ifdef HAVE_LIBGIF
// alas, poor giflib... it isn't extern'ed
extern "C" {
#  include <gif_lib.h>
};
#endif


#include "image_io_gif.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>

#ifdef HAVE_LIBGIF

//
//
// LIBGIF support
//
//

BEGIN_NCBI_SCOPE

//
// ReadImage()
// read an entire GIF image into memory.  This will read only the first image
// in an image set.
//
CImage* CImageIOGif::ReadImage(const string& file)
{
    // open our file for reading
    GifFileType* fp = DGifOpenFileName(file.c_str());
    if ( !fp ) {
        string msg("CImageIOGif::ReadImage(): cannot open file ");
        msg += file;
        msg += " for reading";
        NCBI_THROW(CImageException, eReadError, msg);
    }

    // allocate an image
    CRef<CImage> image(new CImage(fp->SWidth, fp->SHeight, 3));
    memset(image->SetData(), fp->SBackGroundColor,
           image->GetWidth() * image->GetHeight() * image->GetDepth());

    // we also allocate a single row
    // this row is a color indexed row, and will be decoded row-by-row into the
    // image
    AutoPtr<unsigned char, ArrayDeleter<unsigned char> > row_data
        (new unsigned char[image->GetWidth()]);
    unsigned char* row_ptr = row_data.get();

    bool done = false;
    while ( !done ) {
        // determine what sort of record type we have
        // these can be image, extension, or termination
        GifRecordType type;
        if (DGifGetRecordType(fp, &type) == GIF_ERROR) {
            DGifCloseFile(fp);
            string msg("CImageIOGif::ReadImage(): error reading file ");
            msg += file;
            NCBI_THROW(CImageException, eReadError, msg);
        }

        switch (type) {
        case IMAGE_DESC_RECORD_TYPE:
            //
            // we only support the first image in a gif
            //
            if (DGifGetImageDesc(fp) == GIF_ERROR) {
                DGifCloseFile(fp);
                string msg("CImageIOGif::ReadImage(): error reading file ");
                msg += file;
                NCBI_THROW(CImageException, eReadError, msg);
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
                     DGifCloseFile(fp);
                     string msg("CImageIOGif::ReadImage(): "
                                "error reading file ");
                     msg += file;
                     NCBI_THROW(CImageException, eReadError, msg);
                 }
                 while (extension != NULL) {
                     if (DGifGetExtensionNext(fp, &extension) == GIF_OK) {
                         continue;
                     }

                     DGifCloseFile(fp);
                     string msg("CImageIOGif::ReadImage(): "
                                "error reading file ");
                     msg += file;
                     NCBI_THROW(CImageException, eReadError, msg);
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
    return image.Release();
}


//
// ReadImage
// this version returns a sub-image from the desired image.
//
CImage* CImageIOGif::ReadImage(const string& file,
                               size_t x, size_t y, size_t w, size_t h)
{
    // we use a brain-dead implementation here - this can be done in a more
    // memory-efficient manner...
    CRef<CImage> image(ReadImage(file));
    return image->GetSubImage(x, y, w, h);
}


//
// WriteImage()
// this writes out a GIF image.
//
void CImageIOGif::WriteImage(const CImage& image, const string& file,
                             CImageIO::ECompress)
{
    if ( !image.GetData() ) {
        string msg("CImageIOGif::WriteImage(): "
                   "cannot write empty image to file ");
        msg += file;
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // first, we need to split our image into red/green/blue channels
    // we do this to get proper GIF quantization
    AutoPtr<unsigned char, ArrayDeleter<unsigned char> > red;
    AutoPtr<unsigned char, ArrayDeleter<unsigned char> > green;
    AutoPtr<unsigned char, ArrayDeleter<unsigned char> > blue;

    size_t size = image.GetWidth() * image.GetHeight();
    unsigned char* red_ptr   = NULL;
    unsigned char* green_ptr = NULL;
    unsigned char* blue_ptr  = NULL;
    const unsigned char* from_data = image.GetData();
    const unsigned char* end_data  = image.GetData() + size * image.GetDepth();

    switch (image.GetDepth()) {
    case 3:
        {{
             red.reset (new unsigned char[size]);
             green.reset(new unsigned char[size]);
             blue.reset (new unsigned char[size]);
             red_ptr   = red.get();
             green_ptr = green.get();
             blue_ptr  = blue.get();

             for ( ;  from_data != end_data;  ) {
                 *red_ptr++   = *from_data++;
                 *green_ptr++ = *from_data++;
                 *blue_ptr++  = *from_data++;
             }
         }}
        break;

    case 4:
        {{
             LOG_POST(Warning <<
                      "CImageIOGif::WriteImage(): "
                      "ignoring alpha channel in file " << file);
             red.reset (new unsigned char[size]);
             green.reset(new unsigned char[size]);
             blue.reset (new unsigned char[size]);
             red_ptr   = red.get();
             green_ptr = green.get();
             blue_ptr  = blue.get();

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

    // now, create a GIF color map object
    int cmap_size = 256;
    ColorMapObject* cmap = MakeMapObject(cmap_size, NULL);
    if ( !cmap ) {
        NCBI_THROW(CImageException, eWriteError,
                   "CImageIOGif::WriteImage(): failed to allocate color map");
    }

    // we also allocate a strip of data to hold the indexed colors
    AutoPtr<unsigned char, ArrayDeleter<unsigned char> > qdata
        (new unsigned char[size]);

    // quantize our colors
    if (QuantizeBuffer(image.GetWidth(), image.GetHeight(), &cmap_size,
                       red.get(), green.get(), blue.get(),
                       qdata.get(), cmap->Colors) == GIF_ERROR) {
        free(cmap);
        NCBI_THROW(CImageException, eWriteError,
                   "CImageIOGif::WriteImage(): failed to quantize image");
    }

    //
    // we are now ready to write our file
    //

    // open our file
    GifFileType* fp = EGifOpenFileName(const_cast<char*> (file.c_str()), false);
    if ( !fp ) {
        free(cmap);

        string msg("CImageIOGif::WriteImage(): failed to open file ");
        msg += file;
        msg += " for writing";
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // write the GIF screen description
    if (EGifPutScreenDesc(fp, image.GetWidth(), image.GetHeight(),
                          8, 0, cmap) == GIF_ERROR) {
        EGifCloseFile(fp);
        free(cmap);

        string msg("CImageIOGif::WriteImage(): failed to write GIF screen "
                   "description for file ");
        msg += file;
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // write the GIF image description
    if (EGifPutImageDesc(fp, 0, 0, image.GetWidth(), image.GetHeight(),
                         false, NULL) == GIF_ERROR) {
        EGifCloseFile(fp);
        free(cmap);

        string msg("CImageIOGif::WriteImage(): failed to write GIF image "
                   "description for file ");
        msg += file;
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    // put our data
    unsigned char* out_data = qdata.get();
    for (size_t i = 0;  i < image.GetHeight();  ++i) {
        if (EGifPutLine(fp, out_data, image.GetWidth()) == GIF_ERROR) {
            EGifCloseFile(fp);
            free(cmap);

            string msg("CImageIOGif::WriteImage(): error writing line ");
            msg += NStr::IntToString(i);
            msg += " to file ";
            msg += file;
            NCBI_THROW(CImageException, eWriteError, msg);
        }

        out_data += image.GetWidth();
    }

    // clean-up and close
    if (EGifCloseFile(fp) == GIF_ERROR) {
        free(cmap);

        string msg("CImageIOGif::WriteImage(): error closing file ");
        msg += file;
        NCBI_THROW(CImageException, eWriteError, msg);
    }

    free(cmap);
}


//
// WriteImage()
// this writes out a sub-image as a GIF file.  We use a brain-dead
// implementation currently - subset the image and write it out, rather than
// simply quantizing just the sub-image's data
//
void CImageIOGif::WriteImage(const CImage& image, const string& file,
                             size_t x, size_t y, size_t w, size_t h,
                             CImageIO::ECompress compress)
{
    CRef<CImage> subimage(image.GetSubImage(x, y, w, h));
    WriteImage(*subimage, file, compress);
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

CImage* CImageIOGif::ReadImage(const string&)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::ReadImage(): GIF format read unimplemented");
}


CImage* CImageIOGif::ReadImage(const string&,
                               size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::ReadImage(): GIF format partial "
               "read unimplemented");
}


void CImageIOGif::WriteImage(const CImage&, const string&,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::WriteImage(): GIF format write unimplemented");
}


void CImageIOGif::WriteImage(const CImage&, const string&,
                             size_t, size_t, size_t, size_t,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOGif::WriteImage(): GIF format partial "
               "write unimplemented");
}


END_NCBI_SCOPE

#endif  // !HAVE_LIBGIF

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.3  2003/07/01 12:08:44  dicuccio
 * Compilation fixes for MSVC
 *
 * Revision 1.2  2003/06/09 19:28:17  dicuccio
 * Fixed compilation error - conversion from const char* to char* required for
 * gif_lib
 *
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

