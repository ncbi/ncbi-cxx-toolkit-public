/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software / database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software / database is freely available
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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "gnomon_seq.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

const EResidue k_toMinus[5] = { enT, enG, enC, enA, enN };
const char *const k_aa_table = "KNKNXTTTTTRSRSXIIMIXXXXXXQHQHXPPPPPRRRRRLLLLLXXXXXEDEDXAAAAAGGGGGVVVVVXXXXX*Y*YXSSSSS*CWCXLFLFXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

void Convert(const CResidueVec& src, CEResidueVec& dst)
{
    int len = src.size();
    dst.clear();
    dst.reserve(len);
    for(int i = 0; i < len; ++i)
	dst.push_back( fromACGT(src[i]) );
}

void Convert(const CResidueVec& src, CDoubleStrandSeq& dst)
{
    Convert(src,dst[ePlus]);
    Complement(dst[ePlus],dst[eMinus]);
}

void Convert(const CEResidueVec& src, CResidueVec& dst)
{
    int len = src.size();
    dst.clear();
    dst.reserve(len);
    for(int i = 0; i < len; ++i)
	dst.push_back( toACGT(src[i]) );
}

void Complement(const CEResidueVec& src, CEResidueVec& dst)
{
    int len = src.size();
    dst.clear();
    dst.reserve(len);
    for(int i = len-1; i >= 0; --i)
	dst.push_back(k_toMinus[(int)src[i]]);
}

END_SCOPE(gnomon)
END_NCBI_SCOPE
