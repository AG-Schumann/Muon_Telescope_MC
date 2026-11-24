// ActionInitialization.hh
#ifndef ActionInitialization_h
#define ActionInitialization_h 1

#include "G4VUserActionInitialization.hh"
#include "globals.hh"
#include "SourceType.hh"


class ActionInitialization : public G4VUserActionInitialization
{
public:
    ActionInitialization(SourceType sourceType,
                     const G4String& muonSourceFile,
                     const G4String& outputFilename,
                     std::uint64_t seed,
                     G4bool optical = false);
    ~ActionInitialization() override;

    void BuildForMaster() const override;
    void Build() const override;

private:
    SourceType fSourceType;
    G4String   fMuonSourceFile;
    G4String   fOutputFilename;
    std::uint64_t fSeed;
    G4bool fOptical;   // -p: photon counting actions and ntuples
};

#endif