.PHONY = clean all tidy-check format

# define the commands we will use for compilation and library building
CXX = clang++-19
# define useful flags to cc/ld/etc.
CXXFLAGS += -g3 -gdwarf-4 -Wall -Wpedantic -Wextra -std=c++2b

TESTOBJS = test_threadpool.o test_suite.o catch.o

CPP_SOURCE_FILES = ThreadPool.cpp
HPP_SOURCE_FILES = ThreadPool.hpp

all: test_suite

test_suite: $(TESTOBJS) $(COMMON_OBJS) $(HEADERS) ThreadPool.o
	$(CXX) $(CXXFLAGS) -o $@ $(TESTOBJS) ThreadPool.o

test_suite.o: test_suite.cpp catch.hpp
	$(CXX) $(CXXFLAGS) -c $<

catch.o: catch.cpp catch.hpp
	$(CXX) $(CXXFLAGS) -c $<

test_threadpool.o: test_threadpool.cpp catch.hpp ThreadPool.hpp
	$(CXX) $(CXXFLAGS) -c $<

ThreadPool.o: ThreadPool.cpp ThreadPool.hpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f *.o *~ test_suite

tidy-check: 
	clang-tidy-19 \
        --extra-arg=--std=c++2b \
        -warnings-as-errors=* \
        -header-filter=.* \
        $(CPP_SOURCE_FILES) $(HPP_SOURCE_FILES)

format:
	clang-format-19 -i --verbose --style=Chromium $(CPP_SOURCE_FILES) $(HPP_SOURCE_FILES)
