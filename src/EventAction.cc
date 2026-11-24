#include "EventAction.hh"
#include "EventHit.hh"

#include "G4Event.hh"
#include "G4SDManager.hh"
#include "G4HCofThisEvent.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"

#include "TrackingAction.hh"
#include "StackingAction.hh"
#include "SteppingAction.hh"
#include "PhotonHit.hh"
#include "DetectorConstruction.hh"
#include "G4RunManager.hh"

#include <iostream>
#include <map>



EventAction::EventAction(TrackingAction* tracking, StackingAction* stacking,
                         SteppingAction* stepping)
    : fTracking(tracking), fStacking(stacking), fStepping(stepping) {}
EventAction::~EventAction() {}

// Fills one panel ntuple and returns the total energy deposited in that panel.
// The four panels are identical, so keeping this in one place avoids the
// copy-paste mistakes that come with four nearly identical loops.
static G4double FillPanel(EventHitsCollection* hc, G4int ntupleId, G4int evtID,
                          const TrackingAction* tracking)
{
    auto analysisManager = G4AnalysisManager::Instance();
    G4double edepTotal = 0.;

    for (size_t i = 0; i < hc->entries(); ++i) {
        auto hit = (*hc)[i];
        if (!hit) continue;
        edepTotal += hit->GetEdep();

        analysisManager->FillNtupleIColumn(ntupleId, 0, evtID);
        analysisManager->FillNtupleIColumn(ntupleId, 1, hit->GetTrackID());
        analysisManager->FillNtupleIColumn(ntupleId, 2, hit->GetParentID());
        analysisManager->FillNtupleIColumn(ntupleId, 3, hit->GetPDG());
        analysisManager->FillNtupleDColumn(ntupleId, 4, hit->GetEdep() / MeV);
        analysisManager->FillNtupleDColumn(ntupleId, 5, hit->GetPosition().x() / mm);
        analysisManager->FillNtupleDColumn(ntupleId, 6, hit->GetPosition().y() / mm);
        analysisManager->FillNtupleDColumn(ntupleId, 7, hit->GetPosition().z() / mm);
        analysisManager->FillNtupleDColumn(ntupleId, 8, hit->GetTime() / ns);
        analysisManager->FillNtupleDColumn(ntupleId, 9, hit->GetKineticEnergy() / MeV);
        analysisManager->FillNtupleIColumn(ntupleId, 10,
            tracking ? tracking->GetAncestor(hit->GetTrackID()) : 0);
        analysisManager->AddNtupleRow(ntupleId);
    }
    return edepTotal;
}

void EventAction::BeginOfEventAction(const G4Event*)
{
    if (fTracking) fTracking->Clear();
    if (fStacking) fStacking->Clear();
    if (fStepping) fStepping->Clear();
}

void EventAction::EndOfEventAction(const G4Event* event)
{

    // Save Hit collection IDs on first event
    if (fPanel0HCID < 0 ){
        auto sdManager = G4SDManager::GetSDMpointer();
        fPanel0HCID = sdManager->GetCollectionID("Panel0SD/Panel0HitsCollection");
        fPanel1HCID = sdManager->GetCollectionID("Panel1SD/Panel1HitsCollection");
        fPanel2HCID = sdManager->GetCollectionID("Panel2SD/Panel2HitsCollection");
        fPanel3HCID = sdManager->GetCollectionID("Panel3SD/Panel3HitsCollection");
    }

    // Get hit collections
    auto hce = event->GetHCofThisEvent();

    if(!hce) {
        G4Exception("EventAction::EndOfEventAction()", "EVENT", JustWarning,
                    "No hits collection of this event.");
        return;
    }

    auto panel0HC = static_cast<EventHitsCollection*>(hce->GetHC(fPanel0HCID));
    auto panel1HC = static_cast<EventHitsCollection*>(hce->GetHC(fPanel1HCID));
    auto panel2HC = static_cast<EventHitsCollection*>(hce->GetHC(fPanel2HCID));
    auto panel3HC = static_cast<EventHitsCollection*>(hce->GetHC(fPanel3HCID));
    if (!panel0HC || !panel1HC || !panel2HC || !panel3HC) {
        G4Exception("EventAction::EndOfEventAction()", "NoHitsCollection",
                    FatalException,
                    "Panel hits collections not found. The sensitive detectors "
                    "are probably not attached to the current geometry.");
    }

    auto analysisManager = G4AnalysisManager::Instance();
    G4int evtID = event->GetEventID();

    const G4double panel0Edep = FillPanel(panel0HC, 0, evtID, fTracking);
    const G4double panel1Edep = FillPanel(panel1HC, 1, evtID, fTracking);
    const G4double panel2Edep = FillPanel(panel2HC, 2, evtID, fTracking);
    const G4double panel3Edep = FillPanel(panel3HC, 3, evtID, fTracking);
        
    // Coincidence
    G4bool anyHit = false;
    G4bool hit[4];

    for (G4int p = 0; p < 4; ++p) {
        hit[p] = (p == 0) ? (panel0Edep > 0.) :
                 (p == 1) ? (panel1Edep > 0.) :
                 (p == 2) ? (panel2Edep > 0.) :
                            (panel3Edep > 0.);
        if (hit[p]) anyHit = true;
    }
    
    analysisManager->FillNtupleIColumn(4, 0, evtID);
    analysisManager->FillNtupleIColumn(4, 1, (hit[0] && hit[1] && hit[2] && hit[3]) ? 1 : 0); // 4-fold coincidence
    analysisManager->FillNtupleIColumn(4, 2, (hit[0] && hit[1]) ? 1 : 0); // Top-2 panel coincidence
    analysisManager->FillNtupleIColumn(4, 3, (hit[2] && hit[3]) ? 1 : 0); // Bottom-2 panel coincidence
    analysisManager->FillNtupleIColumn(4, 4, (hit[0] && hit[3]) ? 1 : 0); // Outer-2 panel coincidence
    analysisManager->AddNtupleRow(4);  // Coincidence

    if (fStacking) FillOptical(event, hce);
}

