#ifndef _HGVS_PARSER_COMMON_HPP_
#define _HGVS_PARSER_COMMON_HPP_

#define VALASSIGN \
    [_val = _1]

#define ACTION0(func) \
    [bind(func, _val)]
           
#define ACTION1(func) \
    [bind(func, _1, _val)]

#define ACTION2(func) \
    [bind(func, _1, _2, _val)]
                         
#define ACTION3(func) \
    [bind(func, _1, _2, _3, _val)]

#endif
