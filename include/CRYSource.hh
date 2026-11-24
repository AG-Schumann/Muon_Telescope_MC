// CRYSource.hh
#ifndef CRYSource_hh
#define CRYSource_hh 1

#ifdef USE_CRY

#include "IMuonSource.hh"

#include "CRYGenerator.h"
#include "CRYParticle.h"
#include "CRYSetup.h"

#include "globals.hh"

#include <cstdint>
#include <vector>


class CRYSourceMessenger; // forward declaration


// Cosmic-ray shower source based on the CRY library (LLNL, v1.7).
//
// One CRY shower = one Geant4 event. A shower can contain several particles,
// which is why this class implements GetNextShower() instead of GetNextEvent().
//
// Unit conventions:
//   - CRY internally: MeV, metres, seconds
//   - Setter API: Geant4 units (m, cm, ...), like the EcoMug messenger
//   - MuonEvent output: mm and GeV/c, plus a time in ns
//
// The generator is built the first time a shower is requested, so all macro
// commands are applied before CRY reads its configuration.


class CRYSource : public IMuonSource {
public:
    explicit CRYSource(std::uint64_t seed = 0);
    ~CRYSource() override;

    // IMuonSource interface
    bool GetNextShower(std::vector<MuonEvent>& particles) override;
    bool GetNextEvent(MuonEvent& event) override;

    // Setter API
    void SetDataDir(const G4String& dir);  // Just like the CRY Example
    void SetBoxLength(G4double length);    // Geant4 length unit
    void SetPlaneZ(G4double z);            // Geant4 length unit
    void SetAltitude(G4double altitude);   // Geant4 length unit 0 / 2100 11300 m
    void SetLatitude(G4double latitude);   // degrees
    void SetDate(const G4String& date);      // "month-dat-year" (e.g. "1-1-2020")

    void SetReturnMuons(G4bool value);
    void SetReturnElectrons(G4bool value);
    void SetReturnGammas(G4bool value);
    void SetReturnNeutrons(G4bool value);
    void SetReturnProtons(G4bool value);
    void SetReturnPions(G4bool value);


    // Optional metadata, written to the RunInfo ntuple
    void PrintConfiguration() override;
    G4String GetSourceLabel()  const override { return "CRY"; }
    G4String GetSurfaceLabel() const override { return "plane"; }
    G4double GetGenerationArea() const override { return fBoxLength_m * fBoxLength_m; }
    std::uint64_t GetSeed()      const override { return fSeed; }
    G4double GetNormalizedFlux() const override;   // showers / (s m^2)
    G4double GetFluxError()      const override;
    G4double GetLiveTime(G4int nGenerated) override;   // s

private: 
    void Initialize();

    G4bool AlreadyBuilt(const char* what) const; //honestly, It supposed to be flag

    CRYSetup*           fSetup = nullptr;
    CRYGenerator*       fGenerator = nullptr;
    CRYSourceMessenger* fMessenger = nullptr;

    std::vector<CRYParticle*> fCRYParticles; // every event calls CRYGenerator::genEvent() and fills this vector with the particles of the shower

    //configuration parameters for the CRY generator
    G4String fDataDir = CRY_DATA_DIR;  // default: CRY_DATA_DIR from CRYSetup.hh or CMake (probably better to set it in the CMakeLists.txt)
    G4double fBoxLength_m = 1.0;       // 1 m 
    G4double fPlaneZ_mm = 1300.;         // 1300 mm
    G4double fAltitude_m = 0.;      
    G4double fLatitude_deg = 48.0;  // default: Freiburg, Germany
    G4String fDate          = "1-1-2026";

    G4bool fReturnMuons = true;
    G4bool fReturnElectrons = false;
    G4bool fReturnGammas = false;
    G4bool fReturnNeutrons = false;
    G4bool fReturnProtons = false;
    G4bool fReturnPions = false;

    std::uint64_t fSeed = 0; // 0 means random seed
    G4long        fNShowers = 0;

};


#endif // USE_CRY
#endif // CRYSource_hh