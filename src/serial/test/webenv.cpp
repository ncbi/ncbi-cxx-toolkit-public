#include "webenv.hpp"
#include <serial/serial.hpp>
#include <serial/asntypes.hpp>
#include <serial/classinfo.hpp>

#include <asn.h>
#include <webenv.h>

BEGIN_NCBI_SCOPE

BEGIN_STRUCT_INFO(Web_Env)
    ADD_ASN_MEMBER(arguments, SetOf)->SetOptional();
    ADD_ASN_MEMBER(db_Env, SetOf)->SetOptional();
    ADD_ASN_MEMBER(queries, SequenceOf)->SetOptional();
END_STRUCT_INFO

BEGIN_STRUCT_INFO(Db_Env)
    ADD_CLASS_MEMBER(name);
    ADD_ASN_MEMBER(arguments, SetOf)->SetOptional();
    ADD_ASN_MEMBER(filters, SetOf)->SetOptional();
    ADD_ASN_MEMBER(clipboard, Sequence)->SetOptional();
END_STRUCT_INFO

BEGIN_STRUCT_INFO(Argument)
    ADD_CLASS_MEMBER(name);
    ADD_CLASS_MEMBER(value);
END_STRUCT_INFO

BEGIN_STRUCT_INFO(Query_History)
    ADD_CLASS_MEMBER(seqNumber);
    ADD_CHOICE_MEMBER(time, Time);
    ADD_CHOICE_MEMBER(command, QueryCommand);
END_STRUCT_INFO

BEGIN_CHOICE_INFO(QueryCommand)
    ADD_CHOICE_VARIANT(search, Sequence, Query_Search);
    ADD_CHOICE_VARIANT(select, Sequence, Query_Select);
    ADD_CHOICE_VARIANT(related, Sequence, Query_Related);
END_CHOICE_INFO

BEGIN_STRUCT_INFO(Query_Search)
    ADD_CLASS_MEMBER(db);
    ADD_CLASS_MEMBER(term);
    ADD_CLASS_MEMBER(field);
    ADD_ASN_MEMBER(filters, SetOf)->SetOptional();
    ADD_CLASS_MEMBER(count);
	ADD_CLASS_MEMBER(flags)->SetOptional();
END_STRUCT_INFO

BEGIN_STRUCT_INFO(Query_Select)
    ADD_CLASS_MEMBER(db);
    ADD_ASN_MEMBER(items, Sequence);
END_STRUCT_INFO

BEGIN_CHOICE_INFO(Query_Related_items)
    ADD_CHOICE_VARIANT(items, Sequence, Item_Set);
    ADD_CHOICE_STD_VARIANT(itemCount, int);
END_CHOICE_INFO

BEGIN_STRUCT_INFO(Query_Related)
    ADD_CHOICE_MEMBER(base, QueryCommand);
    ADD_CLASS_MEMBER(relation);
    ADD_CLASS_MEMBER(db);
    ADD_CHOICE_MEMBER(Items_items, Query_Related_items);
END_STRUCT_INFO

BEGIN_STRUCT_INFO(Filter_Value)
    ADD_CLASS_MEMBER(name);
    ADD_CLASS_MEMBER(value);
END_STRUCT_INFO

BEGIN_CHOICE_INFO(Time)
    ADD_CHOICE_STD_VARIANT(unix, int);
    ADD_CHOICE_VARIANT("full", Sequence, Full_Time);
END_CHOICE_INFO

BEGIN_STRUCT_INFO(Full_Time)
    ADD_CLASS_MEMBER(year);
    ADD_CLASS_MEMBER(month);
    ADD_CLASS_MEMBER(day);
    ADD_CLASS_MEMBER(hour);
    ADD_CLASS_MEMBER(minute);
    ADD_CLASS_MEMBER(second);
END_STRUCT_INFO

BEGIN_STRUCT_INFO(Item_Set)
    ADD_ASN_MEMBER(items, OctetString);
    ADD_CLASS_MEMBER(count);
END_STRUCT_INFO

END_NCBI_SCOPE
