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

    printf(m_Opts.output_format == eJSON ?

        "\t\"server_host\": \"%s\",\n"
        "\t\"server_port\": %hu,\n"
        "\t\"id\": %u,\n"
        "\t\"key_generation_time\": \"%s\",\n"
        "\t\"random\": %u,\n" :

        "server_address: %s:%hu\n"
        "id: %u\n"
        "key_generation_time: %s\n"
        "random: %u\n",

        g_NetService_TryResolveHost(key.GetHost()).c_str(),
        key.GetPort(),
        key.GetId(),
        generation_time.AsString().c_str(),
        (unsigned) key.GetRandomPart());

    string service(key.GetServiceName());

    if (!service.empty())
        printf(m_Opts.output_format == eJSON ?
                "\t\"service_name\": \"%s\"\n" : "service_name: %s\n",
                service.c_str());
    else if (m_Opts.output_format == eJSON)
        printf("\t\"service_name\": null\n");
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

void CGridCommandLineInterfaceApp::PrintSelectedICacheServer()
{
    CNetServer selected_server(m_NetICacheClient.GetCurrentServer());
    printf("Server: %s:%hu\n",
        g_NetService_gethostnamebyaddr(selected_server.GetHost()).c_str(),
        selected_server.GetPort());
}

int CGridCommandLineInterfaceApp::Cmd_BlobInfo()
{
    bool icache_mode = IsOptionSet(eCache);

    if (icache_mode)
        ParseICacheKey();

    SetUp_NetCacheCmd(icache_mode ? eNetICacheClient : eNetCacheAPI);

    try {
        CNetServerMultilineCmdOutput output;

        if (!icache_mode) {
            PrintBlobMeta(CNetCacheKey(m_Opts.id, m_CompoundIDPool));

            output = m_NetCacheAPI.GetBlobInfo(m_Opts.id);
        } else {
            output = m_NetICacheClient.GetBlobInfo(m_Opts.icache_key.key,
                m_Opts.icache_key.version, m_Opts.icache_key.subkey);

            PrintSelectedICacheServer();
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

        if (!icache_mode)
            printf("Size: %lu\n", (unsigned long)
                m_NetCacheAPI.GetBlobSize(m_Opts.id));
        else {
            size_t blob_size = m_NetICacheClient.GetSize(m_Opts.icache_key.key,
                    m_Opts.icache_key.version, m_Opts.icache_key.subkey);

            PrintSelectedICacheServer();

            printf("Size: %lu\n", (unsigned long) blob_size);
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetBlob()
{
    bool icache_mode = IsOptionSet(eCache);

    SetUp_NetCacheCmd(icache_mode ? eNetICacheClient : eNetCacheAPI);

    int reader_select = IsOptionSet(ePassword, OPTION_N(1)) |
        (m_Opts.offset != 0 || m_Opts.size != 0 ? OPTION_N(0) : 0);

    auto_ptr<IReader> reader;

    if (!icache_mode) {
        size_t blob_size = 0;
        switch (reader_select) {
        case 0: /* no special case */
            reader.reset(m_NetCacheAPI.GetReader(
                m_Opts.id,
                &blob_size,
                nc_caching_mode = CNetCacheAPI::eCaching_Disable));
            break;
        case OPTION_N(0): /* use offset */
            reader.reset(m_NetCacheAPI.GetPartReader(
                m_Opts.id,
                m_Opts.offset,
                m_Opts.size,
                &blob_size,
                nc_caching_mode = CNetCacheAPI::eCaching_Disable));
            break;
        case OPTION_N(1): /* use password */
            reader.reset(m_NetCacheAPI.GetReader(
                    m_Opts.id,
                    &blob_size,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_blob_password = m_Opts.password)));
            break;
        case OPTION_N(1) | OPTION_N(0): /* use password and offset */
            reader.reset(m_NetCacheAPI.GetPartReader(
                    m_Opts.id,
                    m_Opts.offset,
                    m_Opts.size,
                    &blob_size,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_blob_password = m_Opts.password)));
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
                    nc_caching_mode = CNetCacheAPI::eCaching_Disable) :
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
                nc_caching_mode = CNetCacheAPI::eCaching_Disable));
            break;
        case OPTION_N(1): /* use password */
            reader.reset(CNetICachePasswordGuard(m_NetICacheClient,
                m_Opts.password)->GetReadStream(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    NULL,
                    nc_caching_mode = CNetCacheAPI::eCaching_Disable));
            break;
        case OPTION_N(1) | OPTION_N(0): /* use password and offset */
            reader.reset(CNetICachePasswordGuard(m_NetICacheClient,
                m_Opts.password)->GetReadStreamPart(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    m_Opts.offset,
                    m_Opts.size,
                    NULL,
                    nc_caching_mode = CNetCacheAPI::eCaching_Disable));
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
    bool icache_mode = IsOptionSet(eCache);

    SetUp_NetCacheCmd(icache_mode ? eNetICacheClient : eNetCacheAPI);

    auto_ptr<IEmbeddedStreamWriter> writer;

    string blob_key = m_Opts.id;

    if (!icache_mode) {
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
            CNetICachePasswordGuard(m_NetICacheClient,
                m_Opts.password)->GetNetCacheWriter(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    nc_blob_ttl = m_Opts.ttl) :
            m_NetICacheClient.GetNetCacheWriter(
                m_Opts.icache_key.key,
                m_Opts.icache_key.version,
                m_Opts.icache_key.subkey,
                nc_blob_ttl = m_Opts.ttl));
    }

    if (!writer.get()) {
        NCBI_USER_THROW("Cannot create blob stream");
    }

    if (icache_mode && m_NetICacheClient.GetService().IsLoadBalanced())
        PrintSelectedICacheServer();

    size_t bytes_written;

    if (IsOptionSet(eInput)) {
        if (writer->Write(m_Opts.input.data(), m_Opts.input.length(),
                &bytes_written) != eRW_Success ||
                bytes_written != m_Opts.input.length())
            goto ErrorExit;
    } else {
        char buffer[IO_BUFFER_SIZE];
        size_t bytes_read;

        while ((bytes_read = fread(buffer, 1,
                sizeof(buffer), m_Opts.input_stream)) > 0) {
            if (writer->Write(buffer, bytes_read, &bytes_written) !=
                    eRW_Success || bytes_written != bytes_read)
                goto ErrorExit;
            if (feof(m_Opts.input_stream))
                break;
        }
    }

    writer->Close();

    if (!icache_mode && m_Opts.id.empty())
        NcbiCout << blob_key << NcbiEndl;

    return 0;

ErrorExit:
    NCBI_USER_THROW("Error while writing to NetCache");
}

int CGridCommandLineInterfaceApp::Cmd_RemoveBlob()
{
    bool icache_mode = IsOptionSet(eCache);

    SetUp_NetCacheCmd(icache_mode ? eNetICacheClient : eNetCacheAPI);

    if (!icache_mode)
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
    SetUp_NetCacheCmd(eNetCacheAdmin, eSevereAdminCmd);

    m_NetCacheAdmin.Purge(m_Opts.cache_name);

    return 0;
}
