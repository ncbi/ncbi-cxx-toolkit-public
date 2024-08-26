#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_CDD__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_CDD__HPP

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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: PSG loader support code for CDD entries
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/impl/tse_lock.hpp>
#include <objects/seq/seq_id_handle.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;

class CObjectIStream;
class CPSG_BioId;
class CPSG_BlobInfo;
class CPSG_BlobData;

BEGIN_NAMESPACE(objects);

class CPsgBlobId;
class CDataSource;
class CTSE_Lock;
class CTSE_LoadLock;
class CTSE_Chunk_Info;

BEGIN_NAMESPACE(psgl);


struct SPsgBioseqInfo;
struct SPsgBlobInfo;
class CPSGBioseqCache;
class CPSGBlobMap;
class CPSGAnnotCache;
class CPSGCDDInfoCache;

/////////////////////////////////////////////////////////////////////////////
// common PSG loader/processors stuff
/////////////////////////////////////////////////////////////////////////////


extern bool s_GetBlobByIdShouldFail;
unsigned s_GetDebugLevel();
CSeq_id_Handle PsgIdToHandle(const CPSG_BioId& id);
void UpdateOMBlobId(CTSE_LoadLock& load_lock, const CConstRef<CPsgBlobId>& dl_blob_id);
CObjectIStream* GetBlobDataStream(const CPSG_BlobInfo& blob_info,
                                  const CPSG_BlobData& blob_data);


/////////////////////////////////////////////////////////////////////////////
// CDD stuff
/////////////////////////////////////////////////////////////////////////////


const char kCDDAnnotName[] = "CDD";
const bool kCreateLocalCDDEntries = true;
const char kLocalCDDEntryIdPrefix[] = "CDD:";
const char kLocalCDDEntryIdSeparator = '|';


struct SCDDIds
{
    CSeq_id_Handle gi;
    CSeq_id_Handle acc_ver;
    bool empty() const {
        return !gi; // TODO: && !acc_ver;
    }
    DECLARE_OPERATOR_BOOL(!empty());
};


SCDDIds x_GetCDDIds(const vector<CSeq_id_Handle>& ids);
bool x_IsLocalCDDEntryId(const CPsgBlobId& blob_id);
SCDDIds x_ParseLocalCDDEntryId(const CPsgBlobId& blob_id);
bool x_ParseLocalCDDEntryId(const CPsgBlobId& blob_id, SCDDIds& ids);
CTSE_Lock x_CreateLocalCDDEntry(CDataSource* data_source, const SCDDIds& ids);
string x_MakeLocalCDDEntryId(const SCDDIds& cdd_ids);
CPSG_BioId x_LocalCDDEntryIdToBioId(const SCDDIds& cdd_ids);
CPSG_BioId x_LocalCDDEntryIdToBioId(const CPsgBlobId& blob_id);
void x_CreateEmptyLocalCDDEntry(CDataSource* data_source, CTSE_Chunk_Info* chunk);
bool x_IsEmptyCDD(const CTSE_Info& tse);

END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_CDD__HPP
