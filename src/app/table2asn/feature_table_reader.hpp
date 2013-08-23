#ifndef __FEATURE_TABLE_READER_HPP_INCLUDED__
#define __FEATURE_TABLE_READER_HPP_INCLUDED__

BEGIN_NCBI_SCOPE

class CSerialObject;
class ILineReader;
namespace objects
{
class CSeq_entry;
class IMessageListener;
};


class CFeatureTableReader
{
public:
// MergeCDSFeatures looks for cdregion features in the feature tables 
//    in sequence annotations and creates protein sequences based on them 
//    as well as converting the sequence or a seq-set into nuc-prot-set
   void MergeCDSFeatures(objects::CSeq_entry& obj);
// This method reads 5 column table and attaches these features 
//    to corresponding sequences
// This method requires certain postprocessing of plain features added
   void ReadFeatureTable(objects::CSeq_entry& obj, ILineReader& line_reader);
private:
};

END_NCBI_SCOPE



#endif
