#include <ncbi_pch.hpp>
#include <ncbiconf.h>
#if HAVE_NCBI_C
#include "cppwebenv.hpp"
#include <serial/serialimpl.hpp>
#include <serial/serialasn.hpp>

#include <asn.h>
#include "twebenv.h"

USING_NCBI_SCOPE;

BEGIN_NAMED_ASN_STRUCT_INFO("Web-Env", Web_Env)
    ADD_ASN_MEMBER(arguments, SetOf)->SetOptional();
    ADD_ASN_MEMBER(db_Env, SetOf)->SetOptional();
    ADD_ASN_MEMBER(queries, SequenceOf)->SetOptional();
END_ASN_STRUCT_INFO

BEGIN_NAMED_ASN_STRUCT_INFO("Db-Env", Db_Env)
    ADD_STD_MEMBER(name);
    ADD_ASN_MEMBER(arguments, SetOf)->SetOptional();
    ADD_ASN_MEMBER(filters, SetOf)->SetOptional();
//    ADD_ASN_MEMBER(clipboard, Sequence)->SetOptional();
END_ASN_STRUCT_INFO

BEGIN_ASN_STRUCT_INFO(Argument)
    ADD_STD_MEMBER(name);
    ADD_STD_MEMBER(value);
END_ASN_STRUCT_INFO

BEGIN_NAMED_ASN_STRUCT_INFO("Query-History", Query_History)
    ADD_STD_MEMBER(name)->SetOptional();
    ADD_STD_MEMBER(seqNumber);
    ADD_ASN_CHOICE_MEMBER(time, Time);
    ADD_ASN_CHOICE_MEMBER(command, Query_Command);
END_ASN_STRUCT_INFO

BEGIN_NAMED_ASN_CHOICE_INFO("Query-Command", Query_Command)
    ADD_ASN_CHOICE_VARIANT(search, Sequence, Query_Search);
    ADD_ASN_CHOICE_VARIANT(select, Sequence, Query_Select);
    ADD_ASN_CHOICE_VARIANT(related, Sequence, Query_Related);
END_ASN_CHOICE_INFO

BEGIN_NAMED_ASN_STRUCT_INFO("Query-Search", Query_Search)
    ADD_STD_MEMBER(db);
    ADD_STD_MEMBER(term);
    ADD_STD_MEMBER(field)->SetOptional();
    ADD_ASN_MEMBER(filters, SetOf)->SetOptional();
    ADD_STD_MEMBER(count);
	ADD_STD_MEMBER(flags)->SetOptional();
END_ASN_STRUCT_INFO

BEGIN_NAMED_ASN_STRUCT_INFO("Query-Select", Query_Select)
    ADD_STD_MEMBER(db);
    ADD_ASN_MEMBER(items, Sequence);
END_ASN_STRUCT_INFO

BEGIN_ASN_CHOICE_INFO(Query_Related_items)
    ADD_ASN_CHOICE_VARIANT(items, Sequence, Item_Set);
    ADD_ASN_CHOICE_STD_VARIANT(itemCount, int);
END_ASN_CHOICE_INFO

BEGIN_NAMED_ASN_STRUCT_INFO("Query-Related", Query_Related)
    ADD_ASN_CHOICE_MEMBER(base, Query_Command);
    ADD_STD_MEMBER(relation);
    ADD_STD_MEMBER(db);
    ADD_NAMED_ASN_CHOICE_MEMBER("items", Items_items, Query_Related_items);
END_ASN_STRUCT_INFO

BEGIN_NAMED_ASN_STRUCT_INFO("Filter-Value", Filter_Value)
    ADD_STD_MEMBER(name);
    ADD_STD_MEMBER(value);
END_ASN_STRUCT_INFO

BEGIN_ASN_CHOICE_INFO(Time)
    ADD_ASN_CHOICE_STD_VARIANT(unix, int);
    ADD_ASN_CHOICE_VARIANT(full, Sequence, Full_Time);
END_ASN_CHOICE_INFO

BEGIN_NAMED_ASN_STRUCT_INFO("Full-Time", Full_Time)
    ADD_STD_MEMBER(year);
    ADD_STD_MEMBER(month);
    ADD_STD_MEMBER(day);
    ADD_STD_MEMBER(hour);
    ADD_STD_MEMBER(minute);
    ADD_STD_MEMBER(second);
END_ASN_STRUCT_INFO

BEGIN_NAMED_ASN_STRUCT_INFO("Item-Set", Item_Set)
    ADD_ASN_MEMBER(items, OctetString);
    ADD_STD_MEMBER(count);
END_ASN_STRUCT_INFO

#endif
