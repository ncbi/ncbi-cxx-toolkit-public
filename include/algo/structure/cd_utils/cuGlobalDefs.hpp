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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *        
 *          Collect a few global enums, constants and typedefs for CDTree.
 *
 * ===========================================================================
 */

#ifndef CU_GLOBAL_DEFS_HPP
#define CU_GLOBAL_DEFS_HPP


// include ncbistd.hpp, ncbiobj.hpp, ncbi_limits.h, various stl containers
#include <corelib/ncbiargs.hpp>   
#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils) 

typedef CSeq_align::C_Segs::TDendiag TDendiag;
typedef TDendiag::iterator TDendiag_it;
typedef TDendiag::const_iterator TDendiag_cit;

enum CoordMapDir {
    CHILD_TO_MASTER = 0,
    MASTER_TO_CHILD
};

enum AlignmentScoreType {
    E_VALUE     = 0x01,
    RAW_SCORE   = 0x02,
    BIT_SCORE   = 0x04,
    N_IDENTICAL = 0x08
};

const double E_VAL_NOT_FOUND  = 1.0e6;
const int    SCORE_NOT_FOUND  = 0;
const int    INVALID_POSITION = -1;      // residue mapping between rows fails

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // ALGCDTREEDEFS_HPP

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
