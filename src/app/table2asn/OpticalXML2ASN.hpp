#ifndef __OPTICALXML2ASN_HPP_INCLUDED__
#define __OPTICALXML2ASN_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

class COpticalxml2asnOperatorImpl;

	
class COpticalxml2asnOperator
{
public:
	COpticalxml2asnOperator();
	~COpticalxml2asnOperator();

	CRef<CSerialObject> LoadXML(const string& FileIn, const CSerialObject* templ);
private:
	auto_ptr<COpticalxml2asnOperatorImpl> m_impl;
};

END_NCBI_SCOPE

#endif
