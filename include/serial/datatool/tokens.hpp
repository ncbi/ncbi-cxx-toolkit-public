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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/12/15 15:38:35  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.2  2000/04/07 19:26:13  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:23  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.2  1999/11/15 19:36:19  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

BEGIN_NCBI_SCOPE

static const TToken T_IDENTIFIER = 1;
static const TToken T_TYPE_REFERENCE = 2;
static const TToken T_STRING = 3;
static const TToken T_NUMBER = 4;
static const TToken T_BINARY_STRING = 5;
static const TToken T_HEXADECIMAL_STRING = 6;
static const TToken T_DEFINE = 7;

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

END_NCBI_SCOPE

#endif

