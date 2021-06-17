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
* Author:  Aaron Ucko
*
* File Description:
*   Best-codings tables for use in split packing.
*
* ===========================================================================
*/

#define N2 CSeqUtil::e_Ncbi2na
#define N4 CSeqUtil::e_Ncbi4na
#define NX CSeqUtil::e_Ncbi4na_expand // mix of the above

#ifdef COUNT_GAPS
#  define gp CSeqUtil::e_not_set
#else
#  define gp N4
#endif

// #define N8 CSeqUtil::e_Ncbi8na
#define N8 gp

#define AB CSeqUtil::e_Ncbistdaa
// #define A_ CSeqUtil::e_Ncbi8aa
#define A_ CSeqUtil::e_not_set
#define AT CSeqUtil::e_Ncbieaa

#define N_ gp
//////////////// iupacna
{
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N2, N4, N2, N4, N4, N4, N2, N4, N4, N4, N4, N4, N4, gp, N4, // ACGN
    N4, N4, N4, N4, N2, N2, N4, N4, N4, N4, N4, N_, N_, N_, N_, N_, // TU
    N_, N2, N4, N2, N4, N4, N4, N2, N4, N4, N4, N4, N4, N4, gp, N4, // acgn
    N4, N4, N4, N4, N2, N2, N4, N4, N4, N4, N4, N_, N_, N_, N_, N_, // tu
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_
},
#undef N_

#define N_ NX
#ifdef COUNT_GAPS
#  define NG N_
#else
#  define NG N4
#endif
//////////////// ncbi4na
{
    gp, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, gp, // -
    NG, N2, N2, N_, N2, N_, N_, N_, N2, N_, N_, N_, N_, N_, N_, NG, // A
    NG, N2, N2, N_, N2, N_, N_, N_, N2, N_, N_, N_, N_, N_, N_, NG, // C
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N2, N2, N_, N2, N_, N_, N_, N2, N_, N_, N_, N_, N_, N_, NG, // G
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N2, N2, N_, N2, N_, N_, N_, N2, N_, N_, N_, N_, N_, N_, NG, // T
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    NG, N_, N_, N4, N_, N4, N4, N4, N_, N4, N4, N4, N4, N4, N4, NG,
    gp, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, NG, gp  // N
},
#undef N_

#define N_ N8
//////////////// ncbi8na
{
    gp, N2, N2, N4, N2, N4, N4, N4, N2, N4, N4, N4, N4, N4, N4, gp,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
    N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_
},
#undef N_

#ifndef COUNT_GAPS
#  undef gp
#  define gp AT
#endif
//////////////// ncbieaa
{
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, AT, A_, A_, gp, A_, A_, // *, -
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT,
    AT, AT, AT, AT, AT, AT, AT, AT, gp, AT, AT, A_, A_, A_, A_, A_, // X
    A_, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT, AT,
    AT, AT, AT, AT, AT, AT, AT, AT, gp, AT, AT, A_, A_, A_, A_, A_, // x
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_    
},

#ifndef COUNT_GAPS
#  undef gp
#  define gp AB
#endif
//////////////// ncbi8aa
{
    gp, AB, AB, AB, AB, AB, AB, AB, AB, AB, AB, AB, AB, AB, AB, AB, // -
    AB, AB, AB, AB, AB, gp, AB, AB, AB, AB, AB, AB, A_, A_, A_, A_, // X
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_,
    A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_, A_
}

#undef A_
#undef AB
#undef AT
#undef N2
#undef N4
#undef N8
#undef NG
#undef NX
#undef gp
