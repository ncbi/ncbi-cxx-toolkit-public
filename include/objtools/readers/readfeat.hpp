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
 *
 * Author:  Jonathan Kans
 *
 * File Description:
 *   Feature table reader
 *
 */

#ifndef OBJTOOLS_READERS___READFEAT__HPP
#define OBJTOOLS_READERS___READFEAT__HPP

#include <corelib/ncbistd.hpp>

#include <objtools/readers/reader_base.hpp>

// Objects includes
#include <objects/seq/Seq_annot.hpp>

#include <memory> // for auto_ptr<>

#include <util/line_reader.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CFeature_table_reader_imp;
class ILineErrorListener;
class ITableFilter;
class CSeq_entry;

// public interface for (single instance) feature table reader

class NCBI_XOBJREAD_EXPORT CFeature_table_reader : public CReaderBase
{
public:
    /// Some flags that control feature table generation.
    /// Note that "bad key" could mean many different conditions, from 
    /// unrecognized feature name to qualifier without a feature, and so on.
    enum EFlags {
        fReportBadKey           = (1<<0), ///< = 0x01 (Report bad keys into the error container)
        fKeepBadKey             = (1<<1), ///< = 0x02 (As much as possible, try to use bad keys as if they were acceptable)
        fTranslateBadKey        = (1<<2), ///< = 0x04 (yields misc_feature /standard_name="...")
        fIgnoreWebComments      = (1<<3), ///< = 0x08 (ignore web comment lines such as lines that start with " INFO:", or consist of many equals signs)
        fCreateGenesFromCDSs    = (1<<4), ///< = 0x10 (If a CDS has a gene xref, create a gene with the same intervals if one doesn't already exist.)
        fCDSsMustBeInTheirGenes = (1<<5), ///< = 0x20 (If a CDS has a gene xref, it *must* be inside of that gene)
        fReportDiscouragedKey = (1<<6), ///< = 0x40 (Report discouraged keys into the error container)
    };
    typedef int TFlags;

    CFeature_table_reader(TReaderFlags fReaderFlags = 0);

    // For CReaderBase
    CRef<CSerialObject> ReadObject   (ILineReader &lr, ILineErrorListener *pErrors);
    CRef<CSeq_annot>    ReadSeqAnnot (ILineReader &lr, ILineErrorListener *pErrors);

    // read 5-column feature table and return Seq-annot
    static CRef<CSeq_annot> ReadSequinFeatureTable(ILineReader& reader,
                                                   const TFlags flags = 0,
                                                   ILineErrorListener* pMessageListener=0,
                                                   ITableFilter *filter = 0);

    static CRef<CSeq_annot> ReadSequinFeatureTable (CNcbiIstream& ifs,
                                                    const TFlags flags = 0,
                                                    ILineErrorListener* pMessageListener=0,
                                                   ITableFilter *filter = 0);

    static CRef<CSeq_annot> ReadSequinFeatureTable (ILineReader& reader,
                                                    const string& seqid,
                                                    const string& annotname,
                                                    const TFlags flags = 0,
                                                    ILineErrorListener* pMessageListener=0,
                                                   ITableFilter *filter = 0);

    static CRef<CSeq_annot> ReadSequinFeatureTable (CNcbiIstream& ifs,
                                                    const string& seqid,
                                                    const string& annotname,
                                                    const TFlags flags = 0,
                                                    ILineErrorListener* pMessageListener=0,
                                                   ITableFilter *filter = 0);

    // read all feature tables available from the input, attaching each
    // at an appropriate position within the Seq-entry object
    static void ReadSequinFeatureTables(ILineReader& reader,
                                        CSeq_entry& entry,
                                        const TFlags flags = 0,
                                        ILineErrorListener* pMessageListener=0,
                                        ITableFilter *filter = 0);

    static void ReadSequinFeatureTables(CNcbiIstream& ifs,
                                        CSeq_entry& entry,
                                        const TFlags flags = 0,
                                        ILineErrorListener* pMessageListener=0,
                                        ITableFilter *filter = 0);

    // create single feature from key
    static CRef<CSeq_feat> CreateSeqFeat (const string& feat,
                                          CSeq_loc& location,
                                          const TFlags flags = 0,
                                          ILineErrorListener* pMessageListener = 0,
                                          unsigned int line = 0,
                                          std::string *seq_id = 0,
                                          ITableFilter *filter = 0);

    // add single qualifier to feature
    static void AddFeatQual (CRef<CSeq_feat> sfp,
                             const string& feat_name,
                             const string& qual,
                             const string& val,
                             const TFlags flags = 0,
                             ILineErrorListener* pMessageListener=0,
                             int line = 0, 	
                             const string &seq_id = std::string() );

    /// If line_arg is a feature line (e.g. ">Feature lcl|seq1"), then
    /// it will parse it and return true.
    ///
    /// @param line_arg
    ///   The input line that will be parsed.
    /// @param out_seqid
    ///   If returning true, this will hold the seqid of line_arg
    ///   (or empty if there is none)
    /// @param out_annotname
    ///   If returning true, this will hold the annotname of 
    ///   line_arg (or empty if there is none)
    /// @returns
    ///   True if it is a feature line and false if it's not.
    static bool ParseInitialFeatureLine (
        const CTempString& line_arg,
        string & out_seqid,
        string & out_annotname );

private:
    // this class uses a singleton internally to manage the specifics
    // of the feature table reader implementation
    // these are the variables / functions that control the singleton
    static auto_ptr<CFeature_table_reader_imp> sm_Implementation;

    static void                       x_InitImplementation(void);
    static CFeature_table_reader_imp& x_GetImplementation (void);
};

inline
CFeature_table_reader_imp& CFeature_table_reader::x_GetImplementation(void)
{
    if ( !sm_Implementation.get() ) {
        x_InitImplementation();
    }
    return *sm_Implementation;
}


END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___READFEAT__HPP
