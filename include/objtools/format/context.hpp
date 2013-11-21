#ifndef OBJTOOLS_FORMAT___CONTEXT__HPP
#define OBJTOOLS_FORMAT___CONTEXT__HPP

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
* Author:  Aaron Ucko, Mati Shomrat
*
* File Description:
*   new (early 2003) flat-file generator -- context needed when (pre)formatting
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/feature.hpp>

#include <util/range.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/items/reference_item.hpp>

#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CMasterContext;
class CFlatFileContext;
class CTopLevelSeqEntryContext;

/////////////////////////////////////////////////////////////////////////////
//
// CBioseqContext
//
// information on the bioseq being formatted

class NCBI_FORMAT_EXPORT CBioseqContext : public CObject
{
public:
    // types
    typedef CRef<CReferenceItem>    TRef;
    typedef vector<TRef>            TReferences;
    typedef CRange<TSeqPos>         TRange;

    // constructor
    CBioseqContext(
        const CBioseq_Handle& seq, 
        CFlatFileContext& ffctx,
        CMasterContext* mctx = 0,
        CTopLevelSeqEntryContext *tlsec = 0);

    CBioseqContext(
        const CBioseq_Handle& prev_seq, 
        const CBioseq_Handle& seq, 
        const CBioseq_Handle& next_seq,
        CFlatFileContext& ffctx,
        CMasterContext* mctx = 0,
        CTopLevelSeqEntryContext *tlsec = 0);
    // destructor
    ~CBioseqContext(void);

    // Get the bioseq's handle (and those of its neighbors)
    CBioseq_Handle& GetPrevHandle(void) { return m_PrevHandle; }
    CBioseq_Handle& GetHandle(void) { return m_Handle; }
    CBioseq_Handle& GetNextHandle(void) { return m_NextHandle; }
    CScope& GetScope(void) { return m_Handle.GetScope(); }
    feature::CFeatTree& GetFeatTree(void) { return m_FeatTree; }

    // -- id information
    CSeq_id* GetPrimaryId(void) { return m_PrimaryId; }
    CSeq_id_Handle GetPreferredSynonym(const CSeq_id& id) const;
    const string& GetAccession(void) const { return m_Accession; }
    TGi  GetGI(void) const { return m_Gi; }
    
    // molecular type (nucleotide / protein)
    bool IsProt(void) const { return m_IsProt;  }
    bool IsNuc (void) const { return !m_IsProt; }

    CSeq_inst::TRepr  GetRepr   (void) const { return m_Repr;    }
    CSeq_inst::TMol   GetMol    (void) const { return m_Mol;     }
    const CMolInfo*   GetMolinfo(void) const { return m_Molinfo; }
    CMolInfo::TTech   GetTech   (void) const;
    CMolInfo::TBiomol GetBiomol (void) const;
    const CBioseq::TId& GetBioseqIds(void) const;

    // segmented bioseq
    bool IsSegmented(void) const { return m_Repr == CSeq_inst::eRepr_seg; }
    bool HasParts(void) const { return m_HasParts; }
    
    // part of a segmented bioseq
    bool IsPart(void) const { return m_IsPart; }
    SIZE_TYPE GetPartNumber   (void) const { return m_PartNumber; }
    SIZE_TYPE GetTotalNumParts(void) const;
    CMasterContext& GetMaster (void) { return *m_Master; }
    bool CanGetMaster         (void) const { return m_Master.NotNull(); }
    void SetMaster(CMasterContext& mctx);

    // top-level Seq-entry info
    CTopLevelSeqEntryContext& GetTLSeqEntryCtx(void) { return *m_TLSeqEntryCtx; }
    bool CanGetTLSeqEntryCtx                  (void) const { return m_TLSeqEntryCtx.NotNull(); }
    bool IsBioseqInSameTopLevelSeqEntry(const CBioseq_Handle & seq);
    bool IsSeqIdInSameTopLevelSeqEntry(const CSeq_id & seq_id );

