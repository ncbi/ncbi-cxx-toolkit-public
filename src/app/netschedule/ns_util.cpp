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
 * Authors:  Victor Joukov
 *
 * File Description: Utility functions for NetSchedule
 *
 */

#include <ncbi_pch.hpp>

#include <util/bitset/bmalgo.h>

#include "ns_util.hpp"


BEGIN_NCBI_SCOPE

string NS_EncodeBitVector(TNSBitVector& bv)
{
    string str;
    TNSBitVector::enumerator en(bv.first());
    unsigned range_cur = 0;
    int range_cnt = 0;
    unsigned id;
    for (; en.valid(); ++en) {
        id = *en;
        if (range_cnt) {
            if (range_cur + 1 == id) {
                // Range goes on
                ++range_cur;
                ++range_cnt;
            } else {
                // Break the range
                if (range_cnt > 2) {
                    // True range
                    str += '-';
                    str += NStr::UIntToString(range_cur);
                }
                str += ',';
                str += NStr::UIntToString(id);
                range_cur = id;
                range_cnt = 1;
            }
        } else {
            str += NStr::UIntToString(id);
            range_cur = id;
            range_cnt = 1;
        }
    }
    if (range_cnt > 1) {
        if (range_cnt > 2) {
            // True range
            str += '-';
        } else {
            str += ',';
        }
        str += NStr::UIntToString(id);
    }
    return str;
}


TNSBitVector NS_DecodeBitVector(const string& s)
{
    TNSBitVector bv;
    list<string> ids;
    ITERATE(list<string>, it, NStr::Split(s, ",", ids)) {
        string str1, str2;
        if (NStr::SplitInTwo(*it, "-", str1, str2)) {
            unsigned id, last;
            id = NStr::StringToUInt(str1);
            last = NStr::StringToUInt(str2);
            for (; id <= last; ++id) {
                bv.set(id);
            }
        } else {
            unsigned id = NStr::StringToUInt(*it);
            bv.set(id);
        }
    }
    return bv;
}


END_NCBI_SCOPE
