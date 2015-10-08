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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: NetCache-specific commands of the grid_cli application.
 *
 */

#include <ncbi_pch.hpp>

#include "grid_cli.hpp"

USING_NCBI_SCOPE;

void CGridCommandLineInterfaceApp::SetUp_NetCacheCmd(
        CGridCommandLineInterfaceApp::EAPIClass api_class,
        CGridCommandLineInterfaceApp::EAdminCmdSeverity cmd_severity)
{
    m_APIClass = api_class;

    CNcbiRegistry& reg(CNcbiApplication::Instance()->GetConfig());

    if (IsOptionSet(eEnableMirroring))
        reg.Set("netcache_api", "enable_mirroring", "true");

    switch (api_class) {
    case eNetCacheAPI:
        m_NetCacheAPI = CNetCacheAPI(m_Opts.nc_service,
                m_Opts.auth, m_NetScheduleAPI);

        if (!m_Opts.id.empty() && IsOptionExplicitlySet(eNetCache)) {
            string host, port;

            if (!NStr::SplitInTwo(m_Opts.nc_service, ":", host, port)) {
                NCBI_THROW(CArgException, eInvalidArg,
                    "When blob ID is given, '--" NETCACHE_OPTION "' "
                    "must be a host:port server address.");
            }

            m_NetCacheAPI.GetService().GetServerPool().StickToServer(host,
                    (unsigned short) NStr::StringToInt(port));
        }

        m_NetCacheAPI.SetEventHandler(new CNetCacheWarningLogger);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (IsOptionSet(eAllowXSiteConn))
            m_NetCacheAPI.GetService().AllowXSiteConnections();
#endif
        break;

    case eNetICacheClient:
        m_NetICacheClient = CNetICacheClient(m_Opts.nc_service,
            m_Opts.cache_name, m_Opts.auth);

        if (m_Opts.nc_service.empty()) {
            NCBI_THROW(CArgException, eNoValue, "'--" NETCACHE_OPTION "' "
                "option is required in icache mode.");
        }

        m_NetICacheClient.SetEventHandler(new CNetCacheWarningLogger);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (IsOptionSet(eAllowXSiteConn))
            m_NetICacheClient.GetService().AllowXSiteConnections();
#endif

        if (!IsOptionSet(eCompatMode))
            m_NetICacheClient.SetFlags(ICache::fBestReliability);
        break;

    default: /* case eNetCacheAdmin: */
        if (cmd_severity != eReadOnlyAdminCmd &&
                !IsOptionExplicitlySet(eNetCache)) {
            NCBI_THROW(CArgException, eNoValue, "'--" NETCACHE_OPTION "' "
                "must be explicitly specified.");
        }
        m_NetCacheAPI = CNetCacheAPI(m_Opts.nc_service, m_Opts.auth);
        m_NetCacheAdmin = m_NetCacheAPI.GetAdmin();

        m_NetCacheAPI.SetEventHandler(new CNetCacheWarningLogger);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (IsOptionSet(eAllowXSiteConn))
            m_NetCacheAPI.GetService().AllowXSiteConnections();
#endif
    }
}

void CGridCommandLineInterfaceApp::PrintBlobMeta(const CNetCacheKey& key)
{
    CTime generation_time;

    generation_time.SetTimeT((time_t) key.GetCreationTime());

    if (key.GetVersion() != 3)
        printf("server_address: %s:%hu\n",
            g_NetService_TryResolveHost(key.GetHost()).c_str(), key.GetPort());
    else
        printf("server_address_crc32: 0x%08X\n", key.GetHostPortCRC32());

    printf("id: %u\nkey_generation_time: %s\nrandom: %u\n",
        key.GetId(),
        generation_time.AsString().c_str(),
        (unsigned) key.GetRandomPart());

    string service(key.GetServiceName());

    if (!service.empty())
        printf("service_name: %s\n", service.c_str());
}

void CGridCommandLineInterfaceApp::ParseICacheKey(
    bool permit_empty_version, bool* version_is_defined)
{
    vector<string> key_parts;

    NStr::Tokenize(m_Opts.id, ",", key_parts);
    if (key_parts.size() != 3) {
        NCBI_THROW_FMT(CArgException, eInvalidArg,
            "Invalid ICache key specification \"" << m_Opts.id << "\" ("
            "expected a comma-separated key,version,subkey triplet).");
    }
    if (!key_parts[1].empty())
        m_Opts.icache_key.version = NStr::StringToInt(key_parts[1]);
    else if (permit_empty_version)
        *version_is_defined = false;
    else {
        NCBI_THROW(CArgException, eNoValue,
            "blob version parameter is missing");
    }
    m_Opts.icache_key.key = key_parts.front();
    m_Opts.icache_key.subkey = key_parts.back();
}

void CGridCommandLineInterfaceApp::PrintServerAddress(CNetServer server)
{
    printf("Server: %s:%hu\n",
        g_NetService_gethostnamebyaddr(server.GetHost()).c_str(),
        server.GetPort());
}

