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
* Author:  Christiam Camacho
*
* File Description:
*   Reading FASTA sequences from input file
*
*/

#ifndef BLAST_INPUT__HPP
#define BLAST_INPUT__HPP

#include <corelib/ncbistd.hpp>
#include <algo/blast/api/blast_aux.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
USING_SCOPE(objects);

TSeqLocVector
BLASTGetSeqLocFromStream(CNcbiIstream& in, CScope* scope, 
    ENa_strand strand, TSeqPos from, TSeqPos to, int* counter, 
    BlastMask** lcase_mask = NULL);

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* BLAST_INPUT__HPP */
