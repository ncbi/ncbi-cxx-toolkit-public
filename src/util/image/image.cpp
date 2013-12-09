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

#include <ncbi_pch.hpp>
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>
#include <util/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Util_Image


BEGIN_NCBI_SCOPE


CImage::CImage()
    : m_Width(0),
      m_Height(0),
      m_Depth(3)
{
    return;
}


CImage::CImage(size_t width, size_t height, size_t depth)
    : m_Width(0),
      m_Height(0),
      m_Depth(depth)
{
    Init(width, height, depth);
}


void CImage::Init(size_t width, size_t height, size_t depth)
{
    _TRACE("CImage::Init(): " << width << ", " << height << ", " << depth);
    switch (depth) {
    case 3:
    case 4:
        m_Data.resize(width * height * depth);
        break;

    default:
        {{
             string msg("CImage::Init(): depth = ");
             msg += NStr::NumericToString(depth);
             msg += " not implemented";
             NCBI_THROW(CImageException, eInvalidDimension, msg);
        }}
    }
    m_Width  = width;
    m_Height = height;
    m_Depth  = depth;
}


const unsigned char* CImage::GetData(void) const
{
    _ASSERT(m_Data.size());
    return &(m_Data[0]);
}


unsigned char* CImage::SetData(void)
{
    _ASSERT(m_Data.size());
    return &(m_Data[0]);
}


//
// GetAspectRatio()
// This computes the aspect ratio for the image (width / height)
//
float CImage::GetAspectRatio(void) const
{
    return float (m_Width) / float (m_Height);
}


//
// set the alpha channel to a given value.  The red channel is channel 3.
// This will optionnally add the channel if it doesn't exist.
//
void CImage::SetAlpha(unsigned char val, bool add_if_unavailable)
{
    if (GetDepth() == 3) {
        if (add_if_unavailable) {
            SetDepth(4);
        } else {
            NCBI_THROW(CImageException, eInvalidDimension,
                       "CImage::SetAlpha(): attempt to set alpha in "
                       "24-bit image");
        }
    }
    SetChannel(eAlpha, val);
}


//
// SetDepth()
// This will force the image to be 24-bit or 32-bit
//
void CImage::SetDepth(size_t depth)
{
    if ( !m_Data.size() ) {
        return;
    }

    switch (depth) {
    case 3:
        if (m_Depth == 4) {
            // squash our data.  we can do this without explicitly reallocating
            // our buffer - we just have junk on the end
            TImageStrip::const_iterator from_data = m_Data.begin();
            TImageStrip::iterator       to_data   = m_Data.begin();
            for ( ;  from_data != m_Data.end();  ) {
                *to_data++ = *from_data++;
                *to_data++ = *from_data++;
                *to_data++ = *from_data++;

                // skip the alpha channel
                ++from_data;
            }

            m_Data.resize(m_Width * m_Height * 3);
            m_Depth = 3;
        }
        break;

    case 4:
        if (m_Depth == 3) {
            // expand our data.
            // we do this by expanding in-place
            m_Data.resize(m_Width * m_Height * 4);
            m_Depth = 4;

            // now we need to expand our data
            // we can do this by marching backwares through the data
            TImageStrip::const_reverse_iterator from_data(m_Data.end());
            TImageStrip::const_reverse_iterator end_data (m_Data.begin());
            TImageStrip::reverse_iterator       to_data  (m_Data.end());

            // march to the actual end of the data
            from_data += m_Width * m_Height;

            for ( ;  from_data != end_data;  ) {
                // insert an alpha channel - this is the first because we're
                // marching backwards
                *to_data++ = 255;

                // copy (in reverse order) BGR data
                *to_data++ = *from_data++;
                *to_data++ = *from_data++;
                *to_data++ = *from_data++;
            }
        }
        break;

    default:
        {{
            string msg("CImage::SetDepth(): invalid depth: ");
            msg += NStr::NumericToString(depth);
            NCBI_THROW(CImageException, eInvalidDimension, msg);
        }}
    }
}


//
// SetChannel()
// Sets a given channel to a certain value
//
void CImage::SetChannel(size_t channel, unsigned char val)
{
    if ( !m_Data.size() ) {
        return;
    }

    if (channel > 3) {
        NCBI_THROW(CImageException, eInvalidDimension,
                   "CImage::SetChannel(): channel out of bounds");
    }

    TImageStrip::iterator data = m_Data.begin() + channel;
    TImageStrip::iterator end  =
        m_Data.begin() + m_Width * m_Height * m_Depth + channel;
    for ( ;  data != end;  data += m_Depth) {
        *data = val;
    }
}


//
// GetSubImage()
// This function returns a new image that is a subimage of the current image
// the width and height will be clamped to the left / bottom margins; the x and
// y positions must be valid.
//
CImage* CImage::GetSubImage(size_t x, size_t y, size_t w, size_t h) const
{
    if ( !m_Data.size() ) {
        NCBI_THROW(CImageException, eInvalidImage,
                   "CImage::GetSubImage(): image is empty");
    }

    if (x >= m_Width  ||  y >= m_Height) {
        string msg("CImage::GetSubImage(): invalid starting pos: ");
        msg += NStr::NumericToString(x);
        msg += ", ";
        msg += NStr::NumericToString(y);
        NCBI_THROW(CImageException, eInvalidImage, msg);
    }

    if (x + w > m_Width) {
        w = m_Width - x;
        LOG_POST_X(1, Warning << "CImage::GetSubImage(): clamping width to " << w);
    }

    if (y + h > m_Height) {
        h = m_Height - y;
        LOG_POST_X(2, Warning << "CImage::GetSubImage(): clamping height to " << h);
    }

    CRef<CImage> image(new CImage(w, h, m_Depth));
    const unsigned char* from_data = GetData() +
                                     y * m_Width * m_Depth + x * m_Depth;
    unsigned char*       to_data   = image->SetData();

    size_t from_stride = m_Width * m_Depth;
    size_t to_stride   = w * m_Depth;

    for (size_t i = 0;  i < h;  ++i) {
        memcpy(to_data, from_data, w * m_Depth);
        to_data   += to_stride;
        from_data += from_stride;
    }

    return image.Release();
}


void CImage::Flip(void)
{
    if ( !m_Data.size() ) {
        return;
    }

    unsigned char* from = SetData();
    unsigned char* to   = from + (m_Height - 1) * m_Width * m_Depth;
    size_t stride = m_Width * m_Depth;

    while (to > from) {
        for (size_t pos = 0;  pos < stride;  ++pos) {
            std::swap(from[pos], to[pos]);
        }
        from += stride;
        to   -= stride;
    }
}


END_NCBI_SCOPE
