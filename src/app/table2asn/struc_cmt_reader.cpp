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

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

    CBioseq* FindObjectById(CSeq_entry& entry, const CSeq_id& id)
    {
        switch (entry.Which())
        {
        case CSeq_entry::e_Seq:
            ITERATE(CBioseq::TId, it, entry.GetSeq().GetId())
            {
                if ((**it).Compare(id) == CSeq_id::e_YES)
                   return &entry.SetSeq();
            }
            break;
        case CSeq_entry::e_Set:
            NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, entry.SetSet().SetSeq_set())
            {
                CBioseq* obj = FindObjectById(**it, id);
                if (obj)
                    return obj;
            }
            break;
        default:
            break;
        }
        return 0;
    }


}

CTable2AsnStructuredCommentsReader::CTable2AsnStructuredCommentsReader(ILineErrorListener* logger) : CStructuredCommentsReader(logger)
{
}

CTable2AsnStructuredCommentsReader::~CTable2AsnStructuredCommentsReader()
{
}

CUser_object* CTable2AsnStructuredCommentsReader::AddStructuredComment(CUser_object* user_obj, CSeq_descr& descr, const CTempString& name, const CTempString& value)
{
    if (name.compare("StructuredCommentPrefix") == 0)
        user_obj = 0; // reset user obj so to create a new one
    else
        if (user_obj == 0)
            user_obj = FindStructuredComment(descr);

    if (user_obj == 0)
    {
        // create new user object
        CRef<CSeqdesc> user_desc(new CSeqdesc);
        user_obj = &(user_desc->SetUser());
        user_obj->SetType().SetStr("StructuredComment");
        descr.Set().push_back(user_desc);
    }
    user_obj->AddField(name, value);
    // create next user object
    if (name.compare("StructuredCommentSuffix") == 0)
        return 0;
    else
        return user_obj;
}


void CTable2AsnStructuredCommentsReader::AddStructuredCommentToAllObjects(CSeq_entry& entry, const string& name, const string& value)
{
    if (entry.IsSet())
    {
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            AddStructuredComment(0, (**it).SetDescr(), name, value);
        }
    }
    else
        AddStructuredComment(0, entry.SetSeq().SetDescr(), name, value);

}

void CTable2AsnStructuredCommentsReader::ProcessCommentsFileByCols(ILineReader& reader, CSeq_entry& container)
{
    vector<string> cols;

    _LoadHeaderLine(reader, cols);

    while (!reader.AtEOF())
    {
        reader.ReadLine();
        // First line is a collumn definitions
        CTempString current = reader.GetCurrentLine();

        if (!current.empty())
        {
            // Each line except first is a set of values, first collumn is a sequence id
            vector<CTempString> values;
            NStr::Split(current, "\t", values);
            if (!values[0].empty())
            {
                // try to find destination sequence
                CSeq_id id(values[0], CSeq_id::fParse_AnyLocal);
                CBioseq* dest = FindObjectById(container, id);
                if (dest)
                {
                    CUser_object* obj = FindStructuredComment(dest->SetDescr());

                    for (size_t i=1; i<values.size(); i++)
                    {
                        if (!values[i].empty())
                        {
                            // apply structure comment
                            obj = AddStructuredComment(obj, dest->SetDescr(), cols[i], values[i]);
                        }
                    }
                }
            }
        }
    }
}

void CTable2AsnStructuredCommentsReader::ProcessCommentsFileByRows(ILineReader& reader, CSeq_entry& container)
{
    while (!reader.AtEOF())
    {
        reader.ReadLine();
        string current = reader.GetCurrentLine();
        if (!current.empty())
        {
            size_t index = current.find('\t');
            if (index != string::npos )
            {
                string commentname = current.substr(0, index);
                current.erase(current.begin(), current.begin()+index+1);
                AddStructuredCommentToAllObjects(container, commentname, current);
            }
        }
    }
}

END_NCBI_SCOPE
