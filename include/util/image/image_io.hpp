#ifndef GUI_IMAGE___IMAGE_READER__HPP
#define GUI_IMAGE___IMAGE_READER__HPP

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
 *    CImageIO -- framework for reading/writing images
 */


#include <corelib/ncbistd.hpp>
#include <util/image/image.hpp>


BEGIN_NCBI_SCOPE

class CImageIOHandler;


//
// class CImageIO defines a static interface for reading and writing images
// from files.
//
// This class is a front end to a variety of format-specific readers and
// writers, and supported importing and exporing images in JPEG, PNG, GIF, BMP,
// SGI, and TIFF formats.  In addition, this class supports importing and
// exporting sub-regions of images.
//

class NCBI_XIMAGE_EXPORT CImageIO
{
public:

    // enumerated list of recognized image types
    enum EType {
        eUnknown,
        eBmp,
        eGif,
        eJpeg,
        ePng,
        eSgi,
        eTiff,
        eXpm,
        eRaw
    };

    // enumerated list of compression grades
    enum ECompress {
        eCompress_None,
        eCompress_Low,
        eCompress_Medium,
        eCompress_High,

        eCompress_Min     = eCompress_None,
        eCompress_Max     = eCompress_High,
        eCompress_Default = eCompress_Medium
    };

    // retrieve an image type from its magic number
    static EType GetTypeFromMagic(CNcbiIstream& istr);
    static EType GetTypeFromMagic(const string& file);

    // retrieve an image type from its file name
    // this uses the provided extension as a hint to guess the expected file
    // type
    static EType GetTypeFromFileName(const string& file);

    // read an image from a file, returning the object for user management
    static CImage* ReadImage(const string& file,
                             EType type = CImageIO::eUnknown);
    static CImage* ReadImage(CNcbiIstream& istr,
                             EType type = CImageIO::eUnknown);

    static bool ReadImageInfo(const string& file,
                              size_t* width, size_t* height, size_t* depth,
                              EType* type);
    static bool ReadImageInfo(CNcbiIstream& istr,
                              size_t* width, size_t* height, size_t* depth,
                              EType* type);

    // read only part of an image from a file
    static CImage* ReadSubImage(CNcbiIstream& istr,
                                size_t x, size_t y, size_t w, size_t h,
                                EType type = CImageIO::eUnknown);
    static CImage* ReadSubImage(const string& file,
                                size_t x, size_t y, size_t w, size_t h,
                                EType type = CImageIO::eUnknown);

    // write an image to a file in a specified format.  If the format type is
    // eUnknown, it will be guessed from the file extension.
    static bool WriteImage(const CImage& image, CNcbiOstream& ostr,
                           EType type,
                           ECompress compress = eCompress_Default);
    static bool WriteImage(const CImage& image, const string& file,
                           EType type = eUnknown,
                           ECompress compress = eCompress_Default);

    // write only part of an image to a file
    static bool WriteSubImage(const CImage& image, CNcbiOstream& ostr,
                              size_t x, size_t y, size_t w, size_t h,
                              EType type,
                              ECompress compress = eCompress_Default);
    static bool WriteSubImage(const CImage& image, const string& file,
                              size_t x, size_t y, size_t w, size_t h,
                              EType type = eUnknown,
                              ECompress compress = eCompress_Default);

private:

    // retrieve an image I/O handler for a given type
    static CImageIOHandler* x_GetHandler(EType type);
};


END_NCBI_SCOPE

#endif  // GUI_IMAGE___IMAGE_READER__HPP
