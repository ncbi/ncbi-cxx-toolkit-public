#ifndef LDS_QUERY_HPP__
#define LDS_QUERY_HPP__
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
 * File Description: Different query functions to LDS database.
 *
 */

#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objmgr/util/obj_sniff.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//////////////////////////////////////////////////////////////////
///
/// CLDS_Query different queries to the LDS database.
///

class NCBI_LDS_EXPORT CLDS_Query
{
public:
    CLDS_Query(SLDS_TablesCollection& lds_tables)
    : m_db(lds_tables)
    {}

    /// Scan the database, find the file, return TRUE if file exists.
    /// Linear scan, no idx optimization.
    bool FindFile(const string& path);

    /// Scan the objects database, search for sequences.
    /// All found ids are added to the obj_ids set.
    void FindSequences(const vector<string>& seqids, CLDS_Set* obj_ids);

    /// Scan seq_id_list, search for referred sequence ids .
    void FindSeqIdList(const vector<string>& seqids, CLDS_Set* obj_ids);

    /// Scan the objects database, search for sequences
    /// All found ids are added to the obj_ids set.
    void FindSequences(const string& query, CLDS_Set* obj_ids);

    /// Structure describes the indexed object
    struct SObjectDescr
    {
        int                     id;
        bool                    is_object;
        string                  type_str;
        CFormatGuess::EFormat   format;
        string                  file_name;
        size_t                  offset;
        string                  title;
    };

    // Return object's description.
    // SObjectDescr.id == 0 if requested object cannot be found.
    SObjectDescr GetObjectDescr(const map<string, int>& type_map, 
                                int id,
                                bool trace_to_top = false);

private:
    SLDS_TablesCollection& m_db;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/03/11 18:43:10  kuznets
 * + FindSequences (by a query string)
 *
 * Revision 1.5  2003/08/06 20:47:58  kuznets
 * SObjectDescr receives title field to facilitate structure reuse in gbench UI component)
 *
 * Revision 1.4  2003/07/10 20:09:26  kuznets
 * Implemented GetObjectDescr query. Searches both objects and annotations.
 *
 * Revision 1.3  2003/07/09 19:31:56  kuznets
 * Added query scanning sequence id list.
 *
 * Revision 1.2  2003/06/20 19:55:50  kuznets
 * Implemented new function "FindSequences"
 *
 * Revision 1.1  2003/06/16 14:54:08  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 * ===========================================================================
 */

#endif
