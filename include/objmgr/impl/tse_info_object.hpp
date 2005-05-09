#ifndef OBJECTS_OBJMGR_IMPL___TSE_INFO_OBJECT__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_INFO_OBJECT__HPP

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
 * Author: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description:
 *   Bioseq info for data source
 *
 */

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CTSE_Info;
class CSeq_entry;
class CSeq_entry_Info;
class CSeq_annot;
class CSeq_annot_Info;
class CSeq_descr;

////////////////////////////////////////////////////////////////////
//
//  CTSE_Info_Object::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


class NCBI_XOBJMGR_EXPORT CTSE_Info_Object : public CObject
{
public:
    // 'ctors
    CTSE_Info_Object(void);
    virtual ~CTSE_Info_Object(void);

    // info tree
    bool HasDataSource(void) const;
    CDataSource& GetDataSource(void) const;

    bool HasTSE_Info(void) const;
    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    bool HasParent_Info(void) const;
    const CTSE_Info_Object& GetBaseParent_Info(void) const;
    CTSE_Info_Object& GetBaseParent_Info(void);

    // info tree initialization
    void x_DSAttach(CDataSource& ds);
    void x_DSDetach(CDataSource& ds);

    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    void x_TSEAttach(CTSE_Info& tse);
    void x_TSEDetach(CTSE_Info& tse);

    virtual void x_TSEAttachContents(CTSE_Info& tse);
    virtual void x_TSEDetachContents(CTSE_Info& tse);

    // index support
    bool x_DirtyAnnotIndex(void) const;
    void x_SetDirtyAnnotIndex(void);
    void x_SetParentDirtyAnnotIndex(void);
    void x_ResetDirtyAnnotIndex(void);
    virtual void x_SetDirtyAnnotIndexNoParent(void);
    virtual void x_ResetDirtyAnnotIndexNoParent(void);

    void x_UpdateAnnotIndex(CTSE_Info& tse);
    virtual void x_UpdateAnnotIndexContents(CTSE_Info& tse);

    enum ENeedUpdateAux {
        /// number of bits for fields
        kNeedUpdate_bits              = 8
    };
    enum ENeedUpdate {
        /// all fields of this object
        fNeedUpdate_this              = (1<<kNeedUpdate_bits)-1,
        /// all fields of children objects
        fNeedUpdate_children          = fNeedUpdate_this<<kNeedUpdate_bits,

        /// specific fields of this object
        fNeedUpdate_descr             = 1<<0, //< descr of this object
        fNeedUpdate_annot             = 1<<1, //< annot of this object
        fNeedUpdate_seq_data          = 1<<2, //< seq-data of this object
        fNeedUpdate_core              = 1<<3, //< core
        fNeedUpdate_assembly          = 1<<4, //< assembly of this object

        /// specific fields of children
        fNeedUpdate_children_descr    = fNeedUpdate_descr   <<kNeedUpdate_bits,
        fNeedUpdate_children_annot    = fNeedUpdate_annot   <<kNeedUpdate_bits,
        fNeedUpdate_children_seq_data = fNeedUpdate_seq_data<<kNeedUpdate_bits,
        fNeedUpdate_children_core     = fNeedUpdate_core    <<kNeedUpdate_bits,
        fNeedUpdate_children_assembly = fNeedUpdate_assembly<<kNeedUpdate_bits
    };
    typedef int TNeedUpdateFlags;
    bool x_NeedUpdate(ENeedUpdate flag) const;
    void x_SetNeedUpdate(TNeedUpdateFlags flags);
    virtual void x_SetNeedUpdateParent(TNeedUpdateFlags flags);

    void x_Update(TNeedUpdateFlags flags) const;
    virtual void x_DoUpdate(TNeedUpdateFlags flags);

    void x_UpdateComplete(void) const;
    void x_UpdateCore(void) const;

    typedef int TChunkId;
    typedef vector<TChunkId> TChunkIds;
    void x_LoadChunk(TChunkId chunk_id) const;
    void x_LoadChunks(const TChunkIds& chunk_ids) const;

protected:
    void x_BaseParentAttach(CTSE_Info_Object& parent);
    void x_BaseParentDetach(CTSE_Info_Object& parent);
    void x_AttachObject(CTSE_Info_Object& object);
    void x_DetachObject(CTSE_Info_Object& object);

private:
    CTSE_Info_Object(const CTSE_Info_Object&);
    CTSE_Info_Object& operator=(const CTSE_Info_Object&);

