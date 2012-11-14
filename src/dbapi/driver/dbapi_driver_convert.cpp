/*
 * =====================================================================================
 *
 *       Filename:  dbapi_driver_convert.cpp
 *
 *    Description:  <CURSOR>
 *
 *        Version:  1.0
 *        Created:  11/03/08 11:56:54
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Dr. Fritz Mehner (mn), mehner@fh-swf.de
 *        Company:  FH Südwestfalen, Iserlohn
 *
 * =====================================================================================
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_driver_convert.hpp>

BEGIN_NCBI_SCOPE

namespace value_slice
{

////////////////////////////////////////////////////////////////////////////////
CValueConvert<SRunTimeCP, CDB_Result>::CValueConvert(obj_type& value)
: m_Value(&value)
{
}

CValueConvert<SRunTimeCP, CDB_Result>::operator bool(void) const
{
    typedef bool TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator Int1(void) const
{
    typedef Int1 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator Uint1(void) const
{
    typedef Uint1 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator Int2(void) const
{
    typedef Int2 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator Uint2(void) const
{
    typedef Uint2 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator Int4(void) const
{
    typedef Int4 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator Uint4(void) const
{
    typedef Uint4 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator Int8(void) const
{
    typedef Int8 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator Uint8(void) const
{
    typedef Uint8 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator float(void) const
{
    typedef float TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator double(void) const
{
    typedef double TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeCP, CDB_Result>::operator string(void) const
{
    typedef string TO;

    const int item_num = m_Value->CurrentItemNo();
    const EDB_Type db_type = m_Value->ItemDataType(item_num);

    switch (db_type) {
    case eDB_Int:
        return ConvertFrom<TO, CDB_Int>();
    case eDB_SmallInt:
        return ConvertFrom<TO, CDB_SmallInt>();
    case eDB_TinyInt:
        return ConvertFrom<TO, CDB_TinyInt>();
    case eDB_BigInt:
        return ConvertFrom<TO, CDB_BigInt>();
    case eDB_VarChar:
        return ConvertFromStr<TO, CDB_VarChar>();
    case eDB_Char:
        return ConvertFromChar<TO, CDB_Char>(item_num);
    case eDB_VarBinary:
        return ConvertFromStr<TO, CDB_VarBinary>();
    case eDB_Binary:
        return ConvertFromChar<TO, CDB_Binary>(item_num);
    case eDB_Float:
        return ConvertFrom<TO, CDB_Float>();
    case eDB_Double:
        return ConvertFrom<TO, CDB_Double>();
    // case eDB_DateTime:
    //     return ConvertFrom<TO, CDB_DateTime>();
    // case eDB_SmallDateTime:
    //     return ConvertFrom<TO, CDB_SmallDateTime>();
    case eDB_Text:
        return ConvertFromLOB<TO, CDB_Text>();
    case eDB_Image:
        return ConvertFromLOB<TO, CDB_Image>();
    case eDB_Bit: 
        return ConvertFrom<TO, CDB_Bit>();
    case eDB_Numeric:
        return ConvertFrom<TO, CDB_Numeric>();
    case eDB_LongChar:
        return ConvertFromChar<TO, CDB_LongChar>(item_num);
    case eDB_LongBinary:
        {
            CDB_LongBinary db_obj(m_Value->ItemMaxSize(item_num));

            const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
            if (db_obj.IsNULL()) {
                NCBI_REPORT_CONSTANT_CONVERSION_ERROR("NULL long binary");
            }

            return Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.DataSize()));
        }
    default:
        NCBI_REPORT_CONVERSION_ERROR("CDB_Object of unsupported type "
                                     << db_type);
    }

    return TO();
}

// This version is different from a template one ...
CValueConvert<SRunTimeCP, CDB_Result>::operator CTime(void) const
{
    typedef CTime TO;

    const int item_num = m_Value->CurrentItemNo();
    const EDB_Type db_type = m_Value->ItemDataType(item_num);

    switch (db_type) {
    case eDB_Int:
        return Convert2CTime<TO, CDB_Int>();
    case eDB_SmallInt:
        return ConvertFrom<TO, CDB_SmallInt>();
    case eDB_TinyInt:
        return ConvertFrom<TO, CDB_TinyInt>();
    case eDB_BigInt:
        return Convert2CTime<TO, CDB_BigInt>();
    case eDB_VarChar:
        return ConvertFromStr<TO, CDB_VarChar>();
    case eDB_Char:
        return ConvertFromChar<TO, CDB_Char>(item_num);
    case eDB_VarBinary:
        return ConvertFromStr<TO, CDB_VarBinary>();
    case eDB_Binary:
        return ConvertFromChar<TO, CDB_Binary>(item_num);
    case eDB_Float:
        return Convert2CTime<TO, CDB_Float>();
    case eDB_Double:
        return Convert2CTime<TO, CDB_Double>();
    // case eDB_DateTime:
    //     return Convert2CTime<TO, CDB_DateTime>();
    // case eDB_SmallDateTime:
    //     return Convert2CTime<TO, CDB_SmallDateTime>();
    case eDB_Text:
        return ConvertFromLOB<TO, CDB_Text>();
    case eDB_Image:
        return ConvertFromLOB<TO, CDB_Image>();
    case eDB_Bit: 
        return Convert2CTime<TO, CDB_Bit>();
    case eDB_Numeric:
        return ConvertFrom<TO, CDB_Numeric>();
    case eDB_LongChar:
        return ConvertFromChar<TO, CDB_LongChar>(item_num);
    case eDB_LongBinary:
        {
            CDB_LongBinary db_obj(m_Value->ItemMaxSize(item_num));

            const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
            if (db_obj.IsNULL()) {
                NCBI_REPORT_CONSTANT_CONVERSION_ERROR("NULL long binary");
            }

            return Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.DataSize()));
        }
    default:
        NCBI_REPORT_CONVERSION_ERROR("CDB_Object of unsupported type "
                                     << db_type);
    }

    return TO();
}

////////////////////////////////////////////////////////////////////////////////
CValueConvert<SRunTimeSqlCP, CDB_Result>::CValueConvert(obj_type& value)
: m_Value(&value)
{
}

CValueConvert<SRunTimeSqlCP, CDB_Result>::operator bool(void) const
{
    typedef bool TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator Int1(void) const
{
    typedef Int1 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator Uint1(void) const
{
    typedef Uint1 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator Int2(void) const
{
    typedef Int2 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator Uint2(void) const
{
    typedef Uint2 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator Int4(void) const
{
    typedef Int4 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator Uint4(void) const
{
    typedef Uint4 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator Int8(void) const
{
    typedef Int8 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator Uint8(void) const
{
    typedef Uint8 TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator float(void) const
{
    typedef float TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator double(void) const
{
    typedef double TO;

    TO value = TO();

    ReadCDBObject(value);
    
    return value;
}


CValueConvert<SRunTimeSqlCP, CDB_Result>::operator string(void) const
{
    typedef string TO;

    const int item_num = m_Value->CurrentItemNo();
    const EDB_Type db_type = m_Value->ItemDataType(item_num);

    switch (db_type) {
    case eDB_Int:
        return ConvertFrom<TO, CDB_Int>();
    case eDB_SmallInt:
        return ConvertFrom<TO, CDB_SmallInt>();
    case eDB_TinyInt:
        return ConvertFrom<TO, CDB_TinyInt>();
    case eDB_BigInt:
        return ConvertFrom<TO, CDB_BigInt>();
    case eDB_VarChar:
        return ConvertFromStr<TO, CDB_VarChar>();
    case eDB_Char:
        return ConvertFromChar<TO, CDB_Char>(item_num);
    case eDB_VarBinary:
        return ConvertFromStr<TO, CDB_VarBinary>();
    case eDB_Binary:
        return ConvertFromChar<TO, CDB_Binary>(item_num);
    case eDB_Float:
        return ConvertFrom<TO, CDB_Float>();
    case eDB_Double:
        return ConvertFrom<TO, CDB_Double>();
    // case eDB_DateTime:
    //     return ConvertFrom<TO, CDB_DateTime>();
    // case eDB_SmallDateTime:
    //     return ConvertFrom<TO, CDB_SmallDateTime>();
    case eDB_Text:
        return ConvertFromLOB<TO, CDB_Text>();
    case eDB_Image:
        return ConvertFromLOB<TO, CDB_Image>();
    case eDB_Bit: 
        return ConvertFrom<TO, CDB_Bit>();
    case eDB_Numeric:
        return ConvertFrom<TO, CDB_Numeric>();
    case eDB_LongChar:
        return ConvertFromChar<TO, CDB_LongChar>(item_num);
    case eDB_LongBinary:
        {
            CDB_LongBinary db_obj(m_Value->ItemMaxSize(item_num));

            const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
            if (db_obj.IsNULL()) {
                return TO();
            }

            return Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.DataSize()));
        }
    default:
        NCBI_REPORT_CONVERSION_ERROR("CDB_Object of unsupported type "
                                     << db_type);
    }

    return TO();
}

// This version is different from a template one ...
CValueConvert<SRunTimeSqlCP, CDB_Result>::operator CTime(void) const
{
    typedef CTime TO;

    const int item_num = m_Value->CurrentItemNo();
    const EDB_Type db_type = m_Value->ItemDataType(item_num);

    switch (db_type) {
    case eDB_Int:
        return Convert2CTime<TO, CDB_Int>();
    case eDB_SmallInt:
        return ConvertFrom<TO, CDB_SmallInt>();
    case eDB_TinyInt:
        return ConvertFrom<TO, CDB_TinyInt>();
    case eDB_BigInt:
        return Convert2CTime<TO, CDB_BigInt>();
    case eDB_VarChar:
        return ConvertFromStr<TO, CDB_VarChar>();
    case eDB_Char:
        return ConvertFromChar<TO, CDB_Char>(item_num);
    case eDB_VarBinary:
        return ConvertFromStr<TO, CDB_VarBinary>();
    case eDB_Binary:
        return ConvertFromChar<TO, CDB_Binary>(item_num);
    case eDB_Float:
        return Convert2CTime<TO, CDB_Float>();
    case eDB_Double:
        return Convert2CTime<TO, CDB_Double>();
    // case eDB_DateTime:
    //     return Convert2CTime<TO, CDB_DateTime>();
    // case eDB_SmallDateTime:
    //     return Convert2CTime<TO, CDB_SmallDateTime>();
    case eDB_Text:
        return ConvertFromLOB<TO, CDB_Text>();
    case eDB_Image:
        return ConvertFromLOB<TO, CDB_Image>();
    case eDB_Bit: 
        return Convert2CTime<TO, CDB_Bit>();
    case eDB_Numeric:
        return ConvertFrom<TO, CDB_Numeric>();
    case eDB_LongChar:
        return ConvertFromChar<TO, CDB_LongChar>(item_num);
    case eDB_LongBinary:
        {
            CDB_LongBinary db_obj(m_Value->ItemMaxSize(item_num));

            const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
            if (db_obj.IsNULL()) {
                return TO();
            }

            return Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.DataSize()));
        }
    default:
        NCBI_REPORT_CONVERSION_ERROR("CDB_Object of unsupported type "
                                     << db_type);
    }

    return TO();
}


}

END_NCBI_SCOPE

