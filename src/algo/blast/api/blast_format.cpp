/* $Id$
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_format.cpp

Author: Ilya Dondoshansky

Contents: Formatting of BLAST results (SeqAlign)

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <algo/blast/api/blast_format.hpp>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/api/blast_seq.hpp>
#include <algo/blast/api/seqsrc_readdb.h>

CBlastFormatOptions::CBlastFormatOptions(CBlastOption::EProgram program, 
    CNcbiOstream &ostr) THROWS((CBlastException))
        : m_ostr(&ostr)
{
   m_believe_query = FALSE;
   m_descriptions = BLAST_NUM_DESCRIPTIONS;
   m_alignments = BLAST_NUM_ALIGNMENTS;
   
   m_print_options = 0;
   m_align_options = 0;
   m_align_options += TXALIGN_COMPRESS;
   m_align_options += TXALIGN_END_NUM;
   m_align_options += TXALIGN_SHOW_GI;
   m_print_options += TXALIGN_SHOW_GI;
   
   m_align_options += TXALIGN_MATRIX_VAL;
   m_align_options += TXALIGN_SHOW_QS;
   if (program == CBlastOption::eBlastx)
      m_align_options += TXALIGN_BLASTX_SPECIAL;
   
   m_align_view = BLAST_ALIGN_VIEW;
}

CBlastFormatOptions::~CBlastFormatOptions()
{
}


