/* 
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
 *  Please cite the authors in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Lewis Y. Geer
 *
 * File Description:
 *    code to do mass spec scoring
 *
 * ===========================================================================
 */


#ifndef OMSSASCORE__HPP
#define OMSSASCORE__HPP

#include <objects/omssa/omssa__.hpp>

// for log of gamma fcn
#include <util/math/miscmath.h>
// for erf fcn
#include <algo/blast/core/ncbi_math.h>

#define BEGIN_OMSSA_SCOPE \
    BEGIN_NCBI_SCOPE BEGIN_SCOPE(objects) BEGIN_SCOPE(omssa)
#define END_OMSSA_SCOPE END_NCBI_SCOPE END_SCOPE(objects) END_SCOPE(omssa)

#define USING_OMSSA_SCOPE \
    USING_NCBI_SCOPE; USING_SCOPE(objects); USING_SCOPE(omssa);

BEGIN_OMSSA_SCOPE

/** m/z type */
typedef int TMSMZ;

/** intensity type */
typedef unsigned TMSIntensity;

/** ion type, e.g. eMSIonTypeA */
typedef signed char TMSIonSeries;

/** charge type */
typedef signed char TMSCharge;

/** ion sequence number, starting from 0. */
typedef short TMSNumber;

/** density of experimental ions type */
typedef double TMSExpIons;

/** intensity rank */
typedef int TMSRank;

#define MSLNGAMMA BLAST_LnGammaInt
#define MSERF NCBI_Erf

// define convergence delta for scoring
#define MSDOUBLELIMIT 1e-300L

END_OMSSA_SCOPE

// this is a wrapper for msscore.hpp so that msscore.hpp can be ncbi toolkit independent
#include "msscore.hpp"

#endif

