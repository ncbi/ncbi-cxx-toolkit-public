/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * File Description: Numeric conversions
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <string>
#include <stdio.h>


BEGIN_NCBI_SCOPE


#define MAXPRECISION 		50


static int s_NumericBytesPerPrec[] =
{2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8, 9, 9, 9,
 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15,
 16, 16, 16, 17, 17, 18, 18, 19, 19, 19, 20, 20, 21, 21, 21,
 22, 22, 23, 23, 24, 24, 24, 25, 25, 26, 26, 26};


NCBI_DBAPIDRIVER_EXPORT
unsigned char*  longlong_to_numeric (Int8 l_num, unsigned int prec, unsigned char* cs_num)
{
    bool needs_del= false;

    if(prec == 0) return 0;

    if (cs_num == 0) {
        cs_num= new unsigned char[MAXPRECISION];
        needs_del= true;
    }
    memset (cs_num, 0, prec);

    int BYTE_NUM = s_NumericBytesPerPrec[prec-1];
    unsigned char* number = &cs_num[BYTE_NUM - 1];
    if (l_num != 0) {
        if (l_num < 0) {
            l_num *= (-1);
            cs_num[0] = 0x1;
        }
        while (l_num != 0 && number >= cs_num) {
            Int8 rem = l_num%256;
            *number = (unsigned char)rem;
            l_num = l_num/256;
            number--;
            if (number <= cs_num) {
                if (needs_del) delete[] cs_num;
                return 0;
            }
        }
    }
    return cs_num;

}


NCBI_DBAPIDRIVER_EXPORT
Int8 numeric_to_longlong(unsigned int precision, unsigned char* cs_num)

{

    if(precision == 0) return 0;

    int BYTE_NUM = s_NumericBytesPerPrec[precision - 1];
    Int8 my_long = 0;

    for (int i = 1; i < BYTE_NUM; i++) {
        if (my_long > kMax_I8 >> 8) {
            return 0;
        }
        my_long = my_long*256 + cs_num[i];
    }
    if (cs_num[0] != 0) {
        my_long*= -1;
    }

    return my_long;
}


NCBI_DBAPIDRIVER_EXPORT
void swap_numeric_endian(unsigned int precision, unsigned char* num)
{
    if(precision == 0) return;

    int BYTE_NUM= s_NumericBytesPerPrec[precision - 1] - 1;
    unsigned char c;
    int i, j;

    for(i= 0, j= BYTE_NUM-1; i < j; i++, j--) {
        c= num[i];
        num[i]= num[j];
        num[j]= c;
    }
}


END_NCBI_SCOPE

