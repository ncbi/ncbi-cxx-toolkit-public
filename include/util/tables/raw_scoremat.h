#ifndef UTIL_TABLES___SCOREMAT__H
#define UTIL_TABLES___SCOREMAT__H

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
 */

/** @file scoremat.h
 ** Protein alignment score matrices; shared between the two toolkits.
 **/

#include <util/tables/tables_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** data types */

typedef signed char TNCBIScore;
typedef struct SNCBIPackedScoreMatrix {
    const char*       symbols;  /**< order of residues */
    const TNCBIScore* scores;   /**< strlen(symbols) x strlen(symbols) */
    TNCBIScore        defscore; /**< score for unknown residues */
} SNCBIPackedScoreMatrix;

/** These two functions aren't very fast, but avoid the memory and time
 ** overhead of unpacking.
 ** Residues (aa, aa1, aa2) may be either NCBIstdaa, NCBIeaa, or
 ** lowercase NCBIeaa, though matrices generally don't cover U.
 **/
extern NCBI_TABLES_EXPORT
int        NCBISM_GetIndex(const SNCBIPackedScoreMatrix* sm, int aa);
extern NCBI_TABLES_EXPORT
TNCBIScore NCBISM_GetScore(const SNCBIPackedScoreMatrix* sm,
                           int aa1, int aa2);

/** Recommended approach: unpack and index directly. */
#define NCBI_FSM_DIM 128
typedef struct SNCBIFullScoreMatrix {
    TNCBIScore s[NCBI_FSM_DIM][NCBI_FSM_DIM];
} SNCBIFullScoreMatrix;

extern NCBI_TABLES_EXPORT
void NCBISM_Unpack(const SNCBIPackedScoreMatrix* psm,
                   SNCBIFullScoreMatrix* fsm);

/** The standard matrices. */
extern NCBI_TABLES_EXPORT const SNCBIPackedScoreMatrix NCBISM_Blosum45;
extern NCBI_TABLES_EXPORT const SNCBIPackedScoreMatrix NCBISM_Blosum50;
extern NCBI_TABLES_EXPORT const SNCBIPackedScoreMatrix NCBISM_Blosum62;
extern NCBI_TABLES_EXPORT const SNCBIPackedScoreMatrix NCBISM_Blosum80;
extern NCBI_TABLES_EXPORT const SNCBIPackedScoreMatrix NCBISM_Blosum90;
extern NCBI_TABLES_EXPORT const SNCBIPackedScoreMatrix NCBISM_Pam30;
extern NCBI_TABLES_EXPORT const SNCBIPackedScoreMatrix NCBISM_Pam70;
extern NCBI_TABLES_EXPORT const SNCBIPackedScoreMatrix NCBISM_Pam250;

const SNCBIPackedScoreMatrix* NCBISM_GetStandardMatrix(const char* name);

#ifdef __cplusplus
}
#endif

#endif  /* UTIL_TABLES___SCOREMAT__H */
