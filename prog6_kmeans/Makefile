include ../common/common.mk

CXXFLAGS=-I../common -Iobjs/ -O3 -march=native -std=c++17 -Wall

APP_NAME=kmeans
GEN_NAME=gendata
OBJDIR=objs
COMMONDIR=../common

default: $(APP_NAME) $(GEN_NAME)

.PHONY: dirs clean data

dirs:
	/bin/mkdir -p $(OBJDIR)/

clean:
	/bin/rm -rf $(OBJDIR) *.ppm *.log *.png *~ $(APP_NAME) $(GEN_NAME)

OBJS=$(OBJDIR)/main.o $(OBJDIR)/kmeansThread.o $(OBJDIR)/utils.o

$(APP_NAME): dirs $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lm -lpthread

# Generates ./data.dat locally (replaces the /afs copy used at Stanford).
$(GEN_NAME): dirs $(OBJDIR)/gendata.o $(OBJDIR)/utils.o
	$(CXX) $(CXXFLAGS) -o $@ $(OBJDIR)/gendata.o $(OBJDIR)/utils.o -lm

data: $(GEN_NAME)
	./$(GEN_NAME)

$(OBJDIR)/%.o: %.cpp | dirs
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/main.o: $(COMMONDIR)/CycleTimer.h
