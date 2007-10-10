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

#include <ncbi_pch.hpp>
#include <util/image/image_io.hpp>
#include <util/image/image_exception.hpp>
#include <corelib/stream_utils.hpp>
#include <util/error_codes.hpp>

#include "image_io_raw.hpp"
#include "image_io_bmp.hpp"
#include "image_io_gif.hpp"
#include "image_io_jpeg.hpp"
#include "image_io_png.hpp"
#include "image_io_sgi.hpp"
#include "image_io_tiff.hpp"


#define NCBI_USE_ERRCODE_X   Util_Image


BEGIN_NCBI_SCOPE

//
// file magic signatures for various formats
//

// we read at most 16 bytes
static const int kMaxMagic = 16;

struct SMagicInfo
{
    CImageIO::EType m_Type;
    unsigned int    m_Length;
    unsigned char   m_Signature[kMaxMagic];
};
static const struct SMagicInfo kMagicTable[] = {
    { CImageIO::eBmp,     2, "BM" },
    { CImageIO::eGif,     4, "GIF8" },
    { CImageIO::eJpeg,    2, "\xff\xd8" },
    { CImageIO::ePng,     4, "\x89PNG" },
    { CImageIO::eSgi,     2, "\x1\xda" },
    { CImageIO::eTiff,    4, "II*\0" },
    { CImageIO::eTiff,    4, "MM\0*" }, // this one might need to be { 'M', 'M', 0, '*' }
    { CImageIO::eXpm,     7, "/* XPM */" },
    { CImageIO::eRaw,     4, "RAW\0" },

    // must be last
    { CImageIO::eUnknown, 0, "" }
};

// extract an image's type from its magic number
CImageIO::EType CImageIO::GetTypeFromMagic(const string& file)
{
    CNcbiIfstream i_file (file.c_str());
    if (!i_file) {
        return eUnknown;
    }

    return GetTypeFromMagic(i_file);
}

CImageIO::EType CImageIO::GetTypeFromMagic(CNcbiIstream& istr)
{
    // magic numbers live in the first few bytes
    unsigned char magic[kMaxMagic];
    memset(magic, 0x00, kMaxMagic);

    istr.read((char *)magic, kMaxMagic);
    istr.seekg(-istr.gcount(), ios::cur);
    //CStreamUtils::Pushback(istr, (CT_CHAR_TYPE*)magic, istr.gcount());

    EType type = eUnknown;

    // we just compare against our (small) table of known image magic values
    for (const SMagicInfo* i = kMagicTable;  i->m_Length;  ++i) {
        if (memcmp(magic, i->m_Signature, i->m_Length) == 0) {
            type = i->m_Type;
            break;
        }
    }

    return type;
}


//
// magic from extension info
//
struct SExtMagicInfo
{
    CImageIO::EType m_Type;
    const char* m_Ext;
};

static const SExtMagicInfo kExtensionMagicTable[] = {
    { CImageIO::eBmp,  "bmp" },
    { CImageIO::eGif,  "gif" },
    { CImageIO::eJpeg, "jpeg" },
    { CImageIO::eJpeg, "jpg" },
    { CImageIO::ePng,  "png" },
    { CImageIO::eSgi,  "sgi" },
    { CImageIO::eTiff, "tif" },
    { CImageIO::eTiff, "tiff" },
    { CImageIO::eXpm,  "xpm" },
    { CImageIO::eRaw,  "raw" },

    // must be last
    { CImageIO::eUnknown, NULL }
};

//
// GetTypeFromFileName()
// This function will determine a file's expected type from a given file name
//
CImageIO::EType CImageIO::GetTypeFromFileName(const string& fname)
{
    string::size_type pos = fname.find_last_of(".");
    if (pos == string::npos) {
        return eUnknown;
    }

    string ext(fname.substr(pos + 1, fname.length() - pos - 1));
    ext = NStr::ToLower(ext);

    // compare the extension against our table
    for (const SExtMagicInfo* i = kExtensionMagicTable;  i->m_Ext;  ++i) {
        if (ext == i->m_Ext) {
            return i->m_Type;
        }
    }
    return eUnknown;
}

/// read just the image information 
bool CImageIO::ReadImageInfo(const string& file,
                             size_t* width, size_t* height, size_t* depth,
                             EType* type)
{
    CNcbiIfstream istr(file.c_str(), ios::in|ios::binary);
    return ReadImageInfo(istr, width, height, depth, type);
}


bool CImageIO::ReadImageInfo(CNcbiIstream& istr,
                             size_t* width, size_t* height, size_t* depth,
                             EType* type)
{
    try {
        /// first, set up a simple buffer - we hold the first 16k
        /// or so of the image.  this will allow us to manage things like TIFF
        /// image directories

        EType image_type = GetTypeFromMagic(istr);
        if (type) {
            *type = image_type;
        }
        CRef<CImageIOHandler> handler(x_GetHandler(image_type));

        size_t pos = istr.tellg();
        bool res = handler->ReadImageInfo(istr, width, height, depth);
        istr.seekg(pos, ios::beg);
        return res;
    }
    catch (CImageException& e) {
        LOG_POST_X(3, Error << "Error reading image: " << e.what());
    }
    return false;
}



