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
 *   check biosource and structured comment descriptors against biosample database
 *
 */

#ifndef BIOSAMPLE_CHK__STRUC_TABLE_COLUMN__HPP
#define BIOSAMPLE_CHK__STRUC_TABLE_COLUMN__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/general/User_object.hpp>

#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CStructuredCommentTableColumnBase : public CObject
{
public:
    // default is to do no command.  Subclasses will almost always override this
    virtual void AddToComment(CUser_object & user_object, const string & newValue ) { }
    virtual void ClearInComment(CUser_object & user_object ) { }
    virtual string GetFromComment(const CUser_object & user_object ) const { return ""; }
    virtual string GetLabel() const { return ""; }
};

class CGenericStructuredCommentColumn : public CStructuredCommentTableColumnBase
{
public:
    CGenericStructuredCommentColumn(const string& field_name) { m_FieldName = field_name; };
    virtual void AddToComment(CUser_object & user_object, const string & newValue );
    virtual void ClearInComment(CUser_object & user_object );
    virtual string GetFromComment(const CUser_object & user_object ) const;
    virtual string GetLabel() const { return m_FieldName; } 
private:
    string m_FieldName;
};


class CStructuredCommentTableColumnBaseFactory
{
public:
    static CRef<CStructuredCommentTableColumnBase> Create(string sTitle);
};


typedef vector< CRef<CStructuredCommentTableColumnBase> > TStructuredCommentTableColumnList;

TStructuredCommentTableColumnList GetStructuredCommentFields(const CUser_object& src);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //STRUC_TABLE_COLUMN
