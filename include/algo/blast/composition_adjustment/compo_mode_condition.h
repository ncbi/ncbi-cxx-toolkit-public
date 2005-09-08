/* ===========================================================================
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
* ===========================================================================*/

/*****************************************************************************

File name: Mode_condition.h

Authors: Alejandro Schaffer, Yi-Kuo Yu

Contents: Definitions used only in Mode_condition.c

******************************************************************************/
/*
 * $Log$
 * Revision 1.1  2005/09/08 20:10:46  gertz
 * Initial revision.
 *
 * Revision 1.1  2005/05/16 16:11:41  papadopo
 * Initial revision
 *
 */
#ifndef MODE_CONDITION
#define MODE_CONDITION

#define Mode_1_per  0.3
#define Mode_unchange_per 0.6
#define RE_mode_1_limit 0.18

double *Get_bg_freq(char *matrix_name);


int
chooseMode(int length1, int length2,
           double * probArray1, double * probArray2,
           char *matrixName, int testFunctionIndex);

#endif
