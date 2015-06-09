#ifndef RAPIDJSON_FILESTREAM_H_
#define RAPIDJSON_FILESTREAM_H_

#include <cstdio>

namespace rapidjson {

//! Wrapper of C file stream for input or output.
/*!
	This simple wrapper does not check the validity of the stream.
	\implements Stream
*/
class FileStream {
public:
	typedef char Ch;	//!< Character type. Only support char.

	FileStream(FILE* fp) : fp_(fp), count_(0) { Read(); }
	char Peek() const { return current_; }
	char Take() { char c = current_; Read(); return c; }
	size_t Tell() const { return count_; }
	void Put(char c) { fputc(c, fp_); }

	// Not implemented
	char* PutBegin() { return 0; }
	size_t PutEnd(char*) { return 0; }

private:
	void Read() {
		RAPIDJSON_ASSERT(fp_ != 0);
		int c = fgetc(fp_);
		if (c != EOF) {
			current_ = (char)c;
			count_++;
		}
		else
			current_ = '\0';
	}

	FILE* fp_;
	char current_;
	size_t count_;
};

//NCBI: added CppIStream  and CppOStream classes
#if 1
class CppIStream {
public:

	CppIStream(std::istream& in) : in_(&in), count_(0) {
    }
    CppIStream& operator=(CppIStream& i) {
        in_ = i.in_;
        current_ = i.current_;
        count_ = i.count_;
        return *this;
    }

	char Peek() const {
        char c = in_->peek();
        if (c == std::istream::traits_type::eof()) { c='\0';}
        return c;
    }
	char Take() {
        char c = in_->get();
        return c;
    }
	size_t Tell() const {
        return (size_t)(in_->gcount());
    }
	void Put(char) {
    }

	// Not implemented
	char* PutBegin() {
        return 0;
    }
	size_t PutEnd(char*) {
        return 0;
    }

private:
	void Read() {
        current_ = (char)in_->get();
		if (!in_->fail()) {
			count_++;
        }
        else {
			current_ = '\0';
        }
    }
	std::istream* in_;
    char current_;
	size_t count_;
};


class CppOStream {
public:

	CppOStream(std::ostream& out) : out_(&out), current_('\0'), count_(0) {
        Read();
    }

	char Peek() const {
        return current_;
    }
	char Take() {
        return current_;
    }
	size_t Tell() const {
        return count_;
    }
	void Put(char c) {
        out_->put(c);
        count_++;
//        fputc(c, fp_);
    }

	// Not implemented
	char* PutBegin() {
        return 0;
    }
	size_t PutEnd(char*) {
        return 0;
    }

private:
	void Read() {
    }
	std::ostream* out_;
    char current_;
	size_t count_;
};
#endif // NCBI


} // namespace rapidjson

#endif // RAPIDJSON_FILESTREAM_H_
