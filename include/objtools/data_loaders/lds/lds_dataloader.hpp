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
 * File Description: Object manager compliant data loader for local data
 *                   storage (LDS).
 *
 */

#ifndef LDS_DATALOADER_HPP
#define LDS_DATALOADER_HPP

#include <objmgr/data_loader.hpp>

#include <objtools/lds/lds_set.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CLDS_Database;

//////////////////////////////////////////////////////////////////
//
// CLDS_DataLoader.
// CDataLoader implementation for LDS.
//

class NCBI_XLOADER_LDS_EXPORT CLDS_DataLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CLDS_DataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);

    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CLDS_Database& lds_db,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(CLDS_Database& lds_db);

    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& db_path,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const string& db_path);

    // Public constructor not to break CSimpleClassFactoryImpl code
    CLDS_DataLoader(void);

    virtual ~CLDS_DataLoader(void);

    virtual void GetRecords(const CSeq_id_Handle& idh,
                            EChoice choice);
    
    virtual void DropTSE(const CTSE_Info& tse_info);

    void SetDatabase(CLDS_Database& lds_db,
                     const string&  dl_name);

private:
    CLDS_DataLoader(const string& dl_name);

    // Construct dataloader, attach the external LDS database
    CLDS_DataLoader(const string& dl_name,
                    CLDS_Database& lds_db);

    // Construct dataloader, with opening its own database
    CLDS_DataLoader(const string& dl_name,
                    const string& db_path);

    CLDS_Database*      m_LDS_db;        // Reference on the LDS database 
    CLDS_Set            m_LoadedObjects; // Set of already loaded objects
    bool                m_OwnDatabase;   // "TRUE" if datalaoder owns m_LDS_db
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2004/07/26 14:13:31  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.10  2004/07/21 15:51:23  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.9  2003/12/16 20:49:17  vasilche
 * Fixed compile errors - added missing includes and declarations.
 *
 * Revision 1.8  2003/12/16 20:17:25  kuznets
 * Added empty constructor prototype
 *
 * Revision 1.7  2003/11/28 13:40:40  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.6  2003/10/28 14:00:13  kuznets
 * Added constructor parameter name of the dataloader
 *
 * Revision 1.5  2003/09/30 16:36:36  vasilche
 * Updated CDataLoader interface.
 *
 * Revision 1.4  2003/07/30 16:35:38  kuznets
 * Fixed export macro NCBI_XLOADER_LDS_EXPORT
 *
 * Revision 1.3  2003/06/18 18:47:53  kuznets
 * Minor change: LDS Dataloader can now own the LDS database.
 *
 * Revision 1.2  2003/06/18 15:04:05  kuznets
 * Added dll export/import specs.
 *
 * Revision 1.1  2003/06/16 15:47:53  kuznets
 * Initial revision
 *
 *
 */

#endif

