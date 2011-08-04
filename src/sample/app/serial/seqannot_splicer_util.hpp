#ifndef SEQANNOT_SPLICE_UTIL__HPP
#define SEQANNOT_SPLICE_UTIL__HPP

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
* Author:  David McElhany
*
* File Description:
*   See main application file.
*
* ===========================================================================
*/

#include <list>

#include <corelib/ncbiobj.hpp>

class ncbi::CObjectIStream;
class ncbi::objects::CSeq_annot;
class ncbi::objects::CSeq_id;


///////////////////////////////////////////////////////////////////////////
// Typedefs

typedef ncbi::CRef<ncbi::objects::CSeq_id>      TSeqRef;

// TIds is defined by CSeq_annot_Base::C_Data::GetData() and used here.
// typedef std::list<TSeqRef>                      TIds;

// Record whether the context of a Seq-annot is the enclosing Bioseq or
// Bioseq-set.
enum EContextType {
    eNotSet,
    eBioseq_set,
    eBioseq
};

// Seq-annot Choice Mask Flags
// These flags are used to select Seq-annot choices for use in determining
// how Seq-annot's get spliced into Seq-entry's.
// The exact choice of which Seq-annot types are used depends on the splicing
// algorithm.
// By default, the 'seq-table' choice is not enabled due to an apparent bug
// in parsing seq-table CHOICE values.
enum ESeqAnnotChoiceMaskFlags {
    fSAMF_NotSet            = 0,            ///< No variant selected
    fSAMF_Ftable            = (1 <<  0),
    fSAMF_Align             = (1 <<  1),
    fSAMF_Graph             = (1 <<  2),
    fSAMF_Ids               = (1 <<  3),    ///< used for communication between tools
    fSAMF_Locs              = (1 <<  4),    ///< used for communication between tools
    fSAMF_Seq_table         = (1 <<  5),
    fSAMF_All               = ~0,
    fSAMF_AllButSeq_table   = fSAMF_All & ~fSAMF_Seq_table,
    fSAMF_Default           = fSAMF_AllButSeq_table
};
typedef unsigned int TSeqAnnotChoiceMaskFlags; ///< Binary OR of "ESeqAnnotChoiceMaskFlags"


// Seq-id Choice Mask Flags
// These flags are used to select Seq-id choices for use in determining
// how Seq-annot's get spliced into Seq-entry's.
// The exact choice of which Seq-id types are used depends on the splicing
// algorithm.
// By default, the 'local' choice is not enabled because local Seq-id's may
// not be meaningful across many Seq-entry files.  On the other hand, it's
// quite conceivable that it might be desirable to work only with local
// Seq-id's.
enum ESeqIdChoiceMaskFlags {
    fSIMF_NotSet            = 0,            ///< No variant selected
    fSIMF_Local             = (1 <<  0),    ///< local use
    fSIMF_Gibbsq            = (1 <<  1),    ///< Geninfo backbone seqid
    fSIMF_Gibbmt            = (1 <<  2),    ///< Geninfo backbone moltype
    fSIMF_Giim              = (1 <<  3),    ///< Geninfo import id
    fSIMF_Genbank           = (1 <<  4),
    fSIMF_Embl              = (1 <<  5),
    fSIMF_Pir               = (1 <<  6),
    fSIMF_Swissprot         = (1 <<  7),
    fSIMF_Patent            = (1 <<  8),
    fSIMF_Other             = (1 <<  9),    ///< for historical reasons, 'other' = 'refseq'
    fSIMF_General           = (1 << 10),    ///< for other databases
    fSIMF_Gi                = (1 << 11),    ///< GenInfo Integrated Database
    fSIMF_Ddbj              = (1 << 12),
    fSIMF_Prf               = (1 << 13),    ///< PRF SEQDB
    fSIMF_Pdb               = (1 << 14),    ///< PDB sequence
    fSIMF_Tpg               = (1 << 15),    ///< Third Party Annot/Seq Genbank
    fSIMF_Tpe               = (1 << 16),    ///< Third Party Annot/Seq EMBL
    fSIMF_Tpd               = (1 << 17),    ///< Third Party Annot/Seq DDBJ
    fSIMF_Gpipe             = (1 << 18),    ///< Internal NCBI genome pipeline processing ID
    fSIMF_Named_annot_track = (1 << 19),    ///< Internal named annotation tracking ID
    fSIMF_All               = ~0,
    fSIMF_AllButLocal       = fSIMF_All & ~fSIMF_Local,
    fSIMF_Default           = fSIMF_All
};
typedef unsigned int TSeqIdChoiceMaskFlags;  ///< Binary OR of "ESeqIdChoiceMaskFlags"


///////////////////////////////////////////////////////////////////////////
// Program global function declarations

extern void AddSeqIdToCurrentContext(ncbi::CRef<ncbi::objects::CSeq_id> id);

extern void ContextEnd(void);
extern void ContextStart(ncbi::CObjectIStream& in, EContextType type);

extern void ContextEnter(void);
extern void ContextLeave(void);

extern void ContextInit(void);

extern void CurrentContextContainsSeqAnnots(void);

extern ncbi::ESerialDataFormat GetFormat(const std::string& name);

extern bool IsSeqAnnotChoiceSelected(TSeqAnnotChoiceMaskFlags flags);
extern bool IsSeqAnnotChoiceSelected(const ncbi::objects::CSeq_annot* annot);

extern bool IsSeqIdChoiceSelected(TSeqIdChoiceMaskFlags flags);
extern bool IsSeqIdChoiceSelected(const ncbi::objects::CSeq_id* seqid);

extern void ProcessSeqEntryAnnot(std::auto_ptr<ncbi::CObjectIStream>& sai,
                                 ncbi::COStreamContainer& osc);

extern void ResetSeqEntryProcessing(void);

extern void SeqAnnotMapSeqId(TSeqRef seqid);

extern void SeqAnnotSet_Pre(ncbi::CObjectIStream& in);

extern void SetSeqAnnotChoiceMask(const std::string& mask);
extern void SetSeqAnnotChoiceMask(const TSeqAnnotChoiceMaskFlags mask = fSAMF_Default);

extern void SetSeqIdChoiceMask(const std::string& mask);
extern void SetSeqIdChoiceMask(const TSeqIdChoiceMaskFlags mask = fSIMF_Default);


#endif  /* SEQANNOT_SPLICE_UTIL__HPP */
