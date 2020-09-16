/* buf_data_loader.h
 *
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
 * File Name:  buf_data_loader.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------
 *
 */

#ifndef BUF_DATA_LOADER_H
#define BUF_DATA_LOADER_H

#include <objmgr/data_loader.hpp>

BEGIN_NCBI_SCOPE
struct Parser;
size_t CheckOutsideEntry(Parser* pp, const char* acc, Int2 vernum);

BEGIN_SCOPE(objects)

//////////////////////////////////////////////////////////////////
//
// CBuffer_DataLoader.
// CDataLoader implementation for buffer based ff2asn functionality.
//

// Parameter names used by loader factory

class CBuffer_DataLoader : public CDataLoader
{
public:

    CBuffer_DataLoader(const string& name, Parser* parser);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice);
    virtual bool CanGetBlobById() const;
    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    typedef SRegisterLoaderInfo<CBuffer_DataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(CObjectManager& om, Parser* params, CObjectManager::EIsDefault is_default, CObjectManager::TPriority priority);

protected:

private:
    void x_LoadData(const CSeq_id_Handle& idh, CTSE_LoadLock& lock);

    Parser* m_parser;

    static const string& GetLoaderNameFromArgs(Parser*);
    typedef CParamLoaderMaker<CBuffer_DataLoader, Parser*> TLoaderMaker;
    friend class CParamLoaderMaker<CBuffer_DataLoader, Parser*>;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // BUF_DATA_LOADER_H
