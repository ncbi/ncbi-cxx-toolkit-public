# $Id$

# The connect library uses a split between a shared C toolkit library and an
# independent C++ toolkit library
# These variables capture the split

if (UNIX)
    set(lbsm_src ncbi_lbsm ncbi_lbsm_ipc)
endif()

set(SRC_MBED
    aes aesni arc4 aria asn1parse asn1write base64 bignum blowfish
    camellia ccm certs chacha20 chachapoly cipher cipher_wrap cmac
    constant_time ctr_drbg debug des dhm ecdh ecdsa ecjpake ecp
    ecp_curves entropy entropy_poll error gcm havege hkdf hmac_drbg
    mbedtls_md5 mbedtls_version md md2 md4 memory_buffer_alloc
    mps_reader mps_trace net_sockets nist_kw oid padlock pem pk
    pk_wrap pkcs11 pkcs12 pkcs5 pkparse pkwrite platform platform_util
    poly1305 psa_crypto psa_crypto_aead psa_crypto_cipher
    psa_crypto_client psa_crypto_driver_wrappers psa_crypto_ecp
    psa_crypto_hash psa_crypto_mac psa_crypto_rsa psa_crypto_se
    psa_crypto_slot_management psa_crypto_storage psa_its_file
    ripemd160 rsa rsa_internal sha1 sha256 sha512 ssl_cache
    ssl_ciphersuites ssl_cli ssl_cookie ssl_msg ssl_srv ssl_ticket
    ssl_tls ssl_tls13_keys threading timing version_features x509
    x509_create x509_crl x509_crt x509_csr x509write_crt x509write_csr
    xtea)
list(TRANSFORM SRC_MBED PREPEND "mbedtls/")
set(SRC_TLS ncbi_gnutls ncbi_mbedtls ncbi_tls ${SRC_MBED})

set(SRC_C
    ncbi_ansi_ext ncbi_types ncbi_core ncbi_priv ncbi_util
    ncbi_buffer ncbi_socket ncbi_connector ncbi_connection
    ncbi_connutil ncbi_ipv6 ncbi_server_info ncbi_host_info
    ncbi_service ncbi_sendmail ncbi_base64 ncbi_heapmgr ncbi_lb
    ncbi_socket_connector ncbi_file_connector ncbi_http_connector
    ncbi_memory_connector ncbi_service_connector ncbi_ftp_connector
    ncbi_version ncbi_iprange ncbi_local ncbi_lbsmd ncbi_dispd
    ncbi_linkerd ncbi_namerd parson
    ncbi_localip ncbi_lbdns
    ${lbsm_src}
    )

set(SRC_CXX
    ncbi_socket_cxx ncbi_core_cxx email_diag_handler
    ncbi_conn_streambuf ncbi_conn_stream ncbi_conn_test
    ncbi_misc ncbi_namedpipe ncbi_namedpipe_connector
    ncbi_pipe ncbi_pipe_connector ncbi_conn_reader_writer
    ncbi_userhost ncbi_http_session ncbi_monkey
    ncbi_service_cxx ncbi_localip_cxx ncbi_blowfish ncbi_usage_report
    connect_misc
    ${SRC_TLS}
    )

NCBI_disable_pch()
NCBI_add_library(connssl connect xxconnect xxconnect2 xconnect xthrserv)
NCBI_add_subdirectory(services ext test)
