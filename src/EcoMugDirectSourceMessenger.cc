// EcoMugDirectSourceMessenger.cc
#include "EcoMugDirectSourceMessenger.hh"
#include "EcoMugDirectSource.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWith3VectorAndUnit.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4SystemOfUnits.hh"
#include "G4UIcmdWithABool.hh"

// Constructor: creates and configures the commands
EcoMugDirectSourceMessenger::EcoMugDirectSourceMessenger(EcoMugDirectSource* source)
    : fSource(source)
{
    // Directory hierarchy
    fMuonSourceDir = new G4UIdirectory("/muonSource/");
    fMuonSourceDir->SetGuidance("Commands for muon source configuration.");

    fEcoMugDir = new G4UIdirectory("/muonSource/ecomug/");
    fEcoMugDir->SetGuidance("EcoMug runtime muon generator parameters.");

    // Surface type
    fSurfaceCmd = new G4UIcmdWithAString("/muonSource/ecomug/surface", this);
    fSurfaceCmd->SetGuidance("Set the EcoMug generation surface type.");
    fSurfaceCmd->SetGuidance("  hsphere  : half-sphere (hemisphere)");
    fSurfaceCmd->SetGuidance("  sky      : flat horizontal surface");
    fSurfaceCmd->SetGuidance("  cylinder : cylindrical surface");
    fSurfaceCmd->SetGuidance("  target   : sphere enclosing the detector (fastest)");
    fSurfaceCmd->SetParameterName("type", false);
    fSurfaceCmd->SetCandidates("hsphere sky cylinder target");
    fSurfaceCmd->SetDefaultValue("sky");
    fSurfaceCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // HSphere parameters
    fHSphereRadiusCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/hsphereRadius", this);
    fHSphereRadiusCmd->SetGuidance("Set the half-sphere generation radius.");
    fHSphereRadiusCmd->SetParameterName("radius", false);
    fHSphereRadiusCmd->SetUnitCategory("Length");
    fHSphereRadiusCmd->SetDefaultUnit("m");
    fHSphereRadiusCmd->SetRange("radius > 0");
    fHSphereRadiusCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fHSphereCenterCmd = new G4UIcmdWith3VectorAndUnit("/muonSource/ecomug/hsphereCenter", this);
    fHSphereCenterCmd->SetGuidance("Set the half-sphere center position.");
    fHSphereCenterCmd->SetParameterName("x", "y", "z", false);
    fHSphereCenterCmd->SetUnitCategory("Length");
    fHSphereCenterCmd->SetDefaultUnit("m");
    fHSphereCenterCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // Sky parameters
    fSkySizeXCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/skySizeX", this);
    fSkySizeXCmd->SetGuidance("Set the sky surface size along X.");
    fSkySizeXCmd->SetParameterName("lx", false);
    fSkySizeXCmd->SetUnitCategory("Length");
    fSkySizeXCmd->SetDefaultUnit("m");
    fSkySizeXCmd->SetRange("lx > 0");
    fSkySizeXCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSkySizeYCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/skySizeY", this);
    fSkySizeYCmd->SetGuidance("Set the sky surface size along Y.");
    fSkySizeYCmd->SetParameterName("ly", false);
    fSkySizeYCmd->SetUnitCategory("Length");
    fSkySizeYCmd->SetDefaultUnit("m");
    fSkySizeYCmd->SetRange("ly > 0");
    fSkySizeYCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSkyCenterCmd = new G4UIcmdWith3VectorAndUnit("/muonSource/ecomug/skyCenter", this);
    fSkyCenterCmd->SetGuidance("Set the sky surface center position.");
    fSkyCenterCmd->SetParameterName("x", "y", "z", false);
    fSkyCenterCmd->SetUnitCategory("Length");
    fSkyCenterCmd->SetDefaultUnit("m");
    fSkyCenterCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // Cylinder parameters
    fCylRadiusCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/cylRadius", this);
    fCylRadiusCmd->SetGuidance("Set the cylinder generation radius.");
    fCylRadiusCmd->SetParameterName("radius", false);
    fCylRadiusCmd->SetUnitCategory("Length");
    fCylRadiusCmd->SetDefaultUnit("m");
    fCylRadiusCmd->SetRange("radius > 0");
    fCylRadiusCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fCylHeightCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/cylHeight", this);
    fCylHeightCmd->SetGuidance("Set the cylinder generation height.");
    fCylHeightCmd->SetParameterName("height", false);
    fCylHeightCmd->SetUnitCategory("Length");
    fCylHeightCmd->SetDefaultUnit("m");
    fCylHeightCmd->SetRange("height > 0");
    fCylHeightCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fCylCenterCmd = new G4UIcmdWith3VectorAndUnit("/muonSource/ecomug/cylCenter", this);
    fCylCenterCmd->SetGuidance("Set the cylinder center position.");
    fCylCenterCmd->SetParameterName("x", "y", "z", false);
    fCylCenterCmd->SetUnitCategory("Length");
    fCylCenterCmd->SetDefaultUnit("m");
    fCylCenterCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // Target Disk parameters
    fTSphereRadiusCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/tsphereRadius", this);
    fTSphereRadiusCmd->SetGuidance("Set the target sphere generation method radius.");
    fTSphereRadiusCmd->SetParameterName("radius", false);
    fTSphereRadiusCmd->SetUnitCategory("Length");
    fTSphereRadiusCmd->SetDefaultUnit("m");
    fTSphereRadiusCmd->SetRange("radius > 0");
    fTSphereRadiusCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fTSphereCenterCmd = new G4UIcmdWith3VectorAndUnit("/muonSource/ecomug/tsphereCenter", this);
    fTSphereCenterCmd->SetGuidance("Set the target sphere generation method center position.");
    fTSphereCenterCmd->SetParameterName("x", "y", "z", false);
    fTSphereCenterCmd->SetUnitCategory("Length");
    fTSphereCenterCmd->SetDefaultUnit("m");
    fTSphereCenterCmd->AvailableForStates(G4State_PreInit, G4State_Idle);


    // Physical parameter ranges
    fMinMomentumCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/minMomentum", this);
    fMinMomentumCmd->SetGuidance("Set the minimum muon momentum.");
    fMinMomentumCmd->SetParameterName("pmin", false);
    fMinMomentumCmd->SetUnitCategory("Energy");
    fMinMomentumCmd->SetDefaultUnit("GeV");
    fMinMomentumCmd->SetRange("pmin > 0");
    fMinMomentumCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fMaxMomentumCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/maxMomentum", this);
    fMaxMomentumCmd->SetGuidance("Set the maximum muon momentum.");
    fMaxMomentumCmd->SetParameterName("pmax", false);
    fMaxMomentumCmd->SetUnitCategory("Energy");
    fMaxMomentumCmd->SetDefaultUnit("GeV");
    fMaxMomentumCmd->SetRange("pmax > 0");
    fMaxMomentumCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fMinThetaCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/minTheta", this);
    fMinThetaCmd->SetGuidance("Set the minimum nadir angle (EcoMug internal convention).");
    fMinThetaCmd->SetParameterName("tmin", false);
    fMinThetaCmd->SetUnitCategory("Angle");
    fMinThetaCmd->SetDefaultUnit("rad");
    fMinThetaCmd->SetRange("tmin >= 0");
    fMinThetaCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fMaxThetaCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/ecomug/maxTheta", this);
    fMaxThetaCmd->SetGuidance("Set the maximum nadir angle (EcoMug internal convention).");
    fMaxThetaCmd->SetParameterName("tmax", false);
    fMaxThetaCmd->SetUnitCategory("Angle");
    fMaxThetaCmd->SetDefaultUnit("rad");
    fMaxThetaCmd->SetRange("tmax > 0");
    fMaxThetaCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // Isotropic mode
    fIsotropicCmd = new G4UIcmdWithABool("/muonSource/ecomug/isotropic", this);
    fIsotropicCmd->SetGuidance("Enable isotropic angular distribution (J=1, flat in angle).");
    fIsotropicCmd->SetGuidance("  true  : uniform angular distribution (acceptance test)");
    fIsotropicCmd->SetGuidance("  false : standard Gaisser flux");
    fIsotropicCmd->SetParameterName("enable", false);
    fIsotropicCmd->SetDefaultValue(false);
    fIsotropicCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

// Destructor
EcoMugDirectSourceMessenger::~EcoMugDirectSourceMessenger()
{
    delete fSurfaceCmd;
    delete fHSphereRadiusCmd;
    delete fHSphereCenterCmd;
    delete fSkySizeXCmd;
    delete fSkySizeYCmd;
    delete fSkyCenterCmd;
    delete fCylRadiusCmd;
    delete fCylHeightCmd;
    delete fCylCenterCmd;
    delete fMinMomentumCmd;
    delete fMaxMomentumCmd;
    delete fMinThetaCmd;
    delete fMaxThetaCmd;
    delete fEcoMugDir;
    delete fMuonSourceDir;
    delete fIsotropicCmd;
    delete fTSphereRadiusCmd;
    delete fTSphereCenterCmd;

}

// SetNewValue: command dispatch
// Called by the Geant4 UI system when it receives a command.
// The G4UIcmdWith*AndUnit commands convert units automatically:
// "5 m" in the macro gives GetNewDoubleValue() = 5000 (Geant4 internal units).

void EcoMugDirectSourceMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if      (command == fSurfaceCmd)        fSource->SetSurfaceType(newValue);
    else if (command == fHSphereRadiusCmd)  fSource->SetHSphereRadius(fHSphereRadiusCmd->GetNewDoubleValue(newValue));
    else if (command == fHSphereCenterCmd)  fSource->SetHSphereCenter(fHSphereCenterCmd->GetNew3VectorValue(newValue));
    else if (command == fSkySizeXCmd)       fSource->SetSkySizeX(fSkySizeXCmd->GetNewDoubleValue(newValue));
    else if (command == fSkySizeYCmd)       fSource->SetSkySizeY(fSkySizeYCmd->GetNewDoubleValue(newValue));
    else if (command == fSkyCenterCmd)      fSource->SetSkyCenter(fSkyCenterCmd->GetNew3VectorValue(newValue));
    else if (command == fCylRadiusCmd)      fSource->SetCylinderRadius(fCylRadiusCmd->GetNewDoubleValue(newValue));
    else if (command == fCylHeightCmd)      fSource->SetCylinderHeight(fCylHeightCmd->GetNewDoubleValue(newValue));
    else if (command == fCylCenterCmd)      fSource->SetCylinderCenter(fCylCenterCmd->GetNew3VectorValue(newValue));
    else if (command == fTSphereRadiusCmd)    fSource->SetTargetSphereRadius(fTSphereRadiusCmd->GetNewDoubleValue(newValue));
    else if (command == fTSphereCenterCmd)    fSource->SetTargetSphereCenter(fTSphereCenterCmd->GetNew3VectorValue(newValue));    
    else if (command == fMinMomentumCmd)    fSource->SetMinMomentum(fMinMomentumCmd->GetNewDoubleValue(newValue));
    else if (command == fMaxMomentumCmd)    fSource->SetMaxMomentum(fMaxMomentumCmd->GetNewDoubleValue(newValue));
    else if (command == fMinThetaCmd)       fSource->SetMinTheta(fMinThetaCmd->GetNewDoubleValue(newValue));
    else if (command == fMaxThetaCmd)       fSource->SetMaxTheta(fMaxThetaCmd->GetNewDoubleValue(newValue));
    else if (command == fIsotropicCmd)      fSource->SetIsotropicMode(fIsotropicCmd->GetNewBoolValue(newValue));
    else {
        G4cerr << "EcoMugDirectSourceMessenger: unhandled command "
               << command->GetCommandPath() << G4endl;
    }
}
