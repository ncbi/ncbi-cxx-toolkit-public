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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Network cache daemon
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>

#include <util/transmissionrw.hpp>

#include "netcached.hpp"

BEGIN_NCBI_SCOPE


void CNetCacheServer::Process_IC_SetTimeStampPolicy(ICache&           ic,
                                                    CSocket&          sock, 
                                                    SIC_Request&      req,
                                                    SNC_ThreadData&   tdata)
{
    ic.SetTimeStampPolicy(req.i0, req.i1, req.i2);
    WriteMsg(sock, "OK:", "");
}


void CNetCacheServer::Process_IC_GetTimeStampPolicy(ICache&           ic,
                                                    CSocket&          sock, 
                                                    SIC_Request&      req,
                                                    SNC_ThreadData&   tdata)
{
    ICache::TTimeStampFlags flags = ic.GetTimeStampPolicy();
    string str;
    NStr::UIntToString(str, flags);
    WriteMsg(sock, "OK:", str);
}

void CNetCacheServer::Process_IC_SetVersionRetention(ICache&           ic,
                                                     CSocket&          sock, 
                                                     SIC_Request&      req,
                                                     SNC_ThreadData&   tdata)
{
    ICache::EKeepVersions policy;
    if (NStr::CompareNocase(req.key, "KA") == 0) {
        policy = ICache::eKeepAll;
    } else 
    if (NStr::CompareNocase(req.key, "DO") == 0) {
        policy = ICache::eDropOlder;
    } else 
    if (NStr::CompareNocase(req.key, "DA") == 0) {
        policy = ICache::eDropAll;
    } else {
        WriteMsg(sock, "ERR:", "Invalid version retention code");
    }
    ic.SetVersionRetention(policy);
    WriteMsg(sock, "OK:", "");
}

void CNetCacheServer::Process_IC_GetVersionRetention(ICache&           ic,
                                                     CSocket&          sock, 
                                                     SIC_Request&      req,
                                                     SNC_ThreadData&   tdata)
{
    int p = ic.GetVersionRetention();
    string str;
    NStr::IntToString(str, p);
    WriteMsg(sock, "OK:", str);
}

void CNetCacheServer::Process_IC_GetTimeout(ICache&           ic,
                                            CSocket&          sock, 
                                            SIC_Request&      req,
                                            SNC_ThreadData&   tdata)
{
    int to = ic.GetTimeout();
    string str;
    NStr::UIntToString(str, to);
    WriteMsg(sock, "OK:", str);
}

void CNetCacheServer::Process_IC_IsOpen(ICache&              ic,
                                        CSocket&              sock, 
                                        SIC_Request&          req,
                                        SNC_ThreadData&       tdata)
{
    bool io = ic.IsOpen();
    string str;
    NStr::UIntToString(str, (int)io);
    WriteMsg(sock, "OK:", str);
}


void CNetCacheServer::Process_IC_Store(ICache&              ic,
                                       CSocket&             sock, 
                                       SIC_Request&         req,
                                       SNC_ThreadData&      tdata)
{
    WriteMsg(sock, "OK:", "");

    auto_ptr<IWriter> iwrt;

    char* buf = tdata.buffer.get();
    size_t buf_size = GetTLS_Size();
    bool not_eof;

    CSocketReaderWriter  comm_reader(&sock, eNoOwnership);
    CTransmissionReader  transm_reader(&comm_reader, eNoOwnership);

/*ERR_POST("   STORE:" << req.key << " " << req.version << " " << req.subkey
         );*/

    do {
        size_t nn_read;

        CStopWatch  sw(CStopWatch::eStart);

        not_eof = ReadBuffer(sock, &transm_reader, buf, buf_size, &nn_read);

        if (nn_read == 0 && !not_eof) {
            ic.Store(req.key, req.version, req.subkey, 
                           buf, nn_read, req.i0, tdata.auth);
            break;
        }

        //stat.comm_elapsed += sw.Elapsed();
        //stat.blob_size += nn_read;

        if (nn_read) {
            if (iwrt.get() == 0) { // first read

                if (not_eof == false) { // complete read
                    ic.Store(req.key, req.version, req.subkey, 
                                   buf, nn_read, req.i0, tdata.auth);
                    return;
                }

                iwrt.reset(
                    ic.GetWriteStream(req.key, req.version, req.subkey,
                                            req.i0, tdata.auth));
            }
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, nn_read, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "ERR:", "Server I/O error");
                //x_RegisterInternalErr(req.req_type, tdata.auth);
                return;
            }
        } // if (nn_read)

    } while (not_eof);

    if (iwrt.get()) {
        iwrt->Flush();
        iwrt.reset(0);
    }
}

