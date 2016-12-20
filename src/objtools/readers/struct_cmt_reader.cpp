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

#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <util/line_reader.hpp>
#include <objtools/readers/struct_cmt_reader.hpp>


#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CStructuredCommentsReader::CStructuredCommentsReader(ILineErrorListener* logger) : m_logger(logger)
{
}

CStructuredCommentsReader::~CStructuredCommentsReader()
{
}

CUser_object* CStructuredCommentsReader::FindStructuredComment(CSeq_descr& descr)
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

objects::CUser_object* CStructuredCommentsReader::_AddStructuredComment(objects::CUser_object* user_obj, TStructComment& cmt, const CTempString& name, const CTempString& value)
{
    if (name.compare("StructuredCommentPrefix") == 0)
        user_obj = 0; // reset user obj so to create a new one
    else
    {
        //if (user_obj == 0)
        //    user_obj = FindStructuredComment(cmt.m_descs);
    }

    if (user_obj == 0)
    {
        // create new user object
        CRef<CSeqdesc> user_desc(new CSeqdesc);
        user_obj = &(user_desc->SetUser());
        user_obj->SetType().SetStr("StructuredComment");
        cmt.m_descs.push_back(user_desc);
    }
    user_obj->AddField(name, value);
    // create next user object
    if (name.compare("StructuredCommentSuffix") == 0)
        return 0;
    else
        return user_obj;
}

void CStructuredCommentsReader::_BuildStructuredComment(TStructComment& cmt, const vector<string>& cols, const vector<CTempString>& values)
{
    cmt.m_descs.reserve(values.size() - 1);
    objects::CUser_object* user = 0;

    for (size_t i = 1; i<values.size(); i++)
    {
        if (!values[i].empty())
        {
            // create new user object
            user = _AddStructuredComment(user, cmt, cols[i], values[i]);
        }
    }
}

void CStructuredCommentsReader::_LoadHeaderLine(ILineReader& reader, vector<string>& cols)
{
    cols.clear();

    while (!reader.AtEOF() && cols.empty())
    {
        reader.ReadLine();
        // First line is a collumn definitions
        CTempString current = reader.GetCurrentLine();
        if (NStr::StartsWith(current, '#'))
            continue;

        NStr::Split(current, "\t", cols);
    }
}

END_NCBI_SCOPE

