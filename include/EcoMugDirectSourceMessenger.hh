// EcoMugDirectSourceMessenger.hh
#ifndef EcoMugDirectSourceMessenger_hh
#define EcoMugDirectSourceMessenger_hh 1

#include "G4UImessenger.hh"
#include "globals.hh"


class EcoMugDirectSource;
class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWith3VectorAndUnit;
class G4UIcmdWithAnInteger;
class G4UIcmdWithABool;

// Geant4 UI messenger for EcoMugDirectSource.
//
// Sets the EcoMug parameters at runtime from macro commands.
//
// Command hierarchy:
//   /muonSource/ecomug/surface       <hsphere|sky|cylinder>
//   /muonSource/ecomug/hsphereRadius <value> <unit>
//   /muonSource/ecomug/hsphereCenter <x> <y> <z> <unit>
//   /muonSource/ecomug/skySizeX      <value> <unit>
//   /muonSource/ecomug/skySizeY      <value> <unit>
//   /muonSource/ecomug/skyCenter     <x> <y> <z> <unit>
//   /muonSource/ecomug/cylRadius     <value> <unit>
//   /muonSource/ecomug/cylHeight     <value> <unit>
//   /muonSource/ecomug/cylCenter     <x> <y> <z> <unit>
//   /muonSource/ecomug/tsphereRadius <value> <unit>
//   /muonSource/ecomug/tsphereCenter <x> <y> <z> <unit>
//   /muonSource/ecomug/minMomentum   <value> <unit>
//   /muonSource/ecomug/maxMomentum   <value> <unit>
//   /muonSource/ecomug/minTheta      <value> <unit>
//   /muonSource/ecomug/maxTheta      <value> <unit>
//
// All commands work in the PreInit and Idle states
// (i.e. they have to be set before /run/beamOn).
class EcoMugDirectSourceMessenger : public G4UImessenger {
public:
    explicit EcoMugDirectSourceMessenger(EcoMugDirectSource* source);
    ~EcoMugDirectSourceMessenger() override;

    // Called by the Geant4 UI system when it receives a command
    void SetNewValue(G4UIcommand* command, G4String newValue) override;

private:
    // Source pointer: not owned by this class (commands are only forwarded)
    EcoMugDirectSource* fSource;

    // Directories
    G4UIdirectory* fMuonSourceDir;
    G4UIdirectory* fEcoMugDir;

    // Surface type
    G4UIcmdWithAString* fSurfaceCmd;

    // HSphere parameters
    G4UIcmdWithADoubleAndUnit*  fHSphereRadiusCmd;
    G4UIcmdWith3VectorAndUnit*  fHSphereCenterCmd;

    // Sky parameters
    G4UIcmdWithADoubleAndUnit*  fSkySizeXCmd;
    G4UIcmdWithADoubleAndUnit*  fSkySizeYCmd;
    G4UIcmdWith3VectorAndUnit*  fSkyCenterCmd;

    // Cylinder parameters
    G4UIcmdWithADoubleAndUnit*  fCylRadiusCmd;
    G4UIcmdWithADoubleAndUnit*  fCylHeightCmd;
    G4UIcmdWith3VectorAndUnit*  fCylCenterCmd;


    // Target Sphere parameters
    G4UIcmdWith3VectorAndUnit* fTSphereCenterCmd;
    G4UIcmdWithADoubleAndUnit* fTSphereRadiusCmd;


    // Physical parameter ranges
    G4UIcmdWithADoubleAndUnit*  fMinMomentumCmd;
    G4UIcmdWithADoubleAndUnit*  fMaxMomentumCmd;
    G4UIcmdWithADoubleAndUnit*  fMinThetaCmd;
    G4UIcmdWithADoubleAndUnit*  fMaxThetaCmd;

    // Isotropic mode (custom J)
    G4UIcmdWithABool* fIsotropicCmd;
};

#endif