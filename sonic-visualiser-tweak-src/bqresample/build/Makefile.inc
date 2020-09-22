
SRC_DIR	:= src
TEST_DIR := test
SPEEX_DIR := speex
HEADER_DIR := bqresample

SOURCES	:= $(wildcard $(SRC_DIR)/*.cpp) $(wildcard $(SPEEX_DIR)/*.c)
HEADERS	:= $(wildcard $(HEADER_DIR)/*.h) $(wildcard $(SRC_DIR)/*.h)

OBJECTS	:= $(SOURCES:.cpp=.o)
OBJECTS	:= $(OBJECTS:.c=.o)

TEST_SOURCES	:= $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS	:= $(TEST_SOURCES:.cpp=.o)

OPTFLAGS ?= -O3 -ffast-math

CXXFLAGS ?= -std=c++98 -fpic -Wall -Wextra -Werror $(RESAMPLE_DEFINES) $(VECTOR_DEFINES) $(ALLOCATOR_DEFINES) $(OPTFLAGS) -I. $(THIRD_PARTY_INCLUDES) -I../bqvec

CFLAGS	:= -fpic -Wall -Wextra $(RESAMPLE_DEFINES) $(VECTOR_DEFINES) $(ALLOCATOR_DEFINES) $(OPTFLAGS) -I. $(THIRD_PARTY_INCLUDES)

LIBRARY	:= libbqresample.a

all:	$(LIBRARY)

test:	$(LIBRARY) test-resampler
	./test-resampler

valgrind:	$(LIBRARY) test-resampler
	valgrind ./test-resampler

$(LIBRARY):	$(OBJECTS)
	$(AR) rc $@ $^

test-resampler:	test/TestResampler.o $(LIBRARY)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBRARY) -lboost_unit_test_framework -L../bqvec -lbqvec $(THIRD_PARTY_LIBS)

clean:		
	rm -f $(OBJECTS) $(TEST_OBJECTS)

distclean:	clean
	rm -f $(LIBRARY) test-resampler

depend:
	makedepend -Y -fbuild/Makefile.inc $(SOURCES) $(HEADERS) $(TEST_SOURCES)


# DO NOT DELETE

src/Resampler.o: bqresample/Resampler.h
test/TestResampler.o: bqresample/Resampler.h