int CGridCommandLineInterfaceApp::Cmd_BlobInfo()
{
    SetUp_NetCacheCmd();

    if (m_APIClass != eNetCacheAPI)
        ParseICacheKey();

    try {
        CNetServerMultilineCmdOutput output;

        if (m_APIClass == eNetCacheAPI) {
            PrintBlobMeta(CNetCacheKey(m_Opts.id, m_CompoundIDPool));

            output = m_NetCacheAPI.GetBlobInfo(m_Opts.id,
                    nc_try_all_servers = IsOptionSet(eTryAllServers));
        } else {
            CNetServer server_last_used;

            output = m_NetICacheClient.GetBlobInfo(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    (nc_try_all_servers = IsOptionSet(eTryAllServers),
                    nc_server_last_used = &server_last_used));

            PrintServerAddress(server_last_used);
        }

        string line;

        if (output.ReadLine(line)) {
            if (!NStr::StartsWith(line, "SIZE="))
                printf("%s\n", line.c_str());
            while (output.ReadLine(line))
                printf("%s\n", line.c_str());
        }
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() != CNetCacheException::eServerError)
            throw;

        if (m_APIClass == eNetCacheAPI)
            printf("Size: %lu\n", (unsigned long)
                m_NetCacheAPI.GetBlobSize(m_Opts.id));
        else {
            CNetServer server_last_used;

            size_t blob_size = m_NetICacheClient.GetBlobSize(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    nc_server_last_used = &server_last_used);

            PrintServerAddress(server_last_used);

            printf("Size: %lu\n", (unsigned long) blob_size);
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetBlob()
{
    SetUp_NetCacheCmd();

    int reader_select = IsOptionSet(ePassword, OPTION_N(1)) |
        (m_Opts.offset != 0 || m_Opts.size != 0 ? OPTION_N(0) : 0);

    auto_ptr<IReader> reader;

    if (m_APIClass == eNetCacheAPI) {
        size_t blob_size = 0;
        switch (reader_select) {
        case 0: /* no special case */
            reader.reset(m_NetCacheAPI.GetReader(
                m_Opts.id,
                &blob_size,
                (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                nc_try_all_servers = IsOptionSet(eTryAllServers))));
            break;
        case OPTION_N(0): /* use offset */
            reader.reset(m_NetCacheAPI.GetPartReader(
                m_Opts.id,
                m_Opts.offset,
                m_Opts.size,
                &blob_size,
                (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                nc_try_all_servers = IsOptionSet(eTryAllServers))));
            break;
        case OPTION_N(1): /* use password */
            reader.reset(m_NetCacheAPI.GetReader(
                    m_Opts.id,
                    &blob_size,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_blob_password = m_Opts.password,
                    nc_try_all_servers = IsOptionSet(eTryAllServers))));
            break;
        case OPTION_N(1) | OPTION_N(0): /* use password and offset */
            reader.reset(m_NetCacheAPI.GetPartReader(
                    m_Opts.id,
                    m_Opts.offset,
                    m_Opts.size,
                    &blob_size,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_blob_password = m_Opts.password,
                    nc_try_all_servers = IsOptionSet(eTryAllServers))));
        }
    } else {
        bool version_is_defined = true;

        ParseICacheKey(reader_select == 0, &version_is_defined);

        ICache::EBlobVersionValidity validity;
        switch (reader_select) {
        case 0: /* no special case */
            reader.reset(version_is_defined ?
                m_NetICacheClient.GetReadStream(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    NULL,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_try_all_servers = IsOptionSet(eTryAllServers))) :
                m_NetICacheClient.GetReadStream(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.subkey,
                    &m_Opts.icache_key.version,
                    &validity));
            break;
        case OPTION_N(0): /* use offset */
            reader.reset(m_NetICacheClient.GetReadStreamPart(
                m_Opts.icache_key.key,
                m_Opts.icache_key.version,
                m_Opts.icache_key.subkey,
                m_Opts.offset,
                m_Opts.size,
                NULL,
                (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                nc_try_all_servers = IsOptionSet(eTryAllServers))));
            break;
        case OPTION_N(1): /* use password */
            reader.reset(m_NetICacheClient.GetReadStream(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    NULL,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_blob_password = m_Opts.password,
                    nc_try_all_servers = IsOptionSet(eTryAllServers))));
            break;
        case OPTION_N(1) | OPTION_N(0): /* use password and offset */
            reader.reset(m_NetICacheClient.GetReadStreamPart(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    m_Opts.offset,
                    m_Opts.size,
                    NULL,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_blob_password = m_Opts.password,
                    nc_try_all_servers = IsOptionSet(eTryAllServers))));
        }
        if (!version_is_defined)
            NcbiCerr << "Blob version: " <<
                    m_Opts.icache_key.version << NcbiEndl <<
                "Blob validity: " << (validity == ICache::eCurrent ?
                    "current" : "expired") << NcbiEndl;
    }
    if (!reader.get()) {
        NCBI_THROW(CNetCacheException, eBlobNotFound,
            "Cannot find the requested blob");
    }

    char buffer[IO_BUFFER_SIZE];
    ERW_Result read_result;
    size_t bytes_read;

    while ((read_result = reader->Read(buffer,
            sizeof(buffer), &bytes_read)) == eRW_Success)
        fwrite(buffer, 1, bytes_read, m_Opts.output_stream);

    if (read_result != eRW_Eof) {
        ERR_POST("Error while sending data to the output stream");
        return 1;
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_PutBlob()
{
    SetUp_NetCacheCmd();

    auto_ptr<IEmbeddedStreamWriter> writer;

    // Cannot use a reference here because m_Opts.id.empty() is
    // used later to find out whether a blob was given in the
    // command line.
    string blob_key = m_Opts.id;

    CNetServer server_last_used;

    if (m_APIClass == eNetCacheAPI) {
        switch (IsOptionSet(ePassword, 1) | IsOptionSet(eUseCompoundID, 2)) {
        case 1:
            writer.reset(m_NetCacheAPI.PutData(&blob_key,
                    (nc_blob_ttl = m_Opts.ttl,
                    nc_blob_password = m_Opts.password)));
            break;
        case 2:
            writer.reset(m_NetCacheAPI.PutData(&blob_key,
                    (nc_blob_ttl = m_Opts.ttl,
                    nc_use_compound_id = true)));
            break;
        case 3:
            writer.reset(m_NetCacheAPI.PutData(&blob_key,
                    (nc_blob_ttl = m_Opts.ttl,
                    nc_blob_password = m_Opts.password,
                    nc_use_compound_id = true)));
            break;
        default:
            writer.reset(m_NetCacheAPI.PutData(&blob_key,
                    nc_blob_ttl = m_Opts.ttl));
        }
    } else {
        ParseICacheKey();

        writer.reset(IsOptionSet(ePassword) ?
            m_NetICacheClient.GetNetCacheWriter(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    (nc_blob_ttl = m_Opts.ttl,
                    nc_blob_password = m_Opts.password,
                    nc_server_last_used = &server_last_used)) :
            m_NetICacheClient.GetNetCacheWriter(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    (nc_blob_ttl = m_Opts.ttl,
                    nc_server_last_used = &server_last_used)));
    }

    if (!writer.get()) {
        NCBI_USER_THROW("Cannot create blob stream");
    }

    if (m_APIClass != eNetCacheAPI &&
            m_NetICacheClient.GetService().IsLoadBalanced())
        PrintServerAddress(server_last_used);

    size_t bytes_written;

    if (IsOptionSet(eInput)) {
        if (writer->Write(m_Opts.input.data(), m_Opts.input.length(),
                &bytes_written) != eRW_Success ||
                bytes_written != m_Opts.input.length())
            goto ErrorExit;
    } else {
        char buffer[IO_BUFFER_SIZE];

        do {
            m_Opts.input_stream->read(buffer, sizeof(buffer));
            if (m_Opts.input_stream->fail() && !m_Opts.input_stream->eof()) {
                NCBI_USER_THROW("Error while reading from input stream");
            }
            size_t bytes_read = (size_t) m_Opts.input_stream->gcount();
            if (writer->Write(buffer, bytes_read, &bytes_written) !=
                    eRW_Success || bytes_written != bytes_read)
                goto ErrorExit;
        } while (!m_Opts.input_stream->eof());
    }

    writer->Close();

    if (m_APIClass == eNetCacheAPI && m_Opts.id.empty())
        NcbiCout << blob_key << NcbiEndl;

    return 0;

ErrorExit:
    NCBI_USER_THROW("Error while writing to NetCache");
}

int CGridCommandLineInterfaceApp::Cmd_RemoveBlob()
{
    SetUp_NetCacheCmd();

    if (m_APIClass == eNetCacheAPI)
        if (IsOptionSet(ePassword))
            m_NetCacheAPI.Remove(m_Opts.id, nc_blob_password = m_Opts.password);
        else
            m_NetCacheAPI.Remove(m_Opts.id);
    else {
        ParseICacheKey();

        if (IsOptionSet(ePassword))
            m_NetICacheClient.RemoveBlob(m_Opts.icache_key.key,
                    m_Opts.icache_key.version, m_Opts.icache_key.subkey,
                    nc_blob_password = m_Opts.password);
        else
            m_NetICacheClient.RemoveBlob(m_Opts.icache_key.key,
                m_Opts.icache_key.version, m_Opts.icache_key.subkey);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Purge()
{
    SetUp_NetCacheCmd(eNetCacheAdmin, eAdminCmdWithSideEffects);

    m_NetCacheAdmin.Purge(m_Opts.cache_name);

    return 0;
}
