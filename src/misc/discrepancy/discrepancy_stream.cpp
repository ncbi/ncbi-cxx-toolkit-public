/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <sstream>
#include <objmgr/object_manager.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/objcopy.hpp>
#include <util/compress/stream_util.hpp>
#include <util/line_reader.hpp>
#include <util/format_guess.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);


static size_t offset = 0;
string Offset() // LCOV_EXCL_START
{
    string s;
    for (size_t i = 0; i < offset; i++) s += "  ";
    return s;
} // LCOV_EXCL_STOP

class CReadHook_Bioseq_set : public CReadObjectHook
{
public:
    CReadHook_Bioseq_set(CDiscrepancyContext* context) : m_Context(context) {}
    virtual void ReadObject(CObjectIStream& stream, const CObjectInfo& passed_info)
    {
        if (m_Context->Skip()) {
            m_Context->PushNode(CDiscrepancyContext::eSeqSet);
            //cout << Offset() << "Skipping Bioseq_set " << m_Context->m_CurrentNode->m_Index << "\n";
            m_Context->m_CurrentNode->m_Pos = stream.GetStreamPos();
            DefaultSkip(stream, passed_info);
            m_Context->PopNode();
        }
        else {
            bool repeat = m_Context->m_CurrentNode->m_Repeat;
            m_Context->m_CurrentNode->m_Repeat = false;
            if (!repeat) {
                m_Context->PushNode(CDiscrepancyContext::eSeqSet);
            }
            //cout << Offset() << "Reading " << m_Context->m_CurrentNode->Path() << "\n";
            offset++;
            DefaultRead(stream, passed_info);
            offset--;
            //cout << Offset() << "Done    " << m_Context->m_CurrentNode->Path() << "\n";
            m_Context->m_CurrentNode->m_Obj.Reset((CObject*)passed_info.GetObjectPtr());
            if (!repeat) {
                m_Context->PopNode();
            }
        }
    }
protected:
    CDiscrepancyContext* m_Context;
};


class CReadHook_Bioseq : public CReadObjectHook
{
public:
    CReadHook_Bioseq(CDiscrepancyContext* context) : m_Context(context) {}
    virtual void ReadObject(CObjectIStream& stream, const CObjectInfo& passed_info)
    {
        if (m_Context->Skip()) {
            m_Context->PushNode(CDiscrepancyContext::eBioseq);
            //cout << Offset() << "Skipping Bioseq " << m_Context->m_CurrentNode->m_Index << "\n";
            m_Context->m_CurrentNode->m_Pos = stream.GetStreamPos();
            DefaultSkip(stream, passed_info);
            m_Context->PopNode();
        }
        else {
            bool repeat = m_Context->m_CurrentNode->m_Repeat;
            m_Context->m_CurrentNode->m_Repeat = false;
            if (!repeat) {
                m_Context->PushNode(CDiscrepancyContext::eBioseq);
            }
            //cout << Offset() << "Reading " << m_Context->m_CurrentNode->Path() << "\n";
            DefaultRead(stream, passed_info);
            m_Context->m_CurrentNode->m_Obj.Reset((CObject*)passed_info.GetObjectPtr());
            if (!repeat) {
                m_Context->PopNode();
            }
        }
    }
protected:
    CDiscrepancyContext* m_Context;
};


class CReadHook_Bioseq_set_class : public CReadClassMemberHook
{
public:
    CReadHook_Bioseq_set_class(CDiscrepancyContext* context) : m_Context(context) {}
    virtual void ReadClassMember(CObjectIStream& stream, const CObjectInfoMI& passed_info)
    {
        DefaultRead(stream, passed_info);
        const CBioseq_set::TClass& cl = *(const CBioseq_set::TClass*)passed_info.GetMember().GetObjectPtr();
        switch (cl) {
            case CBioseq_set::eClass_nuc_prot:
                m_Context->m_CurrentNode->SetType(CDiscrepancyContext::eSeqSet_NucProt);
                break;
            case CBioseq_set::eClass_gen_prod_set:
                m_Context->m_CurrentNode->SetType(CDiscrepancyContext::eSeqSet_GenProd);
                break;
            case CBioseq_set::eClass_segset:
                m_Context->m_CurrentNode->SetType(CDiscrepancyContext::eSeqSet_SegSet);
                break;
            case CBioseq_set::eClass_small_genome_set:
                m_Context->m_CurrentNode->SetType(CDiscrepancyContext::eSeqSet_Genome);
                break;
            case CBioseq_set::eClass_eco_set:
            case CBioseq_set::eClass_mut_set:
            case CBioseq_set::eClass_phy_set:
            case CBioseq_set::eClass_pop_set:
                m_Context->m_CurrentNode->SetType(CDiscrepancyContext::eSeqSet_Funny);
                break;
            default:
                break;
        }
    }

protected:
    CDiscrepancyContext* m_Context;
};


