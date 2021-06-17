/*===========================================================================
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
* Author:  Alexey Dobronadezhdin, NCBI
*
* File Description:
*   functionality to process big ASN.1 files
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <serial/objhook.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>

#include "bigfile_processing.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


template <typename T> class CCleanupCopyHook : public CCopyObjectHook
{
private:
    IProcessorCallback& m_callback;

public:

    CCleanupCopyHook(IProcessorCallback& callback) :
        m_callback(callback)
    {}

    virtual void CopyObject(CObjectStreamCopier& copier, const CObjectTypeInfo& type) override
    {
        CRef<CSerialObject> object(new T);
        copier.In().ReadObject(object, type.GetTypeInfo());

        m_callback.Process(object);
        copier.Out().WriteObject(object, object->GetThisTypeInfo());
    }
};

class CProcessWholeObjectException
{
    bool m_process_whole_obj;

public:

    CProcessWholeObjectException(bool process_whole_obj) :
        m_process_whole_obj(process_whole_obj) {}

    bool IsProcessWholeObject() const { return m_process_whole_obj; }
};

class CCleanupReadBioseqHook : public CReadObjectHook
{
public:
    void ReadObject(CObjectIStream&, const CObjectInfo&)
    {
        throw CProcessWholeObjectException(true);
    }
};

class CCleanupReadBioseqSetHook : public CReadObjectHook
{
public:
    void ReadObject(CObjectIStream& in, const CObjectInfo& object)
    {
        CObjectInfo obj_info(object.GetTypeInfo());
        bool process_whole_object = false;
        for (CIStreamClassMemberIterator it(in, object.GetTypeInfo()); it; ++it) {

            it.ReadClassMember(obj_info);
            if ((*it).GetAlias() == "class") {
                TMemberIndex mem_idx = (*it).GetMemberIndex();
                CObjectInfoMI mi_info(obj_info, mem_idx);
                CObjectInfo member_info = mi_info.GetMember();

                CBioseq_set::EClass e_class = *CTypeConverter<CBioseq_set::EClass>::SafeCast(member_info.GetObjectPtr());
                process_whole_object = e_class == CBioseq_set::eClass_nuc_prot;
                break;
            }
        }

        throw CProcessWholeObjectException(process_whole_object);
    }
};

class CCleanupSeqEntryCopyHook : public CCopyObjectHook
{
private:
    IProcessorCallback& m_callback;

public:

    CCleanupSeqEntryCopyHook(IProcessorCallback& callback) :
        m_callback(callback)
    {}

    virtual void CopyObject(CObjectStreamCopier& copier, const CObjectTypeInfo& type) override
    {
        CNcbiStreampos cur_pos = copier.In().GetStreamPos();

        CObjectTypeInfo(CType<CBioseq_set>()).SetLocalReadHook(copier.In(), new CCleanupReadBioseqSetHook);
        CObjectTypeInfo(CType<CBioseq>()).SetLocalReadHook(copier.In(), new CCleanupReadBioseqHook);

        CObjectInfo obj_info(type);
        CRef<CSerialObject> entry(new CSeq_entry);

        bool process_whole_object = false;
        try {
            copier.In().ReadObject(entry, type.GetTypeInfo());
        }
        catch (CProcessWholeObjectException& e) {
            process_whole_object = e.IsProcessWholeObject();
        }

        CObjectTypeInfo(CType<CBioseq_set>()).ResetLocalReadHook(copier.In());
        CObjectTypeInfo(CType<CBioseq>()).ResetLocalReadHook(copier.In());

        copier.In().SetStreamPos(cur_pos);

        if (process_whole_object) {

            copier.In().ReadObject(entry, type.GetTypeInfo());
            m_callback.Process(entry);
            copier.Out().WriteObject(entry, entry->GetThisTypeInfo());
        }
        else {
            DefaultCopy(copier, type);
        }
    }
};


bool ProcessBigFile(CObjectIStream& in, CObjectOStream& out, IProcessorCallback& callback, EBigFileContentType content_type)
{
    bool ret = false;

    CObjectStreamCopier copier(in, out);

    if (content_type == eContentSeqSubmit) {
        CObjectTypeInfo(CType<CSubmit_block>()).SetLocalCopyHook(copier, new CCleanupCopyHook<CSubmit_block>(callback));
    }

    //CObjectTypeInfo(CType<CBioseq_set>()).SetLocalCopyHook(copier, new CCleanupCopyHook<CBioseq_set>(callback));
    //CObjectTypeInfo(CType<CBioseq>()).SetLocalCopyHook(copier, new CCleanupCopyHook<CBioseq>(callback));
    
    CObjectTypeInfo(CType<CSeq_entry>()).SetLocalCopyHook(copier, new CCleanupSeqEntryCopyHook(callback));

    CObjectTypeInfo(CType<CSeq_descr>()).SetLocalCopyHook(copier, new CCleanupCopyHook<CSeq_descr>(callback));
    CObjectTypeInfo(CType<CSeq_annot>()).SetLocalCopyHook(copier, new CCleanupCopyHook<CSeq_annot>(callback));

    try {
        ret = true;
        switch (content_type) {
            case eContentSeqSubmit:
                copier.Copy(CType<CSeq_submit>());
                break;
            case eContentSeqEntry:
                copier.Copy(CType<CSeq_entry>());
                break;
            case eContentBioseqSet:
                copier.Copy(CType<CBioseq_set>());
                break;
            default:
                ret = false;
        }
    }
    catch (CException& e) {
        cerr << "Exception: " << e.what() << '\n';
        ret = false;
    }
    catch (...) {
        cerr << "Other exception caught\n";
        ret = false;
    }

    return ret;
}

END_objects_SCOPE
END_NCBI_SCOPE
