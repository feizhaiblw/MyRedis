#pragma once
#include "buffer.h"
#include <string>

enum {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

enum{
    ERROR=1,
    message_too_long=2,
};

void out_nil(Buffer& out);
void out_str(Buffer& out, const char* data, size_t len);
void out_int(Buffer& out, int64_t data);
void out_dbl(Buffer& out, double data);
void out_err(Buffer& out, uint32_t code, const std::string &data);
void out_arr(Buffer& out,uint32_t len);

void response_begin(Buffer& out,size_t* header);
size_t response_size(Buffer& out,size_t header);
void response_end(Buffer& out,size_t header);