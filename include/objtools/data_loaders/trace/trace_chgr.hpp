#ifndef OBJTOOLS_DATA_LOADERS_TRACE___TRACE_CHGR__HPP
#define OBJTOOLS_DATA_LOADERS_TRACE___TRACE_CHGR__HPP

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


#include <corelib/ncbimtx.hpp>
#include <objmgr/data_loader.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CID1Client;

class NCBI_XLOADER_TRACE_EXPORT CTraceChromatogramLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CTraceChromatogramLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);

    // Public constructor not to break CSimpleClassFactoryImpl code
    CTraceChromatogramLoader();

    ~CTraceChromatogramLoader();

    TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice);

private:
    typedef CSimpleLoaderMaker<CTraceChromatogramLoader> TMaker;
    friend class CSimpleLoaderMaker<CTraceChromatogramLoader>;

    CTraceChromatogramLoader(const string& loader_name);

    // cached map of resolved chromatograms
    // here, the int is the trace ID, or 'ti'
    typedef map<int, CRef<CSeq_entry> > TTraceEntries;
    TTraceEntries m_Entries;

    // client for data retrieval
    CRef<CID1Client> m_Client;

    // mutex guarding input into the map
    CMutex m_Mutex;


    // retrieve our ID1 client
    CID1Client& x_GetClient();

    // forbidden
    CTraceChromatogramLoader(CTraceChromatogramLoader&);
    CTraceChromatogramLoader& operator=(CTraceChromatogramLoader&);
};


END_SCOPE(objects)


extern NCBI_XLOADER_TRACE_EXPORT const string kDataLoader_Trace_DriverName;

extern "C"
{

NCBI_XLOADER_TRACE_EXPORT
void NCBI_EntryPoint_DataLoader_Trace(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_TRACE_EXPORT
void NCBI_EntryPoint_xloader_trace(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/08/10 16:56:11  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.7  2004/08/04 19:35:09  grichenk
 * Renamed entry points to be found by dll resolver.
 * GB loader uses CPluginManagerStore to get/put
 * plugin manager for readers.
 *
 * Revision 1.6  2004/08/04 14:56:35  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.5  2004/08/02 17:34:43  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.4  2004/07/28 14:02:57  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.3  2004/07/26 14:13:31  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.2  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.1  2004/03/25 14:19:59  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // OBJTOOLS_DATA_LOADERS_TRACE___TRACE_CHGR__HPP
