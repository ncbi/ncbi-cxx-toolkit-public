#ifndef ALGO_BLAST_CORE___GREEDY_ALIGN_PRI__H
#define ALGO_BLAST_CORE___GREEDY_ALIGN_PRI__H

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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file greedy_align_pri.h
 *  Private low level functions declarations for greedy_align.c and 
 *  blast_gapalign.c
 */


#ifdef __cplusplus
extern "C" {
#endif

/** Find greatest common divisor of 2 integers.
 * @param a First integer [in]
 * @param b Second integer [in]
 * @return The GCD.
 */
Int4 
BLAST_gcd(Int4 a, Int4 b);

/** Divide 3 numbers by their greatest common divisor
 * @param a First integer [in] [out]
 * @param b Second integer [in] [out]
 * @param c Third integer [in] [out]
 * @return The greatest common divisor
 */
Int4 
BLAST_gdb3(Int4* a, Int4* b, Int4* c);

#ifdef __cplusplus
}
#endif


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2004/11/15 16:33:16  dondosha
 * Private header for low level functions needed in greedy_align.c and blast_gapalign.c
 *
 * Revision 1.0  2004/05/18 13:23:26  madden
 * Private declarations for greedy_align.c
 *
 *
 * ===========================================================================
 */

#endif /* !ALGO_BLAST_CORE__GREEDY_ALIGN_PRI__H */
