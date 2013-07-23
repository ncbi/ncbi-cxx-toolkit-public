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

#include <stdio.h>

#include <common/test_assert.h>  /* This header must go last */

#define MAX_RANDOM_TREE_SIZE 100
#define MAX_RANDOM_STRING_LEN 5
#define MIN_WRITE_BUFFER_SIZE ((sizeof(size_t) >> 1) * 5 + 1)

USING_NCBI_SCOPE;

class CJsonOverUTTPTestApp : public CNcbiApplication
{
public:
    CJsonOverUTTPTestApp();

    virtual int Run();

private:
    bool x_ReadOutputBuffer(size_t* packed_length);
    size_t x_ReadWriteAndCompare(char* buffer, size_t buffer_size);

    void x_MakeRandomJsonTree(size_t tree_size);
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

inline CJsonOverUTTPTestApp::CJsonOverUTTPTestApp() :
    m_RandomSeed((CRandom::TValue) time(NULL)),
    m_Random(m_RandomSeed),
    m_JSONWriter(m_UTTPWriter)
{
    LOG_POST("Using random seed " << m_RandomSeed);
}

bool CJsonOverUTTPTestApp::x_ReadOutputBuffer(size_t* packed_length)
{
    const char* output_buffer;
    size_t output_buffer_size;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        *packed_length += output_buffer_size;
        m_UTTPReader.SetNewBuffer(output_buffer, output_buffer_size);
        if (m_JSONReader.ReadMessage(m_UTTPReader)) {
            _ASSERT(!m_JSONWriter.NextOutputBuffer() &&
                    m_JSONWriter.CompleteMessage());

            string read_repr = m_JSONReader.GetMessage().Repr();

            _ASSERT(m_RandomTreeRepr == read_repr);

            return true;
        }
    } while (m_JSONWriter.NextOutputBuffer());

    return false;
}

size_t CJsonOverUTTPTestApp::x_ReadWriteAndCompare(
        char* buffer, size_t buffer_size)
{
    size_t packed_length = 0;

    m_UTTPWriter.Reset(buffer, buffer_size,
            m_RandomTreeRepr.length() > buffer_size ?
                    m_RandomTreeRepr.length() : buffer_size);

    if (!m_JSONWriter.WriteMessage(m_RandomTree))
        do
            if (x_ReadOutputBuffer(&packed_length))
                return packed_length;
        while (!m_JSONWriter.CompleteMessage());

    _ASSERT(x_ReadOutputBuffer(&packed_length));

    return packed_length;
}

int CJsonOverUTTPTestApp::Run()
{
    size_t tree_size = m_Random.GetRand(1, MAX_RANDOM_TREE_SIZE);

    x_MakeRandomJsonTree(tree_size);

    LOG_POST("Random tree size: " << tree_size);

    m_RandomTreeRepr = m_RandomTree.Repr();
    puts(m_RandomTreeRepr.c_str());

    char first_buffer[MIN_WRITE_BUFFER_SIZE];

    size_t packed_length = x_ReadWriteAndCompare(
            first_buffer, sizeof(first_buffer));

    LOG_POST("Packed tree length: " << packed_length);

    char* buffer = new char[packed_length];

    for (size_t buffer_size = MIN_WRITE_BUFFER_SIZE + 1;
            buffer_size <= packed_length; ++buffer_size) {
        m_JSONReader.Reset();
        _ASSERT(x_ReadWriteAndCompare(buffer, buffer_size) == packed_length);
    }

    delete[] buffer;

    return 0;
}

void CJsonOverUTTPTestApp::x_MakeRandomJsonTree(size_t tree_size)
{
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
        size_t container_idx = m_Random.GetRand(0, tree_size - 1);

        while (!tree_elements[container_idx].IsObject() &&
                !tree_elements[container_idx].IsArray())
            container_idx = (container_idx + 1) % tree_size;

        size_t element_idx = (container_idx +
                m_Random.GetRand(1, tree_size - 1)) % tree_size;

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
}

string CJsonOverUTTPTestApp::x_GenerateRandomString()
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

int main(int argc, const char* argv[])
{
    return CJsonOverUTTPTestApp().AppMain(argc, argv);
}
