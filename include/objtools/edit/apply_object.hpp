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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 */


#ifndef _APPLY_OBJECT_H_
#define _APPLY_OBJECT_H_

#include <corelib/ncbistd.hpp>

#include <objmgr/scope.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objtools/edit/string_constraint.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


class NCBI_XOBJEDIT_EXPORT CApplyObject : public CObject
{
public:
    CApplyObject(CSeq_entry_Handle seh, CConstRef<CObject> orig, CRef<CObject> editable)
        : m_SEH(seh), m_Original(orig), m_Editable(editable), m_Delete(false) {};

    CApplyObject(CBioseq_Handle bsh, const CSeqdesc& desc);
    CApplyObject(CBioseq_Handle bsh, CSeqdesc::E_Choice subtype);
    CApplyObject(CBioseq_Handle bsh, const string& user_label);
    CApplyObject(CBioseq_Handle bsh, const CSeq_feat& feat);
    CApplyObject(CBioseq_Handle bsh);
    
    bool PreExists() const { if (m_Original) return true; else return false; };
    CObject& SetObject() { return *m_Editable; };
    const CObject& GetObject() const { return *m_Editable; };
    const CObject* GetOriginalObject() const {return m_Original.GetPointer();};
    void ReplaceEditable(CObject& edit) { m_Editable.Reset(&edit); };
    void DeleteEditable(bool do_delete = true) { m_Delete = do_delete; };
    CSeq_entry_Handle GetSEH() const { return m_SEH; };
    void ApplyChange();

protected:
    CSeq_entry_Handle m_SEH;
    CConstRef<CObject> m_Original;
    CRef<CObject> m_Editable;
    bool m_Delete;
};


NCBI_XOBJEDIT_EXPORT CSeq_entry_Handle GetSeqEntryForSeqdesc (CRef<CScope> scope, const CSeqdesc& seq_desc);


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

