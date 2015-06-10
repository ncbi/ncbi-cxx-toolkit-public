
/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author: Andrei Gourianov
*
*   File Description:
*       JSON Wrapper API test
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <misc/jsonwrapp/jsonwrapp.hpp>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


void Printout(size_t offset, const CJson_ConstNode& node)
{
    CJson_Node::EJsonType type = node.GetType();
    string soff(offset,' ');

    cout << "type: ";
    switch (type) {
    default:                  cout << "UNKNOWN";break;
    case CJson_Node::eNull:   cout << "null";   break;
    case CJson_Node::eBool:   cout << "bool";  break;
    case CJson_Node::eObject: cout << "object"; break;
    case CJson_Node::eArray:  cout << "array";  break;
    case CJson_Node::eString: cout << "string"; break;
    case CJson_Node::eNumber: cout << "number"; break;
    }
    cout << "; ";

    cout << "value: ";
    if (node.IsValue()) {
        CJson_ConstValue value( node.GetValue() );
        cout << node.ToString();
        if (value.IsNull()) {
            cout << "null";
        }
        if (value.IsBool()) {
            cout << (value.GetBool() ? "true" : "false");
        }
        if (value.IsNumber()) {
            cout << "number";
        }
        if (value.IsInt4()) {
            cout << " " << value.GetInt4();
        }
        if (value.IsUint4()) {
            cout << " " << value.GetUint4();
        }
        if (value.IsInt8()) {
            cout << " " << value.GetInt8();
        }
        if (value.IsUint8()) {
            cout << " " << value.GetUint8();
        }
        if (value.IsDouble()) {
            cout << " " << value.GetDouble();
        }
        if (value.IsString()) {
            cout << " " << value.GetString();
        }
    }
    if (node.IsObject()) {
        cout << "{ " << endl;
        CJson_ConstObject obj = node.GetObject();
        CJson_Object::const_iterator mi = obj.begin();
        CJson_Object::const_iterator me = obj.end();
        for( ; mi != me; ++mi) {
            CJson_ConstNode tt( mi->value);
//            tt.SetNull();
            cout << soff;
            cout << "name: " << mi->name << "; ";
            Printout( offset+2, tt );
        }
        cout << "}";
    }
    if (node.IsArray()) {
        cout << "[ " << endl << soff;
        CJson_ConstArray arr( node.GetArray());
        for (CJson_Array::const_iterator i = arr.begin();
            i != arr.end(); i++) {
            Printout( offset+2, *i );
        }
        for (size_t i = 0; i < arr.size(); i++) {
            Printout( offset+2, arr[i] );
        }
        cout << "]";
    }
    cout << endl;
}


class CCrawler : public CJson_WalkHandler
{
public:
    CCrawler(int test=0) : m_TestNo(test) {
    }
    ~CCrawler() {}

    virtual bool BeginObject(const std::string& name) {
        std::cout << "begin_object " << name << ", JPath = "
                  << GetCurrentJPath() << std::endl;
        if (m_TestNo == 1)
        {
            if (name == "obj3") {
                CJson_Document da;
                Read(da);
                cout << da;
            }
        }
        return true;
    }
    virtual bool BeginObjectMember(const std::string& name,
                                   const std::string& member) {
        std::cout << "begin_object_member " << name << "."
                  << member << ", JPath = "
                  << GetCurrentJPath() << std::endl;

        return true;
    }
    virtual bool PlainMemberValue(const std::string& name,
                                  const std::string& member,
                                  const CJson_ConstValue& /*value*/) {
        std::cout << "plain_member_value " << name
                  << "." << member << ", JPath = "
                  << GetCurrentJPath() << std::endl;
        return true;
    }
    virtual bool EndObject(const std::string& name) {
        std::cout << "end_object " << name << ",  JPath = "
                  << GetCurrentJPath() << std::endl;
        return true;
    }

