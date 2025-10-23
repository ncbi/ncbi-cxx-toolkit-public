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
 * Author: Christiam Camacho
 *
 */

/** @file blast_test_util.cpp
 * Utilities to develop and debug unit tests for BLAST
 */

#include <ncbi_pch.hpp>
#include "blast_test_util.hpp"
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbitype.h>
#include <util/random_gen.hpp>

// Serialization includes
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

// Object manager includes
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/data_loaders/blastdb/bdbloader_rmt.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/id2/reader_id2.hpp>

// Object includes
#include <objects/seqalign/Seq_align_set.hpp>

// Formatter includes
#include <objtools/align_format/showalign.hpp>

#include <sstream>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::align_format;

namespace TestUtil {

objects::CSeq_id* GenerateRandomSeqid_Gi() 
{
	static CRandom random_gen(static_cast<CRandom::TValue>(time(0)));
    return new CSeq_id(CSeq_id::e_Gi, random_gen.GetRand(1, 20000000));
}

CRef<CSeq_align_set>
FlattenSeqAlignSet(const CSeq_align_set& sset)
{
    CRef<CSeq_align_set> retval(new CSeq_align_set());

    ITERATE(CSeq_align_set::Tdata, i, sset.Get()) {
        ASSERT((*i)->GetSegs().IsDisc());

        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp_itr,
                (*i)->GetSegs().GetDisc().Get()) {
            retval->Set().push_back((*hsp_itr));
        }
    }

    return retval;
}

void PrintFormattedSeqAlign(ostream& out,
                            const CSeq_align_set* sas,
                            CScope& scope)
{
    ASSERT(sas);

    int align_opt = CDisplaySeqalign::eShowMiddleLine   |
                    CDisplaySeqalign::eShowBlastInfo    |
                    CDisplaySeqalign::eShowBlastStyleId;

    CRef<CSeq_align_set> saset(FlattenSeqAlignSet(*sas));

    CDisplaySeqalign formatter(*saset, scope);
    formatter.SetAlignOption(align_opt);
    formatter.DisplaySeqalign(out);
}

namespace {
    union SUnion14 {
        char end_bytes[4];
        Uint4 end_value;
    };
};

Uint4
EndianIndependentBufferHash(const char * buffer,
                            Uint4        byte_length,
                            Uint4        swap_size,
                            Uint4        hash_seed)
{
    Uint4 hash = hash_seed;
    Uint4 swap_mask = swap_size - 1;
    
    // Check that swapsize is a power of two.
    _ASSERT((swap_size) && (0 == (swap_mask & swap_size)));
    
    // Insure that the byte_length is a multiple of swap_size
    _ASSERT((byte_length & swap_mask) == 0);

    SUnion14 swap_test;
    swap_test.end_bytes[0] = 0x44;
    swap_test.end_bytes[1] = 0x33;
    swap_test.end_bytes[2] = 0x22;
    swap_test.end_bytes[3] = 0x11;
    Uint4 end_value = swap_test.end_value;

    if (end_value == 0x11223344) {
        // Prevent actual swapping on little endian machinery.
        swap_size = 1;
        swap_mask = 0;
    }
    
    Uint4 keep_mask = ~ swap_mask;
    
    // Logical address is the address if the data was little endian.
    
    for(Uint4 logical = 0; logical < byte_length; logical++) {
        Uint4 physical =
            (logical & keep_mask) | (swap_mask - (logical & swap_mask));
        
        // Alternate addition and XOR.  This technique destroys most
        // of the possible mathematical relationships between similar
        // input strings.
        
        if (logical & 1) {
            hash += int(buffer[physical]) & 0xFF;
        } else {
            hash ^= int(buffer[physical]) & 0xFF;
        }
        
        // 1. "Rotate" by a value relatively prime to 32 (any odd
        //    value), to insure that each input bit will eventually
        //    affect each position.
        // 2. Add a per-iteration constant to detect changes in length.
        
        hash = ((hash << 13) | (hash >> 19)) + 1234;
    }
    
    return hash;
}

CBlastOM::CBlastOM(const string& dbname, EDbType dbtype, ELocation location)
: m_ObjMgr(CObjectManager::GetInstance())
{
    x_InitBlastDatabaseDataLoader(dbname, dbtype, location);
    x_InitGenbankDataLoader();
}

void
CBlastOM::x_InitGenbankDataLoader()
{
    try {
        CRef<CReader> reader(new CId2Reader);
        reader->SetPreopenConnection(false);
        m_GbLoaderName = CGBDataLoader::RegisterInObjectManager
            (*m_ObjMgr, reader, CObjectManager::eNonDefault)
            .GetLoader()->GetName();
    } catch (const CException& e) {
        m_GbLoaderName.erase();
        ERR_POST(Warning << e.GetMsg());
    } catch (const std::bad_alloc& e) {
        m_GbLoaderName.erase();
        ERR_POST(Warning << "Failed to initialize GenBank data loader: " << e.what());
    } catch (...) {
        m_GbLoaderName.erase();
        ERR_POST(Warning << "Failed to initialize GenBank data loader: unknown error");
    }
}

void
CBlastOM::x_InitBlastDatabaseDataLoader(const string& dbname,
                                        EDbType dbtype,
                                        ELocation location)
{
    try {
        if (location == eLocal) {
            m_BlastDbLoaderName = CBlastDbDataLoader::RegisterInObjectManager
                (*m_ObjMgr, dbname, dbtype, true,
                 CObjectManager::eNonDefault,
                 CObjectManager::kPriority_NotSet).GetLoader()->GetName();
        } else {
            m_BlastDbLoaderName = CRemoteBlastDbDataLoader::RegisterInObjectManager
                (*m_ObjMgr, dbname, dbtype, true,
                 CObjectManager::eNonDefault,
                 CObjectManager::kPriority_NotSet).GetLoader()->GetName();
        }
    } catch (const CSeqDBException& e) {

        // if the database isn't found, ignore the exception as the Genbank
        // data loader will be the fallback (just issue a warning)

        if (e.GetMsg().find("No alias or index file found ") != NPOS) {
            ERR_POST(Warning << e.GetMsg());
        }

    }
}

CRef<CScope> CBlastOM::NewScope()
{
    CRef<CScope> retval(new CScope(*m_ObjMgr));

    if (!m_BlastDbLoaderName.empty()) {
        retval->AddDataLoader(m_BlastDbLoaderName, 1);
    } 
    if (!m_GbLoaderName.empty()) {
        retval->AddDataLoader(m_GbLoaderName, 2);
    }
    return retval;
}

void CBlastOM::RevokeBlastDbDataLoader()
{
    if (!m_BlastDbLoaderName.empty()) {
        CObjectManager::GetInstance()->RevokeDataLoader(m_BlastDbLoaderName);
    }
}

}

