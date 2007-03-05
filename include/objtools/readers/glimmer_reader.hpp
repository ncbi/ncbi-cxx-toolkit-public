#ifndef __GLIMMER_READER__HPP
#define __GLIMMER_READER__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqfeat/Genetic_code.hpp>

BEGIN_NCBI_SCOPE


class NCBI_XOBJREAD_EXPORT CGlimmerReader
{
public:
    CGlimmerReader();

    /// read in and create a seq-annot for the glimmer input
    /// we also optionally create proteins for the CDSs
    /// we require a scope, as the genomes may be circular, and features
    /// crossing the origin cannot be placed without determining the length of
    /// the matched sequence.  If the genetic code is not supplied, then it is
    /// assumed to be standard bacterial (code = 11)
    CRef<objects::CSeq_entry> Read(CNcbiIstream& istr,
                                   objects::CScope& scope,
                                   int genetic_code_idx = 11);
};


END_NCBI_SCOPE

#endif  // __GLIMMER_READER__HPP
