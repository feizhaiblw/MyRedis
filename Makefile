# Redis项目 Makefile

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -g -O2 -pthread
INCLUDES = -Iinclude
CPPFLAGS = $(INCLUDES)
DEPFLAGS = -MMD -MP
TEST_CPPFLAGS = $(CPPFLAGS) -I. -I$(TESTDIR)
SRCDIR = src
OBJDIR = obj
TESTDIR = test
SERVER_SOURCES = $(filter-out $(SRCDIR)/client.cpp, $(wildcard $(SRCDIR)/*.cpp))
SERVER_OBJECTS = $(SERVER_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
CLIENT_SOURCES = $(SRCDIR)/client.cpp
CLIENT_OBJECTS = $(CLIENT_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS = $(SERVER_OBJECTS:.o=.d) $(CLIENT_OBJECTS:.o=.d)
TARGET = server
CLIENT = client

.PHONY: all clean test test-avltree test-zset test-ttl test-threadpool test-zset-server run help client

all: $(TARGET)

# 创建目标目录
$(OBJDIR):
	mkdir -p $(OBJDIR)

# 编译目标文件
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEPFLAGS) -c $< -o $@

# 链接生成可执行文件
$(TARGET): $(SERVER_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(CLIENT): $(CLIENT_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# 清理编译文件
clean:
	rm -rf $(OBJDIR) $(TARGET) $(CLIENT) test_avl_complete test_zset_complete test_ttl test_thread_pool test_zset_server

# 编译并运行所有测试
test: test-avltree test-zset test-ttl test-threadpool test-zset-server

# 编译并运行AVL树完整测试
test-avltree: test_avl_complete
	./test_avl_complete

# 编译并运行ZSet完整测试
test-zset: test_zset_complete
	./test_zset_complete

# 编译AVL树完整测试程序
test_avl_complete: $(TESTDIR)/test_avl_complete.cpp $(SRCDIR)/avltree.cpp
	$(CXX) $(CXXFLAGS) $(TEST_CPPFLAGS) $^ -o $@

# 编译ZSet完整测试程序
test_zset_complete: $(TESTDIR)/test_zset_complete.cpp $(SRCDIR)/zset.cpp $(SRCDIR)/avltree.cpp $(SRCDIR)/hashtable.cpp
	$(CXX) $(CXXFLAGS) $(TEST_CPPFLAGS) $^ -o $@

# 编译TTL测试程序
test_ttl: $(TESTDIR)/test_ttl.cpp $(SRCDIR)/commands.cpp $(SRCDIR)/db.cpp $(SRCDIR)/buffer.cpp \
	$(SRCDIR)/response.cpp $(SRCDIR)/hashtable.cpp $(SRCDIR)/minheap.cpp \
	$(SRCDIR)/zset.cpp $(SRCDIR)/avltree.cpp
	$(CXX) $(CXXFLAGS) $(TEST_CPPFLAGS) $^ -o $@

# 运行TTL测试
test-ttl: test_ttl
	./test_ttl

# 编译线程池测试程序
test_thread_pool: $(TESTDIR)/test_thread_pool.cpp $(SRCDIR)/thread_pool.cpp
	$(CXX) $(CXXFLAGS) $(TEST_CPPFLAGS) $^ -o $@

# 运行线程池测试
test-threadpool: test_thread_pool
	./test_thread_pool

test_zset_server: $(TESTDIR)/test_zset_server.cpp $(SRCDIR)/zset.cpp $(SRCDIR)/avltree.cpp $(SRCDIR)/hashtable.cpp
	$(CXX) $(CXXFLAGS) $(TEST_CPPFLAGS) $^ -o $@

test-zset-server: test_zset_server
	./test_zset_server

# 运行服务器
run: $(TARGET)
	./$(TARGET)

# 显示帮助信息
help:
	@echo "可用目标:"
	@echo "  all         - 编译项目（默认）"
	@echo "  clean       - 清理编译文件"
	@echo "  run         - 编译并运行服务器"
	@echo "  client      - 编译客户端程序"
	@echo "  test        - 运行所有测试"
	@echo "  test-avltree- 运行AVL树完整测试"
	@echo "  test-zset   - 运行ZSet完整测试"
	@echo "  test-ttl    - 运行TTL测试"
	@echo "  test-threadpool - 运行线程池测试"
	@echo "  test-zset-server - 运行ZSet服务端函数测试"
	@echo "  help        - 显示此帮助信息"

-include $(DEPS)