    // delta sequence
    bool IsDelta(void) const { return m_Repr == CSeq_inst::eRepr_delta; }
    bool IsDeltaLitOnly(void) const { return m_IsDeltaLitOnly; }

    // Whole Genome Shotgun
    bool IsWGS      (void) const { return m_IsWGS;       }
    bool IsWGSMaster(void) const { return m_IsWGSMaster; }
    const string& GetWGSMasterAccn(void) const { return m_WGSMasterAccn; }
    const string& GetWGSMasterName(void) const { return m_WGSMasterName; }

    // Transcriptome Shotgun Assembly 
    bool IsTSA      (void) const { return m_IsTSA;       }
    bool IsTSAMaster(void) const { return m_IsTSAMaster; }
    const string& GetTSAMasterAccn(void) const { return m_TSAMasterAccn; }
    const string& GetTSAMasterName(void) const { return m_TSAMasterName; }

    TReferences& SetReferences(void) { return m_References; }
    const TReferences& GetReferences(void) const { return m_References; }

    // range on the bioseq to be formatted. the location is either
    // whole or an interval (no complex locations allowed)
    const CSeq_loc& GetLocation(void) const { return *m_Location; }
    CSeq_loc_Mapper* GetMapper(void) { return m_Mapper; }

    bool DoContigStyle(void) const;
    bool ShowGBBSource(void) const { return m_ShowGBBSource; }

    bool IsInGPS    (void) const { return m_IsInGPS;     }  // Is in a gene-prod set?
    bool IsInNucProt(void) const { return m_IsInNucProt; }  // Is in a nuc-prot set?

    // type of bioseq?
    bool IsGED            (void) const { return m_IsGED;     }  // Genbank, EMBL or DDBJ
    bool IsGenbank        (void) const { return m_IsGenbank; }  // Genbank
    bool IsEMBL           (void) const { return m_IsEMBL;    }  // EMBL
    bool IsDDBJ           (void) const { return m_IsDDBJ;    }  // DDBJ
    bool IsPDB            (void) const { return m_IsPDB;     }  // PDB
    bool IsSP             (void) const { return m_IsSP;      }  // SwissProt
    bool IsTPA            (void) const { return m_IsTPA;     }  // Third-Party Annotation
    bool IsJournalScan    (void) const { return m_IsJournalScan;  }  // scanned from journal
    bool IsPatent         (void) const { return m_IsPatent; }  // patent
    bool IsGbGenomeProject(void) const { return m_IsGbGenomeProject; } // AE
    bool IsNcbiCONDiv     (void) const { return m_IsNcbiCONDiv; }      // CH
    bool IsNcbiGenomes    (void) const { return m_IsNcbiGenomes; } // db NCBI_GENOMES
    
    // RefSeq ID queries
    bool IsRefSeq(void) const { return m_IsRefSeq; }
    bool IsRSCompleteGenomic  (void) const;  // NC_
    bool IsRSIncompleteGenomic(void) const;  // NG_
    bool IsRSMRna             (void) const;  // NM_
    bool IsRSNonCodingRna     (void) const;  // NR_
    bool IsRSProtein          (void) const;  // NP_
    bool IsRSContig           (void) const;  // NT_
    bool IsRSIntermedWGS      (void) const;  // NW_
    bool IsRSPredictedMRna    (void) const;  // XM_
    bool IsRSPredictedNCRna   (void) const;  // XR_
    bool IsRSPredictedProtein (void) const;  // XP_
    bool IsRSWGSNuc           (void) const;  // NZ_
    bool IsRSWGSProt          (void) const;  // ZP_
    bool IsRSUniqueProt       (void) const;  // WP_
    
    bool IsEncode             (void) const;  // provided by the ENCODE project
    const CUser_object& GetEncode(void) const;

    const string &GetFinishingStatus(void) const;
    bool IsGenomeAssembly(void) const;
    bool IsCrossKingdom(void) const;

