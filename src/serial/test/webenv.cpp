#include "webenv.hpp"
#include <serial/serial.hpp>
#include <serial/asntypes.hpp>
#include <serial/classinfo.hpp>

#include <asn.h>
#include <webenv.h>

BEGIN_NCBI_SCOPE

TTypeInfo GetTypeInfo_struct_Web_Env(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Web_Env CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_ASN_MEMBER(arguments, SetOf)->SetOptional();
        info->ADD_ASN_MEMBER(db_Env, SetOf)->SetOptional();
        info->ADD_ASN_MEMBER(queries, SequenceOf)->SetOptional();
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Db_Env(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Db_Env CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_CLASS_MEMBER(name);
        info->ADD_ASN_MEMBER(arguments, SetOf)->SetOptional();
        info->ADD_ASN_MEMBER(filters, SetOf)->SetOptional();
        info->ADD_ASN_MEMBER(clipboard, Sequence)->SetOptional();
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Argument(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Argument CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_CLASS_MEMBER(name);
        info->ADD_CLASS_MEMBER(value);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Query_History(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Query_History CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_CLASS_MEMBER(seqNumber);
        info->ADD_CHOICE_MEMBER(time, Time);
        info->ADD_CHOICE_MEMBER(command, QueryCommand);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_QueryCommand(void)
{
    static CChoiceValNodeInfo* info = 0;
    if ( info == 0 ) {
        info = new CChoiceValNodeInfo();
        info->ADD_CHOICE_VARIANT(search, Sequence, Query_Search);
        info->ADD_CHOICE_VARIANT(select, Sequence, Query_Select);
        info->ADD_CHOICE_VARIANT(related, Sequence, Query_Related);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Query_Search(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Query_Search CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_CLASS_MEMBER(db);
        info->ADD_CLASS_MEMBER(term);
        info->ADD_CLASS_MEMBER(field);
        info->ADD_ASN_MEMBER(filters, SetOf)->SetOptional();
        info->ADD_CLASS_MEMBER(count);
		info->ADD_CLASS_MEMBER(flags)->SetOptional();
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Query_Select(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Query_Select CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_CLASS_MEMBER(db);
        info->ADD_ASN_MEMBER(items, Sequence);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Query_Related_items(void)
{
    static CChoiceValNodeInfo* info = 0;
    if ( info == 0 ) {
        info = new CChoiceValNodeInfo();
        info->ADD_CHOICE_VARIANT(items, Sequence, Item_Set);
        info->ADD_CHOICE_STD_VARIANT(itemCount, int);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Query_Related(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Query_Related CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_CHOICE_MEMBER(base, QueryCommand);
        info->ADD_CLASS_MEMBER(relation);
        info->ADD_CLASS_MEMBER(db);
        info->ADD_CHOICE_MEMBER(Items_items, Query_Related_items);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Filter_Value(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Filter_Value CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_CLASS_MEMBER(name);
        info->ADD_CLASS_MEMBER(value);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Time(void)
{
    static CChoiceValNodeInfo* info = 0;
    if ( info == 0 ) {
        info = new CChoiceValNodeInfo();
        info->ADD_CHOICE_STD_VARIANT(unix, int);
        info->ADD_CHOICE_VARIANT("full", Sequence, Full_Time);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Full_Time(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Full_Time CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_CLASS_MEMBER(year);
        info->ADD_CLASS_MEMBER(month);
        info->ADD_CLASS_MEMBER(day);
        info->ADD_CLASS_MEMBER(hour);
        info->ADD_CLASS_MEMBER(minute);
        info->ADD_CLASS_MEMBER(second);
    }
    return info;
}

TTypeInfo GetTypeInfo_struct_Item_Set(void)
{
    static CClassInfoTmpl* info = 0;
    typedef struct_Item_Set CClass;
    if ( info == 0 ) {
        info = new CStructInfo<CClass>();
        info->ADD_ASN_MEMBER(items, OctetString);
        info->ADD_CLASS_MEMBER(count);
    }
    return info;
}

END_NCBI_SCOPE
