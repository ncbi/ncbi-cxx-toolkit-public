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

File name: compo_mode_condition.h

Authors: Alejandro Schaffer, Yi-Kuo Yu

Contents: Declarations of functions used to choose the mode for
          composition-based statistics.

******************************************************************************/

#ifndef __COMPO_MODE_CONDITION__
#define __COMPO_MODE_CONDITION__

#ifdef __cplusplus
extern "C" {
#endif

const double *
Blast_GetMatrixBackgroundFreq(const char *matrix_name);

int
Blast_ChooseCompoAdjustMode(int length1, int length2,
                            const double * probArray1, 
                            const double * probArray2,
                            const char * matrixName, 
                            int testFunctionIndex);

#ifdef __cplusplus
}
#endif

#endif
