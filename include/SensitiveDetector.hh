#ifndef SensitiveDetector_hh
#define SensitiveDetector_hh 1

#include "G4VSensitiveDetector.hh"
#include "G4Step.hh"
#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"

#include "EventHit.hh"



class G4Step;
class G4HCofThisEvent;

class SensitiveDetector : public G4VSensitiveDetector 
{
public:
    SensitiveDetector(const G4String& name, const G4String& hitsCollectionName);
    ~SensitiveDetector() override;

    void Initialize(G4HCofThisEvent* hce) override;
    G4bool ProcessHits(G4Step* step, G4TouchableHistory* history) override;
    void EndOfEvent(G4HCofThisEvent* hce) override;

private:
    EventHitsCollection* fHitsCollection = nullptr;
};

#endif