    NCBI_DEPRECATED bool IsUnverified(void) const;
    enum FUnverified {
        fUnverified_None = 0, // for convenience

        fUnverified_Organism              = 1 << 0,
        fUnverified_SequenceOrAnnotation  = 1 << 1
        // if you add more here, make sure 
        // to update CFlatGatherer::x_UnverifiedComment
    };
    typedef Int8 TUnverified;
    TUnverified GetUnverifiedType(void) const;
    bool ShowAnnotCommentAsCOMMENT(void) const { return m_ShowAnnotCommentAsCOMMENT; }

    bool IsHup(void) const { return m_IsHup; }  // !!! should move to global?

    // patent seqid
    int GetPatentSeqId (void) const { return m_PatSeqid; }

    // global data from CFlatFileContext
    const CSubmit_block* GetSubmitBlock(void) const;
    const CSeq_entry_Handle& GetTopLevelEntry(void) const;
    const CFlatFileConfig& Config(void) const;
    const SAnnotSelector* GetAnnotSelector(void) const;
    SAnnotSelector& SetAnnotSelector(void);
    const CSeq_loc* GetMasterLocation(void) const;
    bool IsGenbankFormat(void) const;

    bool HasOperon(void) const;
    bool HasMultiIntervalGenes(void) const;

    /// Checks if CFlatFileConfig has an ICanceled, and throws
    /// if it indicates cancellation.
    void ThrowIfCanceled(void) const;

    /// Empty string if unavailable
    const string & GetTaxname(void) const { return m_Taxname; }

    /// Empty if no Filetrack URL.
    const string & GetFiletrackURL(void) const { return m_FiletrackURL; }

    // empty if there is none
    const string & GetAuthorizedAccess(void) const { return m_AuthorizedAccess; }

    /// Empty or NULL if no points or if this bioseq isn't an optical map.
    const CPacked_seqpnt * GetOpticalMapPoints(void) const { 
        return m_pOpticalMapPoints; }

private:
    void x_Init(const CBioseq_Handle& seq, const CSeq_loc* user_loc);
    void x_SetId(void);
    bool x_HasParts(void) const;
    bool x_IsDeltaLitOnly(void) const;
    bool x_IsPart(void) const;
    CBioseq_Handle x_GetMasterForPart(void) const;
    SIZE_TYPE x_GetPartNumber(void);
    bool x_IsInGPS(void) const;
    bool x_IsInNucProt(void) const;
    void x_SetLocation(const CSeq_loc* user_loc = 0);
    void x_SetMapper(const CSeq_loc& loc);
    void x_SetHasMultiIntervalGenes(void);
    void x_SetDataFromUserObjects(void);
    void x_SetDataFromAnnot(void);
    void x_SetTaxname(void);
    void x_SetFiletrackURL(const CUser_object& uo);
    void x_SetAuthorizedAccess(const CUser_object& uo);
    void x_SetOpticalMapPoints(void);
    void x_SetEncode(const CUser_object& uo);

    CSeq_inst::TRepr x_GetRepr(void) const;
    const CMolInfo* x_GetMolInfo(void) const;
    bool x_HasOperon(void) const;

    // data
    CBioseq_Handle        m_PrevHandle;
    CBioseq_Handle        m_Handle;
    CBioseq_Handle        m_NextHandle;
    feature::CFeatTree    m_FeatTree;
    CRef<CSeq_id>         m_PrimaryId;
    string                m_Accession;
    string                m_WGSMasterAccn;
    string                m_WGSMasterName;
    string                m_TSAMasterAccn;
    string                m_TSAMasterName;
    string                m_FinishingStatus;
    string                m_Taxname;
    string                m_FiletrackURL;
    string                m_AuthorizedAccess;
    const CPacked_seqpnt* m_pOpticalMapPoints;
    // used to destroy m_pOpticalMapPoints if it was manually
    // created.
    auto_ptr<CPacked_seqpnt> m_pOpticalMapPointsDestroyer;
    

