#!/bin/sh

WITH_VARS="libsxslt|DIR gnutls|DIR boost|DIR libxlsxwriter|DIR z|DIR bz2|dir lzo|DIR samtools|DIR glpk|DIR curl|dir sge|DIR
Python|DIR
opengl|DIR mesa|DIR glew|DIR odbc|DIR
mimetic|DIR gsoap|DIR Sybase|DIR"

WITH_LIBXSLT_DOC="use libexslt installation in DIR"
WITH_GNUTLS_DOC="use GNUTLS installation in DIR" #-
WITH_BOOST_DOC="use Boost installation in DIR"
WITH_LIBXLSXWRITER_DOC="use libxlsxwriter installation in DIR"
WITH_Z_DOC="use zlib installation in DIR" #-
WITH_BZ2_DOC="use bzlib installation in DIR" #-
WITH_LZO_DOC="use LZO installation in DIR (requires 2.x or up)" #-
WITH_SAMTOOLS_DOC="use SAMtools installation in DIR"
WITH_GLPK_DOC="use GNU Linear Programming Kit installation in DIR"
WITH_CURL_DOC="use libcurl installation in DIR" #-
WITH_SGE_DOC="use Sun/Univa Grid Engine installation in DIR"
WITH_OPENGL_DOC="use OpenGL installation in DIR"
WITH_MESA_DOC="use MESA installation in DIR"
WITH_GLEW_DOC="use GLEW installation in DIR"

WITH_MIMETIC_DOC="use libmimetic installation in DIR"
WITH_GSOAP_DOC="use gSOAP++ installation in DIR"

#WITHOUT_VARS="libsxslt gnutls boost libxlsxwriter z bz2 lzo"
WITHOUT_LIBXSLT_DOC="don not use libxslt"
WITHOUT_BOOST_DOC="don not use Boost"
WITHOUT_LIBXLSXWRITER_DOC="don not use libxlsxwriter"
WITHOUT_Z_DOC="use internal copy of zlib"
WIHTOUT_BZ2_DOC="use internal copy of bzlib"
WIHTOUT_LZO_DOC="don not use LZO"
WITH_SAMTOOLS_DOC="do not use SAMtools"
WITHOUT_GLPK_DOC="do not use GNU Linear Programming Kit"
WIHTOUT_CURL_DOC="do not use libcurl"
WITHOUT_SGE_DOC="do not use Sun/Univa Grid Engine"
WITHOUT_OPENGL_DOC="do not use OpenGL"
WITHOUT_MESA_DOC="do not use MESA off-screen OpenGL"
WITHOUT_GLEW_DOC="do not use GLEW"

WITHOUT_MIMETIC_DOC="do not use libmimetic"
WITHOUT_GSOAP_DOC="don not use gSOAP++"
