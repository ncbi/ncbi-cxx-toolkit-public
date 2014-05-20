#ifndef DEPTH_FILTER__HPP
#define DEPTH_FILTER__HPP


/*  $Id$
 * ===========================================================================
 *
 *                            PUBliC DOMAIN NOTICE
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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <objects/seqalign/Seq_align.hpp>


BEGIN_NCBI_SCOPE


BEGIN_SCOPE(objects)
class CSeq_align;
END_SCOPE(objects)

class NCBI_XALGOALIGN_EXPORT CAlignDepthFilter {

public:
    
    /* Creates a depth-chart of all of the inputs, 
     * Then remove all alignments that land entirly within 
     *   regions of greater than DepthCutoff 
     * Except, keep any alignments with greater-than-or-equal to
     *   PctIdentRescue, regardless of depth.
     * 
     * Alignments are not divided up by seq-id, or strand, or score, or anything else. 
     * Those divisions are up to the caller.
     */

    static void FilterOneRow(const list< CRef<objects::CSeq_align> >& Input, 
                                   list< CRef<objects::CSeq_align> >& Output, 
                                   int FilterOnRow, 
                                   size_t DepthCutoff=5, 
                                   double PctIdentRescue=95.0);

    static void FilterBothRows(const list< CRef<objects::CSeq_align> >& Input, 
                                     list< CRef<objects::CSeq_align> >& Output, 
                                     size_t DepthCutoff=5, 
                                     double PctIdentRescue=95.0);

};

END_NCBI_SCOPE


#endif // end DEPTH_FILTER__HPP

