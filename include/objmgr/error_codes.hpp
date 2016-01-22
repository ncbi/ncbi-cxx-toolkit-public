#ifndef OBJMGR___ERROR_CODES__HPP
#define OBJMGR___ERROR_CODES__HPP

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
 * Authors:  Pavel Ivanov
 *
 */

/// @file error_codes.hpp
/// Definition of all error codes used in objmgr libraries
/// (xobjmgr.lib, xobjutil.lib and others).
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(ObjMgr_Main,         1201,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_Scope,        1202, 17);
NCBI_DEFINE_ERRCODE_X(ObjMgr_ScopeTrans,   1203,  8);
NCBI_DEFINE_ERRCODE_X(ObjMgr_SeqAnnot,     1204,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_TSEinfo,      1205,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_DataSource,   1206,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_AnnotObject,  1207,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_AnnotCollect, 1208,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_Sniffer,      1209,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_SeqUtil,      1210,  9);
NCBI_DEFINE_ERRCODE_X(ObjMgr_IdRange,      1211,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_BlobSplit,    1212,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_SeqTable,     1213,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_ObjSplitInfo, 1214,  0);
NCBI_DEFINE_ERRCODE_X(ObjMgr_Rd_Split,     1215,  0);

END_NCBI_SCOPE


#endif  /* OBJMGR___ERROR_CODES__HPP */
