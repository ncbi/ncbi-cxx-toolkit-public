#ifndef TEST_WEBENV_HPP
#define TEST_WEBENV_HPP

#include <corelib/ncbistd.hpp>
#include <serial/serial.hpp>
#include <serial/asntypes.hpp>

ASN_TYPE_REF(Web_Env)
ASN_TYPE_REF(Argument)
ASN_TYPE_REF(Db_Env)
ASN_TYPE_REF(Query_History)
ASN_TYPE_REF(Filter_Value)
ASN_TYPE_REF(Item_Set)
ASN_CHOICE_REF(Time)
ASN_TYPE_REF(Full_Time)
ASN_CHOICE_REF(QueryCommand)
ASN_TYPE_REF(Query_Search)
ASN_TYPE_REF(Query_Select)
ASN_TYPE_REF(Query_Related)

#endif
