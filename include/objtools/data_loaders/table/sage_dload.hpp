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

// Parameter names used by loader factory

const string kCFParam_Sage_InputFile  = "InputFile";  // = string, mandatory
const string kCFParam_Sage_TempFile   = "TempFile";   // = string, mandatory
const string kCFParam_Sage_DeleteFile = "DeleteFile"; // = bool ("1" = true)

//
// class CSageDataLoader interprets a text-file database as a set of SAGE tags
//
class NCBI_XLOADER_TABLE_EXPORT CSageDataLoader : public objects::CDataLoader
{
public:
    struct SSageParam
    {
        SSageParam(const string& input_file,
                   const string& temp_file,
                   bool          delete_file = true)
            : m_InputFile(input_file),
              m_TempFile(temp_file),
              m_DeleteFile(delete_file) {}
        string  m_InputFile;
        string  m_TempFile;
        bool    m_DeleteFile;
    };

    typedef objects::SRegisterLoaderInfo<CSageDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        objects::CObjectManager& om,
        const string& input_file,
        const string& temp_file,
        bool delete_file = true,
        objects::CObjectManager::EIsDefault is_default
            = objects::CObjectManager::eNonDefault,
        objects::CObjectManager::TPriority priority
            = objects::CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const SSageParam& param);
    static string GetLoaderNameFromArgs(const string& input_file,
                                        const string& temp_file,
                                        bool delete_file = true)
        {
            return GetLoaderNameFromArgs(SSageParam(input_file,
                                                    temp_file,
                                                    delete_file));
        }

    // Request features from our database corresponding to a given
    // CSeq_id_Handle
    virtual TTSE_LockSet GetRecords(const objects::CSeq_id_Handle& handle,
                                    EChoice choice);

private:
    typedef objects::CParamLoaderMaker<CSageDataLoader, SSageParam> TMaker;
    friend class objects::CParamLoaderMaker<CSageDataLoader, SSageParam>;

    CSageDataLoader(const string&     loader_name,
                    const SSageParam& param);

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
};


extern NCBI_XLOADER_TABLE_EXPORT const string kDataLoader_Sage_DriverName;

extern "C"
{

NCBI_XLOADER_TABLE_EXPORT
void NCBI_EntryPoint_DataLoader_Sage(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_TABLE_EXPORT
void NCBI_EntryPoint_xloader_sage(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_TABLE___SAGE_DLOAD__HPP
