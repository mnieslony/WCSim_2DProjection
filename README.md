# WCSim_2DProjection

Code to create 2D projections for particle simulations done in WCSim (https://github.com/WCSim/WCSim.git). The C-style macro loops over all entries in the WCSim root tree input file, and creates csv files containing the information of the projected 2D images. These projections were used for a Convolutional Neural Network-based study on classifying DSNB and atmospheric background events in a Super-Kamiokande like detector. Code was originally inside the ToolAnalysis framework (https://github.com/ANNIEsoft/ToolAnalysis.git, more specifically the branched fork https://github.com/mnieslony/ToolAnalysis/tree/CNNImages_SK/ with some modifications), but ported here as a standalone tool for simplicity.

### Prerequisites
ROOT6 should be installed, and the `Setup.sh` script should be sourced in order to make the library needed by WCSim. Afterwards, the tool can be used by typing

```
root -l 'Projection_AtmosphericDSNB.C("filename.root",verbose=true/false)'
```

