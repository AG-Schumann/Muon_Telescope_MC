#ifndef EventAction_hh
#define EventAction_hh 1

#include "G4UserEventAction.hh"
#include "globals.hh"

class TrackingAction;
class StackingAction;
class SteppingAction;
class G4HCofThisEvent;

class EventAction : public G4UserEventAction
{
public:
    // stacking and stepping are only given in optical mode (-p); they hold
    // the photon counters of the event.
    explicit EventAction(TrackingAction* tracking = nullptr,
                         StackingAction* stacking = nullptr,
                         SteppingAction* stepping = nullptr);
    ~EventAction() override;

    void BeginOfEventAction(const G4Event* event) override;
    void EndOfEventAction(const G4Event* event) override;

private:
    // Hit collection IDs, cached instead of looked up in every event
    G4int fPanel0HCID = -1;
    G4int fPanel1HCID = -1;
    G4int fPanel2HCID = -1;
    G4int fPanel3HCID = -1;

    // Not owned; used to look up which primary a hit descends from.
    TrackingAction* fTracking = nullptr;

    // Optical mode, not owned
    StackingAction* fStacking = nullptr;
    SteppingAction* fStepping = nullptr;
    G4int fPhotonHCID = -1;

    void FillOptical(const G4Event* event, G4HCofThisEvent* hce);
};

#endif