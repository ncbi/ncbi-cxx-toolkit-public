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
 * Author: Michael Kornbluh
 *
 * File Description:
 *   stores feats for efficient retrieval in finding the best one.
 *
 */

#ifndef BEST_FEAT_FINDER__HPP
#define BEST_FEAT_FINDER__HPP

#include <map>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

class CSeq_feat;
class CSeq_loc;

//  ============================================================================
class CBestFeatFinder
//  ============================================================================
{
public:
    CBestFeatFinder(void);

    // returns true if successfully added
    bool AddFeat( const CSeq_feat& new_cds );

    // Finds the feat that overlaps the given location with the fewest extra bases.
    CConstRef<CSeq_feat> FindBestFeatForLoc( const CSeq_loc &sought_loc ) const;
    // Same as previous, but allows you to pass start/stop integers rather than having
    // to construct a CSeq_interval object.
    CConstRef<CSeq_feat> FindBestFeatForLoc( const int start_pos, const int stop_pos ) const;

private:

    class CSeqLocSort {
    public:
        bool operator()( const CConstRef<CSeq_loc> &loc1, const CConstRef<CSeq_loc> &loc2 ) const;
    };

    typedef std::multimap< CConstRef<CSeq_loc>, CConstRef<CSeq_feat>, CSeqLocSort > TLocToFeatMap;
    TLocToFeatMap loc_to_feat_map;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // BEST_CDS_FINDER__HPP
