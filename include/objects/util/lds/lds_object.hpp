#ifndef LDS_OBJECT_HPP__
#define LDS_OBJECT_HPP__
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Different operations on LDS Object table
 *
 */

#include <corelib/ncbistd.hpp>
#include <util/format_guess.hpp>

#include <objects/util/lds/lds_db.hpp>
#include <objects/util/lds/lds_set.hpp>
#include <objects/util/lds/lds_expt.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//////////////////////////////////////////////////////////////////
//
// SLDS_ObjectDB related methods.
//

class CLDS_Object
{
public:
    CLDS_Object(SLDS_TablesCollection& db, const map<string, int>& obj_map)    
    : m_db(db),
      m_ObjTypeMap(obj_map),
      m_MaxRecId(0)
    {}

    // Delete all objects living in the specified set of files.
    // All deleted ids are added to deleted set.
    void DeleteCascadeFiles(const CLDS_Set& file_ids, 
                            CLDS_Set* objects_deleted);

    // Reload all objects in given set of files
    void UpdateCascadeFiles(const CLDS_Set& file_ids);

    void UpdateFileObjects(int file_id,
                           const string& file_name,
                           CFormatGuess::EFormat format);

    // Return max record id. 0 if no record found.
    int FindMaxRecId();

private:
    CLDS_Object(const CLDS_Object&);
    CLDS_Object& operator=(const CLDS_Object&);

private:
    SLDS_TablesCollection&  m_db;
    const map<string, int>& m_ObjTypeMap;

    int                     m_MaxRecId;
};



END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/05/23 20:33:33  kuznets
 * Bulk changes in lds library, code reorganizations, implemented top level
 * objects read, metainformation persistance implemented for top level objects...
 *
 * Revision 1.1  2003/05/22 21:01:02  kuznets
 * Initial revision
 *
 * ===========================================================================
 */

#endif