    virtual bool BeginArray(const std::string& name) {
        std::cout << "begin_array " << name << ", JPath = "
                  << GetCurrentJPath() << std::endl;
        if (m_TestNo == 2)
        {
            if (name == "array") {
                CJson_Document da;
                Read(da);
                cout << da;
            }
        }
        if (m_TestNo == 3)
        {
            // abort parsing
            return false;
        }
        return true;
    }
    virtual bool BeginArrayElement(const std::string& name,
                                   size_t index) {
        std::cout << "begin_array_element " << name << "["
                  << index << "], JPath = "
                  << GetCurrentJPath() << std::endl;
        return true;
    }
    virtual bool PlainElementValue(const std::string& name,
                                   size_t index,
                                   const CJson_ConstValue& /*value*/) {
        std::cout << "plain_element_value " << name << "["
                  << index << "], JPath = "
                  << GetCurrentJPath() << std::endl;
        return true;
    }
    virtual bool EndArray(const std::string& name) {
        std::cout << "end_array " << name << ", JPath = "
                  << GetCurrentJPath() << std::endl;
        return true;
    }
private:
    int m_TestNo;
};

BOOST_AUTO_TEST_CASE(s_JsonWrapp)
{
    cout << "Size of data object is " << sizeof(rapidjson::Value) << endl;

    CJson_Document doc(CJson_Value::eObject);
    CJson_Object obj( doc.SetObject());
//will not compile
#if 0
    CJson_Node tn;
    CJson_Value tv;
    CJson_Array ta;
    CJson_Object to;
#endif

// --------------------------------------------------------------------------
// add/delete elements into object
    {
        obj["bool"].SetValue().SetBool(true);
        obj["int4"].SetValue().SetInt4(4);
        BOOST_CHECK(obj.size() == 2);
        obj.erase( obj.begin());
        BOOST_CHECK(obj.size() == 1);
        obj.erase( obj.begin(), obj.end());
        BOOST_CHECK(obj.empty());

        CJson_ConstObject o2(obj);
//will not compile
#if 0
        o2["bool"].IsValue();
#endif
        try {
            o2.at("bool").IsValue();
            BOOST_CHECK(false);
        }
        catch (std::exception& e) {
            cout << e.what() << endl;;
        }
    }


// --------------------------------------------------------------------------
// add elements into object
    obj.insert("null");
    BOOST_CHECK(obj["null"].IsNull());
    BOOST_CHECK(!obj.empty());
    BOOST_CHECK(obj.find("null") == obj.begin());

    {
        CJson_Node n1 = obj["null"];
        CJson_ConstNode n2( obj["null"]);
        n2 = n1;
//will not compile
#if 0
        CJson_Node n3(n2);
        n1 = n2;
#endif
    }

    obj.insert("bool", true);
    BOOST_CHECK(obj["bool"].IsValue());
    BOOST_CHECK(obj["bool"].GetValue().IsBool());
    BOOST_CHECK(obj["bool"].GetValue().GetBool());
    BOOST_CHECK(obj.size() == 2);

    obj.insert("int4", 4);
    BOOST_CHECK(obj["int4"].IsValue());
    BOOST_CHECK(obj["int4"].GetValue().IsNumber());
    BOOST_CHECK(obj["int4"].GetValue().IsInt4());
    BOOST_CHECK(obj["int4"].GetValue().IsInt8());
    BOOST_CHECK(obj["int4"].GetValue().GetInt4() == 4);

    obj.insert("uint4", Uint4(4));
    BOOST_CHECK(obj["uint4"].IsValue());
    BOOST_CHECK(obj["uint4"].GetValue().IsNumber());
    BOOST_CHECK(obj["uint4"].GetValue().IsUint4());
    BOOST_CHECK(obj["uint4"].GetValue().IsUint8());
    BOOST_CHECK(obj["uint4"].GetValue().GetUint4() == 4);

    obj.insert("int8", Int8(-8));
    BOOST_CHECK(obj["int8"].IsValue());
    BOOST_CHECK(obj["int8"].GetValue().IsNumber());
    BOOST_CHECK(obj["int8"].GetValue().IsInt4());
    BOOST_CHECK(obj["int8"].GetValue().IsInt8());
    BOOST_CHECK(obj["int8"].GetValue().GetInt8() == -8);

    obj.insert("uint8", Uint8(8));
    BOOST_CHECK(obj["uint8"].IsValue());
    BOOST_CHECK(obj["uint8"].GetValue().IsNumber());
    BOOST_CHECK(obj["uint8"].GetValue().IsUint4());
    BOOST_CHECK(obj["uint8"].GetValue().IsUint8());
    BOOST_CHECK(obj["uint8"].GetValue().GetUint8() == 8);

    obj["double"].SetValue().SetDouble(12.34);
    BOOST_CHECK(obj["double"].GetValue().IsNumber());
    BOOST_CHECK(obj["double"].GetValue().IsDouble());
    BOOST_CHECK(!obj["double"].GetValue().IsUint4());
    BOOST_CHECK(obj["double"].GetValue().GetDouble() == 12.34);

    obj.insert("float", float(34));
    BOOST_CHECK(obj["float"].GetValue().IsNumber());
    BOOST_CHECK(obj["float"].GetValue().IsDouble());
    BOOST_CHECK(!obj["float"].GetValue().IsInt4());

    obj.insert("string", "stringvalue");
    BOOST_CHECK(!obj["string"].GetValue().IsNumber());
    BOOST_CHECK(obj["string"].GetValue().IsString());
    obj.erase("string");
    BOOST_CHECK(!obj.has("string"));
    obj["string"].SetValue().SetString("stringvalue");
    BOOST_CHECK(obj.has("string"));
    BOOST_CHECK(obj.size() == 9);

    BOOST_CHECK(obj.find("bool") != obj.end());
    BOOST_CHECK(obj.find("double") != obj.end());

    {
        CJson_Object o2 = obj.insert_object("obj2");
        o2.insert("one", 1);

        CJson_Object o3 = o2.insert_object("obj3");
        BOOST_CHECK(!o3.IsNull());
        o3.insert("two", 2);
        CJson_Array a1 = o3.insert_array("array");
        BOOST_CHECK(!a1.IsNull());

// --------------------------------------------------------------------------
// add/delete elements into array
        a1.push_back(1);
        a1.erase(a1.begin());
        BOOST_CHECK(a1.empty());
        a1.push_back(1);
        a1.push_back("two");
        a1.push_back(false);
        CJson_Array::iterator ee = a1.erase(a1.begin(), a1.begin()+2);
        BOOST_CHECK(ee->IsValue());
        BOOST_CHECK(ee->GetValue().IsBool());
        a1.clear();
        BOOST_CHECK(a1.empty());

// --------------------------------------------------------------------------
// add/ elements into array
        a1.push_back(1);
        a1.push_back("two");
        a1.push_back(false);
        a1.push_back();
        CJson_Array a2 = a1.push_back_array();
        a2.push_back(2);
        a2.push_back("three");

        CJson_Object o4 = a1.push_back_object();
        o4["one"].SetValue().SetInt4(1);
        o4["two"].SetValue().SetString("2");
        a1.push_back("last");
    }

    BOOST_CHECK(obj["obj2"].IsObject());
    BOOST_CHECK(obj.has("null"));
    obj.erase("null");
    BOOST_CHECK(!obj.has("null"));
    obj.insert("null");
    BOOST_CHECK(obj.has("null"));

// --------------------------------------------------------------------------
// object iterators
    {
        CJson_Object::iterator ci;
        CJson_Object::iterator ci0 = obj.begin();
        ci = obj.end();
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci = ci0;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
        CJson_Object::iterator ci1(ci);
        BOOST_CHECK(ci1 == ci0);
        BOOST_CHECK(ci0 == ci1);
        ++ci;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci--;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
    }
    {
        CJson_Object::const_iterator ci;
        CJson_Object::const_iterator ci0 = obj.begin();
        ci = obj.end();
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci = ci0;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
        CJson_Object::const_iterator ci1(ci);
        BOOST_CHECK(ci1 == ci0);
        BOOST_CHECK(ci0 == ci1);
        ++ci;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci--;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
    }
    {
        CJson_Object::const_iterator ci;
        CJson_Object::iterator ci0 = obj.begin();
        ci = obj.end();
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci = ci0;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
        CJson_Object::const_iterator ci1(ci);
// will not compile
//            CJson_Object::iterator ci2(ci);
        BOOST_CHECK(ci1 == ci0);
        BOOST_CHECK(ci0 == ci1);
        ++ci;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci--;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
    }

// --------------------------------------------------------------------------
// traversing object elements
    for (CJson_Object::iterator i = obj.begin(); i != obj.end(); ++i) {
        CJson_Node v = i->value;
        cout << i->name << endl;
    }
    for (CJson_ConstObject::const_iterator i = obj.begin(); i != obj.end(); ++i) {
        CJson_ConstNode vc = i->value;
// will not compile
//        CJson_Node v = i->value;
        cout << i->name << endl;
    }
    for (CJson_ConstObject::const_iterator j = obj.begin(); j != obj.end(); ++j) {
        cout << j->name << ": " << j->value << endl;
    }
    ITERATE (CJson_ConstObject, j, obj) {
        cout << j->name << ": " << j->value << endl;
    }
    ITERATE (CJson_Object, j, obj) {
        cout << j->name << ": " << j->value << endl;
    }
    NON_CONST_ITERATE (CJson_ConstObject, j, obj) {
        cout << j->name << ": " << j->value << endl;
    }
    NON_CONST_ITERATE (CJson_Object, j, obj) {
        cout << j->name << ": " << j->value << endl;
    }
    {
        CJson_ConstObject::const_iterator j = obj.find("bool");
        if (j != obj.end()) {
            cout << j->name << ": " << j->value << endl;
        }
    }
#if NCBI_HAVE_CXX11
    for_each(obj.begin(), obj.end(), [](const CJson_ConstObject::const_iterator::pair& v) {
        cout << v.name << ": " << v.value << endl;
    });
    for_each(obj.begin(), obj.end(), [](const CJson_ConstObject_pair& v) {
        cout << v.name << ": " << v.value << endl;
    });
    for_each(obj.begin(), obj.end(), [](CJson_Object_pair& v) {
        cout << v.name << ": " << v.value << endl;
    });
    for(const CJson_ConstObject::const_iterator::pair& v : obj) {
        cout << v.name << ": " << v.value << endl;
    }
    for(const CJson_ConstObject_pair& v : obj) {
        cout << v.name << ": " << v.value << endl;
    }
    for(CJson_Object::iterator::pair& v : obj) {
        cout << v.name << ": " << v.value << endl;
    }
    for(CJson_Object_pair& v : obj) {
        cout << v.name << ": " << v.value << endl;
    }
#endif

// --------------------------------------------------------------------------
// array element access
    CJson_Array arr =
        obj["obj2"].SetObject().at("obj3").SetObject().at("array").SetArray();

    BOOST_CHECK(!arr.empty());
    BOOST_CHECK(arr.size() == 7);
    BOOST_CHECK(arr.back().GetValue().IsString());
    arr.push_back();
    BOOST_CHECK(arr.size() == 8);
    BOOST_CHECK(arr.back().IsNull());
    arr.pop_back();
    BOOST_CHECK(arr.size() == 7);
    BOOST_CHECK(arr.back().GetValue().IsString());
    BOOST_CHECK(arr.front().GetValue().IsNumber());
    BOOST_CHECK(arr[2].GetValue().IsBool());
    BOOST_CHECK(
        obj["obj2"].GetObject().at("obj3").GetObject().at("array").
            GetArray().at(1).GetValue().IsString());
    BOOST_CHECK(obj["obj2"].SetObject()["obj3"].SetObject()["array"].GetArray()[1].GetValue().IsString());
    BOOST_CHECK(obj["obj2"].GetObject()["obj3"].GetObject()["array"].GetArray()[1].GetValue().IsString());
    try {
        arr.at(14).IsObject();
        BOOST_CHECK(false);
    }
    catch (std::exception& e) {
        cout << e.what() << endl;;
    }

// --------------------------------------------------------------------------
// array iterators
    {
        CJson_Array::iterator ci;
        CJson_Array::iterator ci0 = arr.begin();
        ci = ci0 + 2;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci - ci0 == 2);
        ci -= 2;
        BOOST_CHECK(ci == ci0);
        ci = arr.end();
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci = ci0;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
        CJson_Array::iterator ci1(ci);
        BOOST_CHECK(ci1 == ci0);
        BOOST_CHECK(ci0 == ci1);
        ++ci;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci--;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
    }
    {
        CJson_Array::const_iterator ci;
        CJson_Array::const_iterator ci0 = arr.begin();
        ci = ci0 + 2;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci - ci0 == 2);
        ci -= 2;
        BOOST_CHECK(ci == ci0);
        ci = arr.end();
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci = ci0;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
        CJson_Array::const_iterator ci1(ci);
        BOOST_CHECK(ci1 == ci0);
        BOOST_CHECK(ci0 == ci1);
        ++ci;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci--;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
    }
    {
        CJson_Array::const_iterator ci;
        CJson_Array::iterator ci0 = arr.begin();
        ci = ci0 + 2;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci - ci0 == 2);
        ci -= 2;
        BOOST_CHECK(ci == ci0);
        ci = arr.end();
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci = ci0;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
        CJson_Array::const_iterator ci1(ci);
