#ifndef OBJTOOLS_FORMAT___FLAT_FILE_GENERATOR__HPP
#define OBJTOOLS_FORMAT___FLAT_FILE_GENERATOR__HPP

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
* Author:  Mati Shomrat
*
* File Description:
*   User interface for generating flat file reports from ASN.1
*   
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/scope.hpp>

#include <util/range.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatTextOStream;
class CFlatItemOStream;
class CSeq_submit;
class CFFContext;


class NCBI_FORMAT_EXPORT CFlatFileGenerator : public CObject
{
public:
    enum EFormat {
        // formatting styles
        eFormat_GenBank,
        eFormat_EMBL,
        eFormat_DDBJ,
        eFormat_GBSeq,
        eFormat_FTable,
        eFormat_GFF
    };

    enum EMode {
        // determines the tradeoff between strictness and completeness
        eMode_Release, // strict -- for official public releases
        eMode_Entrez,  // somewhat laxer -- for CGIs
        eMode_GBench,  // even laxer -- for editing submissions
        eMode_Dump     // shows everything, regardless of validity
    };
    

    enum EStyle {
        // determines handling of segmented records
        eStyle_Normal,  // default -- show segments iff they're near
        eStyle_Segment, // always show segments
        eStyle_Master,  // merge segments into a single virtual record
        eStyle_Contig   // just an index of segments -- no actual sequence
    };

    enum EFlags {
        fProduceHTML          = 0x1,
        fShowContigFeatures   = 0x2, // not just source features
        fShowContigSources    = 0x4, // not just focus
        fShowFarTranslations  = 0x8,
        fTranslateIfNoProduct = 0x10,
        fAlwaysTranslateCDS   = 0x20,
        fOnlyNearFeatures     = 0x40,
        fFavorFarFeatures     = 0x80,  // ignore near feats on segs w/far feats
        fCopyCDSFromCDNA      = 0x100, // these two are for gen. prod. sets
        fCopyGeneToCDNA       = 0x200,
        fShowContigInMaster   = 0x400,
        fHideImportedFeatures = 0x800,
        fHideRemoteImpFeats   = 0x1000,
        fHideSNPFeatures      = 0x2000,
        fHideExonFeatures     = 0x4000,
        fHideIntronFeatures   = 0x8000,
        fHideMiscFeatures     = 0x10000,
        fHideCDSProdFeatures  = 0x20000,
        fShowTranscriptions   = 0x40000,
        fShowPeptides         = 0x80000,
        fHideGeneRIFs         = 0x100000,
        fOnlyGeneRIFs         = 0x200000,
        fLatestGeneRIFs       = 0x400000,
        fShowContigAndSeq     = 0x800000
    };

    enum EFilterFlags {
        // determines which Bioseqs in an entry to skip
        fSkipNone        = 0x0,
        fSkipNucleotides = 0x1,
        fSkipProteins    = 0x2
    };

    // types
    typedef EFormat         TFormat;
    typedef EMode           TMode;
    typedef EStyle          TStyle;
    typedef unsigned int    TFlags;       // binary OR of "EFlags"
    typedef EFilterFlags    TFilter;
    typedef CRange<TSeqPos> TRange;

    // constructor / destructor
    CFlatFileGenerator(CScope&      scope,
                       TFormat      format = eFormat_GenBank,
                       TMode        mode   = eMode_GBench,
                       TStyle       style  = eStyle_Normal,
                       TFilter      filter = fSkipProteins,
                       TFlags       flags  = 0);
    ~CFlatFileGenerator(void);

    void Generate(const CSeq_entry& entry, CNcbiOstream& os);
    void Generate(const CSeq_submit& submit, CNcbiOstream& os);
    void Generate(const CSeq_loc& loc, CNcbiOstream& os);

    // NB: both the item ostream and the text ostream factory objects should
    //     be allocated on the heap!
    void Generate(const CSeq_entry& entry, CFlatItemOStream& item_os);
    void Generate(const CSeq_submit& submit, CFlatItemOStream& item_os);
    void Generate(const CSeq_loc& loc, CFlatItemOStream& item_os);

private:
    CRef<CFFContext>    m_Ctx;
    //CScope&         m_Scope;
    //TFormat         m_Format;
    //TMode           m_Mode;
    //TStyle          m_Style;
    //TFilter         m_Filter;
    //TFlags          m_Flags;

    // forbidden
    CFlatFileGenerator(const CFlatFileGenerator&);
    CFlatFileGenerator& operator=(const CFlatFileGenerator&);
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/01/15 16:57:08  dicuccio
* Added private unimplemented copy ctor to satisfy MSVC
*
* Revision 1.2  2004/01/14 15:52:40  shomrat
* Added GFF format
*
* Revision 1.1  2003/12/17 19:51:49  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT___FLAT_FILE_GENERATOR__HPP */