    CSeq_inst::TRepr      m_Repr;
    CSeq_inst::TMol       m_Mol;
    CConstRef<CMolInfo>   m_Molinfo; 

    // segmented bioseq
    bool        m_HasParts;
    // part of a segmented bioseq
    bool        m_IsPart;
    SIZE_TYPE   m_PartNumber;
    // delta bioseq
    bool        m_IsDeltaLitOnly;

    bool m_IsProt;         // Protein
    bool m_IsInGPS;        // Gene-Prod Set
    bool m_IsInNucProt;    // Nuc-Prot Set
    bool m_IsGED;          // Genbank, Embl or Ddbj
    bool m_IsGenbank;      // Genbank
    bool m_IsEMBL;         // EMBL
    bool m_IsDDBJ;         // DDBJ
    bool m_IsPDB;          // PDB
    bool m_IsSP;           // SwissProt
    bool m_IsTPA;          // Third Party Annotation
    bool m_IsJournalScan;  // scanned from journal
    bool m_IsRefSeq;
    unsigned int m_RefseqInfo;
    bool m_IsGbGenomeProject;  // GenBank Genome project data
    bool m_IsNcbiCONDiv;       // NCBI CON division
    bool m_IsNcbiGenomes;      // NCBI_GENOMES
    bool m_IsPatent;
    bool m_IsGI;
    bool m_IsWGS;
    bool m_IsWGSMaster;
    bool m_IsTSA;
    bool m_IsTSAMaster;
    bool m_IsHup;
    TGi  m_Gi;
    bool m_ShowGBBSource;
    int  m_PatSeqid;
    bool m_HasOperon;
    bool m_HasMultiIntervalGenes;
    bool m_IsGenomeAssembly;
    bool m_IsCrossKingdom;
    TUnverified m_fUnverified;
    bool m_ShowAnnotCommentAsCOMMENT;

    CConstRef<CUser_object> m_Encode;
    
    TReferences             m_References;
    CConstRef<CSeq_loc>     m_Location;
    CRef<CSeq_loc_Mapper>   m_Mapper;
    CBioseq_Handle          m_Virtual;
    CFlatFileContext&       m_FFCtx;
    CRef<CMasterContext>    m_Master;
    CRef<CTopLevelSeqEntryContext> m_TLSeqEntryCtx;
};


/////////////////////////////////////////////////////////////////////////////
//
// CMasterContext
//
// When formatting segmented bioseq CMasterContext holds information
// on the Master bioseq.

class NCBI_FORMAT_EXPORT CMasterContext : public CObject
{
public:
    // constructor
    CMasterContext(const CBioseq_Handle& master);
    // destructor
    ~CMasterContext(void);

    // Get the segmented bioseq's handle
    const CBioseq_Handle& GetHandle(void) const { return m_Handle;   }
    // Get the number of parts
    SIZE_TYPE GetNumParts          (void) const { return m_NumParts; }
    // Get the base name
    const string& GetBaseName      (void) const { return m_BaseName; }

    // Find the serial number of a part in the segmented bioseq
    SIZE_TYPE GetPartNumber(const CBioseq_Handle& part);

private:
    void x_SetBaseName(void);
    void x_SetNumParts(void);

    // data
    CBioseq_Handle    m_Handle;
    string            m_BaseName;
    SIZE_TYPE         m_NumParts;
};


/////////////////////////////////////////////////////////////////////////////
//
// CFlatFileContext

class NCBI_FORMAT_EXPORT CFlatFileContext : public CObject
{
public:
    // types
    typedef CRef<CBioseqContext>            TSection;
    typedef vector< CRef<CBioseqContext> >  TSections;

    // constructor
    CFlatFileContext(const CFlatFileConfig& cfg) : m_Cfg(cfg) { }
    // destructor
    ~CFlatFileContext(void) {}

