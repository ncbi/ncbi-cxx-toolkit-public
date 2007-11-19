#ifndef ASNTOKENS_HPP
#define ASNTOKENS_HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   ASN.1 tokens
*
*/

BEGIN_NCBI_SCOPE

static const TToken T_IDENTIFIER = 1;
static const TToken T_TYPE_REFERENCE = 2;
static const TToken T_STRING = 3;
static const TToken T_NUMBER = 4;
static const TToken T_BINARY_STRING = 5;
static const TToken T_HEXADECIMAL_STRING = 6;
static const TToken T_DEFINE = 7;
static const TToken T_TAG = 8;
static const TToken T_DOUBLE = 9;

static const TToken K_DEFINITIONS = 101;
static const TToken K_BEGIN = 102;
static const TToken K_END = 103;
static const TToken K_IMPORTS = 104;
static const TToken K_EXPORTS = 105;
static const TToken K_FROM = 106;
static const TToken K_NULL = 107;
static const TToken K_BOOLEAN = 108;
static const TToken K_INTEGER = 109;
static const TToken K_ENUMERATED = 110;
static const TToken K_REAL = 111;
static const TToken K_VisibleString = 112;
static const TToken K_StringStore = 113;
static const TToken K_BIT = 114;
static const TToken K_OCTET = 115;
static const TToken K_STRING = 116;
static const TToken K_SET = 117;
static const TToken K_SEQUENCE = 118;
static const TToken K_OF = 119;
static const TToken K_CHOICE = 120;
static const TToken K_FALSE = 121;
static const TToken K_TRUE = 122;
static const TToken K_OPTIONAL = 123;
static const TToken K_DEFAULT = 124;
static const TToken K_BIGINT = 125;
static const TToken K_UTF8String = 126;


static const TToken T_ENTITY            =  11;
static const TToken T_IDENTIFIER_END    =  12;
static const TToken T_CONDITIONAL_BEGIN =  13;
static const TToken T_CONDITIONAL_END   =  14;

static const TToken K_ELEMENT  = 201;
static const TToken K_ATTLIST  = 202;
static const TToken K_ENTITY   = 203;
static const TToken K_PCDATA   = 204;
static const TToken K_ANY      = 205;
static const TToken K_EMPTY    = 206;
static const TToken K_SYSTEM   = 207;
static const TToken K_PUBLIC   = 208;

static const TToken K_CDATA    = 209;
static const TToken K_ID       = 210;
static const TToken K_IDREF    = 211;
static const TToken K_IDREFS   = 212;
static const TToken K_NMTOKEN  = 213;
static const TToken K_NMTOKENS = 214;
static const TToken K_ENTITIES = 215;
static const TToken K_NOTATION = 216;

static const TToken K_REQUIRED = 217;
static const TToken K_IMPLIED  = 218;
static const TToken K_FIXED    = 219;

static const TToken K_INCLUDE  = 220;
static const TToken K_IGNORE   = 221;
static const TToken K_IMPORT   = 222;

static const TToken K_CLOSING        = 300;
static const TToken K_ENDOFTAG       = 301;
static const TToken K_XML            = 302;
static const TToken K_SCHEMA         = 303;
static const TToken K_ATTPAIR        = K_ATTLIST;
static const TToken K_XMLNS          = 304;
static const TToken K_COMPLEXTYPE    = 305;
static const TToken K_COMPLEXCONTENT = 306;
static const TToken K_SIMPLETYPE     = 307;
static const TToken K_SIMPLECONTENT  = 308;
static const TToken K_EXTENSION      = 309;
static const TToken K_RESTRICTION    = 310;
static const TToken K_ATTRIBUTE      = 311;
static const TToken K_ENUMERATION    = 312;
static const TToken K_ANNOTATION     = 313;
static const TToken K_DOCUMENTATION  = 314;
static const TToken K_ATTRIBUTEGROUP = 315;
static const TToken K_GROUP          = 316;
static const TToken K_APPINFO        = 317;

END_NCBI_SCOPE

#endif
