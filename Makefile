EXENAME=ticking
CXX=c++
CXXLINK=c++
SOURCES=src

.PHONY: all clean

%.o: Makefile ${SOURCES}/%.cpp
	${CXX} -c ${SOURCES}/$*.cpp -o $*.o

all: ${EXENAME}

${EXENAME}: main.o audio_file.o
	${CXXLINK} $^ -o ${EXENAME} -lSDL3
