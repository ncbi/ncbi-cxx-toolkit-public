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

enum TToken {
    T_EOF = -1,
    T_SYMBOL = 0,

    T_IDENTIFIER = 1,
    T_TYPE_REFERENCE = 2,
    T_STRING = 3,
    T_NUMBER = 4,
    T_BINARY_STRING = 5,
    T_HEXADECIMAL_STRING = 6,
    T_DEFINE = 7,
    T_TAG = 8,
    T_DOUBLE = 9,

    K_DEFINITIONS = 101,
    K_BEGIN = 102,
    K_END = 103,
    K_IMPORTS = 104,
    K_EXPORTS = 105,
    K_FROM = 106,
    K_NULL = 107,
    K_BOOLEAN = 108,
    K_INTEGER = 109,
    K_ENUMERATED = 110,
    K_REAL = 111,
    K_VisibleString = 112,
    K_StringStore = 113,
    K_BIT = 114,
    K_OCTET = 115,
    K_STRING = 116,
    K_SET = 117,
    K_SEQUENCE = 118,
    K_OF = 119,
    K_CHOICE = 120,
    K_FALSE = 121,
    K_TRUE = 122,
    K_OPTIONAL = 123,
    K_DEFAULT = 124,
    K_BIGINT = 125,
    K_UTF8String = 126,

    T_TAG_BEGIN   = 128,
    T_TAG_END     = 129,
    K_EXPLICIT    = 130,
    K_IMPLICIT    = 131,
    K_TAGS        = 132,
    K_AUTOMATIC   = 133,
    K_UNIVERSAL   = 134,
    K_APPLICATION = 135,
    K_PRIVATE     = 136,
    K_COMPONENTS  = 137,


    T_ENTITY            =  11,
    T_IDENTIFIER_END    =  12,
    T_CONDITIONAL_BEGIN =  13,
    T_CONDITIONAL_END   =  14,
    T_NMTOKEN           =  15,

    K_ELEMENT  = 201,
    K_ATTLIST  = 202,
    K_ENTITY   = 203,
    K_PCDATA   = 204,
    K_ANY      = 205,
    K_EMPTY    = 206,
    K_SYSTEM   = 207,
    K_PUBLIC   = 208,

    K_CDATA    = 209,
    K_ID       = 210,
    K_IDREF    = 211,
    K_IDREFS   = 212,
    K_NMTOKEN  = 213,
    K_NMTOKENS = 214,
    K_ENTITIES = 215,
    K_NOTATION = 216,

    K_REQUIRED = 217,
    K_IMPLIED  = 218,
    K_FIXED    = 219,

    K_INCLUDE  = 220,
    K_IGNORE   = 221,
    K_IMPORT   = 222,

    K_CLOSING        = 300,
    K_ENDOFTAG       = 301,
    K_XML            = 302,
    K_SCHEMA         = 303,
    K_ATTPAIR        = K_ATTLIST,
    K_XMLNS          = 304,
    K_COMPLEXTYPE    = 305,
    K_COMPLEXCONTENT = 306,
    K_SIMPLETYPE     = 307,
    K_SIMPLECONTENT  = 308,
    K_EXTENSION      = 309,
    K_RESTRICTION    = 310,
    K_ATTRIBUTE      = 311,
    K_ENUMERATION    = 312,
    K_ANNOTATION     = 313,
    K_DOCUMENTATION  = 314,
    K_ATTRIBUTEGROUP = 315,
    K_GROUP          = 316,
    K_APPINFO        = 317,
    K_UNION          = 318,
    K_LIST           = 319,
    K_MINLENGTH      = 320,
    K_MAXLENGTH      = 321,
    K_LENGTH         = 322,
    K_PATTERN        = 323,
    K_INCMIN         = 324,
    K_EXCMIN         = 325,
    K_INCMAX         = 326,
    K_EXCMAX         = 327,

    K_TYPES          = 401,
    K_MESSAGE        = 402,
    K_PART           = 403,
    K_PORTTYPE       = 404,
    K_OPERATION      = 405,
    K_INPUT          = 406,
    K_OUTPUT         = 407,
    K_BINDING        = 408,
    K_SERVICE        = 409,
    K_PORT           = 410,
    K_ADDRESS        = 411,
    K_BODY           = 412,
    K_HEADER         = 413,

    K_BEGIN_OBJECT   = 501,
    K_END_OBJECT     = 502,
    K_BEGIN_ARRAY    = 503,
    K_END_ARRAY      = 504,
    K_KEY            = 505,
    K_VALUE          = 506
};

END_NCBI_SCOPE

#endif
