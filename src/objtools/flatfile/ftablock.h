/* $Id$
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
 * File Name: ftablock.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *
 */

#ifndef _BLOCK_
#define _BLOCK_

#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seq/Delta_seq.hpp>

#include <objtools/flatfile/flatfile_parse_info.hpp>
#include <forward_list>
#include <variant>
#include "valnode.h"

BEGIN_NCBI_SCOPE

typedef std::list<CRef<objects::CSeq_feat>>      TSeqFeatList;
typedef std::list<std::string>                   TAccessionList;
typedef std::list<CRef<objects::CSeq_id>>        TSeqIdList;
typedef std::list<CRef<objects::COrgMod>>        TOrgModList;
typedef std::vector<CRef<objects::CGb_qual>>     TGbQualVector;
typedef std::list<CRef<objects::CSeqdesc>>       TSeqdescList;
typedef std::vector<CRef<objects::CUser_object>> TUserObjVector;
typedef std::list<CRef<objects::CPub>>           TPubList;
typedef std::list<CRef<objects::CSeq_loc>>       TSeqLocList;
typedef std::list<CRef<objects::CDelta_seq>>     TDeltaList;


#define ParFlat_ENTRYNODE 500

string location_to_string_or_unknown(const objects::CSeq_loc&);

//  ============================================================================
struct InfoBioseq
//  ============================================================================
{
    TSeqIdList ids; // for this Bioseq
    string     mLocus;
    string     mAccNum;
};
using InfoBioseqPtr = InfoBioseq*;

struct ProtBlk {
    objects::CSeq_entry* biosep = nullptr; /* for the toppest level of the BioseqSet */
    bool                 IsBioseqSet = false;   /* TRUE if a BioseqSet SeqEntry */
    TEntryList           entries;          /* a ProtRef SeqEntry list, link to above
                                   biosep */
    TSeqFeatList         feats;            /* a CodeRegionPtr list to link the BioseqSet
                                   with class = nuc-prot */

    objects::CGenetic_code::C_E gcode; /* for this Bioseq */

    InfoBioseq* ibp        = nullptr;
    Uint1       genome     = 0;
    Int4        orig_gcode = 0;
};
using ProtBlkPtr = ProtBlk*;

struct LocusCont {
    Int4 bases    = 0;
    Int4 bp       = 0;
    Int4 strand   = 0;
    Int4 molecule = 0;
    Int4 topology = 0;
    Int4 div      = 0;
    Int4 date     = 0;
};

using LocusContPtr = LocusCont*;


struct GapFeats {
    Int4   from             = 0;
    Int4   to               = 0;
    Int4   estimated_length = 0;
    bool   leftNs           = false;
    bool   rightNs          = false;
    bool   assembly_gap     = false;
    string gap_type;

    objects::CSeq_gap::TType                      asn_gap_type = objects::CSeq_gap::eType_unknown;
    objects::CLinkage_evidence::TLinkage_evidence asn_linkage_evidence;
};
using TGapFeatsList = forward_list<GapFeats>;
using GapFeatsPtr = forward_list<GapFeats>::iterator;

using TokenBlkList = forward_list<string>;

struct TokenStatBlk {
    TokenBlkList list;
    Int2         num = 0; /* total number of token in the chain list */
};
using TokenStatBlkPtr = TokenStatBlk*;

struct XmlIndex {
    Int4      tag        = -1;
    Int4      order      = -1;
    size_t    start      = 0; /* Offset from the beginning of the record, not file! */
    size_t    end        = 0; /* Offset from the beginning of the record, not file! */
    Int4      start_line = -1;
    Int4      end_line   = -1;
    Int2      type       = 0; /* Used for references */
    forward_list<XmlIndex> subtags;
};
using TXmlIndexList = forward_list<XmlIndex>;

typedef std::list<std::string> TKeywordList;

struct Indexblk {
    Char   acnum[200];      /* accession num */
    Int2   vernum = 0;      /* version num */
    size_t offset = 0;      /* byte-offset of in the flatfile at
                                               which the entry starts */
    Char   locusname[200];  /* locus name */
    Char   division[4];     /* division code */
    size_t bases    = 0;    /* basepair length of the entry */
    Char   blocusname[200]; /* base locus name s.t. w/o tailing
                                               number */
    size_t linenum = 0;     /* line number at which the entry
                                               starts */
    bool   drop    = false; /* True if the accession should be
                                               dropped, otherwise False */
    size_t len     = 0;     /* total length (or sizes in bytes)
                                               of the entry */

    CRef<objects::CDate_std> date; /* the record's entry-date or last
                                                  update's date */

    CRef<objects::CPatent_seq_id> psip; /* patent reference */

    bool        EST            = false; /* special EST entries */
    bool        STS            = false; /* special STS entries */
    bool        GSS            = false; /* special Genome servey entries */
    bool        HTC            = false; /* high throughput cDNA */
    Int2        htg            = 0;     /* special HTG [0,1,2,3] entries */
    bool        is_contig      = false; /* TRUE if entry has CONTIG line,
                                                   otherwise FALSE */
    bool        is_mga         = false; /* TRUE if entry has MGA line,
                                                   otherwise FALSE */
    bool        origin         = false; /* TRUE if sequence is present */
    bool        is_pat         = false; /* TRUE if accession prefix is
                                                   patented and matches source.
                                                   FALSE - otherwise. */
    bool        is_wgs         = false;
    bool        is_tpa         = false;
    bool        is_tsa         = false;
    bool        is_tls         = false;
    bool        is_tpa_wgs_con = false; /* TRUE if "is_contig", "is_wgs" and
                                                  "is_tpa" are TRUE */
    bool        tsa_allowed    = false;
    LocusCont   lc;
    string      moltype; /* the value of /mol_type qual */
    TGapFeatsList gaps;

