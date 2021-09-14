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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for SRA data loader
*
* ===========================================================================
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <sra/data_loaders/bam/bamloader.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objtools/readers/idmapper.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <corelib/ncbi_system.hpp>
#include <thread>

#include <klib/rc.h>
#include <klib/writer.h>
#include <kfs/file.h>
#include <vfs/path.h>
#include <vfs/manager.h>
#ifdef _MSC_VER
# include <io.h>
#else
# include <unistd.h>
#endif

#include <corelib/ncbisys.hpp> // for NcbiSys_write
#include <stdio.h> // for perror

#include <corelib/test_boost.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

void CheckRc(rc_t rc, const char* code, const char* file, int line)
{
    if ( rc ) {
        char buffer1[4096];
        size_t error_len;
        RCExplain(rc, buffer1, sizeof(buffer1), &error_len);
        char buffer2[8192];
        unsigned len = sprintf(buffer2, "%s:%d: %s failed: %#x: %s\n",
                             file, line, code, rc, buffer1);
        const char* ptr = buffer2;
        while ( len ) {
            int written = NcbiSys_write(2, ptr, len);
            if ( written == -1 ) {
                perror("stderr write failed");
                exit(1);
            }
            len -= written;
            ptr += written;
        }
        exit(1);
    }
}
#define CALL(call) CheckRc((call), #call, __FILE__, __LINE__)


static string Hex(const vector<char>& v)
{
    CNcbiOstrstream s;
    const char* kHex = "0123456789abcdef";
    for ( size_t i = 0; i < v.size(); ++i ) {
        if ( i ) {
            if ( i % 16 == 0 ) {
                s << '\n';
            }
            else {
                s << ' ';
                if ( i % 4 == 0 )
                    s << ' ';
            }
        }
        char c = v[i];
        s << kHex[(c>>4)&15];
        s << kHex[(c)&15];
    }
    return CNcbiOstrstreamToString(s);
}

BOOST_AUTO_TEST_CASE(OpenMultipleBAMFiles)
{
    string urls[] = {
        "https://ftp.ncbi.nlm.nih.gov/toolbox/gbench/samples/udc_seqgraphic_rmt_testing/remote_BAM_remap_UUD-324/human/grch38_wgsim_gb_accs.bam",
        "https://ftp.ncbi.nlm.nih.gov/toolbox/gbench/samples/udc_seqgraphic_rmt_testing/remote_BAM_remap_UUD-324/human/grch38_wgsim_gb_accs.bam.bai",
        "https://ftp.ncbi.nlm.nih.gov/toolbox/gbench/samples/udc_seqgraphic_rmt_testing/remote_BAM_remap_UUD-324/yeast/yeast_wgsim_ucsc.bam",
        "https://ftp.ncbi.nlm.nih.gov/toolbox/gbench/samples/udc_seqgraphic_rmt_testing/remote_BAM_remap_UUD-324/yeast/yeast_wgsim_ucsc.bam.bai",
        "https://ftp.ncbi.nlm.nih.gov/toolbox/gbench/samples/udc_seqgraphic_rmt_testing/remote_BAM_remap_UUD-324/yeast/yeast_wgsim_gb_accs.bam",
        "https://ftp.ncbi.nlm.nih.gov/toolbox/gbench/samples/udc_seqgraphic_rmt_testing/remote_BAM_remap_UUD-324/yeast/yeast_wgsim_gb_accs.bam.bai",
    };
    const size_t URL_COUNT = ArraySize(urls);
    uint64_t sizes[URL_COUNT];
    uint32_t magics[URL_COUNT];
    const uint64_t kMaxDataSize = 1*1024*1024;
    vector< vector<char> > data(URL_COUNT);

    LOG_POST("Reading info of "<<URL_COUNT<<" urls");
    VFSManager* mgr = 0;
    CALL(VFSManagerMake(&mgr));
    for ( size_t i = 0; i < URL_COUNT; ++i ) {
        VPath* path = 0;
        CALL(VFSManagerMakePath(mgr, &path, urls[i].c_str()));
        const KFile* file = 0;
        CALL(VFSManagerOpenFileRead(mgr, &file, path));
        
        CALL(KFileSize(file, &sizes[i]));
        CALL(KFileReadExactly(file, 0, &magics[i], sizeof(magics[i])));
        uint64_t size = min(kMaxDataSize, sizes[i]);
        data[i].resize(size);
        CALL(KFileReadExactly(file, 0, &data[i][0], size));
        CALL(KFileRelease(file));
        CALL(VPathRelease(path));
        LOG_POST(urls[i]<<": "<<sizes[i]<<" "<<hex<<magics[i]<<dec);
        //LOG_POST("data: \n"<<Hex(data[i]);
    }
    const size_t NUM_PASSES = 4;
    const size_t NUM_THREADS = URL_COUNT;
    CAtomicCounter error_count;
    error_count.Set(0);
    bool single_thread = false;
    CMutex start_mutex;
    for ( size_t pass = 0; pass < NUM_PASSES; ++pass ) {
        CMutexGuard start_guard(eEmptyGuard);
        if ( single_thread ) {
            LOG_POST("Sequential reading pass "<<pass);
        }
        else {
            start_guard.Guard(start_mutex);
            LOG_POST("Parallel reading pass "<<pass);
        }
        vector<thread> tt(NUM_THREADS);
        for ( size_t i = 0; i < NUM_THREADS; ++i ) {
            size_t url_index = i%URL_COUNT;
            tt[i] =
                thread([&]
                       (const string& url, uint64_t exp_size, uint32_t exp_magic, const vector<char>& exp_data)
                       {
                           {
                               CMutexGuard start_guard(start_mutex);
                           }
                           VPath* path = 0;
                           CALL(VFSManagerMakePath(mgr, &path, url.c_str()));
                           
                           const KFile* file = 0;
                           CALL(VFSManagerOpenFileRead(mgr, &file, path));
                           uint64_t size = 0;
                           CALL(KFileSize(file, &size));
                           uint32_t magic = 0;
                           uint64_t data_size = min(kMaxDataSize, size);
                           vector<char> buffer(data_size);
                           CALL(KFileReadExactly(file, 0, &buffer[0], data_size));
                           memcpy(&magic, &buffer[0], sizeof(magic));
                           if ( size != exp_size ) {
                               error_count.Add(1);
                               ERR_POST(url<<": size "<<size<<" <> "<<exp_size);
                           }
                           if ( magic != exp_magic ) {
                               error_count.Add(1);
                               ERR_POST(url<<": size "<<hex<<magic<<" <> "<<exp_magic<<dec);
                           }
                           if ( buffer != exp_data ) {
                               error_count.Add(1);
                               ERR_POST(url<<": data is different:\n"<<Hex(buffer)<<" <> \n"<<Hex(exp_data));
                           }
                           CALL(KFileRelease(file));
                           CALL(VPathRelease(path));
                       }, urls[url_index], sizes[url_index], magics[url_index], data[url_index]);
            if ( single_thread ) {
                tt[i].join();
            }
        }
        if ( !single_thread ) {
            start_guard.Release();
            for ( size_t i = 0; i < NUM_THREADS; ++i ) {
                tt[i].join();
            }
        }
    }
    
    CALL(VFSManagerRelease(mgr));
    BOOST_CHECK_EQUAL(error_count.Get(), 0u);
}