    const CSeq_entry_Handle& GetEntry(void) const { return m_Entry;  }
    void SetEntry(const CSeq_entry_Handle& entry) { m_Entry = entry; }

    const CSubmit_block* GetSubmitBlock(void) const { return m_Submit; }
    void SetSubmit(const CSubmit_block& sub) { m_Submit = &sub; }

    CFlatFileConfig& GetConfig(void) { return m_Cfg; }

    const SAnnotSelector* GetAnnotSelector(void) const;
    SAnnotSelector& SetAnnotSelector(void);
    void SetAnnotSelector(const SAnnotSelector&);

    const CSeq_loc* GetLocation(void) const { return m_Loc; }
    void SetLocation(const CSeq_loc* loc) { m_Loc.Reset(loc); }

    void AddSection(TSection& section) { m_Sections.push_back(section); }

    void Reset(void);

private:

    CFlatFileConfig             m_Cfg;
    CSeq_entry_Handle           m_Entry;
    TSections                   m_Sections;
    CConstRef<CSubmit_block>    m_Submit;
    auto_ptr<SAnnotSelector>    m_Selector;
    CConstRef<CSeq_loc>         m_Loc;
};

/////////////////////////////////////////////////////////////////////////////
//
// CTopLevelSeqEntryContext

// This class is for information about a top-level Seq-entry which
// doesn't belong in the entry itself because it has to be calculated
// from it. Also, it doesn't belong in CFlatFileContext because it's
// not formatting information.

class CTopLevelSeqEntryContext  : public CObject
{
public:
    CTopLevelSeqEntryContext( const CSeq_entry_Handle &entry_handle );

    bool GetCanSourcePubsBeFused(void) const { return m_CanSourcePubsBeFused; }
    bool GetHasSmallGenomeSet(void) const { return m_HasSmallGenomeSet; }

private:
    bool m_CanSourcePubsBeFused;
    bool m_HasSmallGenomeSet;
};


/////////////////////////////////////////////////////////////////////////////
// inline methods

// -------- CBioseqContext

inline
const CBioseq::TId& CBioseqContext::GetBioseqIds(void) const
{
    return m_Handle.GetBioseqCore()->GetId();
}

inline
SIZE_TYPE CBioseqContext::GetTotalNumParts(void) const
{
    return m_Master->GetNumParts();
}

inline
void CBioseqContext::SetMaster(CMasterContext& mctx) {
    m_Master.Reset(&mctx);
}

inline
bool CBioseqContext::IsBioseqInSameTopLevelSeqEntry(const CBioseq_Handle & seq)
{
    return seq.GetTopLevelEntry() == m_Handle.GetTopLevelEntry();
}

inline
bool CBioseqContext::IsSeqIdInSameTopLevelSeqEntry(const CSeq_id & seq_id)
{
    return GetScope().GetBioseqHandleFromTSE(seq_id, m_Handle);
}

inline
bool CBioseqContext::IsRSCompleteGenomic(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_chromosome;  // NC_
}

inline
bool CBioseqContext::IsRSIncompleteGenomic(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_genomic;  // NG_
}

inline
bool CBioseqContext::IsRSMRna(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_mrna;  // NM_
}

inline
bool CBioseqContext::IsRSNonCodingRna(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_ncrna;  // NR_
}

inline
bool CBioseqContext::IsRSProtein(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_prot;  // NP_
}

inline
bool CBioseqContext::IsRSContig(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_contig;  // NT_
}

inline
bool CBioseqContext::IsRSIntermedWGS(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_wgs_intermed;  // NW_
}

inline
bool CBioseqContext::IsRSPredictedMRna(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_mrna_predicted;  // XM_
}

inline
bool CBioseqContext::IsRSPredictedNCRna(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_ncrna_predicted;  // XR_
}

inline
bool CBioseqContext::IsRSPredictedProtein(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_prot_predicted;  // XP_
}

