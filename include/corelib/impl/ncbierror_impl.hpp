#ifndef CORELIB___NCBIERROR_IMPL__HPP
#define CORELIB___NCBIERROR_IMPL__HPP

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
 * Author:  Andrei Gourianov
 *
 *
 */

/////////////////////////////////////////////////////////////////////////////
// missing errno codes (copied from errno.h)
#if NCBI_COMPILER_MSVC
#  if _MSC_VER < 1600

#    define EADDRINUSE      100
#    define EADDRNOTAVAIL   101
#    define EAFNOSUPPORT    102
#    define EALREADY        103
#    define EBADMSG         104
#    define ECANCELED       105
#    define ECONNABORTED    106
#    define ECONNREFUSED    107
#    define ECONNRESET      108
#    define EDESTADDRREQ    109
#    define EHOSTUNREACH    110
#    define EIDRM           111
#    define EINPROGRESS     112
#    define EISCONN         113
#    define ELOOP           114
#    define EMSGSIZE        115
#    define ENETDOWN        116
#    define ENETRESET       117
#    define ENETUNREACH     118
#    define ENOBUFS         119
#    define ENODATA         120
#    define ENOLINK         121
#    define ENOMSG          122
#    define ENOPROTOOPT     123
#    define ENOSR           124
#    define ENOSTR          125
#    define ENOTCONN        126
#    define ENOTRECOVERABLE 127
#    define ENOTSOCK        128
#    define ENOTSUP         129
#    define EOPNOTSUPP      130
#    define EOTHER          131
#    define EOVERFLOW       132
#    define EOWNERDEAD      133
#    define EPROTO          134
#    define EPROTONOSUPPORT 135
#    define EPROTOTYPE      136
#    define ETIME           137
#    define ETIMEDOUT       138
#    define ETXTBSY         139
#    define EWOULDBLOCK     140

#  endif
#endif // NCBI_COMPILER_MSVC


#endif  /* CORELIB___NCBIERROR_IMPL__HPP */
