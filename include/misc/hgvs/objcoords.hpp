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
 * Author:  Victor Joukov, Yury Voronov
 *
 * File Description: Library to retrieve mapped sequence coordinates of
 *                   the feature products given a sequence location
 *
 *
 */

#include <objects/seqloc/Seq_id.hpp>
// our ASN
#include <objects/coords/HGVS_Coordinate.hpp>
#include <objects/coords/HGVS_Coordinate_Set.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
END_SCOPE(objects)

USING_SCOPE(ncbi);
USING_SCOPE(objects);


class CObjectCoords : public CObject
{
public:
    // how far to search for features from the pos given
    static const TSeqPos kSearchWindow = 2000;
    CObjectCoords(objects::CScope& scope);
    CRef<CHGVS_Coordinate_Set> GetCoordinates(const CSeq_id& id, TSeqPos pos, TSeqPos window = kSearchWindow);
    void GetCoordinates(CHGVS_Coordinate_Set& coords, const CSeq_id& id, TSeqPos pos, TSeqPos window = kSearchWindow);

protected:
    CRef<objects::CScope> m_Scope;
};


END_NCBI_SCOPE
