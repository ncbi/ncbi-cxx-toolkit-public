#ifndef ALGO_TEXT___VECTOR_SCORE__HPP
#define ALGO_TEXT___VECTOR_SCORE__HPP

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

#include <corelib/ncbistd.hpp>
#include <math.h>

BEGIN_NCBI_SCOPE


///
/// Cosine similarity measure
///

template <class iterator1, class iterator2>
float Cosine(iterator1 iter1, iterator1 end1,
             iterator2 iter2, iterator2 end2)
{
    float cosine = 0;
    float len_a = 0;
    float len_b = 0;

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            cosine += float(iter1->second) * float(iter2->second);
            len_a += iter1->second * iter1->second;
            len_b += iter2->second * iter2->second;
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                len_a += iter1->second * iter1->second;
                ++iter1;
            } else {
                len_b += iter2->second * iter2->second;
                ++iter2;
            }
        }
    }

    for ( ;  iter1 != end1;  ++iter1) {
        len_a += iter1->second * iter1->second;
    }

    for ( ;  iter2 != end2;  ++iter2) {
        len_b += iter2->second * iter2->second;
    }

    cosine /= sqrt(len_a * len_b);

    return cosine;
}

///
/// Minkowski similarity measure
///

template <class iterator1, class iterator2>
float Minkowski(iterator1 iter1, iterator1 end1,
                iterator2 iter2, iterator2 end2,
                size_t power)
{
    float mink = 0;
    float len_a = 0;
    float len_b = 0;

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            mink += float(iter1->second) * float(iter2->second);
            len_a += pow(iter1->second, power);
            len_b += pow(iter2->second, power);
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                len_a += pow(iter1->second, power);
                ++iter1;
            } else {
                len_b += pow(iter2->second, power);
                ++iter2;
            }
        }
    }

    for ( ;  iter1 != end1;  ++iter1) {
        len_a += pow(iter1->second, power);
    }

    for ( ;  iter2 != end2;  ++iter2) {
        len_b += pow(iter2->second, power);
    }

    mink /= pow(len_a * len_b, 1.0f / float(power));
    return mink;
}


///
/// Dot-product similarity
///
template <class iterator1, class iterator2>
float Dot(iterator1 iter1, iterator1 end1,
          iterator2 iter2, iterator2 end2)
{
    float dot = 0;

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            dot += float(iter1->second) * float(iter2->second);
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                ++iter1;
            } else {
                ++iter2;
            }
        }
    }

    return dot;
}


///
/// Euclidean distance measure
///

template <class iterator1, class iterator2>
float Distance(iterator1 iter1, iterator1 end1,
               iterator2 iter2, iterator2 end2)
{
    float dist = 0;
    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            float diff = float(iter1->second) - iter2->second;
            dist += diff * diff;
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                dist += iter1->second * iter1->second;
                ++iter1;
            } else {
                dist += iter2->second * iter2->second;
                ++iter2;
            }
        }
    }

    for ( ;  iter1 != end1;  ++iter1) {
        dist += iter1->second * iter1->second;
    }

    for ( ;  iter2 != end2;  ++iter2) {
        dist += iter2->second * iter2->second;
    }

    return sqrt(dist);
}


///
/// Dot and distance in one step
///

template <class iterator1, class iterator2>
void DotAndDistance(iterator1 iter1, iterator1 end1,
                    iterator2 iter2, iterator2 end2,
                    float* dot_in, float* dist_in)
{
    float dot = 0;
    float dist = 0;
    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            float diff = iter1->second - iter2->second;
            dist += diff * diff;
            dot += iter1->second * iter2->second;

            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                dist += iter1->second * iter1->second;
                ++iter1;
            } else {
                dist += iter2->second * iter2->second;
                ++iter2;
            }
        }
    }

    for ( ;  iter1 != end1;  ++iter1) {
        dist += iter1->second * iter1->second;
    }

    for ( ;  iter2 != end2;  ++iter2) {
        dist += iter2->second * iter2->second;
    }

    if (dot_in) {
        *dot_in = dot;
    }

    if (dist_in) {
        *dist_in = sqrt(dist);
    }
}




///
/// Jaccard similarity
///

template <class iterator1, class iterator2>
float Jaccard(iterator1 iter1, iterator1 end1,
              iterator2 iter2, iterator2 end2)
{
    float dot = 0;
    float score_a = 0;
    float score_b = 0;

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            float v1 = float(iter1->second);
            float v2 = float(iter2->second);

            dot     += v1 * v2;
            score_a += v1 * v1;
            score_b += v2 * v2;

            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                score_a += iter1->second * iter1->second;
                ++iter1;
            } else {
                score_b += iter2->second * iter2->second;
                ++iter2;
            }
        }
    }

    for ( ;  iter1 != end1;  ++iter1) {
        score_a += iter1->second * iter1->second;
    }

    for ( ;  iter2 != end2;  ++iter2) {
        score_b += iter2->second * iter2->second;
    }

    return (dot / (score_a + score_b - dot));
}


///
/// Dice coefficient
///

template <class iterator1, class iterator2>
float Dice(iterator1 iter1, iterator1 end1,
           iterator2 iter2, iterator2 end2)
{
    float dot = 0;
    float score_a = 0;
    float score_b = 0;

    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            float v1 = float(iter1->second);
            float v2 = float(iter2->second);

            dot     += v1 * v2;
            score_a += iter1->second;
            score_b += iter2->second;

            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                score_a += iter1->second;
                ++iter1;
            } else {
                score_b += iter2->second;
                ++iter2;
            }
        }
    }

    for ( ;  iter1 != end1;  ++iter1) {
        score_a += iter1->second;
    }

    for ( ;  iter2 != end2;  ++iter2) {
        score_b += iter2->second;
    }

    return (dot / (score_a + score_b));
}


///
/// Overlap measure
///

template <class iterator1, class iterator2>
float Overlap(iterator1 iter1, iterator1 end1,
              iterator2 iter2, iterator2 end2)
{
    float dot = 0;
    float sum_a = 0;
    float sum_b = 0;
    for ( ;  iter1 != end1  &&  iter2 != end2; ) {
        if (iter1->first == iter2->first) {
            dot += float(iter1->second) * float(iter2->second);
            sum_a += iter1->second;
            sum_b += iter2->second;
            ++iter1;
            ++iter2;
        } else {
            if (iter1->first < iter2->first) {
                sum_a += iter1->second;
                ++iter1;
            } else {
                sum_b += iter2->second;
                ++iter2;
            }
        }
    }

    return dot / min(sum_a, sum_b);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/12/17 17:20:02  dicuccio
 * Removed unnecessary typedefs
 *
 * Revision 1.1  2006/12/17 14:12:19  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // ALGO_TEXT___VECTOR_SCORE__HPP
