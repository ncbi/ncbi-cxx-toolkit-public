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
 * Author:  Vladimir Soussov
 *   
 * File Description:  Pot of pointers implementation
 *
 */


#include <ncbi_pch.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>
#include <string.h>


BEGIN_NCBI_SCOPE


void CQuickSortStack::Push(int i1, int i2)
{
    if (m_NofItems >= m_NofRooms) {
        m_NofRooms *= 2;
        int* p = new int[m_NofRooms];
        memcpy(p, m_Items, m_NofItems * sizeof(int));
        delete[] m_Items;
        m_Items = p;
    }
    m_Items[m_NofItems++] = i2;
    m_Items[m_NofItems++] = i1;
}


void CPointerPot::Add(const TPotItem item, int check_4_unique)
{
    if ( check_4_unique ) {
        for (int i = 0;  i < m_NofItems;  i++) {
            if (m_Items[i] == item)
                return;
        }
    }
    
    if (m_NofItems >= m_NofRooms) {
        m_NofRooms += m_NofRooms / 2 + 2;
        TPotItem* n_pot = new TPotItem[m_NofRooms];
        memcpy(n_pot, m_Items, m_NofItems * sizeof(TPotItem));
        delete[] m_Items;
        m_Items = n_pot;
    }
    m_Items[m_NofItems++] = item;
}


void CPointerPot::Remove(int n)
{
    if (n >= 0  &&  n < m_NofItems) {
        if (n != (--m_NofItems)) {
            memmove(&m_Items[n], &m_Items[n+1],
                    (m_NofItems - n) * sizeof(TPotItem));
        }
    }
}


void CPointerPot::Remove(TPotItem item)
{
    for (int i = 0;  i < m_NofItems;  i++) {
        if (m_Items[i] == item) {
            // we've found it
            if (i != (--m_NofItems)) {
                memmove(&m_Items[i], &m_Items[i+1],
                        (m_NofItems - i) * sizeof(TPotItem));
            }
            i--;
        }
    }    
}


static const int kSmallArraySize = 14;

void CPointerPot::x_SimpleSort(TPotItem* arr, int nof_items, FPotCompare cf)
{
    for (bool need_cont = true;  need_cont; ) {
        need_cont = false;

        for (int i = 1;  i < nof_items;  i++) {
            if ((*cf)(arr[i-1], arr[i]) <= 0)
                continue;

            TPotItem t = arr[i-1];
            arr[i-1]   = arr[i];
            arr[i]     = t;

            need_cont = true;
        }
    }
}


void CPointerPot::Sort(FPotCompare cf)
{
    if (m_NofItems <= kSmallArraySize) {
        x_SimpleSort(m_Items, m_NofItems, cf);
        return;
    }

    CQuickSortStack qs(32);
    int l_bnd, r_bnd;

    qs.Push(0, m_NofItems - 1);

    while ( qs.Pop(&l_bnd, &r_bnd) ) {
        int m = r_bnd - l_bnd;
        if (m < 1)
            continue;

        if (m == 1) {
            if ((*cf)(m_Items[l_bnd], m_Items[r_bnd]) > 0) {
                TPotItem t     = m_Items[l_bnd];
                m_Items[l_bnd] = m_Items[r_bnd];
                m_Items[r_bnd] = t;
            }
            continue;
        }

        if (m <= kSmallArraySize) {
            x_SimpleSort(&m_Items[l_bnd], m + 1, cf);
            continue;
        }

        TPotItem itm = m_Items[r_bnd];
        int      l   = l_bnd - 1;
        int      r   = r_bnd;

        for (;;) {
            while ((*cf)(m_Items[++l], itm) < 0   &&  l < r_bnd)
                continue;

            while ((*cf)(m_Items[--r], itm) >= 0  &&  r > l_bnd)
                continue;

            if (l < r) {
                TPotItem t = m_Items[l];
                m_Items[l] = m_Items[r];
                m_Items[r] = t;
            }
            else {
                m_Items[r_bnd] = m_Items[l];
                m_Items[l] = itm;
                qs.Push(l_bnd, l-1);
                qs.Push(l+1, r_bnd);
                break;
            }
        }
    }
}


CPointerPot& CPointerPot::operator= (CPointerPot& pot)
{
    if (m_NofRooms < pot.m_NofItems) {
        delete[] m_Items;
        m_NofRooms = pot.m_NofItems;
        m_Items = new TPotItem[m_NofRooms];
    }

    m_NofItems = pot.m_NofItems;
    if (m_NofItems > 0) {
        memcpy(m_Items, pot.m_Items, m_NofItems * sizeof(TPotItem));
    }
    return *this;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2001/11/06 17:59:53  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/09/21 23:40:00  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */


#if 0

#include <stdlib.h>
#include <stdio.h>
#include <time.h>


USING_NCBI_SCOPE;


int my_cmp(TPotItem i1, TPotItem i2)
{
    return (int) i1 - (int) i2;
}

int main(int argc, char* argv[])
{
    CPointerPot pot(32000);
    int i, j, k;
    TPotItem itm;
    clock_t t1, t2;


    for (i = 0;  i < 164000;  i++) {
        itm = rand();
        pot.Add(itm);
    }

    t1= clock();
    pot.Sort(my_cmp);
    t2= clock();
    printf("done in %ld\n", (long)(t2 - t1));

    for (i=0;  i < 31900;  i+= 500) {
        for (j = i;  j < i+10;  j++) {
            k = (int) pot.get(j);
            printf("%d\t", k);
        }
        printf("\n");
    }
}

#endif
