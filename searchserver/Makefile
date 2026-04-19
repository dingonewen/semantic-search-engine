.PHONY = clean all tidy-check format

# define the commands we will use for compilation and library building
CXX = clang++-19
# define useful flags to cc/ld/etc.
CXXFLAGS += -g3 -gdwarf-4 -Wall -Wpedantic -Wextra -std=c++2b

TESTOBJS = test_threadpool.o test_suite.o catch.o

CPP_SOURCE_FILES = ThreadPool.cpp
HPP_SOURCE_FILES = ThreadPool.hpp


all: test_suite searchserver searchclient

# build a minimal searchserver binary for the assignment demo
SRV_OBJS = searchserver.o HttpServer.o Request.o Response.o StaticFile.o InvertedIndex.o ThreadPool.o

searchserver: $(SRV_OBJS)
	$(CXX) $(CXXFLAGS) -o searchserver $(SRV_OBJS)

searchserver.o: searchserver.cpp HttpServer.hpp
	$(CXX) $(CXXFLAGS) -c searchserver.cpp

HttpServer.o: HttpServer.cpp HttpServer.hpp Request.hpp Response.hpp StaticFile.hpp InvertedIndex.hpp ThreadPool.hpp
	$(CXX) $(CXXFLAGS) -c HttpServer.cpp

Request.o: Request.cpp Request.hpp
	$(CXX) $(CXXFLAGS) -c Request.cpp

Response.o: Response.cpp Response.hpp
	$(CXX) $(CXXFLAGS) -c Response.cpp

StaticFile.o: StaticFile.cpp StaticFile.hpp Response.hpp
	$(CXX) $(CXXFLAGS) -c StaticFile.cpp

InvertedIndex.o: InvertedIndex.cpp InvertedIndex.hpp
	$(CXX) $(CXXFLAGS) -c InvertedIndex.cpp

# client shell
client_shell: client_shell.o
	$(CXX) $(CXXFLAGS) -o client_shell client_shell.o

client_shell.o: client_shell.cpp client_shell.hpp
	$(CXX) $(CXXFLAGS) -c client_shell.cpp

# searchclient
searchclient: searchclient.o
	$(CXX) $(CXXFLAGS) -o searchclient searchclient.o

searchclient.o: searchclient.cpp
	$(CXX) $(CXXFLAGS) -c searchclient.cpp

# crawl_demo removed (legacy demo); source retained as placeholder if needed

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
