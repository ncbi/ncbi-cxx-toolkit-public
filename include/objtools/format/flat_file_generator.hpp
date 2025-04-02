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

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/item_ostream.hpp>
#include <objtools/format/context.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatTextOStream;
class CFlatItemOStream;
class CSeq_submit;
class CSeq_entry;
class CBioseq;
class CSeq_loc;
class CSeq_entry_Handle;
class CBioseq_Handle;
class CSeq_id;
class CTrna_ext;


struct NCBI_FORMAT_EXPORT multiout
{
    CNcbiOstream* m_Os; // all sequence output stream
    CNcbiOstream* m_On; // nucleotide output stream
    CNcbiOstream* m_Og; // genomic output stream
    CNcbiOstream* m_Or; // RNA output stream
    CNcbiOstream* m_Op; // protein output stream
    CNcbiOstream* m_Ou; // unknown output stream

    multiout(CNcbiOstream* os = nullptr, CNcbiOstream* on = nullptr, CNcbiOstream* og = nullptr,
             CNcbiOstream* or_ = nullptr, CNcbiOstream* op = nullptr, CNcbiOstream* ou = nullptr) :
        m_Os(os), m_On(on), m_Og(og), m_Or(or_), m_Op(op), m_Ou(ou)
    {
    }
    operator bool() const { return m_Os || m_On || m_Og || m_Or || m_Op || m_Ou; }
};

class NCBI_FORMAT_EXPORT CFlatFileGenerator : public CObject
{
public:
    // types
    typedef CRange<TSeqPos> TRange;

    // constructors
    CFlatFileGenerator(const CFlatFileConfig& cfg);
    CFlatFileGenerator(
        CFlatFileConfig::TFormat   format = CFlatFileConfig::eFormat_GenBank,
        CFlatFileConfig::TMode     mode   = CFlatFileConfig::eMode_GBench,
        CFlatFileConfig::TStyle    style  = CFlatFileConfig::eStyle_Normal,
        CFlatFileConfig::TFlags    flags  = 0,
        CFlatFileConfig::TView     view   = CFlatFileConfig::fViewNucleotides,
        CFlatFileConfig::TCustom   custom = 0,
        CFlatFileConfig::TPolicy   policy = CFlatFileConfig::ePolicy_Adaptive);

    // destructor
    ~CFlatFileGenerator(void);

    // Supply an annotation selector to be used in feature gathering.
    SAnnotSelector& SetAnnotSelector(void);

    void SetFeatTree(feature::CFeatTree* tree);

    void SetSeqEntryIndex(CRef<CSeqEntryIndex> idx);
    void ResetSeqEntryIndex(void);

    void Generate(const CSeq_entry_Handle& entry, CFlatItemOStream& item_os, const multiout& = {});
    void Generate(const CSeq_entry_Handle& entry, CNcbiOstream& os, const multiout& = {});
    void Generate(const CBioseq_Handle& bsh, CNcbiOstream& os, const multiout& = {});
    void Generate(const CSeq_submit& submit, CScope& scope, CNcbiOstream& os, const multiout& = {});
    void Generate(const CBioseq& bioseq, CScope& scope, CNcbiOstream& os, const multiout& = {});
    void Generate(const CSeq_loc& loc, CScope& scope, CNcbiOstream& os, const multiout& = {});
    void Generate(const CSeq_id& id, const TRange& range, ENa_strand strand,
                CScope& scope, CNcbiOstream& os, const multiout& = {});

    // for use when generating a range of a Seq-submit
    void SetSubmit(const CSubmit_block& sub) { m_Ctx->SetSubmit(sub); }

    static string GetSeqFeatText(const CMappedFeat& feat, CScope& scope,
        const CFlatFileConfig& cfg, CRef<feature::CFeatTree> ftree = null);

    static string GetFTableAnticodonText(const CTrna_ext& trna_ext, CBioseqContext& ctx);
    static string GetFTableAnticodonText(const CSeq_feat& feat, CScope& scope);

    void x_GetLocation(const CSeq_entry_Handle& entry,
         TSeqPos from, TSeqPos to, ENa_strand strand, CSeq_loc& loc);
    CBioseq_Handle x_DeduceTarget(const CSeq_entry_Handle& entry);

    bool Failed() { return m_Failed; }

    //void Reset(void);

    void SetConfig(const CFlatFileConfig& cfg);

    static bool HasInference(const CSeq_entry_Handle& topseh);

protected:
    CRef<CFlatFileContext>    m_Ctx;
    bool                      m_Failed;

    /// Use this class to wrap CFlatItemOStream instances so that they
    /// check if canceled for every item added
    class CCancelableFlatItemOStreamWrapper : public CFlatItemOStream
    {
    public:
        CCancelableFlatItemOStreamWrapper(
            CFlatItemOStream & underlying,
            const ICanceled* pCanceledCallback )
            : m_pUnderlying(&underlying),
              m_pCanceledCallback(pCanceledCallback)
        { }

        void SetFormatter(IFormatter* formatter) override;
        void AddItem(CConstRef<IFlatItem> item) override;

    private:
        CRef<CFlatItemOStream> m_pUnderlying;
        // Raw pointer because we do NOT own it
        const ICanceled * m_pCanceledCallback;
    };

    // forbidden
    CFlatFileGenerator(const CFlatFileGenerator&);
    CFlatFileGenerator& operator=(const CFlatFileGenerator&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___FLAT_FILE_GENERATOR__HPP */
