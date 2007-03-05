#ifndef UTIL_IMAGE__IMAGE_IO_PNG__HPP
#define UTIL_IMAGE__IMAGE_IO_PNG__HPP

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

#include "image_io_handler.hpp"
#include <string>

BEGIN_NCBI_SCOPE

class CImage;


//
// class CImageIOPng
// This is an internal class for handling PNG format files.  It relies on
// libpng as found by configure; if this is not found or is not configured, the
// relevant functions will be stubbed out
//

class CImageIOPng : public CImageIOHandler
{
public:

    // read PNG images from files
    CImage* ReadImage(CNcbiIstream& istr);
    CImage* ReadImage(CNcbiIstream& istr,
                      size_t x, size_t y, size_t w, size_t h);
    bool ReadImageInfo(CNcbiIstream& istr,
                       size_t* width, size_t* height, size_t* depth);

    // write images to file in PNG format
    void WriteImage(const CImage& image, CNcbiOstream& ostr,
                    CImageIO::ECompress compress);
    void WriteImage(const CImage& image, CNcbiOstream& ostr,
                    size_t x, size_t y, size_t w, size_t h,
                    CImageIO::ECompress compress);
};

END_NCBI_SCOPE

#endif  // UTIL_IMAGE__IMAGE_IO_PNG__HPP
