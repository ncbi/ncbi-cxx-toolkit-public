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
 * Authors:  Josh Cherry
 *
 * File Description:  Data loader for user features in tabular form
 *
 */

#ifndef OBJTOOLS_DATA_LOADERS_TABLE___USER_FEATURE_DLOAD__HPP
#define OBJTOOLS_DATA_LOADERS_TABLE___USER_FEATURE_DLOAD__HPP

#include <objmgr/data_loader.hpp>
#include <objtools/data_loaders/table/sqlite_table.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_annot.hpp>

BEGIN_NCBI_SCOPE

// Parameter names used by loader factory

// Mandatory arguments
const string kCFParam_UsrFeat_InputFile  = "InputFile";  // = string
const string kCFParam_UsrFeat_TempFile   = "TempFile";   // = string
const string kCFParam_UsrFeat_DeleteFile = "DeleteFile"; // = bool ("1" = true)
const string kCFParam_UsrFeat_Offset     = "Offset";     // = int (0/1)

// Optional arguments
const string kCFParam_UsrFeat_Type       = "Type";       // = string
const string kCFParam_UsrFeat_GivenId    = "GivenId";    // = fasta id string


//
// class CFeatureDataLoader interprets a text-file database 
// as a set of features
//
class NCBI_XLOADER_TABLE_EXPORT CUsrFeatDataLoader
    : public objects::CDataLoader
{
public:
    /// Whether file uses zero-based or one-based numbering
    //  MUST equal 0 and 1
    typedef enum {
        eBeginIsZero = 0,
        eBeginIsOne = 1
    } EOffset;

    struct SUsrFeatParam
    {
        SUsrFeatParam(const string&           input_file,
                      const string&           temp_file,
                      bool                    delete_file,
                      EOffset                 offset,
                      const string&           type = string(),
                      const objects::CSeq_id* given_id = 0)
            : m_InputFile(input_file),
              m_TempFile(temp_file),
              m_DeleteFile(delete_file),
              m_Offset(offset),
              m_Type(type),
              m_GivenId(given_id) {}
        string                  m_InputFile;
        string                  m_TempFile;
        bool                    m_DeleteFile;
        EOffset                 m_Offset;
        string                  m_Type;
        const objects::CSeq_id* m_GivenId;
    };

    typedef objects::SRegisterLoaderInfo<CUsrFeatDataLoader>
        TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        objects::CObjectManager& om,
        const string& input_file,
        const string& temp_file,
        bool delete_file,
        EOffset offset,
        const string& type = string(),
        const objects::CSeq_id* given_id = 0,
        objects::CObjectManager::EIsDefault is_default =
        objects::CObjectManager::eNonDefault,
        objects::CObjectManager::TPriority priority =
        objects::CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const SUsrFeatParam& param);

    // Request features from our database corresponding to a given
    // CSeq_id_Handle
    virtual TTSE_LockSet GetRecords(const objects::CSeq_id_Handle& handle,
                                    EChoice choice);
    // Request an annot by CSeq_id_Handle
    CRef<objects::CSeq_annot> GetAnnot(const objects::CSeq_id_Handle& idh);
private:
    typedef objects::CParamLoaderMaker<CUsrFeatDataLoader,
                                       SUsrFeatParam> TMaker;
    friend class objects::CParamLoaderMaker<CUsrFeatDataLoader, SUsrFeatParam>;

    CUsrFeatDataLoader(const string& loader_name,
                       const SUsrFeatParam& param);

    enum {
        eUnknown = -1,
        eAccession = 0,
        eStrand,
        eFrom,
        eTo,
        eType,

        eMaxKnownCol
    };

    // Whether file uses zero-based or one-based numbering
    EOffset m_Offset;

    // The 'type' field of the user feature; if this string
    // is non-empty, it is used, even if there's a 'type'
    // column in the file
    string m_Type;

    // mapping of column idx -> meaning
    vector<int> m_ColAssign;

    // mapping of col meaning -> column idx
    int m_ColIdx[eMaxKnownCol];

    // a seq. id, if it's not coming from the file
    CConstRef<objects::CSeq_id> m_SeqId;

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


extern NCBI_XLOADER_TABLE_EXPORT const string kDataLoader_UsrFeat_DriverName;

extern "C"
{

void NCBI_XLOADER_TABLE_EXPORT NCBI_EntryPoint_DataLoader_UsrFeat(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

inline 
void NCBI_XLOADER_TABLE_EXPORT
NCBI_EntryPoint_xloader_usrfeat(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_UsrFeat(info_list, method);
}

} // extern C


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/08/04 19:35:09  grichenk
 * Renamed entry points to be found by dll resolver.
 * GB loader uses CPluginManagerStore to get/put
 * plugin manager for readers.
 *
 * Revision 1.7  2004/08/04 14:56:34  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.6  2004/08/02 17:34:43  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.5  2004/07/28 15:31:44  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.4  2004/07/26 14:13:31  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.3  2004/07/21 17:45:45  grichenk
 * Added RegisterInObjectManager() and GetLoaderNameFromArgs()
 *
 * Revision 1.2  2003/11/28 13:40:40  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.1  2003/11/14 19:09:16  jcherry
 * Initial version
 *
 * ===========================================================================
 */

#endif  // OBJTOOLS_DATA_LOADERS_TABLE___USER_FEATURE_DLOAD__HPP