void CNetCacheServer::Process_IC_StoreBlob(ICache&              ic,
                                           CSocket&             sock, 
                                           SIC_Request&         req,
                                           SNC_ThreadData&      tdata)
{
    WriteMsg(sock, "OK:", "");

    auto_ptr<IWriter> iwrt;

    char* buf = tdata.buffer.get();
    size_t buf_size = GetTLS_Size();
    bool not_eof;

    CSocketReaderWriter  comm_reader(&sock, eNoOwnership);
    CTransmissionReader  transm_reader(&comm_reader, eNoOwnership);

    size_t blob_size = req.i1;
/*
ERR_POST("   STORE SIZE:" << req.key << " " << req.version << " " << req.subkey << 
         " size = " << blob_size
         );
*/
    if (blob_size == 0) {
        ic.Store(req.key, req.version, req.subkey, buf,
                 0, req.i0, tdata.auth);
        return;
    }

    do {
        size_t nn_read;

        CStopWatch  sw(CStopWatch::eStart);

        not_eof = 
            ReadBuffer(sock, 
                       &transm_reader, 
                       buf, buf_size, &nn_read, blob_size);

        if (nn_read == 0 && !not_eof) {
            ic.Store(req.key, req.version, req.subkey, 
                           buf, nn_read, req.i0, tdata.auth);
            break;
        }

        blob_size -= nn_read;

        //stat.comm_elapsed += sw.Elapsed();
        //stat.blob_size += nn_read;

        if (nn_read) {
            if (iwrt.get() == 0) { // first read

                if (not_eof == false || (blob_size == 0)) { // complete read
                    ic.Store(req.key, req.version, req.subkey, 
                                   buf, nn_read, req.i0, tdata.auth);
                    return;
                }

                iwrt.reset(
                    ic.GetWriteStream(req.key, req.version, req.subkey,
                                      req.i0, tdata.auth));
            }
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, nn_read, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "ERR:", "Server I/O error");
                //x_RegisterInternalErr(req.req_type, tdata.auth);
                return;
            }
        } // if (nn_read)

    } while (not_eof && (blob_size > 0));

    if (iwrt.get()) {
        iwrt->Flush();
        iwrt.reset(0);
    }
}


void CNetCacheServer::Process_IC_GetSize(ICache&              ic,
                                         CSocket&             sock,
                                         SIC_Request&         req,
                                         SNC_ThreadData&      tdata)
{
    size_t sz = ic.GetSize(req.key, req.version, req.subkey);
    string str;
    NStr::UIntToString(str, sz);
    WriteMsg(sock, "OK:", str);
}

void CNetCacheServer::Process_IC_GetBlobOwner(ICache&              ic,
                                              CSocket&             sock, 
                                              SIC_Request&         req,
                                              SNC_ThreadData&      tdata)
{
    string owner;
    ic.GetBlobOwner(req.key, req.version, req.subkey, &owner);
    WriteMsg(sock, "OK:", owner);
}

