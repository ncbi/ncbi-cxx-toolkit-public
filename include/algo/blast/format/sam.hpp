/* $Id
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* Author: Amelia Fong
*
* ===========================================================================
*/

/// @file: sam.hpp
/// Formatting of pairwise sequence alignments in SAM form.

#ifndef ALGO_BLAST_FORMAT___SAM_HPP
#define ALGO_BLAST_FORMAT___SAM_HPP

#include <objtools/format/sam_formatter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_XBLASTFORMAT_EXPORT CBlast_SAM_Formatter : public CSAM_Formatter
{
public:
	CBlast_SAM_Formatter (CNcbiOstream& out,
			              CScope& scope,
			              const string & custom_spec,
			              const CSAM_Formatter::SProgramInfo & info);

	void Print(const CSeq_align_set &  aln);

private :
	void x_ProcessCustomSpec(const string & custom_spec, const CSAM_Formatter::SProgramInfo & info);
	void x_Print(const CSeq_align_set & aln);
	unsigned int m_refRow;

};

END_SCOPE(objects)
END_NCBI_SCOPE
#endif /* ALGo_BLAST_FORMAT___SAM_HPP */
