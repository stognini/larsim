////////////////////////////////////////////////////////////////////////
// Class:       GeoAndSimDataExporter_module
// Plugin Type: analyzer
// File:        GeoAndSimDataExporter_module.cc
//
// Generated at Sun Feb  8 13:13:21 2026 by Stefano Tognini using cetskelgen
// from cetlib version 3.18.02.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"

#include "art_root_io/TFileService.h"

#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "larcore/CoreUtils/ServiceUtil.h"
#include "larcore/Geometry/Geometry.h"
#include "larcorealg/Geometry/OpDetGeo.h"
#include "lardataobj/Simulation/OpDetBacktrackerRecord.h"
#include "lardataobj/Simulation/SimEnergyDeposit.h"

#include "TFile.h"
#include "TTree.h"

namespace phot {
  class GeoAndSimDataExporter;
}

//---------------------------------------------------------------------------//
/*!
 * Analyzer module that exports detector geometry information and, \em
 * optionally , \c sim::SimEnergyDeposit objects from \c IonAndScint data for
 * Celeritas unit tests.
 *
 * Usage:
 * Export detector geometry data only:
 * $ lar -c job.fcl
 * Export detector geometry \em and simulation data:
 * $ lar -c job.fcl -s [geant4-output.root]
 *
 * To store a subset of events, use the optional -n [num_events] flag.
 */
class phot::GeoAndSimDataExporter : public art::EDAnalyzer {
public:
  // Construct with input parameters and export geometry data
  explicit GeoAndSimDataExporter(fhicl::ParameterSet const& p);

  //!@{
  // Plugins should not be copied or assigned
  GeoAndSimDataExporter(GeoAndSimDataExporter const&) = delete;
  GeoAndSimDataExporter(GeoAndSimDataExporter&&) = delete;
  GeoAndSimDataExporter& operator=(GeoAndSimDataExporter const&) = delete;
  GeoAndSimDataExporter& operator=(GeoAndSimDataExporter&&) = delete;
  //@!}

  // Create sim data tree
  void beginJob() override;

  // Export simulation data from input file
  void analyze(art::Event const& e) override;

private:
  // Fcl input data
  geo::GeometryCore const& fGeometry;
  int fMaxEdeps;

  // TTree with sim::SimEnergyDeposit data
  TTree* fSimTree;

  // Simplified version of sim::SimEnergyDeposit class to fill tree
  // This object's data is overwritten before every TTree::Fill()
  struct SimEdep {
    int TrackID;

    int NumPhotons;
    int NumFPhotons;
    int NumSPhotons;
    int NumElectrons;

    double ScintYieldRatio;
    double Energy;
    double Time;

    std::array<double, 3> Start;
    std::array<double, 3> End;
    double StepLength;
  } fSimEdep;
};

//---------------------------------------------------------------------------//
/*!
 * Construct with GDML geometry and export its information.
 */
phot::GeoAndSimDataExporter::GeoAndSimDataExporter(fhicl::ParameterSet const& p)
  : EDAnalyzer{p}
  , fGeometry(*(lar::providerFrom<geo::Geometry>()))
  , fMaxEdeps(p.get<int>("max_edeps_per_event"))
{
  // TTree and ROOT file writing is done automatically by the TFileService
  art::ServiceHandle<art::TFileService const> tfs;

  // Geometry information
  auto* det_info = tfs->make<TTree>("detector_info", "detector_info");
  std::string name = fGeometry.DetectorName();

  det_info->Branch("name", &name);
  det_info->Fill();

  auto* geo_data = tfs->make<TTree>("optical_detectors", "optical_detectors");
  std::array<double, 3> pos;
  std::string info;
  geo_data->Branch("pos", &pos);
  geo_data->Branch("info", &info);

  for (unsigned int i = 0; i < fGeometry.NOpDets(); i++) {
    auto const& opdet = fGeometry.OpDetGeoFromOpDet(i);
    auto const& center = opdet.GetCenter();

    info = opdet.OpDetInfo(/* indent = */ "", /* verbosity = */ 1);
    pos[0] = center.x();
    pos[1] = center.y();
    pos[2] = center.z();
    geo_data->Fill();
  }

  mf::LogInfo("GeoAndSimDataExporter") << "Saved detector information to root file";
}

