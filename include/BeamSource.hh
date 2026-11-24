#ifndef BeamSource_hh
#define BeamSource_hh 1

#include "IMuonSource.hh"
#include "G4ThreeVector.hh"
#include "G4UImessenger.hh"
#include "G4String.hh"
#include "G4SystemOfUnits.hh"

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWith3VectorAndUnit;
class G4UIcmdWith3Vector;
class BeamSourceMessenger;

// Pencil beam with a fixed particle, momentum, start point and direction.
// Used for position scans of the light collection: a vertical beam started
// above the top panel crosses all four modules at the same (x, y).
class BeamSource : public IMuonSource {
public:
    BeamSource();
    ~BeamSource() override;

    bool GetNextEvent(MuonEvent& event) override;

    // Setters (called by the messenger, before /run/beamOn)
    void SetParticle(const G4String& name);
    void SetMomentum(G4double pGeV)              { fMomentumGeV = pGeV; }
    void SetPosition(const G4ThreeVector& pos)   { fPosition = pos; }
    void SetDirection(const G4ThreeVector& dir)  { fDirection = dir.unit(); }

    // Metadata
    G4String GetSourceLabel()  const override { return "Beam"; }
    G4String GetSurfaceLabel() const override { return "point"; }
    void PrintConfiguration() override;

private:
    G4String      fParticle    = "mu-";
    G4int         fPDG         = 13;
    G4double      fMomentumGeV = 2.0;
    G4ThreeVector fPosition    = G4ThreeVector(0., 0., 1.2 * CLHEP::m);
    G4ThreeVector fDirection   = G4ThreeVector(0., 0., -1.);

    BeamSourceMessenger* fMessenger;   // owned
};


// UI messenger for BeamSource, kept in the same file like the one of
// IsotropicSource.
//
//   /muonSource/beam/particle   mu- | mu+ | e- | ...
//   /muonSource/beam/momentum   <value> <unit>
//   /muonSource/beam/position   x y z <unit>
//   /muonSource/beam/direction  dx dy dz
class BeamSourceMessenger : public G4UImessenger {
public:
    explicit BeamSourceMessenger(BeamSource* source);
    ~BeamSourceMessenger() override;
    void SetNewValue(G4UIcommand* command, G4String newValue) override;

private:
    BeamSource* fSource;                    // not owned
    G4UIdirectory*              fDir;
    G4UIcmdWithAString*         fParticleCmd;
    G4UIcmdWithADoubleAndUnit*  fMomentumCmd;
    G4UIcmdWith3VectorAndUnit*  fPositionCmd;
    G4UIcmdWith3Vector*         fDirectionCmd;
};

#endif
