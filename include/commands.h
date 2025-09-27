#pragma once
#include "buffer.h"
#include "db.h"
#include "response.h"
#include <vector>
#include <string>

void do_keys(std::vector<std::string>& ,Buffer& out);
void do_get(std::vector<std::string>& cmd,Buffer& out);
void do_set(std::vector<std::string>& cmd,Buffer& out);
void do_setex(std::vector<std::string>& cmd,Buffer& out);
void do_del(std::vector<std::string>& cmd, Buffer& out);
void do_expire(std::vector<std::string>& cmd, Buffer& out);
void do_zadd(std::vector<std::string>& cmd, Buffer& out);
void do_zrem(std::vector<std::string>& cmd, Buffer& out);
void do_zscore(std::vector<std::string>& cmd, Buffer& out);
void do_zrank(std::vector<std::string>& cmd, Buffer& out);
void do_zcard(std::vector<std::string>& cmd, Buffer& out);
void do_zrange(std::vector<std::string>& cmd, Buffer& out);
void do_zrangebyscore(std::vector<std::string>& cmd, Buffer& out);
void do_request(std::vector<std::string>& cmd, Buffer& out);
