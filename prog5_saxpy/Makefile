include ../common/common.mk

CXXFLAGS=-I../common -Iobjs/ -O2 -Wall -fPIC
ISPCFLAGS=-O3 --target=$(ISPC_TARGET) --arch=$(ISPC_ARCH)  --pic

APP_NAME=saxpy
OBJDIR=objs
COMMONDIR=../common

PPM_CXX=$(COMMONDIR)/ppm.cpp
PPM_OBJ=$(addprefix $(OBJDIR)/, $(subst $(COMMONDIR)/,, $(PPM_CXX:.cpp=.o)))

TASKSYS_CXX=$(COMMONDIR)/tasksys.cpp
TASKSYS_LIB=-lpthread
TASKSYS_OBJ=$(addprefix $(OBJDIR)/, $(subst $(COMMONDIR)/,, $(TASKSYS_CXX:.cpp=.o)))

default: $(APP_NAME)

.PHONY: dirs clean check-ispc

dirs:
	/bin/mkdir -p $(OBJDIR)/

check-ispc:
	@test -x "$(ISPC)" || { echo "ISPC not found. Run ../scripts/install_ispc.sh or pass ISPC=/path/to/ispc"; exit 1; }

clean:
	/bin/rm -rf $(OBJDIR) *.ppm *~ $(APP_NAME)

OBJS=$(OBJDIR)/main.o $(OBJDIR)/saxpySerial.o $(OBJDIR)/saxpy_ispc.o $(PPM_OBJ) $(TASKSYS_OBJ) 

$(APP_NAME): dirs $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lm $(TASKSYS_LIB)

$(OBJDIR)/%.o: %.cpp | dirs
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/%.o: $(COMMONDIR)/%.cpp | dirs
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/main.o: $(OBJDIR)/saxpy_ispc.h $(COMMONDIR)/CycleTimer.h

$(OBJDIR)/%_ispc.h $(OBJDIR)/%_ispc.o: %.ispc | dirs check-ispc
	$(ISPC) $(ISPCFLAGS) $< -o $(OBJDIR)/$*_ispc.o -h $(OBJDIR)/$*_ispc.h