void CDiscrepancyContext::PushNode(EObjType type)
{
    CRef<CParseNode> new_node(new CParseNode(type, m_CurrentNode->m_Children.size(), m_CurrentNode));
    m_CurrentNode->m_Children.push_back(new_node);
    m_CurrentNode.Reset(new_node);
}


bool CDiscrepancyContext::Skip()
{
    // Not skipping the first child or nuc-prot children
    return m_Skip && m_CurrentNode->m_Type == eSeqSet && !m_CurrentNode->m_Repeat && m_CurrentNode->m_Children.size();
}


void CDiscrepancyContext::ParseStrings(const string& fname)
{
    m_RootNode.Reset(new CParseNode(eFile, 0));
    m_RootNode->m_Ref->m_Text = fname;
    m_CurrentNode.Reset(m_RootNode);

    CNcbiIfstream istr(fname.c_str());
    CStreamLineReader line_reader(istr);
    do {
        PushNode(eString);
        m_CurrentNode->m_Ref->m_Text = *++line_reader;
        RunTests();
        PopNode();
    }
    while (!line_reader.AtEOF());
}


void CDiscrepancyContext::ParseStream(CObjectIStream& stream, const string& fname, bool skip, const string& default_header)
{
    m_Skip = skip;
    CObjectTypeInfo(CType<CBioseq_set>()).SetLocalReadHook(stream, new CReadHook_Bioseq_set(this));
    CObjectTypeInfo(CType<CBioseq_set>()).FindMember("class").SetLocalReadHook(stream, new CReadHook_Bioseq_set_class(this));
    CObjectTypeInfo(CType<CBioseq>()).SetLocalReadHook(stream, new CReadHook_Bioseq(this));

    m_RootNode.Reset(new CParseNode(eFile, 0));
    m_RootNode->m_Ref->m_Text = fname;
    m_CurrentNode.Reset(m_RootNode);

    while (true) {
        string header = stream.ReadFileHeader();
        if (header.empty()) {
            header = default_header;
        }
        //cout << "Reading " << header << "\n";
        PushNode(eNone);

        if (header == CSeq_submit::GetTypeInfo()->GetName()) {
            PushNode(eSubmit);
            CRef<CSeq_submit> ss(new CSeq_submit);
            stream.Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);
            m_CurrentNode->m_Obj.Reset(ss);
            PopNode();
        }
        else if (header == CSeq_entry::GetTypeInfo()->GetName()) {
            CRef<CSeq_entry> se(new CSeq_entry);
            stream.Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);
        }
        else if (header == CBioseq_set::GetTypeInfo()->GetName()) {
            CRef<CBioseq_set> set(new CBioseq_set);
            stream.Read(ObjectInfo(*set), CObjectIStream::eNoFileHeader);
        }
        else if (header == CBioseq::GetTypeInfo()->GetName()) {
            CRef<CBioseq> seq(new CBioseq);
            stream.Read(ObjectInfo(*seq), CObjectIStream::eNoFileHeader);
        }
        else {
            NCBI_THROW(CException, eUnknown, "Unsupported type " + header); // LCOV_EXCL_LINE
        }
        CNcbiStreampos position = stream.GetStreamPos();
        Extend(*m_CurrentNode, stream);
        if (m_Skip) {
            stream.SetStreamPos(position);
        }
        PopNode();
        if (stream.EndOfData()) {
            break;
        }
    }
}


void CDiscrepancyContext::Extend(CParseNode& node, CObjectIStream& stream)
{
//cout << "Reading " << node.Path() << "\n";
    bool load = (node.m_Type == eSeqSet_NucProt && !InGenProdSet(&node)) || (node.m_Type == eSeqSet_GenProd && !InNucProtSet(&node)) || (node.m_Type == eBioseq && !InNucProtSet(&node) && !InGenProdSet(&node));
    if (load) {
        CRef<CSeq_entry> se(new CSeq_entry());
        if (node.m_Type == eBioseq) {
            se->SetSeq((CBioseq&)*node.m_Obj);
        }
        else { // node.m_Type == eSeqSet_NucProt
            se->SetSet((CBioseq_set&)*node.m_Obj);
        }
        auto handle = m_Scope->AddTopLevelSeqEntry(*se);
        m_FeatTree.Reset(new feature::CFeatTree(handle));
    }
    Populate(node);

    for (size_t i = 0; i < node.m_Children.size(); i++) {
        CParseNode& item = *node.m_Children[i];
        if (!item.m_Obj) {
            stream.SetStreamPos(item.m_Pos);
            item.m_Repeat = true;
            m_CurrentNode.Reset(&item);
            if (item.m_Type == eBioseq) {
                CRef<CBioseq> seq(new CBioseq);
                stream.Read(ObjectInfo(*seq), CObjectIStream::eNoFileHeader);
            }
            else if (item.m_Type == eSeqSet) {
                CRef<CBioseq_set> set(new CBioseq_set);
                stream.Read(ObjectInfo(*set), CObjectIStream::eNoFileHeader);
            }
        }
        Extend(item, stream);
        if (node.m_Type != eSeqSet_NucProt && node.m_Type != eSeqSet_GenProd) {
            node.m_Children[i].Reset();
        }
    }

//cout << "Running tests on " << node.Path() << " ...\n";
    m_CurrentNode.Reset(&node);
    RunTests();

    if (load) {
        //m_FeatTree.Reset();
        m_Scope->ResetDataAndHistory();
    }
}


