#ifndef __FEATURE_TABLE_READER_HPP_INCLUDED__
#define __FEATURE_TABLE_READER_HPP_INCLUDED__


#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CSeq_entry;
    class ILineErrorListener;
    class CSeq_entry_Handle;
    class CScope;
    class CDelta_seq;
    class CBioseq_Handle;
    class CSeq_feat;
    class CFeat_id;
    namespace feature
    {
        class CFeatTree;
    }
}

class CSerialObject;
class ILineReader;
class CTable2AsnContext;
struct TAsyncToken;
class CFeatureTableReader
{
public:
    CFeatureTableReader(CTable2AsnContext& context);
    ~CFeatureTableReader();

    int m_local_id_counter = 0;
    void ConvertNucSetToSet(CRef<objects::CSeq_entry>& entry) const;

    // MergeCDSFeatures looks for cdregion features in the feature tables
    //    in sequence annotations and creates protein sequences based on them
    //    as well as converting the sequence or a seq-set into nuc-prot-set
    void MergeCDSFeatures(objects::CSeq_entry&, TAsyncToken&);

    // This method reads 5 column table and attaches these features
    //    to corresponding sequences
    // This method requires certain postprocessing of plain features added
    void FindOpenReadingFrame(objects::CSeq_entry& entry) const;
    CRef<objects::CSeq_entry> ReadProtein(ILineReader& line_reader);
    void AddProteins(const objects::CSeq_entry& possible_proteins, objects::CSeq_entry& entry);
    CRef<objects::CSeq_entry> m_replacement_protein;

    void MoveRegionsToProteins(objects::CSeq_entry& entry);
    void MoveProteinSpecificFeats(objects::CSeq_entry& entry);

    void MakeGapsFromFeatures(objects::CSeq_entry_Handle seh) const;
    void MakeGapsFromFeatures(objects::CSeq_entry& entry) const;
    void MakeGapsFromFeatures(objects::CBioseq& bioseq) const;
    CRef<objects::CDelta_seq> MakeGap(objects::CBioseq& bioseq, const objects::CSeq_feat& feature_gap) const;
    static
    void RemoveEmptyFtable(objects::CBioseq& bioseq);
    void ChangeDeltaProteinToRawProtein(objects::CSeq_entry& entry) const;

private:
    bool xCheckIfNeedConversion(const objects::CSeq_entry& entry) const;
    void xConvertSeqIntoSeqSet(objects::CSeq_entry& entry, bool nuc_prod_set) const;

    void xParseCdregions(objects::CSeq_entry& entry, TAsyncToken&);
    void xMergeCDSFeatures_impl(objects::CSeq_entry&, TAsyncToken&);
    CRef<objects::CSeq_entry> xTranslateProtein(
        const objects::CBioseq& bioseq,
        objects::CSeq_feat& cd_feature,
        list<CRef<CSeq_feat>>& seq_ftable,
        TAsyncToken&);
    bool xAddProteinToSeqEntry(const objects::CBioseq& protein, objects::CSeq_entry_Handle seh);
    void xMoveCdRegions(objects::CSeq_entry_Handle entry_h, objects::CSeq_annot::TData::TFtable& seq_ftable, objects::CSeq_annot::TData::TFtable& set_ftable, TAsyncToken&);

    CTable2AsnContext& m_context;
};

END_NCBI_SCOPE

#endif
