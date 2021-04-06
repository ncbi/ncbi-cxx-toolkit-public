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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for structured comments for sequences
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <util/line_reader.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>

#include "struc_cmt_reader.hpp"
#include "table2asn_context.hpp"
#include "visitors.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CTable2AsnStructuredCommentsReader::CTable2AsnStructuredCommentsReader(ILineErrorListener* logger) : CStructuredCommentsReader(logger)
{
}

CTable2AsnStructuredCommentsReader::~CTable2AsnStructuredCommentsReader()
{
}

void CTable2AsnStructuredCommentsReader::ProcessCommentsFileByCols(ILineReader& reader, CSeq_entry& entry)
{
    list<CStructComment> comments;
    LoadComments(reader, comments, CSeq_id::fParse_AnyLocal);
    for (CStructComment& comment : comments) {
        _CheckStructuredCommentsSuffix(comment);
        _AddStructuredComments(entry, comment);
    }
}

void CTable2AsnStructuredCommentsReader::AddStructuredComments(CSeq_descr& descr, const CStructComment& comments)
{
    for (const auto& new_desc : comments.m_descs)
    {
        bool append_desc = true;

        const string& index = CStructComment::GetPrefix(*new_desc);
        if (index.empty())
            continue;

        for (auto& desc : descr.Set()) // push to create setdescr
        {
            if (!desc->IsUser()) continue;

            auto& user = desc->SetUser();

            const string& other = CStructComment::GetPrefix(*desc);
            if (other.empty())
                continue;

            if (NStr::Equal(other, index))
            {
                append_desc = false;
                // Merge
                for (const auto& field : new_desc->GetUser().GetData())
                {
                    user.SetFieldRef(field->GetLabel().GetStr())->SetValue(field->GetData().GetStr());
                }
            }
        }
        if (append_desc)
        {
            CRef<CSeqdesc> add_desc(new CSeqdesc);
            add_desc->Assign(*new_desc);
            descr.Set().push_back(add_desc);
        }
    }
}

void CTable2AsnStructuredCommentsReader::_AddStructuredComments(CSeq_entry& entry, const CStructComment& comments)
{
    VisitAllBioseqs(entry, [comments](CBioseq& bioseq)
    {
        if (!bioseq.IsNa())
            return;

        if (comments.m_id.NotEmpty())
        {
            bool matched = false;
            for (const auto& id : bioseq.GetId())
            {
                if (id->Compare(*comments.m_id) == CSeq_id::e_YES)
                {
                    matched = true;
                    break;
                }
            }
            if (!matched)
                return;
        }

        CTable2AsnStructuredCommentsReader::AddStructuredComments(bioseq.SetDescr(), comments);
    });
}


void CTable2AsnStructuredCommentsReader::_CheckStructuredCommentsSuffix(CStructComment& comments)
{
    // assumption: all descriptors are structural comments
    for (auto& desc : comments.m_descs) {
        string prefix, suffix;
        const auto& user = desc->GetUser();
        for (const auto& data : user.GetData()) {
            if (data->IsSetLabel() && data->GetLabel().IsStr()) {
                const string& label = data->GetLabel().GetStr();
                if (label == "StructuredCommentPrefix") {
                    prefix = data->GetData().GetStr();
                }
                else if (label == "StructuredCommentSuffix") {
                    suffix = data->GetData().GetStr();
                }
            }
        }
        if (!prefix.empty() && suffix.empty()) {
            desc->SetUser().AddField("StructuredCommentSuffix", prefix);
        }
    }
}


void CTable2AsnStructuredCommentsReader::ProcessCommentsFileByRows(ILineReader& reader, CSeq_entry& entry)
{
    CStructComment comments;
    LoadCommentsByRow(reader, comments);
    _CheckStructuredCommentsSuffix(comments);
    VisitAllSeqDesc(entry, true, [comments](CBioseq* bioseq, CSeq_descr& descr)
    {
        if (bioseq && !bioseq->IsNa())
            return;

        CTable2AsnStructuredCommentsReader::AddStructuredComments(descr, comments);
    });
}

bool CTable2AsnStructuredCommentsReader::IsVertical(ILineReader& reader)
{
    CTempString line;
    reader.ReadLine();
    if (!reader.AtEOF())
      line = reader.GetCurrentLine();

    bool vert = NStr::StartsWith(line, "StructuredCommentPrefix");
    reader.UngetLine();
    return vert;
}

END_NCBI_SCOPE
