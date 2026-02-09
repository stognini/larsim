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
 * Analyzer module that exports detector and simulation data for Celeritas
 * testing. A Geant4 ROOT input file containing \c IonAndScint energy deposition
 * data is \em optional.
 *
 * Usage:
 * Export detector geometry data only: lar -c job.fcl
 * Export detector geometry and simulation data: lar -c job.fcl -s [g4-sim.root]
 */
class phot::GeoAndSimDataExporter : public art::EDAnalyzer {
public:
  // Construct with no fcl input parameters and export geometry data
  explicit GeoAndSimDataExporter(fhicl::ParameterSet const& p);

  //!@{
  // Plugins should not be copied or assigned
  GeoAndSimDataExporter(GeoAndSimDataExporter const&) = delete;
  GeoAndSimDataExporter(GeoAndSimDataExporter&&) = delete;
  GeoAndSimDataExporter& operator=(GeoAndSimDataExporter const&) = delete;
  GeoAndSimDataExporter& operator=(GeoAndSimDataExporter&&) = delete;
  //@!}

  // Export simulation data from input file
  void analyze(art::Event const& e) override;

private:
  // Detector geometry defined in fcl services
  geo::GeometryCore const& fGeometry;
};

//---------------------------------------------------------------------------//
/*!
 * Construct with GDML geometry and export its information.
 */
phot::GeoAndSimDataExporter::GeoAndSimDataExporter(fhicl::ParameterSet const& p)
  : EDAnalyzer{p}, fGeometry(*(lar::providerFrom<geo::Geometry>()))
{
  // TTree and ROOT file writing is done automatically by the TFileService
  art::ServiceHandle<art::TFileService> tfs;

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
 * Loop over optional larg4 Geant4 output simulation file event data with
 * \c IonAndScint objects and export test data.
 */
void phot::GeoAndSimDataExporter::analyze(art::Event const& e)
{
  art::Handle<std::vector<sim::SimEnergyDeposit>> edepHandle;
  if (!e.getByLabel("IonAndScint", edepHandle)) {
    mf::LogError("GeoAndSimDataExporter")
      << "Cannot find IonAndScint label. Either 1) missing input file (lar -c thisjob.fcl -s "
         "[geant4_output.root]) or 2) missing IonAndScint data in art::Event";
    return;
  }

  //! \todo export data

  mf::LogInfo("GeoAndSimDataExporter") << "Saved simulation data to root file";
}

//---------------------------------------------------------------------------//
DEFINE_ART_MODULE(phot::GeoAndSimDataExporter)
