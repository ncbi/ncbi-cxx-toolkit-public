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
#include <objtools/readers/source_mod_parser.hpp>

#include "struc_cmt_reader.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

    CUser_object* FindStructuredComment(CSeq_descr& descr)
    {
        NON_CONST_ITERATE(CSeq_descr::Tdata, it, descr.Set())
        {
            if ((**it).IsUser())
            {
                if ((**it).GetUser().GetType().GetStr().compare("StructuredComment") == 0)
                    return &((**it).SetUser());
            }
        }
        return 0;
    }

    CBioseq* FindObjectById(CSeq_entry& entry, const CSeq_id& id)
    {
        switch (entry.Which())
        {
        case CSeq_entry::e_Seq:
            if (entry.GetSeq().GetFirstId()->Compare(id) == CSeq_id::e_YES)
                return &entry.SetSeq();
            break;
        case CSeq_entry::e_Set:
            NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry.SetSet().SetSeq_set())
            {
                return FindObjectById(**it, id);
            }
            break;
        default:
            break;
        }
        return 0;
    }


}

CUser_object* CStructuredCommentsReader::AddStructuredComment(CUser_object* user_obj, CSeq_descr& descr, const string& name, const string& value)
{
    if (name.compare("StructuredCommentPrefix") == 0)
        user_obj = 0; // reset user obj so to create a new one
    else
        if (user_obj == 0)
            user_obj = FindStructuredComment(descr);

    if (user_obj == 0)
    {
        // create new user object
        CRef<CSeqdesc> desc(new CSeqdesc);
        user_obj = &(desc->SetUser());
        user_obj->SetType().SetStr("StructuredComment");
        descr.Set().push_back(desc);
    }
    user_obj->AddField(name, value);
    // create next user object
    if (name.compare("StructuredCommentSuffix") == 0)
        return 0;
    else
        return user_obj;
}


void CStructuredCommentsReader::AddStructuredCommentToAllObjects(CSeq_entry& entry, const string& name, const string& value)
{
    AddStructuredComment(0, entry.SetDescr(), name, value);
}

void CStructuredCommentsReader::ProcessCommentsFileByCols(ILineReader& reader, CSeq_entry& container)
{
    vector<string> cols;

    while (!reader.AtEOF())
    {
        reader.ReadLine();
        // First line is a collumn definitions
        string current = reader.GetCurrentLine();
        if (reader.GetLineNumber() == 1)
        {
            NStr::Tokenize(current, "\t", cols);
            continue;
        }

        if (!current.empty())
        {
            // Each line except first is a set of values, first collumn is a sequence id
            vector<string> values;
            NStr::Tokenize(current, "\t", values);
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

void CStructuredCommentsReader::ProcessCommentsFileByRows(ILineReader& reader, CSeq_entry& container)
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

void CStructuredCommentsReader::ProcessSourceQualifiers(ILineReader& reader, CSeq_entry& container)
{
    vector<string> cols;

    while (!reader.AtEOF())
    {
        reader.ReadLine();
        // First line is a collumn definitions
        CTempString current = reader.GetCurrentLine();
        if (reader.GetLineNumber() == 1)
        {
            NStr::Tokenize(current, "\t", cols);
            continue;
        }

        if (!current.empty())
        {
            // Each line except first is a set of values, first collumn is a sequence id
            vector<string> values;
            NStr::Tokenize(current, "\t", values);
            if (!values[0].empty())
            {               
                // try to find destination sequence
                CSeq_id id(values[0], CSeq_id::fParse_AnyLocal);
                CBioseq* dest = FindObjectById(container, id);
                if (dest)
                {
                    for (size_t i=1; i<values.size(); i++)
                    {
                        if (!values[i].empty())
                        {
                            // apply structure comment
                            AddSourceQualifier(*dest, cols[i], values[i]);
                        }
                    }
                }
            }
        }
    }
}

void CStructuredCommentsReader::AddSourceQualifier(CBioseq& container, const string& name, const string& value)
{
    CSourceModParser mod;
    mod.ParseTitle("[" + name + "=" + value + "]", CConstRef<CSeq_id>(container.GetFirstId()));

    mod.ApplyAllMods(container);
}

CStructuredComments::CStructuredComments(objects::CSeq_entry& container)
    :m_entry(&container)
{
}

END_NCBI_SCOPE