void CDiscrepancyContext::ParseObject(const CBioseq& root)
{
    CRef<CParseNode> current = m_CurrentNode;
    PushNode(eBioseq);
    m_CurrentNode->m_Obj.Reset((CObject*)&root);
    m_CurrentNode = current;
}


void CDiscrepancyContext::ParseObject(const CBioseq_set& root)
{
    CRef<CParseNode> current = m_CurrentNode;
    EObjType type = eSeqSet;
    if (root.IsSetClass()) {
        switch (root.GetClass()) {
            case CBioseq_set::eClass_nuc_prot:
                type = eSeqSet_NucProt;
                break;
            case CBioseq_set::eClass_gen_prod_set:
                type = eSeqSet_GenProd;
                break;
            case CBioseq_set::eClass_segset:
                type = eSeqSet_SegSet;
                break;
            case CBioseq_set::eClass_small_genome_set:
                type = eSeqSet_Genome;
                break;
            case CBioseq_set::eClass_eco_set:
            case CBioseq_set::eClass_mut_set:
            case CBioseq_set::eClass_phy_set:
            case CBioseq_set::eClass_pop_set:
                type = eSeqSet_Funny;
                break;
            default:
                break;
        }
    }
    PushNode(type);
    m_CurrentNode->m_Obj.Reset((CObject*)&root);
    if (root.CanGetSeq_set()) {
        for (const auto& entry : root.GetSeq_set()) {
            ParseObject(*entry);
        }
    }
    m_CurrentNode = current;
}


void CDiscrepancyContext::ParseObject(const CSeq_entry& root)
{
    if (root.IsSet()) {
        ParseObject(root.GetSet());
    }
    else if (root.IsSeq()) {
        ParseObject(root.GetSeq());
    }
}


void CDiscrepancyContext::ParseObject(const CSeq_submit& root)
{
    CRef<CParseNode> current = m_CurrentNode;
    PushNode(eSubmit);
    m_CurrentNode->m_Obj.Reset((CObject*)&root);
    if (root.CanGetData() && root.GetData().IsEntrys()) {
        for (const auto& entry : root.GetData().GetEntrys()) {
            ParseObject(*entry);
        }
    }
    m_CurrentNode = current;
}


void CDiscrepancyContext::ParseAll(CParseNode& node)
{
    Populate(node);
    for (auto& item : node.m_Children) {
        ParseAll(*item);
    }
    m_CurrentNode.Reset(&node);
    RunTests();
}


void CDiscrepancyContext::Populate(CParseNode& node)
{
    switch (node.m_Type) {
        case eSeqSet:
        case eSeqSet_NucProt:
        case eSeqSet_GenProd:
        case eSeqSet_SegSet:
        case eSeqSet_Genome:
        case eSeqSet_Funny:
            PopulateSeqSet(node);
            break;
        case eBioseq:
            PopulateBioseq(node);
            break;
        case eSubmit:
            PopulateSubmit(node);
            break;
        default:
            break;
    }
}


void CDiscrepancyContext::PopulateSubmit(CParseNode& node)
{
    const CSeq_submit& sub = dynamic_cast<const CSeq_submit&>(*node.m_Obj);
    if (sub.IsSetSub()) {
        if (sub.GetSub().IsSetCit() && sub.GetSub().GetCit().CanGetAuthors()) {
            const CAuth_list* auth = &sub.GetSub().GetCit().GetAuthors();
            node.m_Authors.push_back(auth);
            node.m_AuthorMap[auth] = &node;
        }
    }
}


