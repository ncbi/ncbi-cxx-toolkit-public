#ifndef OBJTOOLS_DATA_LOADERS_CDD___CDD__HPP
#define OBJTOOLS_DATA_LOADERS_CDD___CDD__HPP

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
 * Author: Philip Johnson, Mike DiCuccio
 *
 * File Description: CDD consensus sequence data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/data_loader.hpp>

#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//
// class CCddDataLoader is used to retrieve consensus sequences from
// the CDD server
//

class NCBI_XLOADER_CDD_EXPORT CCddDataLoader : public CDataLoader
{
public:
    static CCddDataLoader* RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);

    // Public constructor not to break CSimpleClassFactoryImpl code
    CCddDataLoader(void);

    void GetRecords(const CSeq_id_Handle& idh, EChoice choice);

private:
    CCddDataLoader(const string& loader_name);

    // cached map of resolved consensus sequences
    typedef map<int, CRef<CSeq_entry> > TCddEntries;
    TCddEntries m_Entries;

    // mutex guarding input into the map
    CMutex m_Mutex;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/07/21 15:51:23  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.2  2003/11/28 13:14:38  dicuccio
 * Dropped const from EChoice to match base class API
 *
 * Revision 1.1  2003/10/20 17:48:05  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // OBJTOOLS_DATA_LOADERS_CDD___CDD__HPP
