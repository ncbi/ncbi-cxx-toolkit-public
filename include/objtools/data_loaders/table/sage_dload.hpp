#ifndef OBJTOOLS_DATA_LOADERS_TABLE___SAGE_DLOAD__HPP
#define OBJTOOLS_DATA_LOADERS_TABLE___SAGE_DLOAD__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <objmgr/data_loader.hpp>
#include <objtools/data_loaders/table/sqlite_table.hpp>

BEGIN_NCBI_SCOPE


//
// class CSageDataLoader interprets a text-file database as a set of SAGE tags
//
class NCBI_XLOADER_TABLE_EXPORT CSageDataLoader : public objects::CDataLoader
{
public:
    static CSageDataLoader* RegisterInObjectManager(
        objects::CObjectManager& om,
        const string& input_file,
        const string& temp_file,
        bool delete_file = true,
        objects::CObjectManager::EIsDefault is_default
            = objects::CObjectManager::eNonDefault,
        objects::CObjectManager::TPriority priority
            = objects::CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(
        const string& input_file,
        const string& temp_file,
        bool delete_file = true);

    // Request features from our database corresponding to a given
    // CSeq_id_Handle
    virtual void GetRecords(const objects::CSeq_id_Handle& handle,
                            EChoice choice);

private:
    CSageDataLoader(const string& loader_name,
                    const string& input_file,
                    const string& temp_file,
                    bool delete_file = true);

    enum {
        eUnknown = -1,
        eAccession = 0,
        eStrand,
        eFrom,
        eTag,
        eMethod,
        eCount,

        eMaxKnownCol
    };

    // mapping of column idx -> meaning
    vector<int> m_ColAssign;

    // mapping of col meaning -> column idx
    int m_ColIdx[eMaxKnownCol];

    // reference to the table reader we use
    CRef<CSQLiteTable> m_Table;

    // the list of column names we have
    vector<string> m_Cols;

    // struct for sorting CSeq_id_Handle objects by their contents
    struct SIdHandleByContent
    {
        bool operator()(const objects::CSeq_id_Handle& h1,
                        const objects::CSeq_id_Handle& h2) const;
    };

    // the list of IDs we support
    typedef multimap<objects::CSeq_id_Handle, string,
                     SIdHandleByContent> TIdMap;
    TIdMap m_Ids;

    // the map of items we've already loaded
    typedef map<objects::CSeq_id_Handle, CRef<objects::CSeq_entry>,
                SIdHandleByContent> TEntries;
    TEntries m_Entries;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/07/21 15:51:23  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.4  2003/11/28 13:40:40  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.3  2003/11/13 20:59:59  dicuccio
 * Use multimap instead of map for accession strings - corrects errors with
 * identical accessions presented with different qualifiers
 *
 * Revision 1.2  2003/10/02 17:50:00  dicuccio
 * Moved table reader into the data loader project, as it depends on SQLite
 *
 * Revision 1.1  2003/10/02 17:34:40  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // OBJTOOLS_DATA_LOADERS_TABLE___SAGE_DLOAD__HPP