// will not compile
//            CJson_Array::iterator ci2(ci);
        BOOST_CHECK(ci1 == ci0);
        BOOST_CHECK(ci0 == ci1);
        ++ci;
        BOOST_CHECK(ci != ci0);
        BOOST_CHECK(ci0 != ci);
        ci--;
        BOOST_CHECK(ci == ci0);
        BOOST_CHECK(ci0 == ci);
    }

// --------------------------------------------------------------------------
// traversing array elements
    for (CJson_Array::iterator i = arr.begin(); i != arr.end(); ++i) {
        CJson_Node v = *i;
        if (i->IsValue() && i->GetValue().IsString()) {
            cout << v.GetValue().GetString() << endl;
        }
    }
    for (CJson_ConstArray::const_iterator i = arr.begin(); i != arr.end(); ++i) {
// will not compile
            //CJson_Node v = *i;
        if (i->IsValue() && i->GetValue().IsString()) {
            cout << i->GetValue().GetString() << endl;
        }
    }
    ITERATE (CJson_ConstArray, i, arr) {
        cout << *i;
    }
    ERASE_ITERATE (CJson_ConstArray, i, arr) {
        cout << *i;
    }
    ITERATE (CJson_Array, i, arr) {
        cout << *i;
    }
    NON_CONST_ITERATE (CJson_ConstArray, i, arr) {
        cout << *i;
    }
    NON_CONST_ITERATE (CJson_Array, i, arr) {
        cout << *i;
    }
