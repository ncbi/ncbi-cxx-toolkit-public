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
* File Description:
*
*
*/


#include <corelib/ncbistd.hpp>
#include <ctpublic.h>
#include <string>
#include <stdio.h>


USING_NCBI_SCOPE;


#define MAXPRECISION 		50


static int s_NumericBytesPerPrec[] =
{2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8, 9, 9, 9,
 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15,
 16, 16, 16, 17, 17, 18, 18, 19, 19, 19, 20, 20, 21, 21, 21,
 22, 22, 23, 23, 24, 24, 24, 25, 25, 26, 26, 26};

static int mask_for_bit[8] = { 0x1, 0x2,0x4, 0x8, 0x10, 0x20,0x40, 0x80 };

static int multiply_byte (unsigned char *product, int num,
                          unsigned char *multiplier);
static int do_carry (unsigned char *product);
static char *array_to_string (unsigned char *array, int scale, char *s);

char* numeric_to_string (CS_NUMERIC *numeric, char *s)
{
    unsigned char multiplier[MAXPRECISION], temp[MAXPRECISION];
    unsigned char product[MAXPRECISION];
    unsigned char *number;
    int num_bytes;
    int pos;

    memset(multiplier,0,MAXPRECISION);
    memset(product,0,MAXPRECISION);
    multiplier[0]=1;
    number = numeric->array;
    num_bytes = s_NumericBytesPerPrec[numeric->precision-1];

    if (numeric->array[0] == 1)
        *s++ = '-';

    for (pos=num_bytes-1;pos>0;pos--) {
        multiply_byte(product, number[pos], multiplier);

        memcpy(temp, multiplier, MAXPRECISION);
        memset(multiplier,0,MAXPRECISION);
        multiply_byte(multiplier, 256, temp);
    }
    array_to_string(product, numeric->scale, s);
    return s;
}

static int multiply_byte (unsigned char *product, int num,
                          unsigned char *multiplier)
{
    unsigned char number[3];
    int i, top, j, start;

    number[0]=num%10;
    number[1]=(num/10)%10;
    number[2]=(num/100)%10;

    for (top=MAXPRECISION-1;top>=0 && !multiplier[top];top--);
    start=0;
    for (i=0;i<=top;i++) {
        for (j=0;j<3;j++) {
            product[j+start]+=multiplier[i]*number[j];
        }
        do_carry(product);
        start++;
    }
    return 0;
}

static int do_carry (unsigned char *product)
{
    int j;

    for (j=0;j<MAXPRECISION;j++) {
        if (product[j]>9) {
            product[j+1]+=product[j]/10;
            product[j]=product[j]%10;
        }
    }
    return 0;
}

static char *array_to_string (unsigned char *array,
                              int scale, char *s)
{
    int top, i, j;
	
    for (top=MAXPRECISION-1;top>=0 && top>scale && !array[top];top--);

    if (top == -1)
        {
            s[0] = '0';
            s[1] = '\0';
            return s;
        }

    j=0;
    for (i=top;i>=0;i--) {
        if (top+1-j == scale) s[j++]='.';
        s[j++]=array[i]+'0';
    }
    s[j]='\0';
    return s;
}

