#ifndef OBJTOOLS_DATA_LOADERS_SRA___SRALOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_SRA___SRALOADER_IMPL__HPP

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
 * Author: Eugene Vasilchenko
 *
 * File Description: SRA file data loader
 *
 * ===========================================================================
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <sra/data_loaders/sra/sraloader.hpp>
#include <sra/readers/sra/sraread.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;

class CSRADataLoader_Impl : public CObject
{
public:
    CSRADataLoader_Impl(CSraMgr::ETrim trim);
    ~CSRADataLoader_Impl(void);

    CSeq_inst::TMol GetSequenceType(const string& accession,
                                    unsigned spot_id,
                                    unsigned read_id);
    TSeqPos GetSequenceLength(const string& accession,
                              unsigned spot_id,
                              unsigned read_id);

    CRef<CSeq_entry> LoadSRAEntry(const string& accession,
                                  unsigned spot_id);

private:
    // mutex guarding input into the map
    CMutex m_Mutex;
    CSraMgr m_Mgr;
    CSraRun m_Run;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_SRA___SRALOADER_IMPL__HPP
