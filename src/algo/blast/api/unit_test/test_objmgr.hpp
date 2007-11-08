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
* Author:  Christiam Camacho
*
* File Description:
*   Singleton class to facilitate the creation of SSeqLocs.
*
* ===========================================================================
*/
#ifndef _TEST_OBJMRG_HPP
#define _TEST_OBJMRG_HPP

#include <objects/seqloc/Na_strand.hpp>
#include <util/range.hpp>
#include <algo/blast/api/sseqloc.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

// Forward declarations
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CSeq_id;
    class CObjectManager;
END_SCOPE(objects)
BEGIN_SCOPE(blast)
    struct SSeqLoc;
END_SCOPE(blast)
END_NCBI_SCOPE

USING_NCBI_SCOPE;

/// This class wraps the C++ Object Manager to control its lifetime and to
/// facilitate the creation of SSeqLoc structures. One CScope is created for
/// each sequence requested to avoid having multiple sequences in once CScope.
class CTestObjMgr {

public:
    static CTestObjMgr& Instance();
    blast::SSeqLoc* CreateSSeqLoc(objects::CSeq_id& id, 
                                  objects::ENa_strand s = 
                                  objects::eNa_strand_unknown);

    blast::SSeqLoc* CreateSSeqLoc(objects::CSeq_id& id, 
                                  pair<TSeqPos, TSeqPos> range,
                                  objects::ENa_strand s = 
                                  objects::eNa_strand_unknown);

    blast::SSeqLoc* CreateSSeqLoc(objects::CSeq_id& id, 
                                  TSeqRange const & range,
                                  objects::ENa_strand s = 
                                  objects::eNa_strand_unknown);

    blast::SSeqLoc* CreateWholeSSeqLoc(objects::CSeq_id& id);

    blast::SSeqLoc* CreateEmptySSeqLoc(objects::CSeq_id& id);

    CRef<blast::CBlastSearchQuery>
    CreateBlastSearchQuery(objects::CSeq_id& id, 
                           objects::ENa_strand s
                           = objects::eNa_strand_unknown);
    
    objects::CObjectManager& GetObjMgr() const;

private:
    static CRef<objects::CObjectManager>       m_ObjMgr;
    static CTestObjMgr*                        m_Instance;

    CTestObjMgr();
    ~CTestObjMgr();
    CTestObjMgr(const CTestObjMgr& rhs);
    const CTestObjMgr& operator=(const CTestObjMgr& rhs);
};

#endif /* SKIP_DOXYGEN_PROCESSING */

#endif // _TEST_OBJMRG_HPP