void CDiscrepancyContext::PopulateBioseq(CParseNode& node)
{
    const CBioseq& bioseq = dynamic_cast<const CBioseq&>(*node.m_Obj);
    if (bioseq.CanGetDescr() && bioseq.GetDescr().CanGet()) {
        for (const auto& desc : bioseq.GetDescr().Get()) {
            node.AddDescriptor(*desc);
            if (desc->IsMolinfo()) {
                node.m_Molinfo.Reset(desc);
            }
            else if (desc->IsSource()) {
                node.m_Biosource.Reset(desc);
            }
            else if (desc->IsTitle()) {
                node.m_Title.Reset(desc);
            }
        }
    }
    if (bioseq.IsSetAnnot()) {
        for (const auto& annot : bioseq.GetAnnot()) {
            if (annot->IsFtable()) {
                for (const auto& feat : annot->GetData().GetFtable()) {
                    node.AddFeature(*feat);
                }
            }
        }
    }
    node.m_BioseqSummary.reset(new CSeqSummary());
    BuildSeqSummary(bioseq, *node.m_BioseqSummary);
    string label = node.m_BioseqSummary->Label;
    node.m_Ref->m_Text = node.m_BioseqSummary->Label + "\n" + node.m_BioseqSummary->GetStats();
    for (CParseNode* n = node.m_Parent; n; n = n->m_Parent) {
        if ((!IsSeqSet(n->m_Type) && n->m_Type != eSubmit) || !n->m_Ref->m_Text.empty()) {
            break;
        }
        n->m_Ref->m_Text = n->m_Type == eSeqSet_NucProt || n->m_Type == eSeqSet_SegSet ? node.m_BioseqSummary->Label : label;
        label = n->m_Ref->GetText();
    }
    if (node.m_Biosource) {
        for (CParseNode* n = node.m_Parent; n && IsSeqSet(n->m_Type); n = n->m_Parent) {
            if (n->m_Type == eSeqSet_Genome || n->m_Type == eSeqSet_Funny) {
                n->m_SetBiosources.push_back(node.m_Biosource);
            }
        }
    }
}


void CDiscrepancyContext::PopulateSeqSet(CParseNode& node)
{
    const CBioseq_set& seqset = dynamic_cast<const CBioseq_set&>(*node.m_Obj);
    if (seqset.CanGetDescr() && seqset.GetDescr().CanGet()) {
        for (const auto& desc : seqset.GetDescr().Get()) {
            node.AddDescriptor(*desc);
            if (desc->IsMolinfo()) {
                node.m_Molinfo.Reset(desc);
            }
            else if (desc->IsSource()) {
                node.m_Biosource.Reset(desc);
            }
            else if (desc->IsTitle()) {
                node.m_Title.Reset(desc);
            }
        }
    }
    if (seqset.IsSetAnnot()) {
        for (const auto& annot : seqset.GetAnnot()) {
            if (annot->IsFtable()) {
                for (const auto& feat : annot->GetData().GetFtable()) {
                    node.AddFeature(*feat);
                }
            }
        }
    }
    if (node.m_Biosource) {
        for (CParseNode* n = &node; n && IsSeqSet(n->m_Type); n = n->m_Parent) {
            if (n->m_Type == eSeqSet_Genome) {
                n->m_SetBiosources.push_back(node.m_Biosource);
            }
        }
    }
}


CDiscrepancyContext::CParseNode* CDiscrepancyContext::FindNode(const CRefNode& ref)
{
    auto it = m_NodeMap.find(&ref);
    if (it != m_NodeMap.end()) {
        return it->second;
    }
    if (ref.m_Parent) {
        CParseNode* p = FindNode(*ref.m_Parent);
        if (p) {
            switch (ref.m_Type) {
                case eSeqFeat:
                    m_NodeMap[&ref] = p->m_Features[ref.m_Index];
                    break;
                case eSeqDesc:
                    m_NodeMap[&ref] = p->m_Descriptors[ref.m_Index];
                    break;
                default:
                    m_NodeMap[&ref] = p->m_Children[ref.m_Index];
                    break;
            }
            return m_NodeMap[&ref];
        }
    }
    return 0;
}


const CSerialObject* CDiscrepancyContext::FindObject(CReportObj& obj, bool alt)
{
    CDiscrepancyObject* p = static_cast<CDiscrepancyObject*>(&obj);
    CParseNode* node = FindNode(alt ? *p->m_Fix : *p->m_Ref);
    return node ? dynamic_cast<const CSerialObject*>(&*node->m_Obj) : 0;
}


void CDiscrepancyContext::ReplaceObject(CReportObj& obj, CSerialObject* ser, bool alt)
{
    CDiscrepancyObject* p = static_cast<CDiscrepancyObject*>(&obj);
    CParseNode* node = FindNode(alt ? *p->m_Fix : *p->m_Ref);
    node->m_Obj.Reset(ser);
}


void CDiscrepancyContext::ReplaceSeq_feat(CReportObj& obj, const CSeq_feat& old_feat, CSeq_feat& new_feat, bool alt)
{
    if (m_AF_Seq_annot) {
        auto& ftable = m_AF_Seq_annot->SetData().SetFtable();
        for (auto& feat : ftable) {
            if (&*feat == &old_feat) {
                feat.Reset(&new_feat);
            }
        }
    }
    else {
        CSeq_feat_EditHandle feh(GetScope().GetSeq_featHandle(old_feat));
        feh.Replace(new_feat);
    }
    ReplaceObject(obj, &new_feat, alt);
}


// AUTOFIX ////////////////////////////////////////////////////////////////////////

