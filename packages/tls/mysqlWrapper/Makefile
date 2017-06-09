SRCFILES=$(wildcard mysqllib/*.cc)
OBJFILES=$(patsubst mysqllib/%.cc,obj/%.o,$(SRCFILES))
CXXFLAGS=-I/usr/include/mysql -fabi-version=2 -fno-omit-frame-pointer -std=c++11
LDFLAGS=-L/usr/lib/x86_64-linux-gnu -lmysqlclient -lpthread -lz -lm -lrt -ldl
CXX=g++


.PHONY: all


executables:=main createSelect backFieldsToFiles

all: $(executables)

createSelect: obj/createSelect.o $(OBJFILES)
	$(CXX) -o createSelect $^ $(LDFLAGS)

main: obj/main.o $(OBJFILES)
	$(CXX) -o main $^ $(LDFLAGS)

backFieldsToFiles: obj/backFieldsToFiles.o $(OBJFILES)
	$(CXX) -o backFieldsToFiles $^ $(LDFLAGS)
.PHONY:clean


#since we only have xx.cc instead of obj/xx.cc, and we use %.cc, so we have to use obj/%.o instead of %.o here
obj/%.o:%.cc
	@mkdir -p $(@D)
	$(CXX) -MMD $(CXXFLAGS) -g -c $< -o $@


obj/%.o:mysqllib/%.cc
	@mkdir -p $(@D)
	$(CXX) -MMD $(CXXFLAGS) -g -c $< -o $@

clean:
	-rm -rf obj $(executables)
