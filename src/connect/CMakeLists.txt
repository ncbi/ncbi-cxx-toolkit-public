# $Id$

# The connect library uses a split between a shared C toolkit library and an
# independent C++ toolkit library
# These variables capture the split

if (WIN32)
    set(lbsm_src ncbi_lbsmd_stub ncbi_strerror)
else()
    if(EXISTS ${NCBI_CURRENT_SOURCE_DIR}/ncbi_lbsm.c)
        set(lbsm_src ncbi_lbsm.c ncbi_lbsm_ipc.c ncbi_lbsmd.c)
    else(WIN32)
        set(lbsm_src ncbi_lbsmd_stub)
    endif()
endif()

set(SRC_TLS
    ncbi_gnutls ncbi_mbedtls ncbi_tls
    mbedtls/aes mbedtls/aesni mbedtls/arc4 mbedtls/aria
    mbedtls/asn1parse mbedtls/asn1write mbedtls/base64 mbedtls/bignum
    mbedtls/blowfish mbedtls/camellia mbedtls/ccm mbedtls/chacha20
    mbedtls/chachapoly mbedtls/cipher mbedtls/cipher_wrap
    mbedtls/cmac mbedtls/ctr_drbg mbedtls/des mbedtls/dhm mbedtls/ecdh
    mbedtls/ecdsa mbedtls/ecjpake mbedtls/ecp mbedtls/ecp_curves
    mbedtls/entropy mbedtls/entropy_poll mbedtls/error mbedtls/gcm
    mbedtls/havege mbedtls/hkdf mbedtls/hmac_drbg mbedtls/md
    mbedtls/md2 mbedtls/md4 mbedtls/mbedtls_md5 mbedtls/md_wrap
    mbedtls/memory_buffer_alloc mbedtls/nist_kw mbedtls/oid
    mbedtls/padlock mbedtls/pem mbedtls/pk mbedtls/pk_wrap
    mbedtls/pkcs12 mbedtls/pkcs5 mbedtls/pkparse mbedtls/pkwrite
    mbedtls/platform mbedtls/platform_util mbedtls/poly1305
    mbedtls/ripemd160 mbedtls/rsa mbedtls/rsa_internal mbedtls/sha1
    mbedtls/sha256 mbedtls/sha512 mbedtls/threading mbedtls/timing
    mbedtls/mbedtls_version mbedtls/version_features mbedtls/xtea
    mbedtls/certs mbedtls/pkcs11 mbedtls/x509 mbedtls/x509_create
    mbedtls/x509_crl mbedtls/x509_crt mbedtls/x509_csr
    mbedtls/x509write_crt mbedtls/x509write_csr
    mbedtls/debug mbedtls/net_sockets mbedtls/ssl_cache
    mbedtls/ssl_ciphersuites mbedtls/ssl_cli mbedtls/ssl_cookie
    mbedtls/ssl_srv mbedtls/ssl_ticket mbedtls/ssl_tls
    )

set(SRC_C
    ncbi_ansi_ext ncbi_types ncbi_core ncbi_priv ncbi_util
    ncbi_buffer ncbi_socket ncbi_connector ncbi_connection
    ncbi_connutil ncbi_ipv6 ncbi_server_info ncbi_host_info
    ncbi_service ncbi_sendmail ncbi_base64 ncbi_heapmgr ncbi_lb
    ncbi_socket_connector ncbi_file_connector ncbi_http_connector
    ncbi_memory_connector ncbi_service_connector ncbi_ftp_connector
    ncbi_version ncbi_iprange ncbi_local ncbi_dispd
    ncbi_lbos ncbi_linkerd ncbi_namerd parson
    ncbi_localip ncbi_lbdns
    ${lbsm_src}
    )

set(SRC_CXX
    ncbi_socket_cxx ncbi_core_cxx email_diag_handler
    ncbi_conn_streambuf ncbi_conn_stream ncbi_conn_test
    ncbi_misc ncbi_namedpipe ncbi_namedpipe_connector
    ncbi_pipe ncbi_pipe_connector ncbi_conn_reader_writer
    ncbi_userhost ncbi_http_session ncbi_lbos_cxx ncbi_monkey
    ncbi_service_cxx ncbi_localip_cxx ncbi_blowfish ncbi_usage_report
    connect_misc
    ${SRC_TLS}
    )

NCBI_disable_pch()
NCBI_add_library(connect connssl xxconnect xxconnect2 xconnect xthrserv)
NCBI_add_subdirectory(services ext test daemons)

