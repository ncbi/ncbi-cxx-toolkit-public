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
 * ===========================================================================*/
/**
 * @file composition_constants.h
 * Constants used in compositional score matrix adjustment
 *
 * @author E. Michael Gertz, Alejandro Schaffer, Yi-Kuo Yu
 */


#ifndef __COMPOSITION_CONSTANTS__
#define __COMPOSITION_CONSTANTS__

#include <algo/blast/core/ncbi_std.h>

/** Number of standard amino acids */
#define COMPO_NUM_TRUE_AA 20

/** Number of amino acids, including nonstandard ones */
#define COMPO_PROTEIN_ALPHABET 26

/** Minimum score in a matrix */
#define COMPO_SCORE_MIN INT2_MIN

/** An collection of constants that specify all permissible
 * modes of composition adjustment */
typedef enum ECompoAdjustModes {
    eNoCompositionAdjustment       = (-1),
    eNoCompositionBasedStats       = 0,
    eCompositionBasedStats         = 1,
    eCompositionMatrixAdjust       = 2,
    eCompoForceFullMatrixAdjust    = 3,
    eCompoKeepOldMatrix            = 0,
    eUnconstrainedRelEntropy       = 1,
    eRelEntropyOldMatrixNewContext = 2,
    eRelEntropyOldMatrixOldContext = 3,
    eUserSpecifiedRelEntropy       = 4,
    eNumCompoAdjustModes
} ECompoAdjustModes;


typedef enum EMatrixAdjustRules {
    eDontAdjustMatrix = (-1),
    eCompoScaleOldMatrix = 0
} EMatrixAdjustRules;



#endif
