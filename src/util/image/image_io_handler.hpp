#ifndef UTIL_IMAGE__IMAGE_IO_HANDLER__HPP
#define UTIL_IMAGE__IMAGE_IO_HANDLER__HPP

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
 *    class CImageIOHandler -- abstract interface definition for image readers
 *                             and writers
 */

#include <corelib/ncbiobj.hpp>
#include <util/image/image_io.hpp>
#include <string>

BEGIN_NCBI_SCOPE

class CImage;


///
/// class CImageIOHandler
/// This is the base class for all image I/O handlers, and defines the standard
/// interface required for a class that supports reading and writing an image
///

class CImageIOHandler : public CObject
{
public:

    virtual ~CImageIOHandler();

    /// Read an entire image from a stream, returning a pointer to the image.
    /// The callee is responsible for cleaning up the image
    virtual CImage* ReadImage(CNcbiIstream& istr) = 0;

    /// Read a portion of an image from a stream, returning a pointer to the
    /// image.  The callee is responsible for cleaning up the image
    virtual CImage* ReadImage(CNcbiIstream& istr,
                              size_t x, size_t y, size_t w, size_t h) = 0;

    /// Read a scanline of an image from a file.  This will come back as a
    /// single string of unsigned characters; the order is RGBRGBRGB...
    virtual void ReadScanLine(CNcbiIstream&          /* istr */,
                              vector<unsigned char>& /* data */) {}

    /// write images to file in HANDLER format
    virtual void WriteImage(const CImage& image,
                            CNcbiOstream& ostr,
                            CImageIO::ECompress compress) = 0;
    virtual void WriteImage(const CImage& image,
                            CNcbiOstream& ostr,
                            size_t x, size_t y, size_t w, size_t h,
                            CImageIO::ECompress compress) = 0;

};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/02/01 21:47:15  grichenk
 * Fixed warnings
 *
 * Revision 1.4  2005/01/13 15:32:31  dicuccio
 * Doxygenated comments
 *
 * Revision 1.3  2003/12/16 15:49:36  dicuccio
 * Large re-write of image handling.  Added improved error-handling and support
 * for streams-based i/o (via hooks into each client library).
 *
 * Revision 1.2  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

#endif  /// UTIL_IMAGE__IMAGE_IO_HANDLER__HPP
