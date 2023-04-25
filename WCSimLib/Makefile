# $Id: GNUmakefile,v 1.17 2006/09/04 15:43:27 t2k Exp $
# --------------------------------------------------------------
# GNUmakefile for examples module.  Gabriele Cosmo, 06/04/98.
# --------------------------------------------------------------


ROOTCFLAGS   := $(shell root-config --cflags) -DUSE_ROOT -fPIC
ROOTLIBS     := $(shell root-config --libs)

CPPFLAGS  += -Wno-deprecated 
CPPFLAGS  += -fdiagnostics-color=always -Wno-sign-compare
CPPFLAGS  += -I$(PWD)/include
CPPFLAGS  += -I$(ROOTSYS)/include $(ROOTCFLAGS) 
EXTRALIBS += $(ROOTLIBS)

CXX = g++
G4SYSTEM=Linux-g++

G4WORKDIR := .
G4TMPDIR := $(G4WORKDIR)/tmp/$(G4SYSTEM)/WCSim




# Note dependencies not yet set up right yet

ROOTSO    := libWCSimRoot.so

ROOTSRC  := ./src/WCSimRootEvent.cc ./include/WCSimRootEvent.hh ./src/WCSimRootGeom.cc ./include/WCSimRootGeom.hh ./include/WCSimPmtInfo.hh ./src/WCSimEnumerations.cc ./include/WCSimEnumerations.hh ./src/WCSimRootOptions.cc ./include/WCSimRootOptions.hh ./include/WCSimRootLinkDef.hh ./src/WCSimRootTools.cc ./include/WCSimRootTools.hh

ROOTOBJS  := $(G4WORKDIR)/tmp/$(G4SYSTEM)/WCSim/WCSimRootEvent.o $(G4WORKDIR)/tmp/$(G4SYSTEM)/WCSim/WCSimRootGeom.o $(G4WORKDIR)/tmp/$(G4SYSTEM)/WCSim/WCSimPmtInfo.o $(G4WORKDIR)/tmp/$(G4SYSTEM)/WCSim/WCSimEnumerations.o $(G4WORKDIR)/tmp/$(G4SYSTEM)/WCSim/WCSimRootOptions.o $(G4WORKDIR)/tmp/$(G4SYSTEM)/WCSim/WCSimRootDict.o $(G4WORKDIR)/tmp/$(G4SYSTEM)/WCSim/WCSimRootTools.o





.PHONY: directories

all: directories ./src/WCSimRootDict.cxx libWCSimRoot.so

directories: $(G4TMPDIR)

$(G4TMPDIR) :
	mkdir -p $(G4TMPDIR)


libWCSimRoot.so : $(ROOTOBJS) 
	@echo Compiling libWCSimRoot.so with GNUMakefile ...
	@if [ ! -d $(G4TMPDIR) ] ; then mkdir $(G4TMPDIR) ; echo mkdir $(G4TMPDIR) ;fi
	@$(CXX) -shared -O $^ -o $(ROOTSO) $(ROOTLIBS)
	@cp src/WCSimRootDict_rdict.pcm $(G4WORKDIR)

#./src/WCSimRootDict.cc : $(ROOTSRC)
#	@echo Compiling rootcint ...
#	rootcint  -f ./src/WCSimRootDict.cc -c -I./include -I$(shell root-config --incdir) WCSimRootEvent.hh WCSimRootGeom.hh  WCSimPmtInfo.hh WCSimLAPPDInfo.hh WCSimLAPPDpulse.hh WCSimLAPPDpulseCluster.hh WCSimEnumerations.hh WCSimRootLinkDef.h

./src/WCSimRootDict.cxx : $(ROOTSRC)
	@echo Compiling rootcint ...
	rootcling -f ./src/WCSimRootDict.cxx -rml libWCSimRoot.so -rmf libWCSimRoot.rootmap -I./include -I$(shell root-config --incdir) WCSimRootEvent.hh WCSimRootGeom.hh WCSimPmtInfo.hh WCSimEnumerations.hh WCSimRootOptions.hh WCSimRootTools.hh WCSimRootLinkDef.hh

rootcint: ./src/WCSimRootDict.cxx

$(G4TMPDIR)/%.o: src/%.cc include/%.hh
	@echo Compiling $*.cc ...
	@if [ ! -d $(G4TMPDIR) ] ; then mkdir $(G4TMPDIR) ; echo mkdir $(G4TMPDIR) ;fi
	@echo $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $(G4TMPDIR)/$(*F).o $<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $(G4TMPDIR)/$(*F).o $<

$(G4TMPDIR)/WCSimRootDict.o: src/WCSimRootDict.cxx
	@echo Compiling $*.cc ...
	@if [ ! -d $(G4TMPDIR) ] ; then mkdir $(G4TMPDIR) ; echo mkdir $(G4TMPDIR) ;fi
	@echo $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $(G4TMPDIR)/$(*F).o $<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $(G4TMPDIR)/$(*F).o $<

clean :
	@rm -f $(G4TMPDIR)/*.o
	@rm -f ./src/WCSimRootDict.cxx
	@rm -f libWCSimRoot.so
	@rm -f libWCSimRoot.rootmap 
	@rm -f WCSimRootDict_rdict.pcm 
	@rm -f src/WCSimRootDict_rdict.pcm