// Optical mode: OpticalSummary (ntuple 7) and PhotonHits (ntuple 8).
void EventAction::FillOptical(const G4Event* event, G4HCofThisEvent* hce)
{
    if (fPhotonHCID < 0) {
        fPhotonHCID = G4SDManager::GetSDMpointer()->GetCollectionID("PhotonSD/PhotonHitsCollection");
    }
    auto photonHC = static_cast<PhotonHitsCollection*>(hce->GetHC(fPhotonHCID));
    if (!photonHC) {
        G4Exception("EventAction::FillOptical()", "NoPhotonHits", FatalException,
                    "PhotonHitsCollection not found.");
        return;
    }

    auto detector = static_cast<const DetectorConstruction*>(
        G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    const G4bool storePhotons = detector && detector->GetStorePhotons();

    auto analysisManager = G4AnalysisManager::Instance();
    const G4int evtID = event->GetEventID();
    const G4double hc = 1239.84193*eV*nm;

    // Detected photons: [module][origin][process] and first arrival time
    G4int    detected[4][3][2] = {};
    G4double tFirst[4] = {-1., -1., -1., -1.};

    for (size_t i = 0; i < photonHC->entries(); ++i) {
        auto hit = (*photonHC)[i];
        const G4int m = hit->GetModule();
        if (m < 0 || hit->GetOrigin() < 0 || hit->GetProcess() < 0) continue;
        ++detected[m][hit->GetOrigin()][hit->GetProcess()];
        if (tFirst[m] < 0. || hit->GetTime() < tFirst[m]) tFirst[m] = hit->GetTime();

        if (storePhotons) {
            analysisManager->FillNtupleIColumn(8, 0, evtID);
            analysisManager->FillNtupleIColumn(8, 1, m);
            analysisManager->FillNtupleIColumn(8, 2, hit->GetOrigin());
            analysisManager->FillNtupleIColumn(8, 3, hit->GetProcess());
            analysisManager->FillNtupleDColumn(8, 4, hit->GetEnergy() / eV);
            analysisManager->FillNtupleDColumn(8, 5, (hc / hit->GetEnergy()) / nm);
            analysisManager->FillNtupleDColumn(8, 6, hit->GetTime() / ns);
            analysisManager->FillNtupleDColumn(8, 7, hit->GetVertex().x() / mm);
            analysisManager->FillNtupleDColumn(8, 8, hit->GetVertex().y() / mm);
            analysisManager->FillNtupleDColumn(8, 9, hit->GetVertex().z() / mm);
            analysisManager->AddNtupleRow(8);
        }
    }

    analysisManager->FillNtupleIColumn(7, 0, evtID);
    for (G4int m = 0; m < 4; ++m) {
        const G4int c = 1 + 12*m;
        analysisManager->FillNtupleIColumn(7, c + 0, fStacking->GetProduced(m, kOriginScint,  kProcScint));
        analysisManager->FillNtupleIColumn(7, c + 1, fStacking->GetProduced(m, kOriginScint,  kProcCerenkov));
        analysisManager->FillNtupleIColumn(7, c + 2, fStacking->GetProduced(m, kOriginGuide,  kProcCerenkov));
        analysisManager->FillNtupleIColumn(7, c + 3, fStacking->GetProduced(m, kOriginWindow, kProcCerenkov));
        analysisManager->FillNtupleIColumn(7, c + 4, detected[m][kOriginScint][kProcScint]);
        analysisManager->FillNtupleIColumn(7, c + 5, detected[m][kOriginScint][kProcCerenkov]);
        analysisManager->FillNtupleIColumn(7, c + 6, detected[m][kOriginGuide][kProcCerenkov]);
        analysisManager->FillNtupleIColumn(7, c + 7, detected[m][kOriginWindow][kProcCerenkov]);
        analysisManager->FillNtupleDColumn(7, c + 8, tFirst[m] < 0. ? -1. : tFirst[m] / ns);
        analysisManager->FillNtupleIColumn(7, c + 9,  fStepping ? fStepping->GetBulkAbsorbed(m)    : 0);
        analysisManager->FillNtupleIColumn(7, c + 10, fStepping ? fStepping->GetSurfaceAbsorbed(m) : 0);
        analysisManager->FillNtupleIColumn(7, c + 11, fStepping ? fStepping->GetNoRindex(m)        : 0);
    }
    analysisManager->AddNtupleRow(7);
}

