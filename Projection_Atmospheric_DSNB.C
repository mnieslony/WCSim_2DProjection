#include <iostream>
#include <TH1F.h>
#include <stdio.h>     
#include <stdlib.h>

#include "TTree.h"
#include "TH1F.h"
#include "TStyle.h"
#include "TROOT.h"
#include "TSystem.h"
#include "TCanvas.h"
#include "TFile.h"

//WCSim includes
#include "WCSimLib/include/WCSimRootOptions.hh"
#include "WCSimLib/include/WCSimRootGeom.hh"
#include "WCSimLib/include/WCSimRootEvent.hh"

//ToolAnalysis includes
#include "./include/Hit.h"
#include "./include/Particle.h"
#include "./include/Detector.h"
#include "./include/Geometry.h"
#include "./include/ChannelKey.h"
#include "./include/Paddle.h"
#include "./include/Channel.h"
#include "./include/Position.h"

// Small macro which reads in WCSim files and produces the necessary outputs for convolutional neural network classification of the 2D projected images
// Macro produces csv output files which show the 2D-projected charge and time images of the prompt events (e+ for DSNB, gamma for Atmospheric events)
// Re-purposed general structure of the sample_readin.C file from https://github.com/WCSim/WCSim.git
// Main code is implemented from the original implementation in the ToolAnalysis framework (https://github.com/mnieslony/ToolAnalysis/CNN_SK)

// Run code as a root macro via `root -l 'Projection_Atmospheric_DNSB("/path/to/file.root",true/false)'

// Helper functions
void progress_bar(int current_ev, int total_ev){
  int barWidth = 70;

  double progress = double(current_ev)/double(total_ev);
  std::cout << "[";
  int pos = barWidth * progress;
  for (int i = 0; i < barWidth; ++i) {
    if (i < pos) std::cout << "=";
    else if (i == pos) std::cout << ">";
    else std::cout << " ";
  }
  std::cout << "] " << int(progress * 100.0) << " %\r";
  std::cout.flush();

}

void printWCSimRootTrigger(WCSimRootTrigger* wcsimrootevent, WCSimRootEvent* wcsimrootsuperevent){
  printf("********************************************************");
  printf("Evt, date %d %d\n", wcsimrootevent->GetHeader()->GetEvtNum(),
       int(wcsimrootevent->GetHeader()->GetDate()));
  printf("Mode %d\n", wcsimrootevent->GetMode());
  printf("Number of subevents %d\n",
    wcsimrootsuperevent->GetNumberOfSubEvents());
      
  printf("Vtxvol %d\n", wcsimrootevent->GetVtxvol());
  printf("Vtx %f %f %f\n", wcsimrootevent->GetVtx(0),
    wcsimrootevent->GetVtx(1),wcsimrootevent->GetVtx(2));

  printf("Jmu %d\n", wcsimrootevent->GetJmu());
  printf("Npar %d\n", wcsimrootevent->GetNpar());
  printf("Ntrack %d\n", wcsimrootevent->GetNtrack());
}

void printWCSimRootTrack(WCSimRootTrack* wcsimroottrack){
  printf("Track ipnu: %d\n",wcsimroottrack->GetIpnu());
  printf("Track parent ID: %d\n",wcsimroottrack->GetParenttype());
      
  for (int j=0; j<3; j++)
    printf("Track dir: %d %f\n",j, wcsimroottrack->GetDir(j));
  printf("Track energy: %f\n", wcsimroottrack->GetE());
  printf("Track momentum: %f\n", wcsimroottrack->GetP());
  printf("Track mass: %f\n", wcsimroottrack->GetM());
}

Geometry* ConstructToolChainGeometry(WCSimRootGeom *wcsimrootgeom, std::map<int,unsigned long> &pmt_tubeid_to_channelkey, std::map<unsigned long,int> &channelkey_to_pmtid, int &numtankpmts, bool verbose){

  // Pull details from the WCSimRootGeom
  // ===================================
  double WCSimGeometryVer = 1;                      // TODO: pull this from some suitable variable
  // some variables are available from wcsimrootgeom.  TODO: put everything into wcsimrootgeom
  numtankpmts = wcsimrootgeom->GetWCNumPMT();
  double tank_xcentre = (wcsimrootgeom->GetWCOffset(0)) / 100.;  // convert [cm] to [m]
  double tank_ycentre = (wcsimrootgeom->GetWCOffset(1)) / 100.;
  double tank_zcentre = (wcsimrootgeom->GetWCOffset(2)) / 100.;
  Position tank_centre(tank_xcentre, tank_ycentre, tank_zcentre);
  double tank_radius = (wcsimrootgeom->GetWCCylRadius()) / 100.;
  double tank_halfheight = (wcsimrootgeom->GetWCCylLength()) / 100.;
  //Currently hard-coded; estimated with a tape measure on the ANNIE frame :)
  double pmt_enclosed_radius = 1.0;
  double pmt_enclosed_halfheight = 1.45;


  constexpr int ADC_CHANNELS_PER_CARD=4;
  constexpr int ADC_CARDS_PER_CRATE=20;
  constexpr int MT_CHANNELS_PER_CARD=4;
  constexpr int MT_CARDS_PER_CRATE=20;

  //HV
  constexpr int CAEN_HV_CHANNELS_PER_CARD=16;
  constexpr int CAEN_HV_CARDS_PER_CRATE=10;

  std::map<std::string,std::map<unsigned long,Detector>> dets = std::map<std::string,std::map<unsigned long,Detector>>{};

  // construct the ToolChain Goemetry
  // ================================
  Geometry* anniegeom = new Geometry(double(WCSimGeometryVer),
    tank_centre,
    tank_radius,
    tank_halfheight,
    pmt_enclosed_radius,
    pmt_enclosed_halfheight,
    double(0.),
    double(0.),
    double(0.),
    double(0.),
    numtankpmts,
    int(0),
    int(0),
    int(0),
    geostatus::FULLY_OPERATIONAL,
    std::map<std::string,std::map<unsigned long,Detector>>{});

  if(verbose){
    cout<<"constructed anniegeom at "<<anniegeom<<" with tank origin "; tank_centre.Print();
  }

  // Construct the Detectors and Channels
  // ====================================
  // PMTs
  unsigned int ADC_Crate_Num = 0;
  unsigned int ADC_Card_Num  = 0;
  unsigned int ADC_Chan_Num = 0;
  unsigned int MT_Crate_Num = 0;
  unsigned int MT_Card_Num = 0;
  unsigned int MT_Chan_Num = 0;
  // HV
  unsigned int CAEN_HV_Crate_Num = 0;
  unsigned int CAEN_HV_Card_Num = 0;
  unsigned int CAEN_HV_Chan_Num = 0;
  unsigned int LeCroy_HV_Crate_Num = 0;
  unsigned int LeCroy_HV_Card_Num = 0;
  unsigned int LeCroy_HV_Chan_Num = 0;


  // tank PMTs
  for(int pmti=0; pmti<numtankpmts; pmti++){
    WCSimRootPMT apmt = wcsimrootgeom->GetPMT(pmti);

    // Construct the detector associated with this PMT
    unsigned long uniquedetectorkey = anniegeom->ConsumeNextFreeDetectorKey();
    std::string CylLocString;
    int cylloc = apmt.GetCylLoc();
    CylLocString = "Anywhere";
    Detector adet(uniquedetectorkey,
            "Tank",
            CylLocString,
            Position( apmt.GetPosition(0)/100.,
                      apmt.GetPosition(1)/100.,
                      apmt.GetPosition(2)/100.),
            Direction(apmt.GetOrientation(0),
                      apmt.GetOrientation(1),
                      apmt.GetOrientation(2)),
            apmt.GetName(),
            detectorstatus::ON,
            0.);

    // construct the channel associated with this PMT
    unsigned long uniquechannelkey = anniegeom->ConsumeNextFreeChannelKey();
    pmt_tubeid_to_channelkey.emplace(apmt.GetTubeNo(), uniquechannelkey);
    channelkey_to_pmtid.emplace(uniquechannelkey,apmt.GetTubeNo());

    // fill up ADC cards and channels monotonically, they're arbitrary for simulation
    ADC_Chan_Num++;
    if(ADC_Chan_Num>=ADC_CHANNELS_PER_CARD)  { ADC_Chan_Num=0; ADC_Card_Num++; MT_Chan_Num++; }
    if(ADC_Card_Num>=ADC_CARDS_PER_CRATE)    { ADC_Card_Num=0; ADC_Crate_Num++; }
    if(MT_Chan_Num>=MT_CHANNELS_PER_CARD)    { MT_Chan_Num=0; MT_Card_Num++; }
    if(MT_Card_Num>=MT_CARDS_PER_CRATE)      { MT_Card_Num=0; MT_Crate_Num++; }
    // same for HV
    CAEN_HV_Chan_Num++;
    if(CAEN_HV_Chan_Num>=CAEN_HV_CHANNELS_PER_CARD)    { CAEN_HV_Chan_Num=0; CAEN_HV_Card_Num++;  }
    if(CAEN_HV_Card_Num>=CAEN_HV_CARDS_PER_CRATE) { CAEN_HV_Card_Num=0; CAEN_HV_Crate_Num++; }

    Channel pmtchannel( uniquechannelkey,
              Position(0,0,0.),
              0, // stripside
              0, // stripnum
              ADC_Crate_Num,
              ADC_Card_Num,
              ADC_Chan_Num,
              MT_Crate_Num,
              MT_Card_Num,
              MT_Chan_Num,
              CAEN_HV_Crate_Num,
              CAEN_HV_Card_Num,
              CAEN_HV_Chan_Num,
              channelstatus::ON);

    // Add this channel to the geometry
    adet.AddChannel(pmtchannel);

    // Add this detector to the geometry
    anniegeom->AddDetector(adet);
  }

  return anniegeom;
}