inline
bool CBioseqContext::IsRSWGSNuc(void)  const
{
    // ignore "master" bit if it's there
    return (m_RefseqInfo & ~CSeq_id::fAcc_master) == CSeq_id::eAcc_refseq_wgs_nuc;  // NZ_
}

inline
bool CBioseqContext::IsRSWGSProt(void)  const
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_wgs_prot;  // ZP_
}

inline
bool CBioseqContext::IsRSUniqueProt(void) const  // WP
{
    return m_RefseqInfo == CSeq_id::eAcc_refseq_unique_prot;  // WP_
}

inline
bool CBioseqContext::IsEncode(void)  const
{
    return m_Encode.NotEmpty();
}

inline
const CUser_object& CBioseqContext::GetEncode(void)  const
{
    _ASSERT(IsEncode());
    return *m_Encode;
}

inline
const string &CBioseqContext::GetFinishingStatus(void) const
{
    return m_FinishingStatus;
}

inline
bool CBioseqContext::IsGenomeAssembly(void) const
{
    return m_IsGenomeAssembly;
}

inline
bool CBioseqContext::IsCrossKingdom(void) const
{
    return m_IsCrossKingdom;
}

inline
bool CBioseqContext::IsUnverified(void) const
{
    return ( m_fUnverified != fUnverified_None );
}

inline
CBioseqContext::TUnverified CBioseqContext::GetUnverifiedType(void) const
{
    return m_fUnverified;
}

inline
const CFlatFileConfig& CBioseqContext::Config(void) const
{
    return m_FFCtx.GetConfig();
}

inline
const CSubmit_block* CBioseqContext::GetSubmitBlock(void) const
{
    return m_FFCtx.GetSubmitBlock();
}

inline
const CSeq_entry_Handle& CBioseqContext::GetTopLevelEntry(void) const
{
    return m_FFCtx.GetEntry();
}

inline
const SAnnotSelector* CBioseqContext::GetAnnotSelector(void) const
{
    return m_FFCtx.GetAnnotSelector();
}

inline
SAnnotSelector& CBioseqContext::SetAnnotSelector(void)
{
    return m_FFCtx.SetAnnotSelector();
}

inline
const CSeq_loc* CBioseqContext::GetMasterLocation(void) const
{
    return m_FFCtx.GetLocation();
}


inline
CMolInfo::TTech CBioseqContext::GetTech(void) const
{
    return m_Molinfo ? m_Molinfo->GetTech() : CMolInfo::eTech_unknown;
}


inline
CMolInfo::TBiomol CBioseqContext::GetBiomol(void) const
{
    return m_Molinfo ? m_Molinfo->GetBiomol() : CMolInfo::eBiomol_unknown;
}


inline
bool CBioseqContext::IsGenbankFormat(void) const
{
    return (Config().IsFormatGenbank()  ||
            Config().IsFormatGBSeq()    ||
            Config().IsFormatDDBJ());
}


inline
bool CBioseqContext::HasOperon(void) const
{
    return m_HasOperon;
}

inline 
bool CBioseqContext::HasMultiIntervalGenes(void) const
{
    return m_HasMultiIntervalGenes;
}

inline
void CBioseqContext::ThrowIfCanceled(void) const
{
    Config().ThrowIfCanceled();
}

// -------- CFlatFileContext

inline
const SAnnotSelector* CFlatFileContext::GetAnnotSelector(void) const
{
    return m_Selector.get();
}

inline
SAnnotSelector& CFlatFileContext::SetAnnotSelector(void)
{
    if ( m_Selector.get() == 0 ) {
        m_Selector.reset(new SAnnotSelector);
    }

    return *m_Selector;
}


inline
void CFlatFileContext::SetAnnotSelector(const SAnnotSelector& sel)
{
    m_Selector.reset(new SAnnotSelector(sel));
}


inline
void CFlatFileContext::Reset(void)
{
    m_Entry.Reset();
    m_Sections.clear();
    m_Submit.Reset();
    m_Selector.reset();
    m_Loc.Reset();
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___CONTEXT__HPP */
