#!Makefile
#
# --------------------------------------------------------
#
#    Makefile  2014/2/27
#
# --------------------------------------------------------
#

C_SOURCES = $(shell find . -name "*.cpp")
C_OBJECTS = $(patsubst %.cpp, %.o, $(C_SOURCES))

CXX = g++ -std=c++11
Para = -pthread -lev
C_FLAGS = -c -g -Wall -Iinclude 

all: $(C_OBJECTS) 
	@echo 链接 ...
	$(CXX) $(C_OBJECTS) $(Para) -o memcached

.cpp.o:
	@echo 编译代码文件 $< ...
	$(CXX) $(C_FLAGS) $< -o $@

.PHONY:clean
clean:
	@echo 清理临时文件
	$(RM) $(C_OBJECTS)