std::vector<int> GetHitParentIds(WCSimRootCherenkovDigiHit* digihit, WCSimRootTrigger* firstTrig, std::map<int,int> *trackid_to_mcparticleindex){
  /* Get the ID of the MCParticle(s) that produced this digit */
  std::vector<int> parentids; // a hit could technically have more than one contrbuting particle

  // loop over the photons in this digit
  std::vector<int> truephotonindices = digihit->GetPhotonIds();
  for(int truephoton=0; truephoton<(int)truephotonindices.size(); truephoton++){
    int thephotonsid = truephotonindices.at(truephoton);
  
    // get the CherenkovHitTime objects themselves, which contain the photon parent IDs
    WCSimRootCherenkovHitTime *thehittimeobject =
      (WCSimRootCherenkovHitTime*)(firstTrig->GetCherenkovHitTimes()->At(thephotonsid));
    if(thehittimeobject==nullptr) cerr<<"HITTIME IS NULL"<<endl;
    else {
      int theparenttrackid = thehittimeobject->GetParentID();
      // check if this parent track was saved. Not all particles are saved.
      if(trackid_to_mcparticleindex->count(theparenttrackid)){
        parentids.push_back(trackid_to_mcparticleindex->at(theparenttrackid));
      } // else this photon may have come from e.g. an electron or gamma that wasn't recorded
    }
  }
  return parentids;
}

std::map<std::string,int> IBDSelection(std::vector<MCParticle>* MCParticles, int verbose){

  int n_neutrons = 0;
  int n_sec_neutrons = 0;
  int n_gammas = 0;
  int n_positrons = 0;
  int n_sec_gammas = 0;

  if(MCParticles){
    if (verbose) std::cout <<"Num MCParticles = "<<to_string(MCParticles->size())<<std::endl;
    for(unsigned int particlei=0; particlei<MCParticles->size(); particlei++){
      MCParticle aparticle = MCParticles->at(particlei);
      if(aparticle.GetParentPdg()==0) {                //primary particle
        int pdg = aparticle.GetPdgCode();
        double energy = aparticle.GetStartEnergy();
        if (pdg == 2112) n_neutrons++;
        if (pdg == -11 && energy < 100) n_positrons++;
        if (pdg == 22 && energy < 100) n_gammas++;
      } else {
        int pdg = aparticle.GetPdgCode();
        double energy = aparticle.GetStartEnergy();
        if (pdg == 22 && energy < 100) n_sec_gammas++;
        if (pdg == 2112) n_sec_neutrons++;
      }
    }
  }

  std::map<std::string,int> ibd_count;
  ibd_count["NeutronCount"] = n_neutrons;
  ibd_count["SecNeutronCount"] = n_sec_neutrons;
  ibd_count["PositronCount"] = n_positrons;
  ibd_count["GammaCount"] = n_gammas;
  ibd_count["SecGammaCount"] = n_sec_gammas;
 
  return ibd_count;
}

Position FindTrueVertexFromMC(std::vector<MCParticle> *MCParticles, int verbose) {
  
  // loop over the MCParticles to find the vertex for an event

  Position vertex(9999999.,9999999.,9999999.);
  MCParticle primaryparticle; 
  bool found=false;
    
  if (verbose) std::cout <<"Num MCParticles = " << to_string(MCParticles->size()) << std::endl;
  for(unsigned int particlei=0; particlei<MCParticles->size(); particlei++){
    MCParticle aparticle = MCParticles->at(particlei);
    if(aparticle.GetParentPdg()!=0) continue;      // not a primary particle
    primaryparticle = aparticle;                       // note the particle
    found=true;                                  // note that we found it
    break;                                         // primary particles will have all the same vertex
  }
 
  if(not found){
    std::cout <<"No primary particle found in this event"<<std::endl;
    return vertex;
  }
  
  // retrieve desired information from the particle
  vertex = primaryparticle.GetStartVertex(); 

  return vertex;

}

void ConvertPositionTo2D(Position xyz_pos, double &x, double &y, double min_z, double max_z, double size_top_drawing, double tank_radius, double tank_height){

  if (fabs(xyz_pos.Z()-max_z)<0.01){
    //top PMTs
    x=0.5-size_top_drawing*xyz_pos.X()/tank_radius;
    y=0.5+((0.45*tank_height)/tank_radius+1)*size_top_drawing-size_top_drawing*xyz_pos.Y()/tank_radius;
  } else if (fabs(xyz_pos.Z()-min_z)<0.01){
    //bottom PMTs
    x=0.5-size_top_drawing*xyz_pos.X()/tank_radius;
    y=0.5-(0.45*tank_height/tank_radius+1)*size_top_drawing+size_top_drawing*xyz_pos.Y()/tank_radius;
  } else {
    //barrel PMTs
    double phi=0.;
    if (xyz_pos.Y()>0 && xyz_pos.X()>0) phi = atan(xyz_pos.X()/xyz_pos.Y())+TMath::Pi()/2;
    else if (xyz_pos.Y()>0 && xyz_pos.X()<0) phi = atan(xyz_pos.Y()/-xyz_pos.X());
    else if (xyz_pos.Y()<0 && xyz_pos.X()<0) phi = 3*TMath::Pi()/2+atan(xyz_pos.X()/xyz_pos.Y());
    else if (xyz_pos.Y()<0 && xyz_pos.X()>0) phi = TMath::Pi()+atan(-xyz_pos.Y()/xyz_pos.X());
    else if (fabs(xyz_pos.Y())<0.0001){
      if (xyz_pos.X()>0) phi = TMath::Pi();
      else if (xyz_pos.X()<0) phi = 2*TMath::Pi();
    }
    else if (fabs(xyz_pos.X())<0.0001){
      if (xyz_pos.Y()>0) phi = 0.5*TMath::Pi();
      else if (xyz_pos.Y()<0) phi = 3*TMath::Pi()/2;
    }
    else phi = 0.;
    if (phi>2*TMath::Pi()) phi-=(2*TMath::Pi());
    phi-=TMath::Pi();
    if (phi < - TMath::Pi()) phi = -TMath::Pi();
    if (phi<-TMath::Pi() || phi>TMath::Pi())  std::cout <<"Drawing Event: Phi out of bounds! "<<", x= "<<xyz_pos.X()<<", y="<<xyz_pos.Y()<<", z="<<xyz_pos.Z()<<std::endl;
    x=0.5+phi*size_top_drawing;
    y=0.5+xyz_pos.Z()/tank_height*tank_height/tank_radius*size_top_drawing;
  }
}