class CCopyHook_Bioseq_set : public CCopyObjectHook
{
public:
    CCopyHook_Bioseq_set(CDiscrepancyContext* context) : m_Context(context) {}
    virtual void CopyObject(CObjectStreamCopier& copier, const CObjectTypeInfo& passed_info)
    {
        m_Context->PushNode(CDiscrepancyContext::eSeqSet);
        if (m_Context->CanFixBioseq_set()) {
            m_Context->m_AF_Bioseq_set.Reset(new CBioseq_set);
            copier.In().ReadObject(m_Context->m_AF_Bioseq_set, passed_info.GetTypeInfo());
            m_Context->m_CurrentNode->m_Obj.Reset(m_Context->m_AF_Bioseq_set);
            CRef<CSeq_entry> se(new CSeq_entry());
            se->SetSet(*m_Context->m_AF_Bioseq_set);
            auto handle = m_Context->m_Scope->AddTopLevelSeqEntry(*se);
            m_Context->m_FeatTree.Reset(new feature::CFeatTree(handle));
            m_Context->AutofixBioseq_set();
            copier.Out().WriteObject(m_Context->m_AF_Bioseq_set, passed_info.GetTypeInfo());
            m_Context->m_AF_Bioseq_set.Reset();
        }
        else {
            DefaultCopy(copier, passed_info);
        }
        m_Context->PopNode();
    }
protected:
    CDiscrepancyContext* m_Context;
};


class CCopyHook_Bioseq : public CCopyObjectHook
{
public:
    CCopyHook_Bioseq(CDiscrepancyContext* context) : m_Context(context) {}
    virtual void CopyObject(CObjectStreamCopier& copier, const CObjectTypeInfo& passed_info)
    {
        m_Context->PushNode(CDiscrepancyContext::eBioseq);
        if (m_Context->CanFixBioseq()) {
            m_Context->m_AF_Bioseq.Reset(new CBioseq);
            copier.In().ReadObject(m_Context->m_AF_Bioseq, passed_info.GetTypeInfo());
            m_Context->m_CurrentNode->m_Obj.Reset(m_Context->m_AF_Bioseq);
            CRef<CSeq_entry> se(new CSeq_entry());
            se->SetSeq(*m_Context->m_AF_Bioseq);
            auto handle = m_Context->m_Scope->AddTopLevelSeqEntry(*se);
            m_Context->m_FeatTree.Reset(new feature::CFeatTree(handle));
            m_Context->AutofixBioseq();
            copier.Out().WriteObject(m_Context->m_AF_Bioseq, passed_info.GetTypeInfo());
            m_Context->m_AF_Bioseq.Reset();
        }
        else {
            DefaultCopy(copier, passed_info);
        }
        m_Context->PopNode();
    }
protected:
    CDiscrepancyContext* m_Context;
};


class CCopyHook_Seq_descr : public CCopyObjectHook
{
public:
    CCopyHook_Seq_descr(CDiscrepancyContext* context) : m_Context(context) {}
    virtual void CopyObject(CObjectStreamCopier& copier, const CObjectTypeInfo& passed_info)
    {
        if (m_Context->CanFixSeqdesc()) {
            m_Context->m_AF_Seq_descr.Reset(new CSeq_descr);
            copier.In().ReadObject(m_Context->m_AF_Seq_descr, passed_info.GetTypeInfo());
            m_Context->AutofixSeq_descr();
            copier.Out().WriteObject(m_Context->m_AF_Seq_descr, passed_info.GetTypeInfo());
            m_Context->m_AF_Seq_descr.Reset();
        }
        else {
            DefaultCopy(copier, passed_info);
        }
    }
protected:
    CDiscrepancyContext* m_Context;
};


class CCopyHook_Seq_annot : public CCopyObjectHook
{
public:
    CCopyHook_Seq_annot(CDiscrepancyContext* context) : m_Context(context) {}
    virtual void CopyObject(CObjectStreamCopier& copier, const CObjectTypeInfo& passed_info)
    {
        if (m_Context->CanFixSeq_annot()) {
            m_Context->m_AF_Seq_annot.Reset(new CSeq_annot);
            copier.In().ReadObject(m_Context->m_AF_Seq_annot, passed_info.GetTypeInfo());
            m_Context->AutofixSeq_annot();
            copier.Out().WriteObject(m_Context->m_AF_Seq_annot, passed_info.GetTypeInfo());
            m_Context->m_AF_Seq_annot.Reset();
        }
        else {
            DefaultCopy(copier, passed_info);
        }
    }
protected:
    CDiscrepancyContext* m_Context;
};


class CCopyHook_Submit_block : public CCopyObjectHook
{
public:
    CCopyHook_Submit_block(CDiscrepancyContext* context) : m_Context(context) {}
    virtual void CopyObject(CObjectStreamCopier& copier, const CObjectTypeInfo& passed_info)
    {
    if (m_Context->CanFixSubmit_block()) {
            m_Context->m_AF_Submit_block.Reset(new CSubmit_block);
            copier.In().ReadObject(m_Context->m_AF_Submit_block, passed_info.GetTypeInfo());
            m_Context->AutofixSubmit_block();
            copier.Out().WriteObject(m_Context->m_AF_Submit_block, passed_info.GetTypeInfo());
            m_Context->m_AF_Submit_block.Reset();
        }
        else {
            DefaultCopy(copier, passed_info);
        }
    }
protected:
    CDiscrepancyContext* m_Context;
};


