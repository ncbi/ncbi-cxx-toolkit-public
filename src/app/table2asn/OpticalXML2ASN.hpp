#ifndef __OPTICALXML2ASN_HPP_INCLUDED__
#define __OPTICALXML2ASN_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>
#include "table2asn_context.hpp"

BEGIN_NCBI_SCOPE

namespace objects
{
	class CSeq_submit;
};

class COpticalxml2asnOperatorImpl;

	
class COpticalxml2asnOperator
{
public:
	COpticalxml2asnOperator();
	~COpticalxml2asnOperator();

	CRef<CSerialObject> LoadXML(const string& FileIn, const CTable2AsnContext& context);
private:
	auto_ptr<COpticalxml2asnOperatorImpl> m_impl;
};

END_NCBI_SCOPE

#endif
