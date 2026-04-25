.PHONY = clean all tidy-check format

# define the commands we will use for compilation and library building
CXX = clang++-19
# define useful flags to cc/ld/etc.
CXXFLAGS += -g3 -gdwarf-4 -Wall -Wpedantic -Wextra -std=c++2b

TESTOBJS = test_threadpool.o test_invertedindex.o test_httprequest.o test_httpresponse.o test_staticfile.o test_suite.o catch.o 

CPP_SOURCE_FILES = ThreadPool.cpp HttpServer.cpp HttpRequest.cpp HttpResponse.cpp InvertedIndex.cpp StaticFile.cpp
HPP_SOURCE_FILES = ThreadPool.hpp HttpServer.hpp HttpRequest.hpp HttpResponse.hpp InvertedIndex.hpp StaticFile.hpp


all: test_suite searchserver searchclient

# build a minimal searchserver binary for the assignment demo
SRV_OBJS = searchserver.o HttpServer.o HttpRequest.o HttpResponse.o StaticFile.o InvertedIndex.o ThreadPool.o

searchserver: $(SRV_OBJS)
	$(CXX) $(CXXFLAGS) -o searchserver $(SRV_OBJS)

searchserver.o: searchserver.cpp HttpServer.hpp
	$(CXX) $(CXXFLAGS) -c searchserver.cpp

HttpServer.o: HttpServer.cpp HttpServer.hpp HttpRequest.hpp HttpResponse.hpp StaticFile.hpp InvertedIndex.hpp ThreadPool.hpp
	$(CXX) $(CXXFLAGS) -c HttpServer.cpp

HttpRequest.o: HttpRequest.cpp HttpRequest.hpp
	$(CXX) $(CXXFLAGS) -c HttpRequest.cpp

HttpResponse.o: HttpResponse.cpp HttpResponse.hpp
	$(CXX) $(CXXFLAGS) -c HttpResponse.cpp

StaticFile.o: StaticFile.cpp StaticFile.hpp HttpResponse.hpp
	$(CXX) $(CXXFLAGS) -c StaticFile.cpp

InvertedIndex.o: InvertedIndex.cpp InvertedIndex.hpp
	$(CXX) $(CXXFLAGS) -c InvertedIndex.cpp

# searchclient
searchclient: searchclient.o
	$(CXX) $(CXXFLAGS) -o searchclient searchclient.o

searchclient.o: searchclient.cpp
	$(CXX) $(CXXFLAGS) -c searchclient.cpp

# crawl_demo removed (legacy demo); source retained as placeholder if needed

test_suite: $(TESTOBJS) $(COMMON_OBJS) $(HEADERS) ThreadPool.o HttpRequest.o HttpResponse.o InvertedIndex.o StaticFile.o
	$(CXX) $(CXXFLAGS) -o $@ $(TESTOBJS) ThreadPool.o HttpRequest.o HttpResponse.o InvertedIndex.o StaticFile.o

test_suite.o: test_suite.cpp catch.hpp 
	$(CXX) $(CXXFLAGS) -c $<

catch.o: catch.cpp catch.hpp
	$(CXX) $(CXXFLAGS) -c $<

test_threadpool.o: test_threadpool.cpp catch.hpp ThreadPool.hpp
	$(CXX) $(CXXFLAGS) -c $<

test_invertedindex.o: test_invertedindex.cpp catch.hpp InvertedIndex.hpp
	$(CXX) $(CXXFLAGS) -c $<

test_httprequest.o: test_httprequest.cpp catch.hpp HttpRequest.hpp
	$(CXX) $(CXXFLAGS) -c $<

test_httpresponse.o: test_httpresponse.cpp catch.hpp HttpResponse.hpp
	$(CXX) $(CXXFLAGS) -c $<

test_staticfile.o: test_staticfile.cpp catch.hpp StaticFile.hpp HttpResponse.hpp
	$(CXX) $(CXXFLAGS) -c $<

ThreadPool.o: ThreadPool.cpp ThreadPool.hpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f *.o *~ test_suite searchserver searchclient

tidy-check: 
	clang-tidy-19 \
        --extra-arg=--std=c++2b \
        --extra-arg=-Wall \
        -warnings-as-errors=* \
        -header-filter=.* \
        -p . \
        $(CPP_SOURCE_FILES) $(HPP_SOURCE_FILES) \
        -- -std=c++2b

format:
	clang-format-19 -i --verbose --style=Chromium $(CPP_SOURCE_FILES) $(HPP_SOURCE_FILES)
