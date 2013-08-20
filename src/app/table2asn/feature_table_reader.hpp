#ifndef __FEATURE_TABLE_READER_HPP_INCLUDED__
#define __FEATURE_TABLE_READER_HPP_INCLUDED__

BEGIN_NCBI_SCOPE

struct CTable2AsnContext;
namespace objects
{
//class CSeq_entry;
class CSeq_annot;
class CBioseq;
class CSeq_feat;
};

class CFeatureTableReader
{
public:
   void MergeFeatures(CRef<CSerialObject>& obj);
   void ReadFeatureTable(CRef<CSerialObject>& obj, ILineReader& line_reader);
private:
};

END_NCBI_SCOPE



#endif
