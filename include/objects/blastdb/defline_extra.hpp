#ifndef OBJECTS_BLASTDB___DEFLINE_EXTRA__HPP
#define OBJECTS_BLASTDB___DEFLINE_EXTRA__HPP

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
 * Authors:  Jian Ye
 *
 */

/// @file defline_extra.hpp
/// Blast defline related defines
#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

static const string kAsnDeflineObjLabel = "ASN1_BlastDefLine";
static const string kTaxDataObjLabel    = "TaxNamesData";

enum LinkoutTypes {
  eLocuslink = (1<<0),
  eUnigene   = (1<<1),
  eStructure = (1<<2),
  eGeo       = (1<<3),
  eGene      = (1<<4)
};

END_objects_SCOPE
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/08/19 13:03:47  dicuccio
* Added missing include for ncbistd.hpp (to get BEGIN_NCBI_SCOPE/END_NCBI_SCOPE)
*
* Revision 1.2  2004/08/10 20:08:27  jianye
* Added gene linkout
*
* Revision 1.1  2003/08/06 16:13:00  jianye
* Add new blastdb spec (aka fastdl in the C Toolkit)
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_BLASTDB___DEFLINE_EXTRA__HPP */