#if NCBI_HAVE_CXX11
    for_each(arr.begin(), arr.end(), [](const CJson_ConstNode& v) {
        Printout(0,v);
    });
    for(const CJson_ConstNode& v: arr) {
        cout << v;
    }
    for(CJson_Node& v: arr) {
        cout << v;
    }
#endif


    {
// --------------------------------------------------------------------------
// new document from UTF8 string
        CJson_Document docs("{\"null\": null, \"bool\": true, \"str\": \"str\"}");
        cout << docs;
        BOOST_CHECK(docs.ReadSucceeded());
        BOOST_CHECK(docs.IsObject());
        BOOST_CHECK(docs.GetObject().at("bool").IsValue());
        BOOST_CHECK(docs.GetObject().at("bool").GetValue().IsBool());
        BOOST_CHECK(docs.GetObject().at("bool").GetValue().GetBool());

        docs.ParseString("[\"utf8 string expected\", false, null ]");
        cout << docs;
        BOOST_CHECK(docs.ReadSucceeded());
        BOOST_CHECK(docs.IsArray());
        BOOST_CHECK(docs.GetArray().size() == 3);
        BOOST_CHECK(docs.GetArray().at(1).IsValue());
        BOOST_CHECK(docs.GetArray().at(1).GetValue().IsBool());
        BOOST_CHECK(!docs.GetArray().at(1).GetValue().GetBool());

// --------------------------------------------------------------------------
// new document from array
        CJson_Document doc0(arr);
        cout << arr;
        cout << doc0;

// --------------------------------------------------------------------------
// copy array into another array
        CJson_Document doc1(CJson_Value::eArray);
        CJson_Array arrDst = doc1.SetArray();
        copy(arr.begin(), arr.end(), back_inserter(arrDst));
        BOOST_CHECK(arr == arrDst);
        BOOST_CHECK(arrDst == arr);

        CJson_Document doc2(CJson_Value::eArray);
        CJson_Array arrDst2 = doc2.SetArray();
        arrDst2.AssignCopy(arr);
        BOOST_CHECK(arr == arrDst2);
        BOOST_CHECK(arrDst == arrDst2);

// --------------------------------------------------------------------------
// find an element in array and change it
        CJson_Array::iterator fi = find(arr.begin(), arr.end(),
            CJson_Document(CJson_Node::eBool).SetValue().SetBool(false));
        BOOST_CHECK(fi != arr.end());
        BOOST_CHECK(fi->IsValue());
        BOOST_CHECK(fi->GetValue().IsBool());
        BOOST_CHECK(!fi->GetValue().GetBool());
        fi->SetValue().SetBool(true);
        BOOST_CHECK(fi->GetValue().GetBool());

        BOOST_CHECK(1 == count(arr.begin(), arr.end(),
            CJson_Document(CJson_Node::eBool).SetValue().SetBool(true)));
#if NCBI_HAVE_CXX11
        for_each(arrDst2.begin(), arrDst2.end(), [](CJson_Node& v) {
            v.SetNull();
        });
        BOOST_CHECK(arrDst2.size() == 
                    count(arrDst2.begin(), arrDst2.end(), CJson_Document(CJson_Node::eNull)));
        BOOST_CHECK(all_of(arrDst2.begin(), arrDst2.end(),
                    [](const CJson_ConstNode& v)->bool {return v.IsNull();}));
#endif
// --------------------------------------------------------------------------
// push_back array into array
        arrDst.push_back( arr);
        cout << arr;
        cout << arrDst;

// --------------------------------------------------------------------------
// copy object into another object
        CJson_Document doc3(CJson_Value::eObject);
        doc3.SetObject().AssignCopy( doc.GetObject());
        BOOST_CHECK(doc == doc3);
        BOOST_CHECK(doc.SetObject() == doc3.SetObject());
        BOOST_CHECK(doc.GetObject() == doc3.SetObject());
        cout << doc;
        cout << doc3;

// --------------------------------------------------------------------------
// create copy of a document
        CJson_Document cpy(doc);
        BOOST_CHECK(doc == cpy);
        BOOST_CHECK(doc.SetObject() == cpy.SetObject());
        BOOST_CHECK(doc.GetObject() == cpy.GetObject());

        CJson_Document cpy2;
        cpy2 = doc;
        BOOST_CHECK(cpy2 == cpy);

// --------------------------------------------------------------------------
// insert array into object
        cpy.SetObject().insert("arr", arr);
        cout << cpy;
        cout << arr;

// --------------------------------------------------------------------------
// insert object into object
        cpy.SetObject().insert("ooo", cpy.GetObject());
        cout << cpy;
    }

