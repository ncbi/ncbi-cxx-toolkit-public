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
#include <objects/submit/Seq_submit.hpp>
#include <util/line_reader.hpp>
#include <objtools/readers/source_mod_parser.hpp>

#include "struc_cmt_reader.hpp"

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

}

void CStructuredCommentsReader::FillVector(const string& input, vector<string>& output)
{
	size_t last_index = 0;
	size_t index;
	do
	{
		index = input.find('\t', last_index);
		output.push_back(input.substr(last_index, (index == string::npos) ? string::npos : index-last_index));		
		last_index = index + 1;
	}
	while (index != string::npos);
}

CBioseq* CStructuredCommentsReader::FindObjectById(CSerialObject& container, const CSeq_id& id)
{
	CSeq_entry* entry = dynamic_cast<CSeq_entry*>(&container);
	if (entry)
	{
		switch (entry->Which())
		{
		case CSeq_entry::e_Seq:
			if (entry->GetSeq().GetFirstId()->Compare(id) == CSeq_id::e_YES)
			   return &entry->SetSeq();
			break;
		case CSeq_entry::e_Set:
			NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry->SetSet().SetSeq_set())
			{
				return FindObjectById(**it, id);
			}
			break;
		}
	}
	else
	{
		CSeq_submit* submit = dynamic_cast<CSeq_submit*>(&container);
		NON_CONST_ITERATE(CSeq_submit_Base::C_Data::TEntrys, it, submit->SetData().SetEntrys())
		{
			return FindObjectById(**it, id);
		}
	}
	return 0;
}

CUser_object* CStructuredCommentsReader::AddStructuredComment(CUser_object* user_obj, const string& name, const string& value, CSeq_descr& descr)
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

CUser_object* CStructuredCommentsReader::AddStructuredComment(CUser_object* obj, const string& name, const string& value, CSerialObject& cont)
{
	CSeq_entry* entry = dynamic_cast<CSeq_entry*>(&cont);
	if (entry)
	{
		obj = AddStructuredComment(obj, name, value, entry->SetDescr());
	}
	else
	{
		CSeq_submit* submit = dynamic_cast<CSeq_submit*>(&cont);
		NON_CONST_ITERATE(CSeq_submit_Base::C_Data::TEntrys, it, submit->SetData().SetEntrys())
		{
			obj = AddStructuredComment(obj, name, value, (**it).SetDescr());
		}
	}
	return obj;
}

void CStructuredCommentsReader::ProcessCommentsFileByCols(ILineReader& reader, CSerialObject& container)
{
	vector<string> cols;

	while (!reader.AtEOF())
	{
		reader.ReadLine();
		if (!reader.AtEOF())
		{
			// First line is a collumn definitions
			string current = reader.GetCurrentLine();
			if (reader.GetLineNumber() == 1)
			{
				FillVector(current, cols);
				continue;
			}

			if (!current.empty())
			{
				// Each line except first is a set of values, first collumn is a sequence id
				vector<string> values;
				FillVector(current, values);
				if (!values[0].empty())
				{				
					// try to find destination sequence
					CSeq_id id(values[0], CSeq_id::fParse_AnyLocal);
					CSerialObject* dest = FindObjectById(container, id);
					if (dest)
					{
						CUser_object* obj = 0;

						for (size_t i=1; i<values.size(); i++)
						{
							if (!values[i].empty())
							{
								// apply structure comment
								obj = AddStructuredComment(obj, cols[i], values[i], *dest);
							}
						}
					}
				}
			}
		}
	}
}

void CStructuredCommentsReader::ProcessCommentsFileByRows(ILineReader& reader, CSerialObject& container)
{
	CUser_object* obj = 0;
	while (!reader.AtEOF())
	{
		reader.ReadLine();
		string current = reader.GetCurrentLine();
		if (!current.empty())
		{
			int index = current.find('\t');
			if (index != string::npos )
			{
				string commentname = current.substr(0, index);
				current.erase(current.begin(), current.begin()+index+1);
				obj = AddStructuredComment(obj, commentname, current, container);
			}
		}
	}
}

void CStructuredCommentsReader::ProcessSourceQualifiers(ILineReader& reader, CSerialObject& container)
{
	vector<string> cols;

	while (!reader.AtEOF())
	{
		reader.ReadLine();
		// First line is a collumn definitions
		string current = reader.GetCurrentLine();
		if (reader.GetLineNumber() == 1)
		{
			FillVector(current, cols);
			continue;
		}

		if (!current.empty())
		{
			// Each line except first is a set of values, first collumn is a sequence id
			vector<string> values;
			FillVector(current, values);
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
							AddSourceQualifier(cols[i], values[i], *dest);
						}
					}
				}
			}
		}
	}
}

void CStructuredCommentsReader::AddSourceQualifier(const string& name, const string& value, CBioseq& container)
{
	CSourceModParser mod;
	mod.ParseTitle("[" + name + "=" + value + "]", CConstRef<CSeq_id>(container.GetFirstId()));

	mod.ApplyAllMods(container);
}

END_NCBI_SCOPE

