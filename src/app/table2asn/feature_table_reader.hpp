#ifndef __FEATURE_TABLE_READER_HPP_INCLUDED__
#define __FEATURE_TABLE_READER_HPP_INCLUDED__


#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CSeq_entry;
    class IMessageListener;
};
class CSerialObject;
class ILineReader;


class CFeatureTableReader
{
public:
   CFeatureTableReader(objects::IMessageListener* logger): m_logger(logger)
   {
   }
// MergeCDSFeatures looks for cdregion features in the feature tables 
//    in sequence annotations and creates protein sequences based on them 
//    as well as converting the sequence or a seq-set into nuc-prot-set
   void MergeCDSFeatures(objects::CSeq_entry& obj);
// This method reads 5 column table and attaches these features 
//    to corresponding sequences
// This method requires certain postprocessing of plain features added
   void ReadFeatureTable(objects::CSeq_entry& obj, ILineReader& line_reader);
   void FindOpenReadingFrame(objects::CSeq_entry& entry) const;
   CRef<objects::CSeq_entry> ReadReplacementProtein(ILineReader& line_reader);
   CRef<objects::CSeq_entry> m_replacement_protein;
private:
   void ParseCdregions(objects::CSeq_entry& entry);
   objects::IMessageListener* m_logger;
};

END_NCBI_SCOPE



#endif