// --------------------------------------------------------------------------
// serialization
    string filename( CDirEntry::GetTmpName() );
    for (CJson_Array::const_iterator i = arr.begin(); i != arr.end(); ++i) {
        CJson_Document doct(*i);
        ofstream ofs(filename.c_str());
        ofs << doct;
        ofs.close();
        ifstream ifs(filename.c_str());
        if (ifs.is_open()) {
            CJson_Document doct2;
            ifs >> doct2;
            BOOST_CHECK(doct == doct2);
        }
    }
    {
        ofstream ofs(filename.c_str());
        ofs << doc;
    }

// --------------------------------------------------------------------------
// SAX parsing
    {
        CCrawler wlk;
        doc.Walk(wlk);
    }
    {
        ifstream ifs(filename.c_str());
        if (ifs.is_open()) {
            CCrawler wlk2(0);
            CJson_Document::Walk(ifs,wlk2);
        }
    }
    {
        ifstream ifs(filename.c_str());
        if (ifs.is_open()) {
            CCrawler wlk2(1);
            CJson_Document::Walk(ifs,wlk2);
        }
    }
    {
        ifstream ifs(filename.c_str());
        if (ifs.is_open()) {
            CCrawler wlk2(2);
            CJson_Document::Walk(ifs,wlk2);
        }
    }
    {
        ifstream ifs(filename.c_str());
        if (ifs.is_open()) {
            CCrawler wlk3(3);
            CJson_Document::Walk(ifs,wlk3);
        }
    }

// --------------------------------------------------------------------------
// serialization
    {
        ifstream ifs(filename.c_str());
        if (ifs.is_open()) {
            CJson_Document doc2;
            ifs >> doc2;
        }
    }
    CFile(filename).Remove();
}

