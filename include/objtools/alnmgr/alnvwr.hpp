#ifndef OBJECTS_ALNMGR_DEMO__ALNVWR__HPP
#define OBJECTS_ALNMGR_DEMO__ALNVWR__HPP

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
 * Author:  Kamen Todorov, NCBI
 *
 * File Description:
 *   Various alignment viewers
 *
 */


#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/alnmgr/alnvec.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


class NCBI_XALNMGR_EXPORT CAlnVwr
{
public:
    // CAlnMap printers:

    static void CsvTable(const CAlnMap&          aln_map,
                         CNcbiOstream&           out   = cout);

    static void Segments(const CAlnMap&          aln_map,
                         CNcbiOstream&           out   = cout);

    static void Chunks  (const CAlnMap&          aln_map,
                         CAlnMap::TGetChunkFlags flags = CAlnMap::fAlnSegsOnly,
                         CNcbiOstream&           out   = cout);


    // CAlnVec printer:

    // which algorithm to choose
    enum EAlgorithm {
        eUseChunks,
        eUseSegments,
        eSpeedOptimized
    };

    static void PopsetStyle(const CAlnVec& aln_vec,
                            int            scrn_width = 70,
                            EAlgorithm     algorithm  = eSpeedOptimized,
                            CNcbiOstream&  out        = cout);
};


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/08/28 18:45:38  vakatov
 * Added MS-Win export;  some code beautification.
 * BTW, CNcbiOstream&  out = cout looks a bit fishy.
 *
 * Revision 1.1  2004/08/27 20:53:34  todorov
 * initial revision
 *
 * ===========================================================================
 */

#endif  /* OBJECTS_ALNMGR_DEMO__ALNVWR__HPP */
