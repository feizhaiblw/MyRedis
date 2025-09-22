# Redis项目 Makefile

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -g -O2
INCLUDES = -Iinclude
SRCDIR = src
OBJDIR = obj
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = server
TEST_TARGET = test_buffer

.PHONY: all clean test test-buffer test-avltree

all: $(TARGET)

# 创建目标目录
$(OBJDIR):
	mkdir -p $(OBJDIR)

# 编译目标文件
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 链接生成可执行文件
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# 清理编译文件
clean:
	rm -rf $(OBJDIR) $(TARGET) $(TEST_TARGET) test_avltree test_avltree_simple

# 编译并运行所有测试
test: test-buffer test-avltree

# 编译并运行Buffer测试
test-buffer: $(TEST_TARGET)
	./$(TEST_TARGET)

# 编译并运行AVL树测试
test-avltree: test_avltree_simple
	./test_avltree_simple

# 编译Buffer测试程序
$(TEST_TARGET): test_buffer.cpp $(SRCDIR)/buffer.cpp
	$(CXX) $(CXXFLAGS) -I. $^ -o $@

# 编译AVL树简单测试程序
test_avltree_simple: test_avltree_simple.cpp $(SRCDIR)/avltree.cpp
	$(CXX) $(CXXFLAGS) -I. $^ -o $@

# 运行服务器
run: $(TARGET)
	./$(TARGET)

# 显示帮助信息
help:
	@echo "可用目标:"
	@echo "  all         - 编译项目（默认）"
	@echo "  clean       - 清理编译文件"
	@echo "  run         - 编译并运行服务器"
	@echo "  test        - 运行所有测试"
	@echo "  test-buffer - 运行Buffer模块测试"
	@echo "  test-avltree- 运行AVL树模块测试"
	@echo "  help        - 显示此帮助信息"