CS_NUMERIC*  string_to_numeric (string str_buff, int prec,
                                int scale, CS_NUMERIC* cs_num)
{
    string num_str;
    string sum_str;
    string str_4_rem;
    string sub_str;
    string str;
    char s[1024];
    int rem = -1;
    int result = 0;

    int BYTE_NUM = s_NumericBytesPerPrec[prec-1];

    if (cs_num == NULL) {
        cs_num = (CS_NUMERIC*) malloc (sizeof(CS_NUMERIC));
    }
    memset (cs_num, 0, sizeof(CS_NUMERIC));

    unsigned char* number= &cs_num->array[BYTE_NUM - 1];


    str = str_buff;
    if ((int) str.size() > prec) {
        return NULL;
    }
    string::size_type pos = 0;
    string::size_type p_pos = 0;

    if ((p_pos = str.find ('-')) != string::npos) {
        if (str_buff != "-0") {
            cs_num->array[0] = cs_num->array[0]|mask_for_bit[0];
        }
        str.erase (p_pos-1, 1);

    }
    if ((p_pos = str.find ('0')) == 0 && str.size() > 1) {
        str.erase (p_pos, 1);
    }

    //add 0 to the end of string

    if ( (p_pos = str.find ('.')) != string::npos ) {
        sub_str = str.substr(p_pos+1,  string::npos - (p_pos+1));
        if ((int) sub_str.size() < scale) {
            size_t res = scale - sub_str.size();
            str.append (res, '0');
        } else if ((int) sub_str.size() > scale) {
            ;
        }
        str.erase (p_pos, 1);
    } else {
        str.append (scale, '0');
    }

    if (str_buff != "-0" && str_buff != "0") {
        while (str.size() >  9  ) {
            string::iterator e = str.end();
            string::iterator i ;
            for (i = str.begin(); i != e ; i++) {
                if (!str_4_rem.empty() ) {
                    num_str = str_4_rem + (*i);
                    str_4_rem.erase();
                } else {
                    num_str = (*i);
                }
                if (num_str == "1"  && i==str.begin()) {
                    str_4_rem = "1";
                    continue;
                } else if (num_str == "1" && i!=str.begin()) {
                    str_4_rem = "1";
                }
                result = ( atoi(num_str.c_str()))/2;
                if ((rem = (atoi(num_str.c_str()))%2) != 0) {
                    str_4_rem = "1";
                }
                sprintf(s, "%d", result);
                sum_str += s;
            }
            if (rem != 0) {
                *number = (*number)|mask_for_bit[pos];
                rem = 0;
            }


            str = sum_str;
            sum_str.erase();
            str_4_rem.erase();

            if (pos == 7) {
                number--;
                pos = 0;
                continue;
            }
            pos++;


        }
        if (str.length() <= 9 ) {
            int int_part = atoi (str.c_str());
            while ( (int_part/2) > 0) {
                rem = int_part%2;
                //write remin  into the needed place;

                if (rem != 0) {
                    *number = (*number)|mask_for_bit[pos];

                    rem = 0;
                }

                int_part = int_part/2;
                if (pos == 7) {
                    number--;
                    pos = 0;
                    continue;
                }
                pos++;
            }
            *number = (*number)|mask_for_bit[pos];
        }
    }
    cs_num->precision = prec;
    cs_num->scale = scale;

    return cs_num;
}

CS_NUMERIC*  longlong_to_numeric (long long l_num, int prec,
                                  int scale, CS_NUMERIC* cs_num)
{
    bool needs_del= false;

    if (cs_num == 0) {
        cs_num= new CS_NUMERIC;
        needs_del= true;
    }
    memset (cs_num, 0, sizeof(CS_NUMERIC));

    int BYTE_NUM = s_NumericBytesPerPrec[prec-1];
    unsigned char* number = &cs_num->array[BYTE_NUM - 1];
    if (l_num != 0) {
        if (l_num < 0) {
            l_num *= (-1);
            cs_num->array[0] = cs_num->array[0]|mask_for_bit[0];
        }
        if (scale == 0 ) {
            while (l_num != 0 && number >= 0) {
                long long rem = l_num%256;
                *number = (unsigned char)rem;
                l_num = l_num/256;
                number--;
                if (number <= cs_num->array) {
                    if(needs_del) delete cs_num;
                    return 0;
                }
            }
        }
        else {
            if(needs_del) delete cs_num;
            return 0;
        }
    }
    cs_num->precision = prec;
    cs_num->scale = 0;
    return cs_num;

}

long long numeric_to_longlong(CS_NUMERIC* cs_num)

{

    int i;
    int BYTE_NUM = s_NumericBytesPerPrec[cs_num->precision - 1];
    long long my_long = 0;

    if (cs_num->scale ==0 && BYTE_NUM <= 9) {
        for (i = 1; i< BYTE_NUM; i++) {
            my_long = my_long*256 + cs_num->array[i];
        }
        if (cs_num->array[0] != 0) {
            my_long*= -1;
        }
    } else {
        return 0;
    }

    return my_long;
}
