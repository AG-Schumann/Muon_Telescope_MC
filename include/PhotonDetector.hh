#ifndef PhotonDetector_hh
#define PhotonDetector_hh 1

#include "G4VSensitiveDetector.hh"
#include "PhotonHit.hh"

class G4Step;
class G4HCofThisEvent;

// Ideal photocathode on the PMT windows (PMTWindow0..3). Every optical
// photon is recorded at its first step inside the window and killed there.
// The quantum efficiency is applied offline (analysis/light_collection.py).
class PhotonDetector : public G4VSensitiveDetector
{
public:
    PhotonDetector(const G4String& name, const G4String& hitsCollectionName);
    ~PhotonDetector() override;

    void Initialize(G4HCofThisEvent* hce) override;
    G4bool ProcessHits(G4Step* step, G4TouchableHistory* history) override;

private:
    PhotonHitsCollection* fHitsCollection = nullptr;
};

#endif