unique_ptr<CObjectIStream> OpenUncompressedStream(const string& fname, bool& compressed) // One more copy!!!
{
    unique_ptr<CNcbiIstream> InputStream(new CNcbiIfstream(fname.c_str(), ios::binary));
    CCompressStream::EMethod method;

    CFormatGuess::EFormat format = CFormatGuess::Format(*InputStream);
    switch (format) {
        case CFormatGuess::eGZip:  method = CCompressStream::eGZipFile;  break;
        case CFormatGuess::eBZip2: method = CCompressStream::eBZip2;     break;
        case CFormatGuess::eLzo:   method = CCompressStream::eLZO;       break;
        default:                   method = CCompressStream::eNone;      break;
    }
    compressed = method != CCompressStream::eNone;
    if (compressed) {
        InputStream.reset(new CDecompressIStream(*InputStream.release(), method, CCompressStream::fDefault, eTakeOwnership));
        format = CFormatGuess::Format(*InputStream);
    }

    unique_ptr<CObjectIStream> objectStream;
    switch (format)
    {
        case CFormatGuess::eBinaryASN:
        case CFormatGuess::eTextASN:
            objectStream.reset(CObjectIStream::Open(format == CFormatGuess::eBinaryASN ? eSerial_AsnBinary : eSerial_AsnText, *InputStream.release(), eTakeOwnership));
            break;
        default:
            break;
    }
    objectStream->SetDelayBufferParsingPolicy(CObjectIStream::eDelayBufferPolicyAlwaysParse);
    return objectStream;
}


void CDiscrepancyContext::Autofix(TReportObjectList& tofix, map<string, size_t>& rep, const string& default_header)
{
    if (tofix.size()) {
        sort(tofix.begin(), tofix.end(), CompareRefs);
        bool in_file = false;
        for (const CRefNode* node = static_cast<CDiscrepancyObject&>(*tofix[0]).m_Fix; node; node = node->m_Parent) {
            if (node->m_Type == eFile) in_file = true;
        }
        if (!in_file) { // GBench etc. -- all objects already in the scope
            for (auto& fix : tofix) {
                CDiscrepancyObject& obj = static_cast<CDiscrepancyObject&>(*fix);
                CRef<CAutofixReport> result = static_cast<CDiscrepancyCore&>(*obj.m_Case).Autofix(&obj, *this);
                if (result) {
                    rep[result->GetS()] += result->GetN();
                }
            }
            return;
        }

        vector<vector<CDiscrepancyObject*>> all_fixes;
        string current_path;
        for (auto& fix : tofix) {
            string path;
            CDiscrepancyObject& obj = static_cast<CDiscrepancyObject&>(*fix);
            for (const CRefNode* node = obj.m_Fix; node; node = node->m_Parent) {
                if (node->m_Type == eFile) {
                    path = node->m_Text;
                    break;
                }
            }
            if (path != current_path) {
                current_path = path;
                vector<CDiscrepancyObject*> fixes;
                all_fixes.push_back(fixes);
            }
            all_fixes.back().push_back(&obj);
        }
        for (auto& fix : all_fixes) {
            AutofixFile(fix, default_header);
        }
    }
}


