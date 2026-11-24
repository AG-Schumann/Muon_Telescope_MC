#include "PhotonDetector.hh"

#include "G4Step.hh"
#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4OpticalPhoton.hh"
#include "G4VProcess.hh"


PhotonDetector::PhotonDetector(const G4String& name, const G4String& hitsCollectionName)
    : G4VSensitiveDetector(name)
{
    collectionName.insert(hitsCollectionName);
}

PhotonDetector::~PhotonDetector()
{
}

void PhotonDetector::Initialize(G4HCofThisEvent* hce)
{
    fHitsCollection = new PhotonHitsCollection(SensitiveDetectorName, collectionName[0]);
    G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
    hce->AddHitsCollection(hcID, fHitsCollection);
}

G4bool PhotonDetector::ProcessHits(G4Step* step, G4TouchableHistory*)
{
    G4Track* track = step->GetTrack();
    if (track->GetParticleDefinition() != G4OpticalPhoton::OpticalPhotonDefinition())
        return false;

    // First step in the window: either the photon has just crossed the
    // light-guide/window interface, or it was created inside the glass by a
    // charged particle (Cherenkov) and this is its first step.
    const G4StepPoint* pre = step->GetPreStepPoint();
    const G4bool entered = (pre->GetStepStatus() == fGeomBoundary);
    const G4bool created = (track->GetCurrentStepNumber() == 1);
    if (!entered && !created) {
        track->SetTrackStatus(fStopAndKill);
        return false;
    }

    auto hit = new PhotonHit();
    hit->SetModule(PhotonModuleFromVolume(pre->GetTouchable()->GetVolume()->GetName()));
    hit->SetOrigin(PhotonOriginFromVolume(track->GetLogicalVolumeAtVertex()->GetName()));
    const G4VProcess* creator = track->GetCreatorProcess();
    hit->SetProcess(creator ? PhotonProcessFromName(creator->GetProcessName()) : kProcOther);
    hit->SetEnergy(pre->GetKineticEnergy());
    hit->SetTime(pre->GetGlobalTime());
    hit->SetVertex(track->GetVertexPosition());
    fHitsCollection->insert(hit);

    track->SetTrackStatus(fStopAndKill);
    return true;
}
