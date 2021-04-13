#include <ncbi_pch.hpp>
#include <ncbiconf.h>
#ifdef BM64ADDR
#  include "stress64.cpp"
#else
#  include "stress32.cpp"
#endif
