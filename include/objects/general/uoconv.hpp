#ifndef OBJECTS_GENERAL___UOCONV__HPP
#define OBJECTS_GENERAL___UOCONV__HPP

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
 * Author:  Aaron Ucko
 *
 */

/// @file uoconv.hpp
/// Converts between User-objects and arbitrary other serializable objects.

#include <serial/objectinfo.hpp>
#include <objects/general/User_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

NCBI_GENERAL_EXPORT
CRef<CUser_object> PackAsUserObject(CConstObjectInfo obj);

NCBI_GENERAL_EXPORT
void UnpackUserObject(const CUser_object& uo, CObjectInfo obj);

NCBI_GENERAL_EXPORT
CObjectInfo UnpackUserObject(const CUser_object& uo, const CTypeInfo* ti);

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2004/07/27 15:13:18  ucko
* Add a somewhat rough implementation of User-object <-> ObjectInfo conversion.
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_GENERAL___UOCONV__HPP */
