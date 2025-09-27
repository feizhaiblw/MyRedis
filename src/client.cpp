#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace protocol {

constexpr uint8_t TAG_NIL = 0;
constexpr uint8_t TAG_ERR = 1;
constexpr uint8_t TAG_STR = 2;
constexpr uint8_t TAG_INT = 3;
constexpr uint8_t TAG_DBL = 4;
constexpr uint8_t TAG_ARR = 5;

struct RespValue {
    enum class Type { Nil, Error, String, Int, Double, Array };

    Type type = Type::Nil;
    std::string string_value;
    int64_t int_value = 0;
    double double_value = 0.0;
    uint32_t error_code = 0;
    std::string error_message;
    std::vector<RespValue> array;

    static RespValue make_nil() { return RespValue(); }
    static RespValue make_string(std::string value) {
        RespValue v;
        v.type = Type::String;
        v.string_value = std::move(value);
        return v;
    }
    static RespValue make_int(int64_t value) {
        RespValue v;
        v.type = Type::Int;
        v.int_value = value;
        return v;
    }
    static RespValue make_double(double value) {
        RespValue v;
        v.type = Type::Double;
        v.double_value = value;
        return v;
    }
    static RespValue make_error(uint32_t code, std::string message) {
        RespValue v;
        v.type = Type::Error;
        v.error_code = code;
        v.error_message = std::move(message);
        return v;
    }
    static RespValue make_array(std::vector<RespValue> values) {
        RespValue v;
        v.type = Type::Array;
        v.array = std::move(values);
        return v;
    }

private:
    explicit RespValue(Type t) : type(t) {}
    RespValue() = default;
};

static void append_u32(std::vector<uint8_t>& buf, uint32_t value) {
    uint8_t data[sizeof(uint32_t)];
    std::memcpy(data, &value, sizeof(value));
    buf.insert(buf.end(), data, data + sizeof(value));
}

static void append_bytes(std::vector<uint8_t>& buf, const std::string& value) {
    buf.insert(buf.end(), value.begin(), value.end());
}

std::vector<uint8_t> encode_request(const std::vector<std::string>& cmd) {
    if(cmd.empty()) {
        throw std::invalid_argument("command must not be empty");
    }
    std::vector<uint8_t> payload;
    append_u32(payload, static_cast<uint32_t>(cmd.size()));
    for(const auto& token : cmd) {
        append_u32(payload, static_cast<uint32_t>(token.size()));
        append_bytes(payload, token);
    }
    std::vector<uint8_t> frame(4 + payload.size());
    uint32_t payload_size = static_cast<uint32_t>(payload.size());
    std::memcpy(frame.data(), &payload_size, sizeof(payload_size));
    std::memcpy(frame.data() + 4, payload.data(), payload.size());
    return frame;
}

static uint32_t read_u32(const uint8_t*& cur, const uint8_t* end) {
    if(end - cur < static_cast<ptrdiff_t>(sizeof(uint32_t))) {
        throw std::runtime_error("response truncated (u32)");
    }
    uint32_t value = 0;
    std::memcpy(&value, cur, sizeof(value));
    cur += sizeof(value);
    return value;
}

static int64_t read_i64(const uint8_t*& cur, const uint8_t* end) {
    if(end - cur < static_cast<ptrdiff_t>(sizeof(int64_t))) {
        throw std::runtime_error("response truncated (i64)");
    }
    int64_t value = 0;
    std::memcpy(&value, cur, sizeof(value));
    cur += sizeof(value);
    return value;
}

static double read_double(const uint8_t*& cur, const uint8_t* end) {
    if(end - cur < static_cast<ptrdiff_t>(sizeof(double))) {
        throw std::runtime_error("response truncated (double)");
    }
    double value = 0.0;
    std::memcpy(&value, cur, sizeof(value));
    cur += sizeof(value);
    return value;
}

static std::string read_string(const uint8_t*& cur, const uint8_t* end, uint32_t len) {
    if(end - cur < static_cast<ptrdiff_t>(len)) {
        throw std::runtime_error("response truncated (string)");
    }
    std::string value(reinterpret_cast<const char*>(cur), len);
    cur += len;
    return value;
}

static RespValue parse_value(const uint8_t*& cur, const uint8_t* end);

static RespValue parse_array(const uint8_t*& cur, const uint8_t* end) {
    uint32_t count = read_u32(cur, end);
    std::vector<RespValue> values;
    values.reserve(count);
    for(uint32_t i = 0; i < count; ++i) {
        values.push_back(parse_value(cur, end));
    }
    return RespValue::make_array(std::move(values));
}

static RespValue parse_value(const uint8_t*& cur, const uint8_t* end) {
    if(cur >= end) {
        throw std::runtime_error("unexpected end of response");
    }
    uint8_t tag = *cur++;
    switch(tag) {
    case TAG_NIL:
        return RespValue::make_nil();
    case TAG_STR: {
        uint32_t len = read_u32(cur, end);
        return RespValue::make_string(read_string(cur, end, len));
    }
    case TAG_INT:
        return RespValue::make_int(read_i64(cur, end));
    case TAG_DBL:
        return RespValue::make_double(read_double(cur, end));
    case TAG_ERR: {
        uint32_t code = read_u32(cur, end);
        uint32_t len = read_u32(cur, end);
        std::string message = read_string(cur, end, len);
        return RespValue::make_error(code, std::move(message));
    }
    case TAG_ARR:
        return parse_array(cur, end);
    default:
        throw std::runtime_error("unknown response tag: " + std::to_string(tag));
    }
}