    TokenBlkList secaccs;
    TXmlIndexList xip;
    bool         embl_new_ID     = false;
    bool         env_sample_qual = false; /* TRUE if at least one source
                                                   feature has /environmental_sample
                                                   qualifier */
    bool         is_prot         = false;
    string       organism;                    /* The value of /organism qualifier */
    TTaxId       taxid         = ZERO_TAX_ID; /* The value gotten from source feature
                                                /db_xref qualifier if any */
    bool         no_gc_warning = false;       /* If TRUE then suppress
                                                   ERR_SERVER_GcFromSuppliedLineage
                                                   WARNING message */
    size_t       qsoffset      = 0;
    size_t       qslength      = 0;
    Int4         wgs_and_gi    = 0;     /* 01 - has GI, 02 - WGS contig,
                                                   03 - both above */
    bool         got_plastid   = false; /* Set to TRUE if there is at least
                                                   one /organelle qual beginning
                                                   with "plastid" */
    string       wgssec;                /* Reserved buffer for WGS master or
                                                   project accession as secondary */
    int          gc_genomic = 0;        /* Genomic Genetic code from OrgRef */
    int          gc_mito    = 0;        /* Mitochondrial Genetic code */
    TKeywordList keywords;              /* All keywords from a flat record */
    bool         assembly      = false; /* TRUE for TPA:assembly in
                                                   KEYWORDS line */
    bool         specialist_db = false; /* TRUE for TPA:specialist_db in
                                                   KEYWORDS line */
    bool         inferential   = false; /* TRUE for TPA:inferential in
                                                   KEYWORDS line */
    bool         experimental  = false; /* TRUE for TPA:experimental in
                                                   KEYWORDS line */
    string       submitter_seqid;
    bool         biodrop;               /* TRUE if Biosource will be dropped
                                           due to failed taxonomy lookup */
    bool         no_gbblock_source;     /* TRUE if the value of SOURCE
                                           line matches at least one
                                           source features organism */
    Parser*      ppp = nullptr;

    Indexblk();
};

using IndexblkPtr = Indexblk*;

//  ============================================================================
struct FTAOperon
//  ============================================================================
{
    FTAOperon(
        const string&            featName,
        const string&            operon,
        const objects::CSeq_loc& location) :
        mFeatname(featName),
        mOperon(operon),
        mLocation(&location) {}

    string LocationStr() const
    {
        if (mLocStr.empty()) {
            mLocStr = location_to_string_or_unknown(*mLocation);
        }
        return mLocStr;
    }

    bool IsOperon() const
    {
        return (mFeatname == "operon");
    }

    const string                 mFeatname;
    const string                 mOperon;
    CConstRef<objects::CSeq_loc> mLocation;
    mutable string               mLocStr;
};

struct EntryBlk;
struct FeatBlk;

//  ============================================================================
class DataBlk
//  ============================================================================
{
public:
    DataBlk(
        int    type_  = 0,
        char*  offset = nullptr,
        size_t len_   = 0) :
        mType(type_),
        mDrop(false)
    {
        mBuf.ptr = offset;
        mBuf.len = len_;
    }

    ~DataBlk();

    using TList = std::forward_list<DataBlk>;

    // accessors to mData
    TList&    GetSubBlocks() { return std::get<TList>(mData); }
    const TList& GetSubBlocks() const { return std::get<TList>(mData); }
    void      SetEntryData(EntryBlk*);
    EntryBlk* GetEntryData() const;
    void      SetFeatData(FeatBlk*);
    FeatBlk*  GetFeatData() const;
    void      SetXmlData(const TXmlIndexList& xil) { mData = &xil; }
    const TXmlIndexList& GetXmlData() const { return *std::get<const TXmlIndexList*>(mData); }
    bool      hasData() const { return ! holds_alternative<monostate>(mData); }
    void      deleteData();

public:
    int            mType;    // which keyword block or node type
    std::variant<monostate, TList, EntryBlk*, FeatBlk*, const TXmlIndexList*>
                   mData;
    struct {
        char*      ptr;      // points to beginning of the entry in the memory
        size_t     len;      // lenght of data in bytes
    } mBuf;
    string         mpQscore; // points to quality score buffer
    bool           mDrop;
};
using TDataBlkList = DataBlk::TList;
using DataBlkIter  = TDataBlkList::iterator;
using DataBlkCIter = TDataBlkList::const_iterator;


//  ============================================================================
struct EntryBlk {
//  ============================================================================
    TDataBlkList              chain; /* a header points to key-word
                                           block information */
    CRef<objects::CSeq_entry> seq_entry;       /* points to sequence entry */
};
using EntryBlkPtr = EntryBlk*;

/**************************************************************************/

END_NCBI_SCOPE

#endif