void CDiscrepancyContext::AutofixFile(vector<CDiscrepancyObject*>&fixes, const string& default_header)
{
    string path;
    for (CRefNode* node = fixes[0]->m_Fix; node; node = node->m_Parent) {
        if (node->m_Type == eFile) {
            path = node->m_Text;
            break;
        }
    }
    bool compressed = false;
    unique_ptr<CObjectIStream> in = OpenUncompressedStream(path, compressed);
cout << "Autofixing " << path << "\n";

    int dot = path.find_last_of('.');
    int slash = path.find_last_of("/\\");
    string fixed_path = !compressed && dot > slash ? path.substr(0, dot) + ".autofix" + path.substr(dot) : path + ".autofix.sqn";

    string header = in->ReadFileHeader();
    in = OpenUncompressedStream(path, compressed);
    unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, fixed_path));
    CObjectStreamCopier copier(*in, *out);

    m_Fixes = &fixes;
    m_RootNode.Reset(new CParseNode(eFile, 0));
    m_CurrentNode.Reset(m_RootNode);

    CObjectTypeInfo(CType<CBioseq_set>()).SetLocalCopyHook(copier, new CCopyHook_Bioseq_set(this));
    CObjectTypeInfo(CType<CBioseq>()).SetLocalCopyHook(copier, new CCopyHook_Bioseq(this));
    CObjectTypeInfo(CType<CSeq_descr>()).SetLocalCopyHook(copier, new CCopyHook_Seq_descr(this));
    CObjectTypeInfo(CType<CSeq_annot>()).SetLocalCopyHook(copier, new CCopyHook_Seq_annot(this));
    CObjectTypeInfo(CType<CSubmit_block>()).SetLocalCopyHook(copier, new CCopyHook_Submit_block(this));

    while (true) {
        if (header.empty()) {
            header = default_header;
        }
        //cout << "Reading " << header << "\n";

        PushNode(eNone);

        if (header == CSeq_submit::GetTypeInfo()->GetName()) {
            PushNode(eSubmit);
            copier.Copy(CSeq_submit::GetTypeInfo());
            PopNode();
        }
        else if (header == CSeq_entry::GetTypeInfo()->GetName()) {
            copier.Copy(CSeq_entry::GetTypeInfo());
        }
        else if (header == CBioseq_set::GetTypeInfo()->GetName()) {
            copier.Copy(CBioseq_set::GetTypeInfo());
        }
        else if (header == CBioseq::GetTypeInfo()->GetName()) {
            copier.Copy(CBioseq::GetTypeInfo());
        }
        else {
            NCBI_THROW(CException, eUnknown, "Unsupported type " + header); // LCOV_EXCL_LINE
        }
        PopNode();
        if (in->EndOfData()) {
            break;
        }
        else {
            // this will crash if the file is both compressed and concatenated,
            // but we are not going to support those
            CNcbiStreampos position = in->GetStreamPos();
            header = in->ReadFileHeader();
            in->SetStreamPos(position);
        }
    }
}


bool CDiscrepancyContext::CanFixBioseq_set(CRefNode& refnode)
{
    if (IsSeqSet(refnode.m_Type)) {
        CRef<CRefNode> A(&refnode);
        auto B = m_CurrentNode->m_Ref;
        while (A && B) {
            if (A->m_Index != B->m_Index) {
                return false;
            }
            A = A->m_Parent;
            B = B->m_Parent;
            if (!A && !B) {
                return true;
            }
        }
    }
    return false;
}


bool CDiscrepancyContext::CanFixBioseq_set()
{
    for (auto fix : *m_Fixes) {
        if (CanFixBioseq_set(*fix->m_Fix)) {
            return true;
        }
    }
    return false;
}



bool CDiscrepancyContext::CanFixBioseq(CRefNode& refnode)
{
    if (refnode.m_Type == eBioseq) {
        CRef<CRefNode> A(&refnode);
        auto B = m_CurrentNode->m_Ref;
        while (A && B) {
            if (A->m_Index != B->m_Index) {
                return false;
            }
            A = A->m_Parent;
            B = B->m_Parent;
            if (!A && !B) {
                return true;
            }
        }
    }
    return false;
}


bool CDiscrepancyContext::CanFixBioseq()
{
    for (auto fix : *m_Fixes) {
        if (CanFixBioseq(*fix->m_Fix)) {
            return true;
        }
    }
    return false;
}


bool CDiscrepancyContext::CanFixFeat(CRefNode& refnode)
{
    if (refnode.m_Type == eSeqFeat) {
        auto A = refnode.m_Parent;
        auto B = m_CurrentNode->m_Ref;
        while (A && B) {
            if (A->m_Index != B->m_Index) {
                return false;
            }
            A = A->m_Parent;
            B = B->m_Parent;
            if (!A && !B) {
                return true;
            }
        }
    }
    return false;
}


bool CDiscrepancyContext::CanFixSeq_annot()
{
    for (auto fix : *m_Fixes) {
        if (CanFixFeat(*fix->m_Fix)) {
            return true;
        }
    }
    return false;
}


bool CDiscrepancyContext::CanFixDesc(CRefNode& refnode)
{
    if (refnode.m_Type == eSeqDesc) {
        auto A = refnode.m_Parent;
        auto B = m_CurrentNode->m_Ref;
        while (A && B) {
            if (A->m_Index != B->m_Index) {
                return false;
            }
            A = A->m_Parent;
            B = B->m_Parent;
            if (!A && !B) {
                return true;
            }
        }
    }
    return false;
}


bool CDiscrepancyContext::CanFixSeqdesc()
{
    for (auto fix : *m_Fixes) {
        if (CanFixDesc(*fix->m_Fix)) {
            return true;
        }
    }
    return false;
}


bool CDiscrepancyContext::CanFixSubmit_block(CRefNode& refnode)
{
    if (refnode.m_Type == eSubmit && m_CurrentNode->m_Ref->m_Type == eSubmit) {
        CRef<CRefNode> A(&refnode);
        auto B = m_CurrentNode->m_Ref;
        while (A && B) {
            if (A->m_Index != B->m_Index) {
                return false;
            }
            A = A->m_Parent;
            B = B->m_Parent;
            if (!A && !B) {
                return true;
            }
        }
    }
    return false;
}


