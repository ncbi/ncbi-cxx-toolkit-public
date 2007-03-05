#ifndef OBJTOOLS_MANIP___SAGE_MANIP__HPP
#define OBJTOOLS_MANIP___SAGE_MANIP__HPP

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
 * Author:  Mike DiCuccio
 *
 * File Description:
 *    CSageManip -- manipulator for SAGE data; encodes/decodes from
 *                  a CUser_object
 */

#include <corelib/ncbistd.hpp>
#include <objects/general/User_object.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//
// class CConstSageData is a manipulator providing const access to a
// CUser_object formatted as a container for a SAGE tag.
//
class NCBI_XOBJMANIP_EXPORT CConstSageData
{
public:
    CConstSageData(const CUser_object& obj);
    virtual ~CConstSageData();

    // const-only accessors
    const string& GetTag(void) const;
    int GetCount(void) const;
    const string& GetMethod(void) const;
    const CUser_field& GetField(const string& field, const string& delim = ".");

protected:
    CRef<CUser_object> m_Object;
};


//
// class CSageData provides non-const access to a CUser_object formatted as a
// SAGE tag.  It inherits all the properties of a CConstSageTag, while
// providing non-const accessors.
//
class NCBI_XOBJMANIP_EXPORT CSageData : public CConstSageData
{
public:
    CSageData(CUser_object& obj);

    // non-const accessors only
    string& SetTag(void);
    void SetTag(const string& tag);

    string& SetMethod(void);
    void SetMethod(const string& str);

    void SetCount(int count);
    CUser_field& SetField(const string& field);
    CUser_field& SetField(const string& field, const string& value);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_MANIP___SAGE_MANIP__HPP