void CNetCacheServer::Process_IC_Read(ICache&              ic,
                                      CSocket&             sock, 
                                      SIC_Request&         req,
                                      SNC_ThreadData&      tdata)
{
    char* buf = tdata.buffer.get();

    ICache::SBlobAccessDescr ba_descr;
    buf += 100;
    ba_descr.buf = buf;
    ba_descr.buf_size = GetTLS_Size() - 100;
    ic.GetBlobAccess(req.key, req.version, req.subkey, &ba_descr);

    if (!ba_descr.blob_found) {
blob_not_found:
            string msg = "BLOB not found. ";
            //msg += req_id;
            WriteMsg(sock, "ERR:", msg);
            return;
    }
/*
ERR_POST("   FOUND:" << req.key << " " << req.version << " " << req.subkey << 
         " size = " << ba_descr.blob_size
         );
*/
    if (ba_descr.blob_size == 0) {
        WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        return;
    }

/*
    if (ba_descr.blob_size == 0) { // not found
        if (ba_descr.reader == 0) {
blob_not_found:
            string msg = "BLOB not found. ";
            msg += req_id;
            WriteMsg(sock, "ERR:", msg);
        } else {
            WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        }
        x_RegisterNoBlobErr(req.req_type, tdata.auth);
        return;
    }
*/
//    stat.blob_size = ba_descr.blob_size;

    if (ba_descr.reader.get() == 0) {  // all in buffer
        string msg("OK:BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, ba_descr.blob_size);
        msg += sz;

        const char* msg_begin = msg.c_str();
        const char* msg_end = msg_begin + msg.length();

        for (; msg_end >= msg_begin; --msg_end) {
            --buf;
            *buf = *msg_end;
            ++ba_descr.blob_size;
        }

        // translate BLOB fragment to the network
        CStopWatch  sw(CStopWatch::eStart);

        x_WriteBuf(sock, buf, ba_descr.blob_size);

//        stat.comm_elapsed += sw.Elapsed();
//        ++stat.io_blocks;

        return;

    } // inline BLOB


    // re-translate reader to the network

    auto_ptr<IReader> rdr(ba_descr.reader.release());
    if (!rdr.get()) {
        goto blob_not_found;
    }
    size_t blob_size = ba_descr.blob_size;

    bool read_flag = false;
    
    buf = tdata.buffer.get();

    size_t bytes_read;
    do {
        ERW_Result io_res = rdr->Read(buf, GetTLS_Size(), &bytes_read);
        if (io_res == eRW_Success && bytes_read) {
            if (!read_flag) {
                read_flag = true;
                string msg("BLOB found. SIZE=");
                string sz;
                NStr::UIntToString(sz, blob_size);
                msg += sz;
                WriteMsg(sock, "OK:", msg);
            }

            // translate BLOB fragment to the network
            CStopWatch  sw(CStopWatch::eStart);

            x_WriteBuf(sock, buf, bytes_read);

//            stat.comm_elapsed += sw.Elapsed();
//            ++stat.io_blocks;

        } else {
            break;
        }
    } while(1);
    if (!read_flag) {
        goto blob_not_found;
    }

}

void CNetCacheServer::Process_IC_Remove(ICache&              ic,
                                        CSocket&             sock, 
                                        SIC_Request&         req,
                                        SNC_ThreadData&      tdata)
{
    ic.Remove(req.key, req.version, req.subkey);
    WriteMsg(sock, "OK:", "");
}

void CNetCacheServer::Process_IC_RemoveKey(ICache&              ic,
                                           CSocket&             sock, 
                                           SIC_Request&         req,
                                           SNC_ThreadData&      tdata)
{
    ic.Remove(req.key);
    WriteMsg(sock, "OK:", "");
}


void CNetCacheServer::Process_IC_GetAccessTime(ICache&              ic,
                                               CSocket&             sock, 
                                               SIC_Request&         req,
                                               SNC_ThreadData&      tdata)
{
    time_t t = ic.GetAccessTime(req.key, req.version, req.subkey);
    string str;
    NStr::UIntToString(str, t);
    WriteMsg(sock, "OK:", str);
}


void CNetCacheServer::Process_IC_HasBlobs(ICache&              ic,
                                          CSocket&             sock, 
                                          SIC_Request&         req,
                                          SNC_ThreadData&      tdata)
{
    bool hb = ic.HasBlobs(req.key, req.subkey);
    string str;
    NStr::UIntToString(str, (int)hb);
    WriteMsg(sock, "OK:", str);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2006/01/12 19:31:16  kuznets
 * Commented out some debugging prints
 *
 * Revision 1.5  2006/01/11 15:26:26  kuznets
 * Reflecting changes in ICache
 *
 * Revision 1.4  2006/01/10 14:36:27  kuznets
 * Fixing bugs in ICache network protocol
 *
 * Revision 1.3  2006/01/06 01:58:59  ucko
 * Drop spurious semicolon after BEGIN_NCBI_SCOPE, as some versions of
 * WorkShop object to the resulting empty statement.
 *
 * Revision 1.2  2006/01/04 19:09:15  kuznets
 * Fixed bugs in ICache use
 *
 * Revision 1.1  2006/01/03 15:42:17  kuznets
 * Added support for network ICache interface
 *
 *
 * ===========================================================================
 */