void ConvertPositionTo2D_Bottom(Position xyz_pos, double &x, double &y, int npmtsY, double size_top_drawing, std::vector<double> phi_positions){

  double rho_slice = 0.6666666;
  int num_slices = 25;
  double rho = sqrt(xyz_pos.X()*xyz_pos.X()+xyz_pos.Y()*xyz_pos.Y());
  for (int i_slice = num_slices; i_slice >0; i_slice--){
    if (rho > (i_slice-1)*rho_slice){
      y = (25-(num_slices-i_slice))/double(npmtsY);
      break;
    }
  }
 
  double phi=0.;  
  if (xyz_pos.Y()>0 && xyz_pos.X()>0) phi = atan(xyz_pos.X()/xyz_pos.Y())+TMath::Pi()/2;
  else if (xyz_pos.Y()>0 && xyz_pos.X()<0) phi = atan(xyz_pos.Y()/-xyz_pos.X());
  else if (xyz_pos.Y()<0 && xyz_pos.X()<0) phi = 3*TMath::Pi()/2+atan(xyz_pos.X()/xyz_pos.Y());
  else if (xyz_pos.Y()<0 && xyz_pos.X()>0) phi = TMath::Pi()+atan(-xyz_pos.Y()/xyz_pos.X());
  else if (fabs(xyz_pos.Y())<0.0001){
    if (xyz_pos.X()>0) phi = TMath::Pi();
    else if (xyz_pos.X()<0) phi = 2*TMath::Pi();
  }
  else if (fabs(xyz_pos.X())<0.0001){
    if (xyz_pos.Y()>0) phi = 0.5*TMath::Pi();
    else if (xyz_pos.Y()<0) phi = 3*TMath::Pi()/2;
  }
  else phi = 0.;
  if (phi>2*TMath::Pi()) phi-=(2*TMath::Pi());
  phi-=TMath::Pi();
  if (phi < - TMath::Pi()) phi = -TMath::Pi();
  if (phi<-TMath::Pi() || phi>TMath::Pi())  std::cout <<"Drawing Event: Phi out of bounds! "<<", x= "<<xyz_pos.X()<<", y="<<xyz_pos.Y()<<", z="<<xyz_pos.Z()<<std::endl;
  x=0.5+phi*size_top_drawing;

  double diff = 100000.;
  double x_min=0.;
  for (int i_phi = 0; i_phi < (int) phi_positions.size(); i_phi++){
    double temp_diff = fabs(phi_positions.at(i_phi)-x);
    if (temp_diff < diff) {x_min = phi_positions.at(i_phi); diff = temp_diff;}
  }
  x = x_min;

}

void ConvertPositionTo2D_Top(Position xyz_pos, double &x, double &y, int npmtsY, double size_top_drawing, std::vector<double> phi_positions){

  double rho = sqrt(xyz_pos.X()*xyz_pos.X()+xyz_pos.Y()*xyz_pos.Y());
  double rho_slice = 0.6666666;
  int num_slices = 25;
  for (int i_slice = num_slices; i_slice >0; i_slice--){
    if (rho > (i_slice-1)*rho_slice){
      y = (51+25+num_slices-i_slice)/double(npmtsY);
      break;
    }
  }

  double phi = 0.;
  if (xyz_pos.Y()>0 && xyz_pos.X()>0) phi = atan(xyz_pos.X()/xyz_pos.Y())+TMath::Pi()/2;
  else if (xyz_pos.Y()>0 && xyz_pos.X()<0) phi = atan(xyz_pos.Y()/-xyz_pos.X());
  else if (xyz_pos.Y()<0 && xyz_pos.X()<0) phi = 3*TMath::Pi()/2+atan(xyz_pos.X()/xyz_pos.Y());
  else if (xyz_pos.Y()<0 && xyz_pos.X()>0) phi = TMath::Pi()+atan(-xyz_pos.Y()/xyz_pos.X());
  else if (fabs(xyz_pos.Y())<0.0001){
    if (xyz_pos.X()>0) phi = TMath::Pi();
    else if (xyz_pos.X()<0) phi = 2*TMath::Pi();
  }
  else if (fabs(xyz_pos.X())<0.0001){
    if (xyz_pos.Y()>0) phi = 0.5*TMath::Pi();
    else if (xyz_pos.Y()<0) phi = 3*TMath::Pi()/2;
  }
  else phi = 0.;
  if (phi>2*TMath::Pi()) phi-=(2*TMath::Pi());
  phi-=TMath::Pi();
  if (phi < - TMath::Pi()) phi = -TMath::Pi();
  if (phi<-TMath::Pi() || phi>TMath::Pi())  std::cout <<"Drawing Event: Phi out of bounds! "<<", x= "<<xyz_pos.X()<<", y="<<xyz_pos.Y()<<", z="<<xyz_pos.Z()<<std::endl;
  x=0.5+phi*size_top_drawing; 
  double diff = 100000.;
  double x_min=0.;
  for (int i_phi = 0; i_phi < (int) phi_positions.size(); i_phi++){
    double temp_diff = fabs(phi_positions.at(i_phi)-x);
    if (temp_diff < diff) {x_min = phi_positions.at(i_phi); diff = temp_diff;}
  }
  x = x_min;

}