RespValue parse_payload(const std::vector<uint8_t>& payload) {
    if(payload.size() >= 5) {
        const uint8_t* cur = payload.data();
        const uint8_t* end = cur + payload.size();
        const uint8_t* probe = cur;
        uint32_t bulk_len = read_u32(probe, end);
        if(probe < end && *probe == TAG_STR) {
            ++probe;
            uint32_t str_len = read_u32(probe, end);
            if(str_len == bulk_len) {
                return RespValue::make_string(read_string(probe, end, str_len));
            }
        }
    }
    const uint8_t* cur = payload.data();
    const uint8_t* end = cur + payload.size();
    RespValue value = parse_value(cur, end);
    if(cur != end) {
        throw std::runtime_error("response contains unexpected trailing bytes");
    }
    return value;
}

std::string format_value(const RespValue& value, int indent = 0) {
    std::ostringstream out;
    switch(value.type) {
    case RespValue::Type::Nil:
        out << "(nil)";
        break;
    case RespValue::Type::String:
        out << '"' << value.string_value << '"';
        break;
    case RespValue::Type::Int:
        out << value.int_value;
        break;
    case RespValue::Type::Double:
        out << value.double_value;
        break;
    case RespValue::Type::Error:
        out << "(error " << value.error_code << ": " << value.error_message << ")";
        break;
    case RespValue::Type::Array:
        if(value.array.empty()) {
            out << "[]";
            break;
        }
        out << "[\n";
        for(size_t i = 0; i < value.array.size(); ++i) {
            out << std::string(indent + 2, ' ') << format_value(value.array[i], indent + 2);
            if(i + 1 != value.array.size()) {
                out << ',';
            }
            out << '\n';
        }
        out << std::string(indent, ' ') << ']';
        break;
    }
    return out.str();
}

} // namespace protocol

namespace {

[[noreturn]] void usage(const char* prog) {
    std::cerr << "用法: " << prog << " <host> <port>\n";
    std::exit(EXIT_FAILURE);
}

int connect_tcp(const std::string& host, int port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    std::string port_str = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if(rc != 0) {
        throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(rc));
    }

    int sock = -1;
    for(addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        sock = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if(sock == -1) {
            continue;
        }
        if(::connect(sock, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;
        }
        ::close(sock);
        sock = -1;
    }
    freeaddrinfo(result);

    if(sock == -1) {
        throw std::runtime_error("无法连接到服务端");
    }
    return sock;
}

void send_all(int sock, const std::vector<uint8_t>& data) {
    size_t offset = 0;
    while(offset < data.size()) {
        ssize_t n = ::send(sock, data.data() + offset, data.size() - offset, 0);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            throw std::runtime_error("发送数据失败: " + std::string(std::strerror(errno)));
        }
        offset += static_cast<size_t>(n);
    }
}

void recv_exact(int sock, std::vector<uint8_t>& buffer, size_t count) {
    buffer.resize(count);
    size_t offset = 0;
    while(offset < count) {
        ssize_t n = ::recv(sock, buffer.data() + offset, count - offset, 0);
        if(n == 0) {
            throw std::runtime_error("连接已关闭");
        }
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            throw std::runtime_error("接收数据失败: " + std::string(std::strerror(errno)));
        }
        offset += static_cast<size_t>(n);
    }
}

std::vector<uint8_t> receive_frame(int sock) {
    std::vector<uint8_t> header;
    recv_exact(sock, header, 4);
    uint32_t payload_len = 0;
    std::memcpy(&payload_len, header.data(), sizeof(payload_len));
    if(payload_len == 0) {
        return {};
    }
    std::vector<uint8_t> payload(payload_len);
    recv_exact(sock, payload, payload_len);
    return payload;
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    char quote_char = '\0';

    for(size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if(in_quotes) {
            if(ch == '\\' && i + 1 < line.size()) {
                current.push_back(line[++i]);
            } else if(ch == quote_char) {
                in_quotes = false;
            } else {
                current.push_back(ch);
            }
            continue;
        }

        if(std::isspace(static_cast<unsigned char>(ch))) {
            if(!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        if(ch == '\'' || ch == '"') {
            in_quotes = true;
            quote_char = ch;
            continue;
        }
        if(ch == '\\' && i + 1 < line.size()) {
            current.push_back(line[++i]);
        } else {
            current.push_back(ch);
        }
    }

    if(in_quotes) {
        throw std::runtime_error("未闭合的引号");
    }
    if(!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

} // namespace

int main(int argc, char** argv) {
    if(argc != 3) {
        usage(argv[0]);
    }

    const std::string host = argv[1];
    char* end = nullptr;
    long port_long = std::strtol(argv[2], &end, 10);
    if(end == argv[2] || *end != '\0' || port_long <= 0 || port_long > 65535) {
        usage(argv[0]);
    }
    int port = static_cast<int>(port_long);

    signal(SIGPIPE, SIG_IGN);

    try {
        int sock = connect_tcp(host, port);
        std::cout << "已连接到 " << host << ':' << port << "，输入指令（Ctrl+D 退出）。" << std::endl;
        std::string line;
        while(std::cout << "> " && std::getline(std::cin, line)) {
            if(line.empty()) {
                continue;
            }
            std::vector<std::string> tokens;
            try {
                tokens = tokenize(line);
            } catch(const std::exception& ex) {
                std::cerr << "输入错误: " << ex.what() << std::endl;
                continue;
            }
            if(tokens.empty()) {
                continue;
            }
            try {
                auto request = protocol::encode_request(tokens);
                send_all(sock, request);
                auto payload = receive_frame(sock);
                auto value = protocol::parse_payload(payload);
                std::cout << protocol::format_value(value) << std::endl;
            } catch(const std::exception& ex) {
                std::cerr << "请求失败: " << ex.what() << std::endl;
            }
        }
        ::close(sock);
    } catch(const std::exception& ex) {
        std::cerr << "连接失败: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
