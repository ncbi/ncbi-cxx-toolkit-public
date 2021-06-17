/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Authors:
 *   Dmitry Kazimirov
 *
 * File Description:
 *   JSON over UTTP test.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/json_over_uttp.hpp>

#include <util/random_gen.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <corelib/test_boost.hpp>

#include <stdio.h>

#include <common/test_assert.h>  /* This header must go last */

#define MAX_RANDOM_TREE_SIZE 100
#define MAX_RANDOM_STRING_LEN 5
#define MIN_WRITE_BUFFER_SIZE ((sizeof(size_t) >> 1) * 5 + 1)

USING_NCBI_SCOPE;

class CJsonOverUTTPTest
{
public:
    CJsonOverUTTPTest();

    void MakeRandomJsonTree();
    size_t ReadWriteAndCompare(char* buffer, size_t buffer_size);

private:
    bool x_ReadOutputBuffer(size_t* packed_length);
    string x_GenerateRandomString();

    CRandom::TValue m_RandomSeed;
    CRandom m_Random;

    CJsonNode m_RandomTree;
    string m_RandomTreeRepr;

    CUTTPReader m_UTTPReader;
    CJsonOverUTTPReader m_JSONReader;

    CUTTPWriter m_UTTPWriter;
    CJsonOverUTTPWriter m_JSONWriter;
};

inline CJsonOverUTTPTest::CJsonOverUTTPTest() :
    m_RandomSeed((CRandom::TValue) time(NULL)),
    m_Random(m_RandomSeed),
    m_JSONWriter(m_UTTPWriter)
{
    LOG_POST("Using random seed " << m_RandomSeed);
}

bool CJsonOverUTTPTest::x_ReadOutputBuffer(size_t* packed_length)
{
    const char* output_buffer;
    size_t output_buffer_size;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        *packed_length += output_buffer_size;
        m_UTTPReader.SetNewBuffer(output_buffer, output_buffer_size);
        if (m_JSONReader.ReadMessage(m_UTTPReader)) {
            BOOST_CHECK(!m_JSONWriter.NextOutputBuffer() &&
                    m_JSONWriter.CompleteMessage());

            string read_repr = m_JSONReader.GetMessage().Repr();

            BOOST_CHECK(m_RandomTreeRepr == read_repr);

            return true;
        }
    } while (m_JSONWriter.NextOutputBuffer());

    return false;
}

size_t CJsonOverUTTPTest::ReadWriteAndCompare(char* buffer, size_t buffer_size)
{
    size_t packed_length = 0;

    m_JSONReader.Reset();
    m_UTTPWriter.Reset(buffer, buffer_size,
            m_RandomTreeRepr.length() > buffer_size ?
                    m_RandomTreeRepr.length() : buffer_size);

    if (!m_JSONWriter.WriteMessage(m_RandomTree))
        do
            if (x_ReadOutputBuffer(&packed_length))
                return packed_length;
        while (!m_JSONWriter.CompleteMessage());

    BOOST_CHECK(x_ReadOutputBuffer(&packed_length));

    return packed_length;
}

BOOST_AUTO_TEST_CASE(JsonOverUTTPTest)
{
    CJsonOverUTTPTest test;

    test.MakeRandomJsonTree();

    char first_buffer[MIN_WRITE_BUFFER_SIZE];

    size_t packed_length = test.ReadWriteAndCompare(
            first_buffer, sizeof(first_buffer));

    LOG_POST("Packed tree length: " << packed_length);

    char* buffer = new char[packed_length];

    for (size_t buffer_size = MIN_WRITE_BUFFER_SIZE + 1;
            buffer_size <= packed_length; ++buffer_size) {
        BOOST_CHECK(test.ReadWriteAndCompare(buffer,
                buffer_size) == packed_length);
    }

    delete[] buffer;
}


vector<const char*> plain = {
    "\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0",
    "\xef\xee\xed\xec\xeb\xea\xe9\xe8\xe7\xe6\xe5\xe4\xe3\xe2\xe1\xe0",
    "\xdf\xde\xdd\xdc\xdb\xda\xd9\xd8\xd7\xd6\xd5\xd4\xd3\xd2\xd1\xd0",
    "\xcf\xce\xcd\xcc\xcb\xca\xc9\xc8\xc7\xc6\xc5\xc4\xc3\xc2\xc1\xc0",
    "\xbf\xbe\xbd\xbc\xbb\xba\xb9\xb8\xb7\xb6\xb5\xb4\xb3\xb2\xb1\xb0",
    "\xaf\xae\xad\xac\xab\xaa\xa9\xa8\xa7\xa6\xa5\xa4\xa3\xa2\xa1\xa0",
    "\x9f\x9e\x9d\x9c\x9b\x9a\x99\x98\x97\x96\x95\x94\x93\x92\x91\x90",
    "\x8f\x8e\x8d\x8c\x8b\x8a\x89\x88\x87\x86\x85\x84\x83\x82\x81\x80",
    "\x7f\x7e\x7d\x7c\x7b\x7a\x79\x78\x77\x76\x75\x74\x73\x72\x71\x70",
    "\x6f\x6e\x6d\x6c\x6b\x6a\x69\x68\x67\x66\x65\x64\x63\x62\x61\x60",
    "\x5f\x5e\x5d\x5c\x5b\x5a\x59\x58\x57\x56\x55\x54\x53\x52\x51\x50",
    "\x4f\x4e\x4d\x4c\x4b\x4a\x49\x48\x47\x46\x45\x44\x43\x42\x41\x40",
    "\x3f\x3e\x3d\x3c\x3b\x3a\x39\x38\x37\x36\x35\x34\x33\x32\x31\x30",
    "\x2f\x2e\x2d\x2c\x2b\x2a\x29\x28\x27\x26\x25\x24\x23\x22\x21\x20",
    "\x1f\x1e\x1d\x1c\x1b\x1a\x19\x18\x17\x16\x15\x14\x13\x12\x11\x10",
    "\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01",
};

