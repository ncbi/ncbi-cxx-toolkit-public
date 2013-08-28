#ifndef __OPTICALXML2ASN_HPP_INCLUDED__
#define __OPTICALXML2ASN_HPP_INCLUDED__

BEGIN_NCBI_SCOPE

class CTable2AsnContext;
namespace objects
{
    class CSeq_entry;
};

class COpticalxml2asnOperator
{
public:
    COpticalxml2asnOperator();
    ~COpticalxml2asnOperator();

    CRef<objects::CSeq_entry> 
    LoadXML(const string& FileIn, const CTable2AsnContext& context);
private:
};

END_NCBI_SCOPE

#endif
