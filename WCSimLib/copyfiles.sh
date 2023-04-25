#!/bin/sh
# convenience script to copy the necessary files from WCSim directory to the current directories
# to build the correct dictionaries for a given set of simulation files.
# The corresponding wcsim source files must of course be checked out in the WCSim directory first.
for file in src/*; do FNAME=$(basename ${file}); echo "copying file ${FNAME}"; cp -f ${WCSIMDIR}/src/${FNAME} ./src/; done
mv include/WCSimRootLinkDef.h include/WCSimRootLinkDef.hh
for file in include/*; do FNAME=$(basename ${file}); echo "copying file ${FNAME}"; cp -f ${WCSIMDIR}/include/${FNAME} ./include/; done
mv include/WCSimRootLinkDef.hh include/WCSimRootLinkDef.h