vector<pair<CJsonNode::TReprFlags, vector<const char*>>> serialized = {
    {
        0,
        {
            "[\"\\377\\376\\375\\374\\373\\372\\371\\370\\367\\366\\365\\364\\363\\362\\361\\360\"]",
            "[\"\\357\\356\\355\\354\\353\\352\\351\\350\\347\\346\\345\\344\\343\\342\\341\\340\"]",
            "[\"\\337\\336\\335\\334\\333\\332\\331\\330\\327\\326\\325\\324\\323\\322\\321\\320\"]",
            "[\"\\317\\316\\315\\314\\313\\312\\311\\310\\307\\306\\305\\304\\303\\302\\301\\300\"]",
            "[\"\\277\\276\\275\\274\\273\\272\\271\\270\\267\\266\\265\\264\\263\\262\\261\\260\"]",
            "[\"\\257\\256\\255\\254\\253\\252\\251\\250\\247\\246\\245\\244\\243\\242\\241\\240\"]",
            "[\"\\237\\236\\235\\234\\233\\232\\231\\230\\227\\226\\225\\224\\223\\222\\221\\220\"]",
            "[\"\\217\\216\\215\\214\\213\\212\\211\\210\\207\\206\\205\\204\\203\\202\\201\\200\"]",
            "[\"\\177~}|{zyxwvutsrqp\"]",
            "[\"onmlkjihgfedcba`\"]",
            "[\"_^]\\\\[ZYXWVUTSRQP\"]",
            "[\"ONMLKJIHGFEDCBA@\"]",
            "[\"?>=<;:9876543210\"]",
            "[\"/.-,+*)(\\\'&%$#\\\"! \"]",
            "[\"\\37\\36\\35\\34\\33\\32\\31\\30\\27\\26\\25\\24\\23\\22\\21\\20\"]",
            "[\"\\17\\16\\r\\f\\v\\n\\t\\b\\a\\6\\5\\4\\3\\2\\1\"]",
        },
    },
    {
        CJsonNode::fStandardJson,
        {
            "[\"\377\376\375\374\373\372\371\370\367\366\365\364\363\362\361\360\"]",
            "[\"\357\356\355\354\353\352\351\350\347\346\345\344\343\342\341\340\"]",
            "[\"\337\336\335\334\333\332\331\330\327\326\325\324\323\322\321\320\"]",
            "[\"\317\316\315\314\313\312\311\310\307\306\305\304\303\302\301\300\"]",
            "[\"\277\276\275\274\273\272\271\270\267\266\265\264\263\262\261\260\"]",
            "[\"\257\256\255\254\253\252\251\250\247\246\245\244\243\242\241\240\"]",
            "[\"\237\236\235\234\233\232\231\230\227\226\225\224\223\222\221\220\"]",
            "[\"\217\216\215\214\213\212\211\210\207\206\205\204\203\202\201\200\"]",
            "[\"\177~}|{zyxwvutsrqp\"]",
            "[\"onmlkjihgfedcba`\"]",
            "[\"_^]\\\\[ZYXWVUTSRQP\"]",
            "[\"ONMLKJIHGFEDCBA@\"]",
            "[\"?>=<;:9876543210\"]",
            "[\"/.-,+*)(\'&%$#\\\"! \"]",
            "[\"\\u001f\\u001e\\u001d\\u001c\\u001b\\u001a\\u0019\\u0018\\u0017\\u0016\\u0015\\u0014\\u0013\\u0012\\u0011\\u0010\"]",
            "[\"\\u000f\\u000e\\u000d\\u000c\\u000b\\u000a\\u0009\\u0008\\u0007\\u0006\\u0005\\u0004\\u0003\\u0002\\u0001\"]",
        },
    },
};

