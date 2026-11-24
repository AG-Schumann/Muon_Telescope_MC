#include "SensitiveDetector.hh"
#include "EventHit.hh"
#include "G4OpticalPhoton.hh"



SensitiveDetector::SensitiveDetector(const G4String& name, const G4String& hitsCollectionName): G4VSensitiveDetector(name)
{
    collectionName.insert(hitsCollectionName);
}

SensitiveDetector::~SensitiveDetector()
{
}

void SensitiveDetector::Initialize(G4HCofThisEvent* hce)
{
    // Creates the hits collection of the event and adds it to hce.

    fHitsCollection = new EventHitsCollection(SensitiveDetectorName, collectionName[0]);

    G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
    hce->AddHitsCollection(hcID, fHitsCollection);
    
}

G4bool SensitiveDetector::ProcessHits(G4Step *step, G4TouchableHistory* )
{
    // Optical photons (-p) are not charged-particle hits: the panel ntuples
    // must be the same with and without optical transport.
    if (step->GetTrack()->GetParticleDefinition() == G4OpticalPhoton::OpticalPhotonDefinition())
        return false;

    G4double edep = step->GetTotalEnergyDeposit();
    if (edep <= 0.) return false; // no hit without an energy deposit

    EventHit* hit = new EventHit();

    hit->SetEdep(edep); // energy deposit
    hit->SetPosition(step->GetPreStepPoint()->GetPosition());
    hit->SetTime(step->GetPreStepPoint()->GetGlobalTime());
    hit->SetPDG(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
    hit->SetTrackID(step->GetTrack()->GetTrackID());
    hit->SetParentID(step->GetTrack()->GetParentID());
    hit->SetKineticEnergy(step->GetPreStepPoint()->GetKineticEnergy());

    // Panel name from the touchable
    G4String volumeName = step -> GetPreStepPoint() -> GetTouchable() -> GetVolume() -> GetName();

    hit->SetPanelName(volumeName);

    fHitsCollection->insert(hit);

    return true;


}

void SensitiveDetector::EndOfEvent(G4HCofThisEvent* /*hce*/)
{
    // Empty for now; debug output can be added here if needed
}