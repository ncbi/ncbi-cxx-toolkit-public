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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <sys/stat.h>
#include <util/lmdbxx/lmdb++.h>

#include "objtools/pubseq_gateway/cache/psg_cache_base.hpp"


USING_NCBI_SCOPE;


#define MAP_SIZE_INIT   (256L * 1024 * 1024 * 1024)
#define MAP_SIZE_DELTA  (16L * 1024 * 1024 * 1024)
#define DBI_COUNT       6
#define THREAD_COUNT    1024

CPubseqGatewayCacheBase::CPubseqGatewayCacheBase(const string& file_name) :
    m_FileName(file_name)
{
    m_Env.reset(new lmdb::env(lmdb::env::create()));
}

CPubseqGatewayCacheBase::~CPubseqGatewayCacheBase()
{
}

void CPubseqGatewayCacheBase::Open()
{
    struct stat st;
    int stat_rv = stat(m_FileName.c_str(), &st);
    if (stat_rv < 0)
        lmdb::runtime_error::raise(strerror(errno), errno);
    
    int64_t mapsize = MAP_SIZE_INIT;
    if (st.st_size + MAP_SIZE_DELTA >  mapsize)
        mapsize = st.st_size + MAP_SIZE_DELTA;
        
    m_Env->set_max_dbs(DBI_COUNT);
    m_Env->set_max_readers(THREAD_COUNT);
    m_Env->set_mapsize(mapsize);
    m_Env->open(m_FileName.c_str(), MDB_RDONLY | MDB_NOSUBDIR | MDB_NOSYNC | MDB_NOMETASYNC, 0664);
}