BOOST_AUTO_TEST_CASE(JsonStringSerialize)
{
    CJsonNode       node = CJsonNode::NewStringNode("test");
    BOOST_CHECK(node.Repr() == string("\"test\""));

    CJsonNode       node1 = CJsonNode::NewStringNode("te'st");
    BOOST_CHECK(node1.Repr(CJsonNode::fStandardJson) == string("\"te'st\""));

    // Default behavior is non standard for JSON
    CJsonNode       node2 = CJsonNode::NewStringNode("te'st");
    BOOST_CHECK(node2.Repr() == string("\"te\\'st\""));

    for (const auto& current : serialized) {
        assert(current.second.size() == plain.size());

        for (size_t i = 0; i < plain.size(); ++i) {
            auto node = CJsonNode::NewArrayNode();
            node.AppendString(plain[i]);
            auto result = node.Repr(current.first);
            BOOST_CHECK_EQUAL(result, current.second[i]);
        }
    }
}


BOOST_AUTO_TEST_CASE(JsonStringDeserialize)
{
    string      json = "[\"test\"]";
    CJsonNode   node = CJsonNode::ParseJSON(json);
    BOOST_CHECK(node.GetAt(0).AsString() == string("test"));

    string      json1 = "[\"te'st\"]";
    CJsonNode   node1 = CJsonNode::ParseJSON(json1);
    BOOST_CHECK(node1.GetAt(0).AsString() == string("te'st"));

    string      json2 = "[\"te\\'st\"]";
    CJsonNode   node2 = CJsonNode::ParseJSON(json2);
    BOOST_CHECK(node2.GetAt(0).AsString() == string("te'st"));

    for (const auto& current : serialized) {
        assert(current.second.size() == plain.size());

        for (size_t i = 0; i < plain.size(); ++i) {
            auto node = CJsonNode::ParseJSON(current.second[i], current.first);
            auto result = node.GetAt(0);
            BOOST_CHECK_EQUAL(result.AsString(), plain[i]);
        }
    }
}


void CJsonOverUTTPTest::MakeRandomJsonTree()
{
    size_t tree_size = m_Random.GetRand(1, MAX_RANDOM_TREE_SIZE);

    LOG_POST("Random tree size: " << tree_size);

    CJsonNode* tree_elements = new CJsonNode[tree_size];

    bool have_containter;

    do {
        have_containter = false;
        for (size_t i = 0; i < tree_size; ++i)
            switch (m_Random.GetRand(0, 6)) {
            case 0:
                tree_elements[i] = CJsonNode::NewObjectNode();
                have_containter = true;
                break;
            case 1:
                tree_elements[i] = CJsonNode::NewArrayNode();
                have_containter = true;
                break;
            case 2:
                tree_elements[i] =
                    CJsonNode::NewStringNode(x_GenerateRandomString());
                break;
            case 3:
                tree_elements[i] = CJsonNode::NewIntegerNode(m_Random.GetRand());
                break;
            case 4:
                tree_elements[i] = CJsonNode::NewDoubleNode(
                        (m_Random.GetRand() * 1000.0) / m_Random.GetMax());
                break;
            case 5:
                tree_elements[i] = CJsonNode::NewBooleanNode(
                        (m_Random.GetRand() & 1) != 0);
                break;
            case 6:
                tree_elements[i] = CJsonNode::NewNullNode();
            }
    } while (!have_containter && tree_size > 1);

    while (tree_size > 1) {
        size_t container_idx = m_Random.GetRandIndex((CRandom::TValue)tree_size);

        while (!tree_elements[container_idx].IsObject() &&
                !tree_elements[container_idx].IsArray())
            container_idx = (container_idx + 1) % tree_size;

        size_t element_idx = (container_idx +
                m_Random.GetRand(1, (CRandom::TValue)tree_size - 1)) % tree_size;

        if (tree_elements[container_idx].IsObject()) {
            string key;
            do
                key = x_GenerateRandomString();
            while (tree_elements[container_idx].HasKey(key));
            tree_elements[container_idx].SetByKey(key,
                    tree_elements[element_idx]);
        } else
            tree_elements[container_idx].Append(tree_elements[element_idx]);

        if (--tree_size != element_idx)
            tree_elements[element_idx] = tree_elements[tree_size];
        tree_elements[tree_size] = NULL;
    }

    m_RandomTree = *tree_elements;

    delete[] tree_elements;

    m_RandomTreeRepr = m_RandomTree.Repr();
}

string CJsonOverUTTPTest::x_GenerateRandomString()
{
    static const char* consonant[] = {
        "b", "c", "d", "f", "g", "h", "j", "k", "l", "m", "n", "p", "q",
        "r", "s", "t", "v", "x", "z", "w", "y", "ch", "sh", "th", "zh"
    };
    static const char* vowel[] = {
        "a", "e", "i", "o", "u", "y", "ou", "io", "ua", "ea", "ia",
        "ee", "ue", "ai", "ie", "ei", "oi", "ui", "oo", "au",
        "uo", "oe", "oa", "eu", "aa", "ae", "eo", "iu", "ao"
    };

    int len = m_Random.GetRand(1, MAX_RANDOM_STRING_LEN);

    string random_string;

    int char_class = 0;

    do
        random_string += (char_class ^= 1) ?
                consonant[m_Random.GetRand(0,
                        sizeof(consonant) / sizeof(*consonant) - 1)] :
                vowel[m_Random.GetRand(0,
                        sizeof(vowel) / sizeof(*vowel) - 1)];
    while (--len > 0);

    return random_string;
}