//---------------------------------------------------------------------------//
/*!
 * Create TTree with sim data.
 */
void phot::GeoAndSimDataExporter::beginJob()
{
  // TTree and ROOT file writing is done automatically by the TFileService
  art::ServiceHandle<art::TFileService const> tfs;

  // Branch names mimic sim::SimEnergyDeposit class getters
  fSimTree = tfs->make<TTree>("sim_energy_deposits", "sim_energy_deposits");

#define CREATE_SIM_BRANCH(MEMBER) fSimTree->Branch(#MEMBER, &fSimEdep.MEMBER);

  CREATE_SIM_BRANCH(TrackID);
  CREATE_SIM_BRANCH(NumPhotons);
  CREATE_SIM_BRANCH(NumFPhotons);
  CREATE_SIM_BRANCH(NumSPhotons);
  CREATE_SIM_BRANCH(NumElectrons);
  CREATE_SIM_BRANCH(ScintYieldRatio);
  CREATE_SIM_BRANCH(Energy);
  CREATE_SIM_BRANCH(Time);
  CREATE_SIM_BRANCH(Start);
  CREATE_SIM_BRANCH(End);
  CREATE_SIM_BRANCH(StepLength);

#undef CREATE_SIM_BRANCH
}

//---------------------------------------------------------------------------//
/*!
 * Loop over optional larg4 Geant4 output simulation file event data with
 * \c IonAndScint objects and export test data.
 */
void phot::GeoAndSimDataExporter::analyze(art::Event const& e)
{
  art::Handle<std::vector<sim::SimEnergyDeposit>> energy_deps;
  if (!e.getByLabel("IonAndScint", energy_deps)) {
    mf::LogError("GeoAndSimDataExporter")
      << "Cannot find IonAndScint label. Either 1) missing input file (lar -c thisjob.fcl -s "
         "[geant4_output.root]) or 2) missing IonAndScint data in art::Event";
    return;
  }

  // Verify if data is present
  int const edeps_size = (*energy_deps).size();
  if (edeps_size == 0) {
    mf::LogWarning("GeoAndSimDataExporter")
      << "sim::SimEnergyDeposit data is valid but has zero entries; Skipping event";
    return;
  }

  // If the requested maximum number of energy deposits per event is <= 0,
  // store all. Otherwise, set the limit to be up to the size of the vector
  // to avoid a segfault
  int const num_edeps_stored = (fMaxEdeps <= 0)         ? edeps_size :
                               (fMaxEdeps > edeps_size) ? edeps_size :
                                                          fMaxEdeps;

#define GET_EDEP(MEMBER) fSimEdep.MEMBER = edep.MEMBER();

#define GET_EDEP_ARRAY(MEMBER)                                                                     \
  fSimEdep.MEMBER[0] = edep.MEMBER().x();                                                          \
  fSimEdep.MEMBER[1] = edep.MEMBER().y();                                                          \
  fSimEdep.MEMBER[2] = edep.MEMBER().z();

  for (int i = 0; i < num_edeps_stored; i++) {
    auto const& edep = (*energy_deps)[i];

    GET_EDEP(TrackID);
    GET_EDEP(NumPhotons);
    GET_EDEP(NumFPhotons);
    GET_EDEP(NumSPhotons);
    GET_EDEP(NumElectrons);
    GET_EDEP(ScintYieldRatio);
    GET_EDEP(Energy);
    GET_EDEP(Time);
    GET_EDEP_ARRAY(Start);
    GET_EDEP_ARRAY(End);
    GET_EDEP(StepLength);

    fSimTree->Fill();
  }

#undef GET_EDEP
#undef GET_EDEP_ARRAY

  mf::LogInfo("GeoAndSimDataExporter")
    << "Wrote " << num_edeps_stored << " SimEnergyDeposition object(s) to ROOT file";
}

//---------------------------------------------------------------------------//
DEFINE_ART_MODULE(phot::GeoAndSimDataExporter)
