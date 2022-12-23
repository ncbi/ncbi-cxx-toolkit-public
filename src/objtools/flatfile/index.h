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
 * File Name: index.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Main parser defines.
 *
 */

#ifndef _INDEX_
#define _INDEX_

#include <objtools/flatfile/flatfile_parser.hpp>

// clang-format off
enum EIndexCol {
    /* genbank format old style LOCUS line column position */
    ParFlat_COL_BASES           = 22,
    ParFlat_COL_BP              = 30,
    ParFlat_COL_STRAND          = 33,
    ParFlat_COL_MOLECULE        = 36,
    ParFlat_COL_TOPOLOGY        = 42,
    ParFlat_COL_DIV             = 52,
    ParFlat_COL_DATE            = 62,

    /* genbank format new style LOCUS line column position */
    ParFlat_COL_BASES_NEW       = 29,
    ParFlat_COL_BP_NEW          = 41,
    ParFlat_COL_STRAND_NEW      = 44,
    ParFlat_COL_MOLECULE_NEW    = 47,
    ParFlat_COL_TOPOLOGY_NEW    = 55,
    ParFlat_COL_DIV_NEW         = 64,
    ParFlat_COL_DATE_NEW        = 68,

    ParFlat_REF_END             = 26,
    ParFlat_REF_BTW             = 27,
    ParFlat_REF_SITES           = 28,
    ParFlat_REF_NO_TARGET       = 29,

    ParFlat_COL_FEATKEY         = 5,
    ParFlat_COL_FEATDAT         = 21,
};
// clang-format on

#endif
