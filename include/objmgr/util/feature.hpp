#ifndef FEAT__HPP
#define FEAT__HPP

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
* Author:  Clifford Clausen
*
* File Description:
*   Feature utilities
*/

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_feat;
class CScope;
class CFeat_CI;
class CTSE_Handle;
class CFeat_id;

BEGIN_SCOPE(feature)


/** @addtogroup ObjUtilFeature
 *
 * @{
 */


/** @name GetLabel
 * Returns a label for a CSeq_feat. Label may be based on just the type of 
 * feature, just the content of the feature, or both. If scope is 0, then the
 * label will not include information from feature products.
 * @{
 */

enum ELabelType {
    eType,
    eContent,
    eBoth
};

NCBI_XOBJUTIL_EXPORT
void GetLabel (const CSeq_feat&    feat, 
               string*             label, 
               feature::ELabelType label_type,
               CScope*             scope = 0);


class CFeatIdRemapper : public CObject
{
public:

    void Reset(void);
    size_t GetFeatIdsCount(void) const;

    int RemapId(int old_id, const CTSE_Handle& tse);
    bool RemapId(CFeat_id& id, const CTSE_Handle& tse);
    bool RemapId(CFeat_id& id, const CFeat_CI& feat_it);
    bool RemapIds(CSeq_feat& feat, const CTSE_Handle& tse);
    CRef<CSeq_feat> RemapIds(const CFeat_CI& feat_it);
    
private:
    typedef pair<int, CTSE_Handle> TFullId;
    typedef map<TFullId, int> TIdMap;
    TIdMap m_IdMap;
};


/* @} */


END_SCOPE(feature)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* FEAT__HPP */
