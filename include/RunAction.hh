#ifndef RunAction_hh
#define RunAction_hh 1

#include "G4UserRunAction.hh"
#include "globals.hh"
#include "G4String.hh"

#include "IMuonSource.hh"


class G4Run;

class RunAction : public G4UserRunAction
{
public:
    RunAction(const G4String& outputFilename,
          IMuonSource* source = nullptr,
          const G4String& generatorFlag = "",
          const G4String& sourceFile = "",
          G4bool optical = false);
    ~RunAction() override;

    void BeginOfRunAction(const G4Run* run) override;
    void EndOfRunAction(const G4Run* run) override;
    
private:

    G4String fOutputFilename;
    IMuonSource* fSource;
    G4String fGeneratorFlag;
    G4String fSourceFile;
    G4bool   fOptical;   // adds the OpticalSummary and PhotonHits ntuples

};

#endif