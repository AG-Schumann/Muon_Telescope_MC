#ifndef DetectorMessenger_hh
#define DetectorMessenger_hh 1

#include "G4UImessenger.hh"

class DetectorConstruction;
#include "G4UIdirectory.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithAString.hh"

class DetectorMessenger : public G4UImessenger {
public:
    DetectorMessenger(DetectorConstruction* detectorConstruction);
    ~DetectorMessenger();
    
    virtual void SetNewValue(G4UIcommand* command, G4String newValue);
    
private:
    DetectorConstruction*           fDetectorConstruction;
    
    G4UIdirectory*                  fDetectorDir;
    G4UIcmdWithADoubleAndUnit*      fZenithAngleCmd;
    G4UIcmdWithADoubleAndUnit*      fAzimuthAngleCmd;
    G4UIcmdWithoutParameter*        fRebuildCmd;
    G4UIcmdWithABool*               fRoofCmd;
    G4UIcmdWithADoubleAndUnit*      fRoofThicknessCmd;
    G4UIcmdWithADoubleAndUnit*      fRoofHeightCmd;
    G4UIcmdWithADoubleAndUnit*      fRoofHalfSizeCmd;
    G4UIcmdWithAnInteger*           fRoofSlabsCmd;
    G4UIcmdWithADoubleAndUnit*      fRoofSlabGapCmd;
    G4UIcmdWithABool*               fLeadCmd;
    G4UIcmdWithADoubleAndUnit*      fLeadThicknessCmd;
    G4UIcmdWithADoubleAndUnit*      fLeadGapCmd;
    G4UIcmdWithABool*               fWorldMaterialIsAirCmd;
    G4UIcmdWithAnInteger*           fGeometryModeCmd;
    G4UIcmdWithAnInteger*           fFlipGuideCmd;

    G4UIdirectory*                  fOpticsDir;
    G4UIcmdWithAString*             fWrappingCmd;
    G4UIcmdWithAString*             fPmmaGradeCmd;
    G4UIcmdWithABool*               fStorePhotonsCmd;
    
};

#endif