//
// ReadImage()
// read an image from disk, returning its contents
//
CImage* CImageIO::ReadImage(const string& file, EType type)
{
    CNcbiIfstream istr(file.c_str(), ios::in|ios::binary);
    return ReadImage(istr, type);
}


CImage* CImageIO::ReadImage(CNcbiIstream& istr, EType type)
{
    try {
        if (type == eUnknown) {
            type = GetTypeFromMagic(istr);
        }
        CRef<CImageIOHandler> handler(x_GetHandler(type));
        return handler->ReadImage(istr);
    }
    catch (CImageException& e) {
        LOG_POST_X(4, Error << "Error reading image: " << e.what());
        return NULL;
    }
    // all other exceptions are explicitly not caught - handler in user code
}


//
// ReadSubImage()
// read part of an image from disk, returning its contents
//
CImage* CImageIO::ReadSubImage(const string& file,
                               size_t x, size_t y, size_t w, size_t h,
                               EType type)
{
    CNcbiIfstream istr(file.c_str(), ios::in|ios::binary);
    return ReadSubImage(istr, x, y, w, h, type);
}


CImage* CImageIO::ReadSubImage(CNcbiIstream& istr,
                               size_t x, size_t y, size_t w, size_t h,
                               EType type)
{
    try {
        if (type == eUnknown) {
            type = GetTypeFromMagic(istr);
        }
        CRef<CImageIOHandler> handler(x_GetHandler(type));
        return handler->ReadImage(istr, x, y, w, h);
    }
    catch (CImageException& e) {
        LOG_POST_X(5, Error << "Error reading subimage: " << e.what());
        return NULL;
    }
    // all other exceptions are explicitly not caught - handler in user code
}


//
// WriteImage()
// write an image to disk
//
bool CImageIO::WriteImage(const CImage& image, const string& file,
                          EType type, ECompress compress)
{
    try {
        if (type == eUnknown) {
            type = GetTypeFromFileName(file);
        }
    }
    catch (CImageException& e) {
        LOG_POST_X(6, Error << "Error writing image: " << e.what());
        return false;
    }

    CNcbiOfstream ostr(file.c_str(), ios::out|ios::binary);
    return WriteImage(image, ostr, type, compress);
}


bool CImageIO::WriteImage(const CImage& image, CNcbiOstream& ostr,
                          EType type, ECompress compress)
{
    try {
        CRef<CImageIOHandler> handler(x_GetHandler(type));
        handler->WriteImage(image, ostr, compress);
        return true;
    }
    catch (CImageException& e) {
        LOG_POST_X(7, Error << "Error writing image: " << e.what());
        return false;
    }
    // all other exceptions are explicitly not caught - handler in user code
}


//
// WriteSubImage()
// write part of an image to disk
//
bool CImageIO::WriteSubImage(const CImage& image,
                             const string& file,
                             size_t x, size_t y, size_t w, size_t h,
                             EType type, ECompress compress)
{
    try {
        if (type == eUnknown) {
            type = GetTypeFromFileName(file);
        }
    }
    catch (CImageException& e) {
        LOG_POST_X(8, Error << "Error writing image: " << e.what());
        return false;
    }

    CNcbiOfstream ostr(file.c_str(), ios::out|ios::binary);
    return WriteSubImage(image, ostr, x, y, w, h, type, compress);
}


bool CImageIO::WriteSubImage(const CImage& image,
                             CNcbiOstream& ostr,
                             size_t x, size_t y, size_t w, size_t h,
                             EType type, ECompress compress)
{
    try {
        CRef<CImageIOHandler> handler(x_GetHandler(type));
        handler->WriteImage(image, ostr, x, y, w, h, compress);
        return true;
    }
    catch (CImageException& e) {
        LOG_POST_X(9, Error << "Error writing image: " << e.what());
        return false;
    }
    // all other exceptions are explicitly not caught - handler in user code
}


//
// x_GetImageHandler()
// Retrieve a CImageIOHandler-derived class to handle reading and writing for a
// given image formag
//
CImageIOHandler* CImageIO::x_GetHandler(EType type)
{
    switch (type) {
    default:
    case eUnknown:
        NCBI_THROW(CImageException, eInvalidImage,
                   "Image format not supported");

    case eJpeg:
        return new CImageIOJpeg();

    case eBmp:
        return new CImageIOBmp();

    case ePng:
        return new CImageIOPng();

    case eSgi:
        return new CImageIOSgi();

    case eGif:
        return new CImageIOGif();

    case eRaw:
        return new CImageIORaw();

    case eTiff:
        return new CImageIOTiff();
    }
}


END_NCBI_SCOPE
