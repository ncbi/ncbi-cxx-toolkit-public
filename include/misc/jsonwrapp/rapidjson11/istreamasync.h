// Tencent is pleased to support the open source community by making RapidJSON available.
// 
// Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip. All rights reserved.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed 
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR 
// CONDITIONS OF ANY KIND, either express or implied. See the License for the 
// specific language governing permissions and limitations under the License.

/*
* Asynchronous variant of BasicIStreamWrapper
* Author: Andrei Gourianov, gouriano@ncbi
*/

#ifndef RAPIDJSON_ISTREAMASYNC_H_
#define RAPIDJSON_ISTREAMASYNC_H_

#include "stream.h"
#include <iosfwd>
#include <future>

#ifdef __clang__
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(padded)
#endif

#ifdef _MSC_VER
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(4351) // new behavior: elements of array 'array' will be default initialized
#endif

RAPIDJSON_NAMESPACE_BEGIN

//! Wrapper of \c std::basic_istream into RapidJSON's Stream concept.
/*!
    The classes can be wrapped including but not limited to:

    - \c std::istringstream
    - \c std::stringstream
    - \c std::wistringstream
    - \c std::wstringstream
    - \c std::ifstream
    - \c std::fstream
    - \c std::wifstream
    - \c std::wfstream

    \tparam StreamType Class derived from \c std::basic_istream.
*/
   
template <typename StreamType>
class BasicIStreamAsyncWrapper {
public:
    typedef typename StreamType::char_type Ch;

    BasicIStreamAsyncWrapper(StreamType& stream, size_t bufferSize = 64*1024)
        : stream_(stream), count_(0), bufferSize_(bufferSize)
    {
        buffer[0] = std::make_unique< Ch[] >(bufferSize_);
        buffer[1] = std::make_unique< Ch[] >(bufferSize_);
        destbuffer = 0;
        bufferLast_ = current_ = buffer[destbuffer].get();
        eof_ = false;
        pos_ = stream_.tellg();
        RequestData();
        AcquireData();
    }

    ~BasicIStreamAsyncWrapper() {
        stream_.clear();
        stream_.seekg(pos_ + count_);
    }
    Ch Peek() const { 
        return *current_;
    }

    Ch Take() { 
        Ch c = *current_;
        Read();
        return c;
    }

    // tellg() may return -1 when failed. So we count by ourself.
    size_t Tell() const { return count_; }

    Ch* PutBegin() { RAPIDJSON_ASSERT(false); return 0; }
    void Put(Ch) { RAPIDJSON_ASSERT(false); }
    void Flush() { RAPIDJSON_ASSERT(false); }
    size_t PutEnd(Ch*) { RAPIDJSON_ASSERT(false); return 0; }

    // For encoding detection only.
    const Ch* Peek4() const {
        RAPIDJSON_ASSERT(false); return 0;
    }

private:
    BasicIStreamAsyncWrapper(const BasicIStreamAsyncWrapper&);
    BasicIStreamAsyncWrapper& operator=(const BasicIStreamAsyncWrapper&);

    StreamType& stream_;
    size_t count_;  //!< Number of characters read. Note:

    size_t pos_;
    size_t bufferSize_;
    std::unique_ptr<Ch[]> buffer[2];
    int destbuffer;
    Ch *current_;
    Ch *bufferLast_;
    bool eof_;
    std::future< size_t > ft;

    void RequestData() {
        ft = std::async( std::launch::async, [&]() {
            return (size_t)stream_.rdbuf()->sgetn(buffer[destbuffer].get(), bufferSize_);
        });
    }
    void AcquireData() {
        if (!eof_ && current_ == bufferLast_) {
            current_ = buffer[destbuffer].get();
            bufferLast_ = current_ + ft.get();
            destbuffer = (destbuffer + 1)%2;
            if (current_ != bufferLast_) {
                RequestData();
            }
        }
    }
    void Read() {
        if (current_ < bufferLast_) {
            ++current_;
            ++count_;
        }
        if (!eof_ && current_ == bufferLast_) {
            AcquireData();
            if (current_ == bufferLast_) {
                eof_ = true;
                *current_ = Ch(0);
            }
        }
    }
};

typedef BasicIStreamAsyncWrapper<std::istream> IStreamAsyncWrapper;
//typedef BasicIStreamWrapper<std::wistream> WIStreamWrapper;

#if defined(__clang__) || defined(_MSC_VER)
RAPIDJSON_DIAG_POP
#endif

RAPIDJSON_NAMESPACE_END

#endif // RAPIDJSON_ISTREAMASYNC_H_
