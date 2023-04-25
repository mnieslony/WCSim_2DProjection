#!/bin/bash

echo "Making WCSimLib libraries...."
cd WCSimLib
make clean
make -f Makefile_ROOT6
cp libWCSimRoot.so ../
cp WCSimRootDict_rdict.pcm ../

echo "Done with WCSimLib"
cd ..

echo "Ready to get started! Type"
echo "root -l Projection_Atmospheric_DSNB.C"
echo "as a first test"
