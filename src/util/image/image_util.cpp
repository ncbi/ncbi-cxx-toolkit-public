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
 *
 */

#include <ncbi_pch.hpp>
#include <util/image/image_util.hpp>
#include <util/image/image_exception.hpp>
#include <corelib/ncbi_limits.hpp>


BEGIN_NCBI_SCOPE


CImage* CImageUtil::Scale(const CImage& image, size_t width, size_t height,
                          EScale scale)
{
    CRef<CImage> new_image(new CImage(width, height, image.GetDepth()));

    float w_step = (float)(image.GetWidth())  / (float)(width);
    float h_step = (float)(image.GetHeight()) / (float)(height);
    _TRACE("scale: "
           << image.GetWidth() << "x" << image.GetHeight() << " -> "
           << width << "x" << height);

    float h_offs = 0;
    for (size_t i = 0;  i < height;  ++i) {
        int i_start = (int)h_offs;
        int i_end = (int)(h_offs + h_step);

        float w_offs = 0;
        for (size_t j = 0;  j < width;  ++j) {
            CImage::TPixel to = (*new_image)(i, j);

            int j_start = (int)w_offs;
            int j_end = (int)(w_offs + w_step);

            switch (scale) {
            case eScale_Average:
                {{
                    //
                    // average value of all pixels in the region
                    //
                    size_t count = 0;
                    size_t vals[4];
                    vals[0] = vals[1] = vals[2] = vals[3] = 0;
                    for (int from_i = i_start;  from_i < i_end;  ++from_i) {
                        for (int from_j = j_start;  from_j < j_end;  ++from_j) {
                            CImage::TConstPixel from_pixel =
                                image(from_i, from_j);
                            for (size_t k = 0;  k < image.GetDepth();  ++k) {
                                vals[k] += from_pixel[k];
                            }

                            ++count;
                        }
                    }
                    for (size_t k = 0;  k < new_image->GetDepth();  ++k) {
                        to[k] = (vals[k] / count) & 0xff;
                    }
                }}
                break;

            case eScale_Max:
                {{
                    // we look for the pixel with the maximal luminance
                    // to compute luminance, we borrow from PNG's
                    // integer-only computation:
                    //   Y = (6969 * R + 23434 * G + 2365 * B)/32768
                    size_t max_lum = 0;
                    for (int from_i = i_start;  from_i < i_end;  ++from_i) {
                        for (int from_j = j_start;  from_j < j_end;  ++from_j) {
                            CImage::TConstPixel from_pixel =
                                image(from_i, from_j);

                            size_t lum = ( 6969 * from_pixel[0] +
                                          23434 * from_pixel[1] +
                                           2365 * from_pixel[2])/32768;
                            if (lum > max_lum) {
                                max_lum = lum;
                                for (size_t k = 0;  k < image.GetDepth();  ++k) {
                                    to[k] = from_pixel[k];
                                }
                            }
                        }
                    }
                }}
                break;

            case eScale_Min:
                {{
                    // we look for the pixel with the maximal luminance
                    // to compute luminance, we borrow from PNG's
                    // integer-only computation:
                    //   Y = (6969 * R + 23434 * G + 2365 * B)/32768
                    size_t min_lum = kMax_Int;
                    for (int from_i = i_start;  from_i < i_end;  ++from_i) {
                        for (int from_j = j_start;  from_j < j_end;  ++from_j) {
                            CImage::TConstPixel from_pixel =
                                image(from_i, from_j);

                            size_t lum = ( 6969 * from_pixel[0] +
                                          23434 * from_pixel[1] +
                                           2365 * from_pixel[2])/32768;
                            if (lum < min_lum) {
                                min_lum = lum;
                                for (size_t k = 0;  k < image.GetDepth();  ++k) {
                                    to[k] = from_pixel[k];
                                }
                            }
                        }
                    }
                }}
                break;

            default:
                break;
            }

            w_offs += w_step;
        }

        h_offs += h_step;
    }

    return new_image.Release();
}


void CImageUtil::FlipX(CImage& image)
{
    const size_t scanline_size = image.GetWidth() * image.GetDepth();
    const size_t depth         = image.GetDepth();
    for (size_t i = 0;  i < image.GetHeight();  ++i) {
        unsigned char* start_ptr = image.SetData() + scanline_size * i;
        unsigned char* end_ptr   = image.SetData() + scanline_size * (i + 1);
        end_ptr -= depth;

        switch (depth) {
        case 3:
            for ( ; end_ptr > start_ptr;  end_ptr -= depth, start_ptr += depth) {
                std::swap(*(start_ptr    ), *(end_ptr    ));
                std::swap(*(start_ptr + 1), *(end_ptr + 1));
                std::swap(*(start_ptr + 2), *(end_ptr + 2));
            }
            break;

        case 4:
            for ( ; end_ptr > start_ptr;  end_ptr -= depth, start_ptr += depth) {
                std::swap(*(start_ptr    ), *(end_ptr    ));
                std::swap(*(start_ptr + 1), *(end_ptr + 1));
                std::swap(*(start_ptr + 2), *(end_ptr + 2));
                std::swap(*(start_ptr + 3), *(end_ptr + 3));
            }
            break;

        default:
            NCBI_THROW(CImageException, eInvalidDimension, "unhandled depth");
            break;
        }
    }
}


void CImageUtil::FlipY(CImage& image)
{
    size_t scanline_size = image.GetWidth() * image.GetDepth();
    size_t start = 0;
    size_t end   = image.GetHeight() - 1;

    for ( ; end > start;  --end, ++start) {
        unsigned char* start_ptr = image.SetData() + scanline_size * start;
        unsigned char* end_ptr   = image.SetData() + scanline_size * end;
        for (size_t i = 0;  i < scanline_size;  ++i) {
            std::swap(start_ptr[i], end_ptr[i]);
        }
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/08/11 15:23:33  vakatov
 * Compilation warning fix (unused local vars)
 *
 * Revision 1.4  2004/05/17 21:07:58  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/12/18 13:50:54  dicuccio
 * Added FlipX() and FlipY() routines to flip an image about an axis
 *
 * Revision 1.2  2003/12/16 16:16:56  dicuccio
 * Fixed compiler warnings
 *
 * Revision 1.1  2003/11/03 15:12:09  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
