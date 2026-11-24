// CRYSourceMessenger.cc
#include "CRYSourceMessenger.hh"

#ifdef USE_CRY

#include "CRYSource.hh"

#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithADouble.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIdirectory.hh"
#include "G4SystemOfUnits.hh"

CRYSourceMessenger::CRYSourceMessenger(CRYSource* source)
    : fSource(source)
{
    // Only one muon source is created per run, so this directory is not
    // created twice.
    fMuonSourceDir = new G4UIdirectory("/muonSource/");
    fMuonSourceDir->SetGuidance("Commands for muon source configuration.");

    fCryDir = new G4UIdirectory("/muonSource/cry/");
    fCryDir->SetGuidance("CRY cosmic-ray shower generator parameters.");

    fDataDirCmd = new G4UIcmdWithAString("/muonSource/cry/dataDir", this);
    fDataDirCmd->SetGuidance("Directory holding the CRY data tables.");
    fDataDirCmd->SetParameterName("dir", false);
    fDataDirCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fBoxLengthCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/cry/boxLength", this);
    fBoxLengthCmd->SetGuidance("Side length of the square generation plane (max 300 m).");
    fBoxLengthCmd->SetParameterName("length", false);
    fBoxLengthCmd->SetUnitCategory("Length");
    fBoxLengthCmd->SetDefaultUnit("m");
    fBoxLengthCmd->SetRange("length > 0");
    fBoxLengthCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fPlaneZCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/cry/planeZ", this);
    fPlaneZCmd->SetGuidance("Height of the generation plane in world coordinates.");
    fPlaneZCmd->SetParameterName("z", false);
    fPlaneZCmd->SetUnitCategory("Length");
    fPlaneZCmd->SetDefaultUnit("m");
    fPlaneZCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fAltitudeCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/cry/altitude", this);
    fAltitudeCmd->SetGuidance("Working altitude: only 0, 2100 or 11300 m are available.");
    fAltitudeCmd->SetParameterName("altitude", false);
    fAltitudeCmd->SetUnitCategory("Length");
    fAltitudeCmd->SetDefaultUnit("m");
    fAltitudeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fLatitudeCmd = new G4UIcmdWithADouble("/muonSource/cry/latitude", this);
    fLatitudeCmd->SetGuidance("Latitude in degrees (geomagnetic cutoff of the primaries).");
    fLatitudeCmd->SetParameterName("latitude", false);
    fLatitudeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fDateCmd = new G4UIcmdWithAString("/muonSource/cry/date", this);
    fDateCmd->SetGuidance("Date as month-day-year, e.g. 9-1-2026 (solar cycle).");
    fDateCmd->SetParameterName("date", false);
    fDateCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fReturnMuonsCmd = new G4UIcmdWithABool("/muonSource/cry/returnMuons", this);
    fReturnMuonsCmd->SetGuidance("Include muons in the shower.");
    fReturnMuonsCmd->SetParameterName("enable", false);
    fReturnMuonsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fReturnElectronsCmd = new G4UIcmdWithABool("/muonSource/cry/returnElectrons", this);
    fReturnElectronsCmd->SetGuidance("Include electrons and positrons.");
    fReturnElectronsCmd->SetParameterName("enable", false);
    fReturnElectronsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fReturnGammasCmd = new G4UIcmdWithABool("/muonSource/cry/returnGammas", this);
    fReturnGammasCmd->SetGuidance("Include gammas.");
    fReturnGammasCmd->SetParameterName("enable", false);
    fReturnGammasCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fReturnNeutronsCmd = new G4UIcmdWithABool("/muonSource/cry/returnNeutrons", this);
    fReturnNeutronsCmd->SetGuidance("Include neutrons (needs a hadronic physics list).");
    fReturnNeutronsCmd->SetParameterName("enable", false);
    fReturnNeutronsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fReturnProtonsCmd = new G4UIcmdWithABool("/muonSource/cry/returnProtons", this);
    fReturnProtonsCmd->SetGuidance("Include protons (needs a hadronic physics list).");
    fReturnProtonsCmd->SetParameterName("enable", false);
    fReturnProtonsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fReturnPionsCmd = new G4UIcmdWithABool("/muonSource/cry/returnPions", this);
    fReturnPionsCmd->SetGuidance("Include charged pions (needs a hadronic physics list).");
    fReturnPionsCmd->SetParameterName("enable", false);
    fReturnPionsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

CRYSourceMessenger::~CRYSourceMessenger()
{
    delete fDataDirCmd;
    delete fBoxLengthCmd;
    delete fPlaneZCmd;
    delete fAltitudeCmd;
    delete fLatitudeCmd;
    delete fDateCmd;
    delete fReturnMuonsCmd;
    delete fReturnElectronsCmd;
    delete fReturnGammasCmd;
    delete fReturnNeutronsCmd;
    delete fReturnProtonsCmd;
    delete fReturnPionsCmd;
    delete fCryDir;
    delete fMuonSourceDir;
}

void CRYSourceMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if      (command == fDataDirCmd)   fSource->SetDataDir(newValue);
    else if (command == fBoxLengthCmd) fSource->SetBoxLength(fBoxLengthCmd->GetNewDoubleValue(newValue));
    else if (command == fPlaneZCmd)    fSource->SetPlaneZ(fPlaneZCmd->GetNewDoubleValue(newValue));
    else if (command == fAltitudeCmd)  fSource->SetAltitude(fAltitudeCmd->GetNewDoubleValue(newValue));
    else if (command == fLatitudeCmd)  fSource->SetLatitude(fLatitudeCmd->GetNewDoubleValue(newValue));
    else if (command == fDateCmd)      fSource->SetDate(newValue);
    else if (command == fReturnMuonsCmd)     fSource->SetReturnMuons(fReturnMuonsCmd->GetNewBoolValue(newValue));
    else if (command == fReturnElectronsCmd) fSource->SetReturnElectrons(fReturnElectronsCmd->GetNewBoolValue(newValue));
    else if (command == fReturnGammasCmd)    fSource->SetReturnGammas(fReturnGammasCmd->GetNewBoolValue(newValue));
    else if (command == fReturnNeutronsCmd)  fSource->SetReturnNeutrons(fReturnNeutronsCmd->GetNewBoolValue(newValue));
    else if (command == fReturnProtonsCmd)   fSource->SetReturnProtons(fReturnProtonsCmd->GetNewBoolValue(newValue));
    else if (command == fReturnPionsCmd)     fSource->SetReturnPions(fReturnPionsCmd->GetNewBoolValue(newValue));
    else {
        G4cerr << "CRYSourceMessenger: unhandled command "
               << command->GetCommandPath() << G4endl;
    }
}

#endif  // USE_CRY