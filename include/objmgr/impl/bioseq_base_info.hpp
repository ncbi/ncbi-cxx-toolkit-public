#ifndef OBJECTS_OBJMGR_IMPL___BIOSEQ_BASE_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___BIOSEQ_BASE_INFO__HPP

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
#include <objmgr/impl/tse_info_object.hpp>
#include <objmgr/seq_id_handle.hpp>

#include <vector>

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
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


class NCBI_XOBJMGR_EXPORT CBioseq_Base_Info : public CTSE_Info_Object
{
    typedef CTSE_Info_Object TParent;
public:
    // 'ctors
    CBioseq_Base_Info(void);
    explicit CBioseq_Base_Info(const CBioseq_Base_Info& info);
    virtual ~CBioseq_Base_Info(void);

    // info tree
    const CSeq_entry_Info& GetParentSeq_entry_Info(void) const;
    CSeq_entry_Info& GetParentSeq_entry_Info(void);

    // member modification
    // descr
    typedef CSeq_descr TDescr;
    bool IsSetDescr(void) const;
    const TDescr& GetDescr(void) const;

    void SetDescr(const TDescr& v);
    void ResetDescr(void);

    // annot
    typedef vector< CRef<CSeq_annot_Info> > TAnnot;
    bool IsSetAnnot(void) const;
    const TAnnot& GetAnnot(void) const;

    CRef<CSeq_annot_Info> x_AddAnnot(const CSeq_annot& annot);
    void x_AddAnnot(CRef<CSeq_annot_Info> annot);
    void x_RemoveAnnot(CRef<CSeq_annot_Info> annot);

    // object initialization
    void x_SetDescr(const CSeq_descr& descr);
    typedef list< CRef<CSeq_annot> > TObjAnnot;
    void x_SetAnnot(const TObjAnnot& annot);
    void x_FillAnnot(TObjAnnot& annot) const;

    void x_AttachAnnot(CRef<CSeq_annot_Info> info);
    void x_DetachAnnot(CRef<CSeq_annot_Info> info);

    // info tree initialization
    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    virtual void x_TSEAttachContents(CTSE_Info& tse);
    virtual void x_TSEDetachContents(CTSE_Info& tse);

    void x_ParentAttach(CSeq_entry_Info& parent);
    void x_ParentDetach(CSeq_entry_Info& parent);

    // index support
    void x_UpdateAnnotIndexContents(CTSE_Info& tse);

    // flags of modified members
    enum EMember {
        fMember_first   = 1,
        fMember_descr   = fMember_first << 0,
        fMember_annot   = fMember_first << 1,

        fMember_last_plus_one,
        fMember_last    = fMember_last_plus_one - 1
    };
    typedef int TMembers;

    TMembers x_GetModifiedMembers(void) const;
    bool x_IsModified(void) const;
    bool x_IsModifiedMember(TMembers members) const;
    void x_SetModifiedMember(TMembers members);
    void x_ResetModifiedMembers(void);

    TMembers x_GetSetMembers(void) const;
    bool x_IsSetMember(TMembers member) const;
    void x_CheckSetMember(TMembers member) const;
    void x_SetSetMembers(TMembers members);
    void x_SetResetMembers(TMembers members);

    virtual const char* x_GetTypeName(void) const = 0;
    virtual const char* x_GetMemberName(TMembers member) const;

private:
    friend class CAnnotTypes_CI;
    friend class CSeq_annot_CI;

    CBioseq_Base_Info& operator=(const CBioseq_Base_Info&);

    // member state
    TMembers            m_ModifiedMembers;
    TMembers            m_SetMembers;

    // members
    CConstRef<TDescr>   m_Descr;
    TAnnot              m_Annot;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CBioseq_Base_Info::TMembers CBioseq_Base_Info::x_GetModifiedMembers(void) const
{
    return m_ModifiedMembers;
}


inline
bool CBioseq_Base_Info::x_IsModified(void) const
{
    return x_GetModifiedMembers() != 0;
}


inline
bool CBioseq_Base_Info::x_IsModifiedMember(TMembers member) const
{
    return (x_GetModifiedMembers() & member) != 0;
}


inline
CBioseq_Base_Info::TMembers CBioseq_Base_Info::x_GetSetMembers(void) const
{
    return m_SetMembers;
}


inline
bool CBioseq_Base_Info::x_IsSetMember(TMembers member) const
{
    return (x_GetSetMembers() & member) != 0;
}


inline
bool CBioseq_Base_Info::IsSetDescr(void) const
{
    return x_IsSetMember(fMember_descr);
}


inline
const CBioseq_Base_Info::TDescr& CBioseq_Base_Info::GetDescr(void) const
{
    x_CheckSetMember(fMember_descr);
    return *m_Descr;
}


inline
bool CBioseq_Base_Info::IsSetAnnot(void) const
{
    return x_IsSetMember(fMember_annot);
}


inline
const CBioseq_Base_Info::TAnnot& CBioseq_Base_Info::GetAnnot(void) const
{
    return m_Annot;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2004/03/16 15:47:26  vasilche
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

#endif//OBJECTS_OBJMGR_IMPL___BIOSEQ_BASE_INFO__HPP