bool CDiscrepancyContext::CanFixSubmit_block()
{
    for (auto fix : *m_Fixes) {
        if (CanFixSubmit_block(*fix->m_Fix)) {
            return true;
        }
    }
    return false;
}


void CDiscrepancyContext::AutofixSeq_annot()
{
    if (m_AF_Seq_annot->IsFtable()) {
        for (auto& feat : m_AF_Seq_annot->GetData().GetFtable()) {
            m_CurrentNode->AddFeature(*feat);
        }
    }

    for (auto fix : *m_Fixes) {
        if (CanFixFeat(*fix->m_Fix) && fix->m_Fix->m_Index < m_CurrentNode->m_Features.size()) {
            m_NodeMap[&*fix->m_Fix] = m_CurrentNode->m_Features[fix->m_Fix->m_Index];
            CRef<CAutofixReport> result = static_cast<CDiscrepancyCore&>(*fix->m_Case).Autofix(fix, *this);
        }
    }
}


void CDiscrepancyContext::AutofixSeq_descr()
{
    if (m_AF_Seq_descr->CanGet()) {
        for (auto& desc : m_AF_Seq_descr->Get()) {
            m_CurrentNode->AddDescriptor(*desc);
        }
    }

    for (auto fix : *m_Fixes) {
        if (CanFixDesc(*fix->m_Fix) && fix->m_Fix->m_Index < m_CurrentNode->m_Descriptors.size()) {
            m_NodeMap[&*fix->m_Fix] = m_CurrentNode->m_Descriptors[fix->m_Fix->m_Index];
            CRef<CAutofixReport> result = static_cast<CDiscrepancyCore&>(*fix->m_Case).Autofix(fix, *this);
        }
    }
}


void CDiscrepancyContext::AutofixSubmit_block()
{
    CRef<CParseNode> sblock(new CParseNode(eSubmitBlock, 0));
    sblock->m_Obj.Reset(static_cast<CObject*>(&*m_AF_Submit_block));

    for (auto fix : *m_Fixes) {
        if (CanFixSubmit_block(*fix->m_Fix)) {
            m_NodeMap[&*fix->m_Fix] = sblock;
            CRef<CAutofixReport> result = static_cast<CDiscrepancyCore&>(*fix->m_Case).Autofix(fix, *this);
        }
    }
}


void CDiscrepancyContext::AutofixBioseq_set()
{
    const CBioseq_set* bss = static_cast<const CBioseq_set*>(&*m_CurrentNode->m_Obj);
    if (bss->CanGetDescr() && bss->GetDescr().CanGet()) {
        for (auto& desc : bss->GetDescr().Get()) {
            m_CurrentNode->AddDescriptor(*desc);
        }
    }
    if (bss->IsSetAnnot()) {
        for (auto& annot : bss->GetAnnot()) {
            if (annot->IsFtable()) {
                for (auto& feat : annot->GetData().GetFtable()) {
                    m_CurrentNode->AddFeature(*feat);
                }
            }
        }
    }

    for (auto& se : bss->GetSeq_set()) {
        if (se->IsSet()) {
            PushNode(CDiscrepancyContext::eSeqSet);
            m_CurrentNode->m_Obj.Reset(&se->GetSet());
            AutofixBioseq_set();
        }
        else {
            PushNode(CDiscrepancyContext::eBioseq);
            m_CurrentNode->m_Obj.Reset(&se->GetSeq());
            AutofixBioseq();
        }
        PopNode();
    }

    for (auto fix : *m_Fixes) {
        if (CanFixBioseq_set(*fix->m_Fix)) {
            m_NodeMap[&*fix->m_Fix] = m_CurrentNode;
            CRef<CAutofixReport> result = static_cast<CDiscrepancyCore&>(*fix->m_Case).Autofix(fix, *this);
        }
    }
}


void CDiscrepancyContext::AutofixBioseq()
{
    const CBioseq* bs = static_cast<const CBioseq*>(&*m_CurrentNode->m_Obj);
    if (bs->CanGetDescr() && bs->GetDescr().CanGet()) {
        for (auto& desc : bs->GetDescr().Get()) {
            m_CurrentNode->AddDescriptor(*desc);
        }
    }
    if (bs->IsSetAnnot()) {
        for (auto& annot : bs->GetAnnot()) {
            if (annot->IsFtable()) {
                for (auto& feat : annot->GetData().GetFtable()) {
                    m_CurrentNode->AddFeature(*feat);
                }
            }
        }
    }

    for (auto fix : *m_Fixes) {
        if (CanFixBioseq(*fix->m_Fix)) {
            m_NodeMap[&*fix->m_Fix] = m_CurrentNode;
            CRef<CAutofixReport> result = static_cast<CDiscrepancyCore&>(*fix->m_Case).Autofix(fix, *this);
        }
    }
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