int Projection_Atmospheric_DSNB(const char *filename="wcsim_atmospheric_SK.0.0.root", bool verbose=false)
{

  cout << endl;
  cout <<"#################################"<<endl;
  cout << "  Projection_Atmospheric_DSNB  " << endl;
  cout <<"#################################"<<endl;
  cout << endl;

  cout << "Opening WCSim file " << filename << " ... " << endl;

  // Open the file
  TFile * file = new TFile(filename,"read");
  if (!file->IsOpen()){
    cout << "Error, could not open input file: " << filename << endl;
    return -1;
  }

  cout << "Success!" << endl;
  
  // Get the a pointer to the tree from the file
  TTree *tree = (TTree*)file->Get("wcsimT");
  
  // Get the number of events
  int nevent = tree->GetEntries();
  
  cout << endl;
  printf("File has %d events! \n",nevent);
  
  // Create a WCSimRootEvent to put stuff from the tree in

  WCSimRootEvent* wcsimrootsuperevent = new WCSimRootEvent();

  // Set the branch address for reading from the tree
  TBranch *branch = tree->GetBranch("wcsimrootevent");
  branch->SetAddress(&wcsimrootsuperevent);

  // Force deletion to prevent memory leak 
  tree->GetBranch("wcsimrootevent")->SetAutoDelete(kTRUE);


  // Geometry tree - only need 1 "event"
  TTree *geotree = (TTree*)file->Get("wcsimGeoT");
  WCSimRootGeom *geo = 0; 
  geotree->SetBranchAddress("wcsimrootgeom", &geo);
  if(verbose) std::cout << "Geotree has " << geotree->GetEntries() << " entries" << std::endl;
  if (geotree->GetEntries() == 0) {
      exit(9);
  }
  geotree->GetEntry(0);

  //Construct ToolChain Geometry object
  int numtankpmts;
  std::map<int,unsigned long> pmt_tubeid_to_channelkey;
  std::map<unsigned long,int> channelkey_to_pmtid;
  Geometry *geom = (Geometry*) ConstructToolChainGeometry(geo, pmt_tubeid_to_channelkey, channelkey_to_pmtid, numtankpmts, verbose);

  geom->Print();

  //Get geometry properties
  Position detector_center = geom->GetTankCentre();
  double tank_center_x = detector_center.X();
  double tank_center_y = detector_center.Y();
  double tank_center_z = detector_center.Z();
  int n_tank_pmts = geom->GetNumDetectorsInSet("Tank");

  //Read in PMT positions -> important for creating 2D maps later
  double max_z = -1000000.;
  double min_z = 1000000.;
  std::map<int, double> x_pmt, y_pmt, z_pmt;
  std::vector<unsigned long> pmt_detkeys, pmt_chankeys;
  double size_top_drawing = 0.1;
  std::vector<double> vec_pmt2D_x, vec_pmt2D_x_Top, vec_pmt2D_y_Top, vec_pmt2D_x_Bottom, vec_pmt2D_y_Bottom, vec_pmt2D_y;
  std::vector<double> phi_positions;

  //Settings for creating 2D maps/csv files
  std::string DataMode="Normal";                 //options: Charge/Time
  std::string SaveMode="PMT-wise";               //options: Geometric / PMT-wise
  int dimensionX=151;                            //choose something suitable (32/64/...)
  int dimensionY=101;                            //choose something suitable (32/64/...)
  std::string cnn_outpath="atmospheric_"+std::string(filename);
  bool includeTopBottom=1;

  int npmtsX, npmtsY;
  if (SaveMode == "PMT-wise"){
    npmtsX = 150;    //in every row, we have 150 PMTs
    if (!includeTopBottom) npmtsY = 51;     //we have 51 rows of PMTs (excluding top and bottom PMTs)
    else npmtsY = 101;    //for top & bottom PMTs, include 25 extra rows of PMTs at top and at the bottom      
  }

  ifstream phi_file("phi_positions.txt");
  double temp_phi;
  while (!phi_file.eof()){
    phi_file >> temp_phi;
    phi_positions.push_back(temp_phi);
    if (phi_file.eof()) break;
  }
  phi_file.close();

  double tank_radius = geom->GetTankRadius();
  double tank_height = geom->GetTankHalfheight();

  std::cout <<"Tank Detectors loop start"<<std::endl;
  std::map<std::string,std::map<unsigned long,Detector*> >* Detectors = geom->GetDetectors();

  for (std::map<unsigned long,Detector*>::iterator it  = Detectors->at("Tank").begin();
                                                    it != Detectors->at("Tank").end();
                                                  ++it){
    Detector* apmt = it->second;
    unsigned long detkey = it->first;
    pmt_detkeys.push_back(detkey);
    unsigned long chankey = apmt->GetChannels()->begin()->first;
    pmt_chankeys.push_back(chankey);
    Position position_PMT = apmt->GetDetectorPosition();
    x_pmt.insert(std::pair<int,double>(detkey,position_PMT.X()-tank_center_x));
    y_pmt.insert(std::pair<int,double>(detkey,position_PMT.Y()-tank_center_y));
    z_pmt.insert(std::pair<int,double>(detkey,position_PMT.Z()-tank_center_z));
    if (z_pmt[detkey]>max_z && apmt->GetTankLocation()!="OD") max_z = z_pmt.at(detkey);
    if (z_pmt[detkey]<min_z && apmt->GetTankLocation()!="OD") min_z = z_pmt.at(detkey);
  }
  std::cout <<"CNNImage tool: Loop over tank detectors finished. Max z = "<<std::to_string(max_z)<<", min z = "<< std::to_string(min_z)<<std::endl;

  //Order PMT positions
  std::vector<double> vector_y_top, vector_y_bottom, vector_y_barrel;
  for (unsigned int i_pmt = 0; i_pmt < y_pmt.size(); i_pmt++){
    double x,y;
    unsigned long detkey = pmt_detkeys[i_pmt];
    Position pmt_pos(x_pmt[detkey],y_pmt[detkey],z_pmt[detkey]);
    unsigned long chankey = pmt_chankeys[i_pmt];
    Detector *apmt = geom->ChannelToDetector(chankey);
    if (apmt->GetTankLocation()=="OD") continue;  //don't include OD PMTs
    if ((z_pmt[detkey] >= max_z-0.001 || z_pmt[detkey] <= min_z+0.001) && !includeTopBottom) continue;     //don't include top/bottom PMTs if specified
    if (z_pmt[detkey] >= max_z-0.001) {
      ConvertPositionTo2D_Top(pmt_pos, x, y, npmtsY, size_top_drawing, phi_positions);
      vector_y_top.push_back(y);
    }
    else if (z_pmt[detkey] <= min_z+0.001) {
      ConvertPositionTo2D_Bottom(pmt_pos, x, y, npmtsY, size_top_drawing, phi_positions);
      vector_y_bottom.push_back(y);
    }
    else {
      ConvertPositionTo2D(pmt_pos, x, y, min_z, max_z, size_top_drawing, tank_radius, tank_height);
      vector_y_barrel.push_back(y);
    }
    x = (round(1000*x)/1000.);
    y = (round(1000*y)/1000.);
    if (z_pmt[detkey] >= max_z-0.001) vec_pmt2D_x_Top.push_back(x);
    else if (z_pmt[detkey] <= min_z+0.001) vec_pmt2D_x_Bottom.push_back(x);
    else vec_pmt2D_x.push_back(x);
    vec_pmt2D_y.push_back(y);
  }

  if (verbose) std::cout <<"vec_pmt2D_* size: "<<vec_pmt2D_x.size()<<std::endl;
  std::sort(vec_pmt2D_x.begin(),vec_pmt2D_x.end());
  std::sort(vec_pmt2D_y.begin(),vec_pmt2D_y.end());
  std::sort(vec_pmt2D_x_Top.begin(),vec_pmt2D_x_Top.end());
  std::sort(vec_pmt2D_x_Bottom.begin(),vec_pmt2D_x_Bottom.end());
  std::sort(vector_y_top.begin(),vector_y_top.end());
  std::sort(vector_y_bottom.begin(),vector_y_bottom.end());
  std::sort(vector_y_barrel.begin(),vector_y_barrel.end());
  vec_pmt2D_x.erase(std::unique(vec_pmt2D_x.begin(),vec_pmt2D_x.end()),vec_pmt2D_x.end());
  vec_pmt2D_y.erase(std::unique(vec_pmt2D_y.begin(),vec_pmt2D_y.end()),vec_pmt2D_y.end());
  vec_pmt2D_x_Top.erase(std::unique(vec_pmt2D_x_Top.begin(),vec_pmt2D_x_Top.end()),vec_pmt2D_x_Top.end());
  vec_pmt2D_x_Bottom.erase(std::unique(vec_pmt2D_x_Bottom.begin(),vec_pmt2D_x_Bottom.end()),vec_pmt2D_x_Bottom.end());
  vector_y_top.erase(std::unique(vector_y_top.begin(),vector_y_top.end()),vector_y_top.end());
  vector_y_bottom.erase(std::unique(vector_y_bottom.begin(),vector_y_bottom.end()),vector_y_bottom.end());
  vector_y_barrel.erase(std::unique(vector_y_barrel.begin(),vector_y_barrel.end()),vector_y_barrel.end());
 
  //Print out ordered PMT positions, just for debugging
  if (verbose) {
    std::cout <<"Sorted 2D position vectors: "<<std::endl;
    for (unsigned int i_x=0;i_x<vec_pmt2D_x.size();i_x++){
      std::cout <<"x vector "<<i_x<<": "<<vec_pmt2D_x.at(i_x)<<std::endl;
    }
    for (unsigned int i_y=0;i_y<vec_pmt2D_y.size();i_y++){
      std::cout <<"y vector "<<i_y<<": "<<vec_pmt2D_y.at(i_y)<<std::endl;
    }
    for (unsigned int i_x=0;i_x<vec_pmt2D_x_Top.size();i_x++){
      std::cout <<"x top vector "<<i_x<<": "<<vec_pmt2D_x_Top.at(i_x)<<std::endl;
    }
    for (unsigned int i_x=0;i_x<vec_pmt2D_x_Bottom.size();i_x++){
      std::cout <<"x bottom vector "<<i_x<<": "<<vec_pmt2D_x_Bottom.at(i_x)<<std::endl;
    }
    for (unsigned int i_y=0; i_y < vector_y_top.size(); i_y++){
      std::cout <<"y (top): "<<vector_y_top.at(i_y)<<std::endl;
    }
    for (unsigned int i_y=0; i_y < vector_y_bottom.size(); i_y++){
      std::cout <<"y (bottom): "<<vector_y_bottom.at(i_y)<<std::endl;
    }
    for (unsigned int i_y=0; i_y < vector_y_barrel.size(); i_y++){
      std::cout <<"y (barrel): "<<vector_y_barrel.at(i_y)<<std::endl;
    }
  }
  
  //Define output csv files

  std::string str_charge = "_charge";
  std::string str_time = "_time";
  std::string str_firsttime = "_firsttime";
  std::string str_csv = ".csv";
  std::string str_abs = "_abs";
  std::string str_root = ".root";

  std::string csvfile_name = cnn_outpath + str_charge + str_csv;
  std::string csvfile_time_name = cnn_outpath + str_time + str_csv;
  std::string csvfile_firsttime_name = cnn_outpath + str_firsttime + str_csv;
  std::string csvfile_abs = cnn_outpath + str_charge + str_abs + str_csv;
  std::string csvfile_time_abs = cnn_outpath + str_time + str_abs + str_csv;
  std::string csvfile_firsttime_abs = cnn_outpath + str_firsttime + str_abs + str_csv;
  std::string rootfile_name = cnn_outpath + str_root;

  ofstream outfile, outfile_time, outfile_firsttime, outfile_abs, outfile_abs_time, outfile_abs_firsttime;
  outfile.open(csvfile_name.c_str());
  outfile_time.open(csvfile_time_name.c_str());
  outfile_firsttime.open(csvfile_firsttime_name.c_str());
  outfile_abs.open(csvfile_abs.c_str());
  outfile_abs_time.open(csvfile_time_abs.c_str());
  outfile_abs_firsttime.open(csvfile_firsttime_abs.c_str());

  TFile *root_outfile = new TFile(rootfile_name.c_str(),"RECREATE");

  // Options tree - only need 1 "event"
  TTree *opttree = (TTree*)file->Get("wcsimRootOptionsT");
  WCSimRootOptions *opt = 0; 
  opttree->SetBranchAddress("wcsimrootoptions", &opt);
  if(verbose) std::cout << "Optree has " << opttree->GetEntries() << " entries" << std::endl;
  if (opttree->GetEntries() == 0) {
    exit(9);
  }
  opttree->GetEntry(0);
  if (verbose){
    cout << endl; 
    cout <<"#######################"<<endl;
    cout <<"Detector options: "<<endl;
    cout <<"#######################"<<endl;
    cout << endl;
    opt->Print();
  }

  // start with the main "subevent", as it contains most of the info
  // and always exists.
  WCSimRootTrigger* wcsimrootevent;

  //Initialize ToolAnalysis-specific analysis objects
  int HistoricTriggeroffset = 0;
  bool AllowZeroFlag = true;       // allow particles with flag 0 to be loaded?  
  std::vector<MCParticle>* MCParticles = new std::vector<MCParticle>; //vector to store particle properties
  std::map<unsigned long,std::vector<MCHit>>* MCHits = new std::map<unsigned long,std::vector<MCHit>>; //map to store all PMT hits
  uint32_t EventNumber = 0; //Event Number variable
  int WCSimVersion = 3;  //WCSimVersion variable
  uint64_t EventTimeNs;
  int use_smeared_digit_time = 1;
  std::map<int,int> *trackid_to_mcparticleindex = new std::map<int,int>;
  
  int num_trig=0;
 
  cout << endl; 
  cout <<"#######################"<<endl;
  cout <<"Start loop over events!"<<endl;
  cout <<"#######################"<<endl;
  cout << endl;

  // Now loop over events
  int mcev=0;
  for (int ev=0; ev<nevent; ev++)
  {

    EventNumber = (uint32_t) ev;

    //Show a small progress bar for the event number
    progress_bar(ev+1,nevent);

    // Read the event from the tree into the WCSimRootEvent instance
    tree->GetEntry(ev);      
    wcsimrootevent = wcsimrootsuperevent->GetTrigger(0);
    if(verbose) printWCSimRootTrigger(wcsimrootevent, wcsimrootsuperevent);
    
    EventTimeNs = wcsimrootevent->GetHeader()->GetDate();
    // Now read the tracks in the event
    // Get the number of tracks
    int ntrack = wcsimrootevent->GetNtrack();
    int ntrack_slots = wcsimrootevent->GetNtrack_slots();
    if(verbose) printf("Number of tracks = %d\n",ntrack);

    //Clear objects
    MCParticles->clear();
    MCHits->clear();
    trackid_to_mcparticleindex->clear();

    // Loop through elements in the TClonesArray of WCSimTracks
    int i;
    for (i=0; i<ntrack_slots; i++)
    {
      WCSimRootTrack *wcsimroottrack = (WCSimRootTrack*) (wcsimrootevent->GetTracks())->At(i);
      if(verbose) printWCSimRootTrack(wcsimroottrack);

      tracktype startstoptype = tracktype::UNDEFINED;
      if (wcsimroottrack->GetParenttype()==0 && verbose){
        std::cout <<"flag (track): "<<wcsimroottrack->GetFlag()<<", PDG: "<<wcsimroottrack->GetIpnu()<<", energy: "<<wcsimroottrack->GetE()<<", stop energy: "<<wcsimroottrack->GetEndE()<<std::endl;
      }

      int ipnu = wcsimroottrack->GetIpnu();
      if(!AllowZeroFlag && wcsimroottrack->GetFlag()!=-1 ) continue; // flag 0 only is normal particles: excludes neutrino
      else if (AllowZeroFlag && wcsimroottrack->GetFlag()!=-1 && wcsimroottrack->GetFlag()!=0) continue; 

      //First trigger contains all primary particles already -> only use i==0
      //if (i==0){
      //Define MCParticle
      MCParticle thisparticle(
        wcsimroottrack->GetIpnu(), wcsimroottrack->GetE(), wcsimroottrack->GetEndE(),
        Position(wcsimroottrack->GetStart(0) / 100.,
          wcsimroottrack->GetStart(1) / 100.,
          wcsimroottrack->GetStart(2) / 100.),
        Position(wcsimroottrack->GetStop(0) / 100.,
          wcsimroottrack->GetStop(1) / 100.,
          wcsimroottrack->GetStop(2) / 100.),
        //MC particle times now stored relative to the trigger time
        (static_cast<double>(wcsimroottrack->GetTime()-EventTimeNs)),
        (static_cast<double>(wcsimroottrack->GetStopTime()-EventTimeNs)),
        Direction(wcsimroottrack->GetDir(0), wcsimroottrack->GetDir(1), wcsimroottrack->GetDir(2)),
        (sqrt(pow(wcsimroottrack->GetStop(0)-wcsimroottrack->GetStart(0),2.)+
          pow(wcsimroottrack->GetStop(1)-wcsimroottrack->GetStart(1),2.)+
          pow(wcsimroottrack->GetStop(2)-wcsimroottrack->GetStart(2),2.))) / 100.,
        startstoptype,
        wcsimroottrack->GetId(),
        wcsimroottrack->GetParenttype(),
        wcsimroottrack->GetFlag(),
        wcsimroottrack->GetParentId(),
        wcsimroottrack->GetCreator(),
        wcsimroottrack->GetDestroyer());

      trackid_to_mcparticleindex->emplace(wcsimroottrack->GetId(),MCParticles->size());

      MCParticles->push_back(thisparticle);
      //}
    }

    if (verbose){
      cout<<"MCParticles has "<<MCParticles->size()<<" entries"<<endl;
    }

    // Now look at the Cherenkov hits
    int ncherenkovhits     = wcsimrootevent->GetNcherenkovhits();
    int ncherenkovdigihits = wcsimrootevent->GetNcherenkovdigihits(); 
    
    if(verbose){
      printf("node id: %i\n", ev);
      printf("Ncherenkovhits %d\n",     ncherenkovhits);
      printf("Ncherenkovdigihits %d\n", ncherenkovdigihits);
      cout << "RAW HITS:" << endl;
    }

    // Grab the big arrays of times and parent IDs
    TClonesArray *timeArray = wcsimrootevent->GetCherenkovHitTimes();
    
    int totalPe = 0;
    for (i=0; i< ncherenkovhits; i++)
    {
      WCSimRootCherenkovHit *wcsimrootcherenkovhit = (WCSimRootCherenkovHit*) (wcsimrootevent->GetCherenkovHits())->At(i);

      int tubeNumber     = wcsimrootcherenkovhit->GetTubeID();
      int timeArrayIndex = wcsimrootcherenkovhit->GetTotalPe(0);
      int peForTube      = wcsimrootcherenkovhit->GetTotalPe(1);
      WCSimRootPMT pmt   = geo->GetPMT(tubeNumber-1);
      totalPe += peForTube;
    } // End of loop over Cherenkov hits
    if(verbose) cout << "Total Pe : " << totalPe << endl;
    
    // Look at digitized hit info
    // Get the number of digitized hits
    // Loop over sub events
   
   WCSimRootTrigger *firsttrigt = (WCSimRootTrigger*) wcsimrootsuperevent->GetTrigger(0);
    if(verbose) cout << "DIGITIZED HITS:" << endl;
 //   for (int index = 0 ; index < wcsimrootsuperevent->GetNumberOfEvents(); index++) 
    for (int index = 0 ; index < 1; index++) 
    {
      wcsimrootevent = wcsimrootsuperevent->GetTrigger(index);
      if(verbose) cout << "Sub event number = " << index << "\n";
      int ncherenkovdigihits = wcsimrootevent->GetNcherenkovdigihits();
      if(verbose) printf("Ncherenkovdigihits %d\n", ncherenkovdigihits);
      int ncherenkovdigihits_slots = wcsimrootevent->GetNcherenkovdigihits_slots();
      if(ncherenkovdigihits>0)  num_trig++;
      int idigi = 0;
      //only add hits for trigger 0
      for (i=0;i<ncherenkovdigihits_slots;i++)
      {
        idigi++;
        WCSimRootCherenkovDigiHit *digihit = (WCSimRootCherenkovDigiHit*) (wcsimrootevent->GetCherenkovDigiHits())->At(i);
	
        int tubeid = digihit->GetTubeId();  // geometry TubeID->channelkey map is made INCLUDING offset of 1
        if(pmt_tubeid_to_channelkey.count(tubeid)==0){
          cerr<<"LoadWCSim ERROR: tank PMT with no associated ChannelKey!"<<endl;
          return false;
        }

        unsigned long key = pmt_tubeid_to_channelkey.at(tubeid);
        double digittime;
        if(use_smeared_digit_time){
          digittime = static_cast<double>(digihit->GetT()-HistoricTriggeroffset); // relative to trigger
        } else {
          std::vector<int> photonids = digihit->GetPhotonIds();   // indices of the digit's photons
          double earliestphotontruetime=999999999999;
          for(int& aphotonindex : photonids){
            WCSimRootCherenkovHitTime* thehittimeobject =
             (WCSimRootCherenkovHitTime*)firsttrigt->GetCherenkovHitTimes()->At(aphotonindex);
            if(thehittimeobject==nullptr){
              cerr<<"LoadWCSim Tool: ERROR! Retrieval of photon from digit returned nullptr!"<<endl;
              continue;
            }
            double aphotontime = static_cast<double>(thehittimeobject->GetTruetime());
            if(aphotontime<earliestphotontruetime){ earliestphotontruetime = aphotontime; }
          }
          digittime = earliestphotontruetime;
        }
        float digiq = digihit->GetQ();
        std::vector<int> parents = GetHitParentIds(digihit, firsttrigt, trackid_to_mcparticleindex);
        MCHit nexthit(key, digittime, digiq, parents);
        if(MCHits->count(key)==0) MCHits->emplace(key, std::vector<MCHit>{nexthit});
        else MCHits->at(key).push_back(nexthit);
      } // End of ncherenkovdigihits_slots loop
    } // End of loop over trigger
  
    
    //Event selection

    //Get vertex of the event
    Position vertex;
    vertex = FindTrueVertexFromMC(MCParticles, verbose);

    //Get the number of ibd-like particles (gammas, positrons for prompt, neutrons for delayed)
    std::map<std::string,int> ibd_count;
    ibd_count = IBDSelection(MCParticles, verbose);

    //Create 2D maps
    std::map<unsigned long,double> charge, time, first_time;
    std::vector<unsigned long> hitpmt_detkeys;
    double maximum_pmts;
    double total_charge_pmts;
    int total_hits_pmts = 0;
    double min_time_pmts, max_time_pmts;
    double max_firsttime_pmts, min_firsttime_pmts;

    for (unsigned int i_pmt=0; i_pmt<pmt_detkeys.size();i_pmt++){
      unsigned long detkey = pmt_detkeys[i_pmt];
      charge.emplace(detkey,0.);
      time.emplace(detkey,0.);
      first_time.emplace(detkey,0.);
    }

    std::stringstream ss_hist_time, ss_hist_time_title, ss_hist_charge, ss_hist_charge_title;
    ss_hist_time <<"h_time"<<mcev;
    ss_hist_time_title << "PMT hit times Event "<<mcev;
    ss_hist_charge <<"h_charge"<<mcev;
    ss_hist_charge_title << "Total charge Event "<<mcev;
    TH1F *h_time = new TH1F(ss_hist_time.str().c_str(),ss_hist_time_title.str().c_str(),2000,0,2000);
    TH1F *h_charge = new TH1F(ss_hist_charge.str().c_str(),ss_hist_charge_title.str().c_str(),2000,0,100);

    //---------------------------------------------------------------
    //-------------------Iterate over MCHits ------------------------
    //---------------------------------------------------------------

    int vectsize = MCHits->size();
    double total_charge=0.;

    for(std::pair<unsigned long, std::vector<MCHit>>&& apair : *MCHits){
      unsigned long chankey = apair.first;
      Detector* thistube = geom->ChannelToDetector(chankey);
      unsigned long detkey = thistube->GetDetectorID();
      if (thistube->GetDetectorElement()=="Tank"){
        if (thistube->GetTankLocation()=="OD") continue;
        hitpmt_detkeys.push_back(detkey);
        std::vector<MCHit>& Hits = apair.second;
        int hits_pmt = 0;
        for (MCHit &ahit : Hits){
          if (verbose) std::cout <<"CNNImage tool: time: "<<ahit.GetTime()<<", charge: "<<ahit.GetCharge()<<std::endl;
          h_time->Fill(ahit.GetTime());
          //Time cut --> only relevant hits
          if (ahit.GetTime()>800. && ahit.GetTime()<1200.){
            charge[detkey] += ahit.GetCharge();
            if (DataMode == "Normal") time[detkey] += ahit.GetTime();
            else if (DataMode == "Charge-Weighted") time[detkey] += (ahit.GetTime()*ahit.GetCharge());
            if (hits_pmt==0) first_time[detkey] = ahit.GetTime();
            hits_pmt++;
          }
        }
        h_charge->Fill(charge[detkey]);
        if (DataMode == "Normal" && hits_pmt>0) time[detkey]/=hits_pmt;         //use mean time of all hits on one PMT
        else if (DataMode == "Charge-Weighted" && charge[detkey]>0.) time[detkey] /= charge[detkey];
        total_hits_pmts++;
        total_charge+=charge[detkey];
      }
    }
    if (verbose) std::cout<<"MCHits loop finished."<<std::endl;
  
    //---------------------------------------------------------------
    //------------- Determine max+min values ------------------------
    //---------------------------------------------------------------

    maximum_pmts = 0;
    max_time_pmts = -999999;
    min_time_pmts = 999999.;
    max_firsttime_pmts = -999999.;
    min_firsttime_pmts = 9999999.;
    total_charge_pmts = 0;

    for (unsigned int i_pmt=0;i_pmt<hitpmt_detkeys.size();i_pmt++){
      unsigned long detkey = hitpmt_detkeys[i_pmt];
      if (charge[detkey]>maximum_pmts) maximum_pmts = charge[detkey];
      total_charge_pmts+=charge[detkey];
      if (time[detkey]>max_time_pmts) max_time_pmts = time[detkey];
      if (time[detkey]<min_time_pmts) min_time_pmts = time[detkey];
      if (first_time[detkey]>max_firsttime_pmts) max_firsttime_pmts = first_time[detkey];
      if (first_time[detkey]<min_firsttime_pmts) min_firsttime_pmts = first_time[detkey];
    }
    if (verbose) std::cout<<"Max Time and min time: " << max_time_pmts<<", " << min_time_pmts<<std::endl;
    if (verbose) std::cout <<"Max and min first-time: "<<max_firsttime_pmts<<", "<<min_firsttime_pmts<<std::endl;  

    double global_max_time = max_time_pmts;
    double global_max_charge = maximum_pmts;
    double global_min_charge = 0.;
    double global_min_time = min_time_pmts;

    if (fabs(global_max_time-global_min_time)<0.01) global_max_time = global_min_time+1;
    if (global_max_charge<0.001) global_max_charge=1;  
    if (fabs(max_firsttime_pmts-min_firsttime_pmts)<0.01) max_firsttime_pmts = min_firsttime_pmts+1;  

    //---------------------------------------------------------------
    //-------------- Create CNN images ------------------------------
    //---------------------------------------------------------------

    //define histogram as an intermediate step to the CNN
    std::stringstream ss_cnn, ss_title_cnn, ss_cnn_time, ss_title_cnn_time, ss_cnn_pmtwise, ss_title_cnn_pmtwise, ss_cnn_time_pmtwise, ss_title_cnn_time_pmtwise, ss_cnn_time_first_pmtwise, ss_title_cnn_time_first_pmtwise, ss_cnn_time_first, ss_title_cnn_time_first;
    std::stringstream ss_cnn_abs, ss_title_cnn_abs, ss_cnn_abs_time, ss_title_cnn_abs_time, ss_cnn_abs_pmtwise, ss_title_cnn_abs_pmtwise, ss_cnn_abs_time_pmtwise, ss_title_cnn_abs_time_pmtwise, ss_cnn_abs_time_first_pmtwise, ss_title_cnn_abs_time_first_pmtwise, ss_cnn_abs_time_first, ss_title_cnn_abs_time_first;
    int evnum = mcev;
    ss_cnn<<"hist_cnn"<<evnum;
    ss_title_cnn<<"EventDisplay (CNN), Event "<<evnum;
    ss_cnn_time<<"hist_cnn_time"<<evnum;
    ss_title_cnn_time<<"EventDisplay Time (CNN), Event "<<evnum;
    ss_cnn_time_first<<"hist_cnn_time_first"<<evnum;
    ss_title_cnn_time_first<<"EventDisplay First HitTime (CNN), Event "<<evnum;
    ss_cnn_abs<<"hist_cnn_abs"<<evnum;
    ss_title_cnn_abs<<"EventDisplay Charge(CNN), Event "<<evnum;
    ss_cnn_abs_time<<"hist_cnn_abs_time"<<evnum;
    ss_title_cnn_abs_time<<"EventDisplay Absolute Time (CNN), Event "<<evnum;
    ss_cnn_abs_time_first<<"hist_cnn_abs_time_first"<<evnum;
    ss_title_cnn_abs_time_first<<"EventDisplay Absolute First HitTime (CNN), Event "<<evnum;
    ss_cnn_pmtwise<<"hist_cnn_pmtwise"<<evnum;
    ss_title_cnn_pmtwise<<"EventDisplay (CNN, pmt wise), Event "<<evnum;
    ss_cnn_time_pmtwise << "hist_cnn_time_pmtwise"<<evnum;
    ss_title_cnn_time_pmtwise <<"EventDisplay Time (CNN, pmt wise), Event "<<evnum;
    ss_cnn_time_first_pmtwise <<"hist_cnn_time_first_pmtwise"<<evnum;
    ss_title_cnn_time_first_pmtwise <<"EventDisplay First Hit Time (CNN, pmt wise), Event "<<evnum;
    ss_cnn_abs_pmtwise<<"hist_cnn_abs_pmtwise"<<evnum;
    ss_title_cnn_abs_pmtwise<<"EventDisplay Charge (CNN, pmt wise), Event "<<evnum;
    ss_cnn_abs_time_pmtwise << "hist_cnn_abs_time_pmtwise"<<evnum;
    ss_title_cnn_abs_time_pmtwise <<"EventDisplay Absolute Time (CNN, pmt wise), Event "<<evnum;
    ss_cnn_abs_time_first_pmtwise <<"hist_cnn_abs_time_first_pmtwise"<<evnum;
    ss_title_cnn_abs_time_first_pmtwise <<"EventDisplay Absolute First Hit Time (CNN, pmt wise), Event "<<evnum;
    TH2F *hist_cnn = new TH2F(ss_cnn.str().c_str(),ss_title_cnn.str().c_str(),dimensionX,0.5-TMath::Pi()*size_top_drawing,0.5+TMath::Pi()*size_top_drawing,dimensionY,0.5-(0.45*tank_height/tank_radius+2)*size_top_drawing, 0.5+(0.45*tank_height/tank_radius+2)*size_top_drawing);
    TH2F *hist_cnn_time = new TH2F(ss_cnn_time.str().c_str(),ss_title_cnn_time.str().c_str(),dimensionX,0.5-TMath::Pi()*size_top_drawing,0.5+TMath::Pi()*size_top_drawing,dimensionY,0.5-(0.45*tank_height/tank_radius+2)*size_top_drawing, 0.5+(0.45*tank_height/tank_radius+2)*size_top_drawing);
    TH2F *hist_cnn_time_first = new TH2F(ss_cnn_time_first.str().c_str(),ss_title_cnn_time_first.str().c_str(),dimensionX,0.5-TMath::Pi()*size_top_drawing,0.5+TMath::Pi()*size_top_drawing,dimensionY,0.5-(0.45*tank_height/tank_radius+2)*size_top_drawing, 0.5+(0.45*tank_height/tank_radius+2)*size_top_drawing);
    TH2F *hist_cnn_abs = new TH2F(ss_cnn_abs.str().c_str(),ss_title_cnn_abs.str().c_str(),dimensionX,0.5-TMath::Pi()*size_top_drawing,0.5+TMath::Pi()*size_top_drawing,dimensionY,0.5-(0.45*tank_height/tank_radius+2)*size_top_drawing, 0.5+(0.45*tank_height/tank_radius+2)*size_top_drawing);
    TH2F *hist_cnn_abs_time = new TH2F(ss_cnn_abs_time.str().c_str(),ss_title_cnn_abs_time.str().c_str(),dimensionX,0.5-TMath::Pi()*size_top_drawing,0.5+TMath::Pi()*size_top_drawing,dimensionY,0.5-(0.45*tank_height/tank_radius+2)*size_top_drawing, 0.5+(0.45*tank_height/tank_radius+2)*size_top_drawing);
    TH2F *hist_cnn_abs_time_first = new TH2F(ss_cnn_abs_time_first.str().c_str(),ss_title_cnn_abs_time_first.str().c_str(),dimensionX,0.5-TMath::Pi()*size_top_drawing,0.5+TMath::Pi()*size_top_drawing,dimensionY,0.5-(0.45*tank_height/tank_radius+2)*size_top_drawing, 0.5+(0.45*tank_height/tank_radius+2)*size_top_drawing);
    TH2F *hist_cnn_pmtwise = new TH2F(ss_cnn_pmtwise.str().c_str(),ss_title_cnn_pmtwise.str().c_str(),npmtsX,0,npmtsX,npmtsY,0,npmtsY);
    TH2F *hist_cnn_time_pmtwise = new TH2F(ss_cnn_time_pmtwise.str().c_str(),ss_title_cnn_time_pmtwise.str().c_str(),npmtsX,0,npmtsX,npmtsY,0,npmtsY);
    TH2F *hist_cnn_time_first_pmtwise = new TH2F(ss_cnn_time_first_pmtwise.str().c_str(),ss_title_cnn_time_first_pmtwise.str().c_str(),npmtsX,0,npmtsX,npmtsY,0,npmtsY);
    TH2F *hist_cnn_abs_pmtwise = new TH2F(ss_cnn_abs_pmtwise.str().c_str(),ss_title_cnn_abs_pmtwise.str().c_str(),npmtsX,0,npmtsX,npmtsY,0,npmtsY);
    TH2F *hist_cnn_abs_time_pmtwise = new TH2F(ss_cnn_abs_time_pmtwise.str().c_str(),ss_title_cnn_abs_time_pmtwise.str().c_str(),npmtsX,0,npmtsX,npmtsY,0,npmtsY);
    TH2F *hist_cnn_abs_time_first_pmtwise = new TH2F(ss_cnn_abs_time_first_pmtwise.str().c_str(),ss_title_cnn_abs_time_first_pmtwise.str().c_str(),npmtsX,0,npmtsX,npmtsY,0,npmtsY);

    for (int i_pmt=0;i_pmt<n_tank_pmts;i_pmt++){

      //Convert PMT position to 2D hitmap location
      unsigned long detkey = pmt_detkeys[i_pmt];
      double x,y;
      Position pmt_pos(x_pmt[detkey],y_pmt[detkey],z_pmt[detkey]);
      ConvertPositionTo2D(pmt_pos, x, y, min_z, max_z, size_top_drawing, tank_radius, tank_height);
    
      //Fill geometric 2D-hitmap
      int binx = hist_cnn->GetXaxis()->FindBin(x);
      int biny = hist_cnn->GetYaxis()->FindBin(y);
      if (verbose) std::cout <<"Chankey: "<<std::to_string(detkey)<<", binx: "<<std::to_string(binx)<<", biny: "<<std::to_string(biny)<<", charge fill: "<<std::to_string(charge[detkey])<<", time fill: "+std::to_string(time[detkey])<<std::endl;

      if (maximum_pmts < 0.001) maximum_pmts = 1.;
      double charge_fill = charge[detkey]/global_max_charge;
      hist_cnn->SetBinContent(binx,biny,hist_cnn->GetBinContent(binx,biny)+charge_fill);
      hist_cnn_abs->SetBinContent(binx,biny,hist_cnn_abs->GetBinContent(binx,biny)+charge[detkey]);
      if (fabs(max_time_pmts) < 0.001) max_time_pmts = 1.;
      double time_fill = 0.;
      double time_first_fill = 0.;
      if (charge_fill > 1e-10) {
        time_fill = (time[detkey]-global_min_time)/(global_max_time-global_min_time);
        time_first_fill = (first_time[detkey]-min_firsttime_pmts)/(max_firsttime_pmts-min_firsttime_pmts);
      }
      //For the time files, just accept newest entry as the new overall entry
      hist_cnn_time->SetBinContent(binx,biny,time_fill);
      hist_cnn_time_first->SetBinContent(binx,biny,time_first_fill);
      hist_cnn_abs_time->SetBinContent(binx,biny,time[detkey]);
      hist_cnn_abs_time_first->SetBinContent(binx,biny,first_time[detkey]);

      //Fill the pmt-wise histogram
      if ((z_pmt[detkey]>=max_z || z_pmt[detkey]<=min_z) && !includeTopBottom) continue;       //don't include endcaps in the pmt-wise histogram if specified
      if (z_pmt[detkey]>=max_z) ConvertPositionTo2D_Top(pmt_pos,x,y,npmtsY, size_top_drawing, phi_positions);
      if (z_pmt[detkey]<=min_z) ConvertPositionTo2D_Bottom(pmt_pos,x,y,npmtsY, size_top_drawing, phi_positions);
      double xCorr, yCorr;
      xCorr = (round(1000*x)/1000.);
      yCorr = (round(1000*y)/1000.);
      std::vector<double>::iterator it_x, it_y;
      if (z_pmt[detkey]>=max_z){
        it_x = std::find(vec_pmt2D_x_Top.begin(),vec_pmt2D_x_Top.end(),xCorr);
      }
      else if (z_pmt[detkey]<=min_z){
        it_x = std::find(vec_pmt2D_x_Bottom.begin(),vec_pmt2D_x_Bottom.end(),xCorr);
      }
      else {
        it_x = std::find(vec_pmt2D_x.begin(),vec_pmt2D_x.end(),xCorr);
      }
      it_y = std::find(vec_pmt2D_y.begin(),vec_pmt2D_y.end(),yCorr);
      int index_x, index_y;
      if (z_pmt[detkey]>=max_z) index_x = std::distance(vec_pmt2D_x_Top.begin(),it_x);
      else if (z_pmt[detkey]<=min_z) index_x = std::distance(vec_pmt2D_x_Bottom.begin(),it_x);
      else index_x = std::distance(vec_pmt2D_x.begin(),it_x);
      index_y = std::distance(vec_pmt2D_y.begin(),it_y);
      hist_cnn_pmtwise->SetBinContent(index_x+1,index_y+1,charge_fill);
      hist_cnn_time_pmtwise->SetBinContent(index_x+1,index_y+1,time_fill);
      hist_cnn_time_first_pmtwise->SetBinContent(index_x+1,index_y+1,time_first_fill);
      hist_cnn_abs_pmtwise->SetBinContent(index_x+1,index_y+1,charge[detkey]);
      hist_cnn_abs_time_pmtwise->SetBinContent(index_x+1,index_y+1,time[detkey]);
      hist_cnn_abs_time_first_pmtwise->SetBinContent(index_x+1,index_y+1,first_time[detkey]);
    }

    //Event selection
    int neutron_count = ibd_count["NeutronCount"];
    int sec_neutron_count = ibd_count["SecNeutronCount"];
    int gamma_count = ibd_count["GammaCount"];
    int sec_gamma_count = ibd_count["SecGammaCount"];
    int positron_count = ibd_count["PositronCount"];

    if (verbose) std::cout <<"neutron count: "<<neutron_count<<", secondary neutron count: "<<sec_neutron_count<<", gamma count: "<<gamma_count<<", secondary gamma count: "<<sec_gamma_count<<", positron count: "<<positron_count<<std::endl;
    int total_gamma_count = gamma_count+sec_gamma_count;
    int total_neutron_count = neutron_count+sec_neutron_count;

    //Select events with at least one positron/gamma + at least one neutron for IBD-like selection
    bool is_dsnb_like = false;
    if (total_neutron_count >= 1 && (total_gamma_count >= 1 || positron_count >=1)) is_dsnb_like = true;

    //---------------------------------------------------------------
    //-------------- Write to csv-file ------------------------------
    //---------------------------------------------------------------

    if (verbose) std::cout <<"ev: "<<ev<<"dsnb_like: "<<is_dsnb_like<<std::endl;

    if (is_dsnb_like){
    //save root histograms
    hist_cnn->Write();
    hist_cnn_time->Write();
    hist_cnn_time_first->Write();
    hist_cnn_pmtwise->Write();
    hist_cnn_time_pmtwise->Write();
    hist_cnn_time_first_pmtwise->Write();
    hist_cnn_abs->Write();
    hist_cnn_abs_time->Write();
    hist_cnn_abs_time_first->Write();
    hist_cnn_abs_pmtwise->Write();
    hist_cnn_abs_time_pmtwise->Write();
    hist_cnn_abs_time_first_pmtwise->Write();
    h_time->Write();
    h_charge->Write();

    //csv files
    if (SaveMode == "Geometric"){
      for (int i_binY=0; i_binY < hist_cnn->GetNbinsY();i_binY++){
        for (int i_binX=0; i_binX < hist_cnn->GetNbinsX();i_binX++){
          outfile << hist_cnn->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn->GetNbinsX()-1 || i_binY!=hist_cnn->GetNbinsY()-1) outfile<<",";
          outfile_time << hist_cnn_time->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_time->GetNbinsX()-1 || i_binY!=hist_cnn_time->GetNbinsY()-1) outfile_time<<",";    
          outfile_firsttime << hist_cnn_time_first->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_time_first->GetNbinsX()-1 || i_binY!=hist_cnn_time_first->GetNbinsY()-1) outfile_firsttime<<",";    
          outfile_abs << hist_cnn_abs->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_abs->GetNbinsX()-1 || i_binY!=hist_cnn_abs->GetNbinsY()-1) outfile_abs<<",";
          outfile_abs_time << hist_cnn_abs_time->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_abs_time->GetNbinsX()-1 || i_binY!=hist_cnn_abs_time->GetNbinsY()-1) outfile_abs_time<<",";    
          outfile_abs_firsttime << hist_cnn_abs_time_first->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_abs_time_first->GetNbinsX()-1 || i_binY!=hist_cnn_abs_time_first->GetNbinsY()-1) outfile_abs_firsttime<<",";    
        }
      }
    } else if (SaveMode == "PMT-wise"){
      //std::cout <<"Entering PMT-wise save mode"<<std::endl;
      for (int i_binY=0; i_binY < hist_cnn_pmtwise->GetNbinsY();i_binY++){
        for (int i_binX=0; i_binX < hist_cnn_pmtwise->GetNbinsX();i_binX++){
          outfile << hist_cnn_pmtwise->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_pmtwise->GetNbinsX()-1 || i_binY!=hist_cnn_pmtwise->GetNbinsY()-1) outfile<<",";
          outfile_time << hist_cnn_time_pmtwise->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_time_pmtwise->GetNbinsX()-1 || i_binY!=hist_cnn_time_pmtwise->GetNbinsY()-1) outfile_time<<",";    
          outfile_firsttime << hist_cnn_time_first_pmtwise->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_time_first_pmtwise->GetNbinsX()-1 || i_binY!=hist_cnn_time_first_pmtwise->GetNbinsY()-1) outfile_firsttime<<",";    
          outfile_abs << hist_cnn_abs_pmtwise->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_abs_pmtwise->GetNbinsX()-1 || i_binY!=hist_cnn_abs_pmtwise->GetNbinsY()-1) outfile_abs<<",";
          outfile_abs_time << hist_cnn_abs_time_pmtwise->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_abs_time_pmtwise->GetNbinsX()-1 || i_binY!=hist_cnn_abs_time_pmtwise->GetNbinsY()-1) outfile_abs_time<<",";    
          outfile_abs_firsttime << hist_cnn_abs_time_first_pmtwise->GetBinContent(i_binX+1,i_binY+1);
          if (i_binX != hist_cnn_abs_time_first_pmtwise->GetNbinsX()-1 || i_binY!=hist_cnn_abs_time_first_pmtwise->GetNbinsY()-1) outfile_abs_firsttime<<",";    
        }
      }
    }

    outfile << std::endl;
    outfile_time << std::endl;
    outfile_firsttime << std::endl;
    outfile_abs << std::endl;
    outfile_abs_time << std::endl;
    outfile_abs_firsttime << std::endl;

  }    

    for (i=0; i<wcsimrootsuperevent->GetNumberOfEvents(); i++)
  {
    mcev++;
  }

    // reinitialize super event between loops.
    wcsimrootsuperevent->ReInitialize();
    
  } // End of loop over events

  cout << endl;
  
  std::cout<<"Total number of observed triggers: "<<num_trig<<"\n";

  //Close files
  root_outfile->Close();
  outfile.close();
  outfile_time.close();
  outfile_firsttime.close();
  outfile_abs.close();
  outfile_abs_time.close();
  outfile_abs_firsttime.close();

  std::cout <<"Finished macro"<<std::endl;

  return 0;
}
