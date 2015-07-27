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
 * Author:  Colleen Bollin
 *
 * File Description:
 *      Implementation of utility classes for handling source qualifiers by name
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/object_manager.hpp>

#include <vector>
#include <algorithm>
#include <list>

#include <misc/biosample_util/struc_table_column.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CGenericStructuredCommentColumn::AddToComment(CUser_object & user_object, const string & newValue )
{
	bool found = false;
	NON_CONST_ITERATE (CUser_object::TData, it, user_object.SetData()) {
	    if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() && NStr::EqualNocase(m_FieldName, (*it)->GetLabel().GetStr())) {
			(*it)->SetData().SetStr(newValue);
			found = true;
		}
	}
	if (!found) {
		CRef<CUser_field> new_field(new CUser_field());
		new_field->SetLabel().SetStr(m_FieldName);
		new_field->SetData().SetStr(newValue);
		user_object.SetData().push_back(new_field);
	}
}

void CGenericStructuredCommentColumn::ClearInComment(CUser_object & user_object)
{
	if (!user_object.IsSetData()) {
		return;
	}
	CUser_object::TData::iterator it = user_object.SetData().begin();
	while (it != user_object.SetData().end()) {
	    if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() && NStr::EqualNocase(m_FieldName, (*it)->GetLabel().GetStr())) {
			it = user_object.SetData().erase(it);
		} else {
			++it;
		}
	}
}


string CGenericStructuredCommentColumn::GetFromComment(const CUser_object & user_object ) const
{
    string val = "";
	ITERATE (CUser_object::TData, it, user_object.GetData()) {
		if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() && NStr::EqualNocase(m_FieldName, (*it)->GetLabel().GetStr())) {
			if ((*it)->GetData().IsStr()) {
		        val = (*it)->GetData().GetStr();
				break;
			}
		}
	}
  
    return val;
}


CRef<CStructuredCommentTableColumnBase>
CStructuredCommentTableColumnBaseFactory::Create(string sTitle)
{
	// do not create column handler for blank title
    if( NStr::IsBlank(sTitle) ) {
        return CRef<CStructuredCommentTableColumnBase>( NULL );
    }

    return CRef<CStructuredCommentTableColumnBase>( new CGenericStructuredCommentColumn(sTitle) );
    
}


TStructuredCommentTableColumnList GetStructuredCommentFields(const CUser_object& src)
{
    TStructuredCommentTableColumnList fields;

	ITERATE (CUser_object::TData, it, src.GetData()) {
		if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr()) {
            fields.push_back(CStructuredCommentTableColumnBaseFactory::Create((*it)->GetLabel().GetStr()));
		}
	}

    return fields;
}




END_SCOPE(objects)
END_NCBI_SCOPE
