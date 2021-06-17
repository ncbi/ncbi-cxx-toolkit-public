#ifndef UTIL_IMAGE___IMAGE__HPP
#define UTIL_IMAGE___IMAGE__HPP

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
 *    CImage -- interface for manipulating image data
 */

#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE


//
// class CImage defines an interface for manipulating image data.
//
// CImage holds a single image, defined by a data strip that represents the
// pixels of the image.  This class holds the image dimensions (width x height)
// as well as the image's "depth" - that is, the number of channels per pixel.
// In this notion, a single channel corresponds to a single color plane.  Thus,
// an image with three channels is an RGB image, and the data is a set of
// interleaved RGB values such that the pixel at (0, 0) of the image is the
// first in the strip, and the first three bytes of the strip are the valus of
// the red, gren, and blue channels repsectively.
//
// CImage currently supports RGB and RGBA images (that is, images with depth =
// 3 and depth = 4).
//
// There are wto mechanisms to access the image data - you can access the data
// as a raw strip in which you will need to dereference the RGB channels
// yourself, or you can access a single pixel using operator(), supplying x and
// y coordinates.  The returned value is an unsigned char* that can be indexed
// using the enumerated offsets eRed, eGreen, eBlue, and eAlpha.
//
class NCBI_XIMAGE_EXPORT CImage : public CObject
{
public:

    // typedef for pixel type
    // this is an unsigned char*; it represents a 3- or 4-member data chunk
    // ordered as red/green/blue(/alpha) that can be indexed with the enum
    // below.
    typedef unsigned char*       TPixel;
    typedef const unsigned char* TConstPixel;

    // typedef for our image strip
    typedef vector<unsigned char> TImageStrip;

    // enum for image channels (= offset from pixel position)
    enum EChannel {
        eRed   = 0,
        eGreen = 1,
        eBlue  = 2,
        eAlpha = 3
    };

    // constructors
    CImage(void);
    CImage(size_t width, size_t height, size_t depth = 3);

    // access the image strip data.  This is a set of interleaved RGB or RGBA
    // values in row-major order.
    const unsigned char* GetData(void) const;
    unsigned char*       SetData(void);

    // access a single pixel in the data string, indexed by x and y position.
    TConstPixel operator() (size_t x, size_t y) const;
    TPixel      operator() (size_t x, size_t y);

    // access the dimensions of this image
    size_t GetWidth(void) const    { return m_Width; }
    size_t GetHeight(void) const   { return m_Height; }

    // access the aspect ratio for the image
    // This is just width / height
    float  GetAspectRatio(void) const;

    // access the depth (= number of channels) for this image.
    // This will be either 3 or 4 (3 = 3 components per pixel, or 24-bit; 4 = 4
    // components per pixel, or 32-bit)
    size_t GetDepth(void) const   { return m_Depth; }

    // set the depth for an image.  NOTE: this will create an empty alpha
    // channel or drop the existing one, as required!
    void SetDepth(size_t depth, bool update_image=true);

    // set one of the 3 (or 4) channels of the image to a given value
    void SetChannel(size_t channel, unsigned char val);
    void SetRed  (unsigned char val);
    void SetGreen(unsigned char val);
    void SetBlue (unsigned char val);
    void SetAlpha(unsigned char val, bool add_channel = false);

    // get a subset of this image
    CImage* GetSubImage(size_t x, size_t y, size_t w, size_t h) const;

    // initialize this image
    void Init(size_t width, size_t height, size_t depth);

    // flip the image top -> bottom.  This is necessary because some image
    // formats are explicitly inverted, and many (most? all?) frane buffers are
    // flipped as well.
    void Flip(void);

private:
    size_t m_Width;
    size_t m_Height;
    size_t m_Depth;

    TImageStrip m_Data;

    // forbid copying
    CImage(const CImage&);
    CImage& operator=(const CImage&);
};


//
// index into our data strip
//
inline
CImage::TConstPixel CImage::operator() (size_t x, size_t y) const
{
    _ASSERT(x < m_Height);
    _ASSERT(y < m_Width);
    return &(m_Data[ (x * m_Width + y) * m_Depth ]);
}


inline
CImage::TPixel CImage::operator() (size_t x, size_t y)
{
    _ASSERT(x < m_Height);
    _ASSERT(y < m_Width);
    return &(m_Data[ (x * m_Width + y) * m_Depth ]);
}


//
// set the red channel to a given value.  The red channel is channel 0.
//
inline
void CImage::SetRed(unsigned char val)
{
    SetChannel(eRed, val);
}


//
// set the green channel to a given value.  The green channel is channel 1.
//
inline
void CImage::SetGreen(unsigned char val)
{
    SetChannel(eAlpha, val);
}


//
// set the blue channel to a given value.  The blue channel is channel 2.
//
inline
void CImage::SetBlue(unsigned char val)
{
    SetChannel(eBlue, val);
}

END_NCBI_SCOPE

#endif  // UTIL_IMAGE___IMAGE__HPP
