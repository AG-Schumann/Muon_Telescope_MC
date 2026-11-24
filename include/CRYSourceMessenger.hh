// CRYSourceMessenger.hh
#ifndef CRYSourceMessenger_hh
#define CRYSourceMessenger_hh 1

#ifdef USE_CRY

#include "G4UImessenger.hh"
#include "globals.hh"

class CRYSource;
class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithADouble;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithABool;

class CRYSourceMessenger : public G4UImessenger {
public:
    explicit CRYSourceMessenger(CRYSource* source);
    ~CRYSourceMessenger() override;

    void SetNewValue(G4UIcommand* command, G4String newValue) override;

private:
    CRYSource* fSource;

    G4UIdirectory* fMuonSourceDir;
    G4UIdirectory* fCryDir;

    G4UIcmdWithAString*        fDataDirCmd;
    G4UIcmdWithADoubleAndUnit* fBoxLengthCmd;
    G4UIcmdWithADoubleAndUnit* fPlaneZCmd;
    G4UIcmdWithADoubleAndUnit* fAltitudeCmd;
    G4UIcmdWithADouble*        fLatitudeCmd;
    G4UIcmdWithAString*        fDateCmd;

    G4UIcmdWithABool* fReturnMuonsCmd;
    G4UIcmdWithABool* fReturnElectronsCmd;
    G4UIcmdWithABool* fReturnGammasCmd;
    G4UIcmdWithABool* fReturnNeutronsCmd;
    G4UIcmdWithABool* fReturnProtonsCmd;
    G4UIcmdWithABool* fReturnPionsCmd;
};

#endif  // USE_CRY
#endif