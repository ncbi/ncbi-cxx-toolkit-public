/*
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
*/

/*****************************************************************************

File name: blocka.h

Author: Alejandro Schaffer

Contents: header file for block IMPALA


*****************************************************************************/

#include <ncbi.h>
#include <math.h>
#include <blast.h>
#include <blastdef.h>


typedef struct alignPiece {
  Int4 queryStart;
  Int4 queryEnd;
  Int2 blockNumber;
  BLAST_Score score;
  Int4 index;
  struct alignPiece *next;
} alignPiece;

typedef struct alignBlocks {
  Int4 *queryStarts;
  Int4 *queryEnds;
  Int4 extendedBackScore;
  Int4 extendedForwardScore;
  Int4 score;
  struct alignBlocks *next;
} alignBlocks;
    





