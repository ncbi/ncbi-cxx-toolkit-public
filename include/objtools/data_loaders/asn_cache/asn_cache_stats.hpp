#ifndef INTERNAL_GPIPE_ASN_CACHE_STATS_HPP__
#define INTERNAL_GPIPE_ASN_CACHE_STATS_HPP__

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
 * Authors:  Cheinan Marks
 *
 * File Description:
 *
 * Class to calculate and return statistics from an ASN Cache.  CAsnCacheStats
 * is a friend of CAsnCache.
 */

BEGIN_NCBI_SCOPE

class CAsnIndex;
class CAsnCache;
class CAsnCacheStats
{
public:
    size_t    GetGICount( CNcbiOstream * stream ) const;
    void      DumpSeqIds( CNcbiOstream & stream ) const;
    void      DumpIndex( CNcbiOstream & stream ) const;

    CAsnCacheStats( CAsnCache & asn_cache_ref )
        : m_AsnCacheRef( asn_cache_ref ), m_AsnIndexRef( asn_cache_ref.GetIndexRef() )
            {}
private:
    CAsnCache & m_AsnCacheRef;
    CAsnIndex & m_AsnIndexRef;
};


END_NCBI_SCOPE

#endif  //  INTERNAL_GPIPE_ASN_CACHE_STATS_HPP__

