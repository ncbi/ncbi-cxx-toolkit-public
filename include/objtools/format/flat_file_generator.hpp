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
        CFlatFileConfig::TView     view   = CFlatFileConfig::fViewNucleotides);
    
    // destructor
    ~CFlatFileGenerator(void);

    // Supply an annotation selector to be used in feature gathering.
    SAnnotSelector& SetAnnotSelector(void);

    void Generate(CSeq_submit& submit, CScope& scope, CNcbiOstream& os);
    void Generate(const CBioseq& bioseq, CScope& scope, CNcbiOstream& os);
    void Generate(const CSeq_loc& loc, CScope& scope, CNcbiOstream& os);
    void Generate(const CSeq_entry_Handle& entry, CNcbiOstream& os);
    void Generate(const CBioseq_Handle& bsh, CNcbiOstream& os);
    void Generate(const CSeq_id& id, const TRange& range, 
        ENa_strand strand, CScope& scope, CNcbiOstream& os);

    // NB: the item ostream should be allocated on the heap!
    void Generate(CSeq_submit& submit, CScope& scope, CFlatItemOStream& item_os);
    void Generate(const CBioseq& bioseq, CScope& scope, CFlatItemOStream& item_os);
    void Generate(const CSeq_loc& loc, CScope& scope, CFlatItemOStream& item_os);
    void Generate(const CSeq_entry_Handle& entry, CFlatItemOStream& item_os);
    void Generate(const CBioseq_Handle& bsh, CFlatItemOStream& item_os);
    void Generate(const CSeq_id& id, const TRange& range,
        ENa_strand strand, CScope& scope, CFlatItemOStream& item_os);

    static string GetSeqFeatText(const CMappedFeat& feat, CScope& scope,
        const CFlatFileConfig& cfg);

    //void Reset(void);
protected:
    CRef<CFlatFileContext>    m_Ctx;

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

        virtual void SetFormatter(IFormatter* formatter);
        virtual void AddItem (CConstRef<IFlatItem> item);

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