    // Owner TSE info
    CTSE_Info*              m_TSE_Info;
    CTSE_Info_Object*       m_Parent_Info;
    bool                    m_DirtyAnnotIndex;
    TNeedUpdateFlags        m_NeedUpdateFlags;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
bool CTSE_Info_Object::HasTSE_Info(void) const
{
    return m_TSE_Info != 0;
}


inline
bool CTSE_Info_Object::HasParent_Info(void) const
{
    return m_Parent_Info != 0;
}


inline
bool CTSE_Info_Object::x_DirtyAnnotIndex(void) const
{
    return m_DirtyAnnotIndex;
}


inline
bool CTSE_Info_Object::x_NeedUpdate(ENeedUpdate flag) const
{
    return (m_NeedUpdateFlags & flag) != 0;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.7  2005/05/09 18:41:14  ucko
 * Name the enum containing kNeedUpdate_bits (as ENeedUpdateAux) to
 * avoid build errors with GCC 4.x, which otherwise protests that it
 * can't instantiate an unrelated template definition of operator<<.
 *
 * Revision 1.6  2004/10/18 13:57:43  vasilche
 * Added support for split history assembly.
 *
 * Revision 1.5  2004/10/07 14:03:32  vasilche
 * Use shared among TSEs CTSE_Split_Info.
 * Use typedefs and methods for TSE and DataSource locking.
 * Load split CSeqdesc on the fly in CSeqdesc_CI.
 *
 * Revision 1.4  2004/08/04 14:53:26  vasilche
 * Revamped object manager:
 * 1. Changed TSE locking scheme
 * 2. TSE cache is maintained by CDataSource.
 * 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
 * 4. Fixed processing of split data.
 *
 * Revision 1.3  2004/07/12 16:57:32  vasilche
 * Fixed loading of split Seq-descr and Seq-data objects.
 * They are loaded correctly now when GetCompleteXxx() method is called.
 *
 * Revision 1.2  2004/03/24 18:30:29  vasilche
 * Fixed edit API.
 * Every *_Info object has its own shallow copy of original object.
 *
 * Revision 1.1  2004/03/16 15:47:27  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.15  2003/11/28 15:13:25  grichenk
 * Added CSeq_entry_Handle
 *
 * Revision 1.14  2003/09/30 16:22:00  vasilche
 * Updated internal object manager classes to be able to load ID2 data.
 * SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
 * Scope caches results of requests for data to data loaders.
 * Optimized CSeq_id_Handle for gis.
 * Optimized bioseq lookup in scope.
 * Reduced object allocations in annotation iterators.
 * CScope is allowed to be destroyed before other objects using this scope are
 * deleted (feature iterators, bioseq handles etc).
 * Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
 * Added 'adaptive' option to objmgr_demo application.
 *
 * Revision 1.13  2003/06/02 16:01:37  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.12  2003/04/29 19:51:12  vasilche
 * Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
 * Made some typedefs more consistent.
 *
 * Revision 1.11  2003/04/25 14:23:46  vasilche
 * Added explicit constructors, destructor and assignment operator to make it compilable on MSVC DLL.
 *
 * Revision 1.10  2003/04/24 16:12:37  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.9  2003/04/14 21:31:05  grichenk
 * Removed operators ==(), !=() and <()
 *
 * Revision 1.8  2003/03/12 20:09:31  grichenk
 * Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
 *
 * Revision 1.7  2003/02/05 17:57:41  dicuccio
 * Moved into include/objects/objmgr/impl to permit data loaders to be defined
 * outside of xobjmgr.
 *
 * Revision 1.6  2002/12/26 21:03:33  dicuccio
 * Added Win32 export specifier.  Moved file from src/objects/objmgr to
 * include/objects/objmgr.
 *
 * Revision 1.5  2002/07/08 20:51:01  grichenk
 * Moved log to the end of file
 * Replaced static mutex (in CScope, CDataSource) with the mutex
 * pool. Redesigned CDataSource data locking.
 *
 * Revision 1.4  2002/05/29 21:21:13  gouriano
 * added debug dump
 *
 * Revision 1.3  2002/05/06 03:28:46  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.2  2002/02/21 19:27:05  grichenk
 * Rearranged includes. Added scope history. Added searching for the
 * best seq-id match in data sources and scopes. Updated tests.
 *
 * Revision 1.1  2002/02/07 21:25:05  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif//OBJECTS_OBJMGR_IMPL___TSE_INFO_OBJECT__HPP
