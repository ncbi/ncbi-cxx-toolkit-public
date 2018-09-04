#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_add_app(
  test_ncbi_buffer test_ncbi_core test_ncbi_socket test_ncbi_dsock
  test_ncbi_connutil_hit test_ncbi_connutil_misc socket_io_bouncer
  test_ncbi_socket_connector test_ncbi_file_connector
  test_ncbi_http_connector http_connector_hit test_ncbi_heapmgr
  test_ncbi_service_connector test_ncbi_sendmail test_ncbi_disp
  test_ncbi_http_get test_ncbi_memory_connector test_fw
  test_ncbi_ftp_connector test_ncbi_download test_ncbi_server_info
  test_ncbi_service test_ncbi_conn_stream test_conn_stream_pushback
  test_ncbi_conn test_ncbi_rate_monitor test_ncbi_hmac
  test_ncbi_namedpipe test_ncbi_namedpipe_connector
  test_ncbi_pipe test_ncbi_pipe_connector test_ncbi_trigger
  test_ncbi_ftp_download test_conn_tar test_ncbi_null
  test_server test_threaded_server test_threaded_client
  test_ncbi_conn_stream_mt test_ncbi_http_upload
  test_ncbi_linkerd test_ncbi_linkerd_cxx test_ncbi_linkerd_mt
  test_ncbi_linkerd_proxy
  test_ncbi_namerd test_ncbi_namerd_mt
  test_server_listeners test_ncbi_ipv6 test_ncbi_iprange
  test_ncbi_service_cxx_mt
)

if(OFF)
# Include projects from this directory
include(CMakeLists.test_ncbi_buffer.app.txt)
include(CMakeLists.test_ncbi_core.app.txt)
include(CMakeLists.test_ncbi_socket.app.txt)
include(CMakeLists.test_ncbi_dsock.app.txt)
include(CMakeLists.test_ncbi_connutil_hit.app.txt)
include(CMakeLists.test_ncbi_connutil_misc.app.txt)
include(CMakeLists.socket_io_bouncer.app.txt)
include(CMakeLists.test_ncbi_socket_connector.app.txt)
include(CMakeLists.test_ncbi_file_connector.app.txt)
include(CMakeLists.test_ncbi_http_connector.app.txt)
include(CMakeLists.http_connector_hit.app.txt)
include(CMakeLists.test_ncbi_heapmgr.app.txt)
include(CMakeLists.test_ncbi_service_connector.app.txt)
include(CMakeLists.test_ncbi_sendmail.app.txt)
include(CMakeLists.test_ncbi_disp.app.txt)
include(CMakeLists.test_ncbi_http_get.app.txt)
include(CMakeLists.test_ncbi_memory_connector.app.txt)
include(CMakeLists.test_fw.app.txt)
include(CMakeLists.test_ncbi_ftp_connector.app.txt)
include(CMakeLists.test_ncbi_download.app.txt)
include(CMakeLists.test_ncbi_service.app.txt)
include(CMakeLists.test_ncbi_conn_stream.app.txt)
include(CMakeLists.test_conn_stream_pushback.app.txt)
include(CMakeLists.test_ncbi_conn.app.txt)
include(CMakeLists.test_ncbi_rate_monitor.app.txt)
include(CMakeLists.test_ncbi_hmac.app.txt)
include(CMakeLists.test_ncbi_namedpipe.app.txt)
include(CMakeLists.test_ncbi_namedpipe_connector.app.txt)
include(CMakeLists.test_ncbi_pipe.app.txt)
include(CMakeLists.test_ncbi_pipe_connector.app.txt)
include(CMakeLists.test_ncbi_trigger.app.txt)
include(CMakeLists.test_ncbi_ftp_download.app.txt)
include(CMakeLists.test_conn_tar.app.txt)
include(CMakeLists.test_ncbi_null.app.txt)
include(CMakeLists.test_server.app.txt)
include(CMakeLists.test_threaded_server.app.txt)
include(CMakeLists.test_threaded_client.app.txt)
include(CMakeLists.test_ncbi_conn_stream_mt.app.txt)
include(CMakeLists.test_ncbi_lbos.app.txt)
include(CMakeLists.test_ncbi_lbos_mt.app.txt)
include(CMakeLists.test_ncbi_linkerd.app.txt)
include(CMakeLists.test_ncbi_linkerd_cxx.app.txt)
include(CMakeLists.test_ncbi_linkerd_mt.app.txt)
include(CMakeLists.test_ncbi_linkerd_proxy.app.txt)
include(CMakeLists.test_ncbi_namerd.app.txt)
include(CMakeLists.test_ncbi_namerd_mt.app.txt)
include(CMakeLists.test_ncbi_http_upload.app.txt)
include(CMakeLists.test_ncbi_service_cxx_mt.app.txt)
endif()
