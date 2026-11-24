// EcoMugDirectSource.hh
#ifndef EcoMugDirectSource_hh
#define EcoMugDirectSource_hh 1

#include "IMuonSource.hh"
#include "EcoMug.h"

#include "G4ThreeVector.hh"
#include "globals.hh"

class EcoMugDirectSourceMessenger;  // forward declaration

// Runtime EcoMug-based muon source.
//
// Holds the header-only EcoMug library by composition.
// Every GetNextEvent() call generates a new muon (no input file).
// All parameters are set from the macro through EcoMugDirectSourceMessenger.
//
// Unit conventions:
//   - Setter API: Geant4 units (m, GeV, rad, ...), as given by the user
//   - EcoMug internally: m and GeV (as in the header)
//   - MuonEvent output: mm and GeV/c (Geant4 internal)

struct GenRateResult {
    G4double rate;  // Hz / m^2
    G4double error; // Hz / m^2
};
struct EstimatedTimeResult {
    G4double time;  // seconds
    G4double error; // seconds
};

class EcoMugDirectSource : public IMuonSource {
public:
    explicit EcoMugDirectSource(std::uint64_t seed = 0);
    ~EcoMugDirectSource() override;

    // IMuonSource interface
    bool GetNextEvent(MuonEvent& event) override;

    // Setter API (called by the messenger)

    // Surface type: "hsphere", "sky", "cylinder", "target"
    void SetSurfaceType(const G4String& type);

    // HSphere (hemispherical shell)
    void SetHSphereRadius(G4double radius);
    void SetHSphereCenter(const G4ThreeVector& center);

    // Sky (flat rectangular surface)
    void SetSkySizeX(G4double lx); 
    void SetSkySizeY(G4double ly); 
    void SetSkyCenter(const G4ThreeVector& center);

    // Cylinder
    void SetCylinderRadius(G4double radius);
    void SetCylinderHeight(G4double height);
    void SetCylinderCenter(const G4ThreeVector& center);

    // Target Disk
    void SetTargetSphereRadius(G4double radius);
    void SetTargetSphereCenter(const G4ThreeVector& center);

    // Physical parameter ranges
    void SetMinMomentum(G4double pmin);  // GeV/c
    void SetMaxMomentum(G4double pmax);  // GeV/c
    void SetMinTheta(G4double tmin);    // rad (nadir)
    void SetMaxTheta(G4double tmax);    // rad (nadir)

    // RNG seed
    
    void SetSeed(std::uint64_t seed);

    void SetIsotropicMode(G4bool enable);  // isotropic mode with a custom J


    G4double GetSkySizeX();       
    G4double GetSkySizeY();       
    G4double GetHSphereRadius();
    G4double GetCylinderRadius();
    G4double GetCylinderHeight();
    G4double GetMinMomentum();
    G4double GetMaxMomentum();
    G4double GetMinTheta();
    G4double GetMaxTheta();
    GenRateResult GetAverageGenRate(G4int npoints = 1e6);
    EstimatedTimeResult GetEstimatedTime(G4int npoints = 1e6);
    void PrintConfiguration() override;
    G4String GetSourceLabel()  const override { return "EcoMugDirect"; }
    G4String GetSurfaceLabel() const override { return fSurfaceType; }
    G4double GetGenerationArea() const override { return fMug.GetGenSurfaceArea(); }  // EcoMug raw = m^2 - written to RunInfo
    std::uint64_t GetSeed() const override { return fSeed; }
    G4double GetNormalizedFlux() const override { return fCachedFlux; }
    G4double GetFluxError()      const override { return fCachedFluxErr; }
    G4double GetLiveTime(G4int nGenerated) override { return fMug.GetEstimatedTime(nGenerated); }  // s
    


private:
    EcoMug fMug;                              // composition: EcoMug instance
    EcoMugDirectSourceMessenger* fMessenger;  // owned by this class
    G4bool fIsotropicMode = false;
    G4String fSurfaceType   = "sky";
    std::uint64_t fSeed          = 0;
    
    // Filled by PrintConfiguration(); read back by the RunInfo ntuple so the
    // expensive MC integration runs only once per run.
    G4double fCachedFlux    = 0.;   // Hz/m^2
    G4double fCachedFluxErr = 0.;   // Hz/m^2
};

#endif

