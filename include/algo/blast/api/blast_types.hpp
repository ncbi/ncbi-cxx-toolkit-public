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
* Author:  Ilya Dondoshansky
*
* File Description:
*   Definitions of special type used in BLAST
*
*/

#ifndef ALGO_BLAST_API___BLAST_TYPE__HPP
#define ALGO_BLAST_API___BLAST_TYPE__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Enumeration analogous to blast_type_* defines from blast_def.h
enum EProgram {
    eBlastn = 0,        //< Nucl-Nucl (also includes megablast)
    eBlastp,            //< Protein-Protein
    eBlastx,            //< Translated nucl-Protein
    eTblastn,           //< Protein-Translated nucl
    eTblastx,           //< Translated nucl-Translated nucl
    eBlastUndef = 255   //< Undefined program
};

struct SSeqLoc {
    CConstRef<objects::CSeq_loc>     seqloc;
    mutable CRef<objects::CScope>    scope;

    SSeqLoc()
        : seqloc(), scope() {}
    SSeqLoc(CConstRef<objects::CSeq_loc> sl, CRef<objects::CScope> s)
        : seqloc(sl), scope(s) {}
};
typedef vector<SSeqLoc>   TSeqLocVector;
typedef vector< CRef<objects::CSeq_align_set> > TSeqAlignVector;

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/08/20 14:45:26  dondosha
* All references to CDisplaySeqalign moved to blast_format.hpp
*
* Revision 1.1  2003/08/19 22:10:10  dondosha
* Special types definitions for use in BLAST
*
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___BLAST_TYPE__HPP */
