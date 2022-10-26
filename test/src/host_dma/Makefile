CXX=g++ 
INC=
LIBS=
CXXFLAGS += ${INC} -std=c++11 -g
TARGET = host_dma 
OBJS= 	host_dma.o \

OBJECTS = $(patsubst %.cpp,%.o, $(wildcard ${PWD}/*.cpp))

${TARGET}: ${OBJS}
	${CXX} ${CXXFLAGS} -o $@ $^

clean:
	rm -f *.o *.d ${TARGET}
