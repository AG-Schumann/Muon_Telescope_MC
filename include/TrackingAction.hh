#ifndef TrackingAction_hh
#define TrackingAction_hh 1

#include "G4UserTrackingAction.hh"
#include "globals.hh"

#include <unordered_map>

// Keeps track of which primary particle every track descends from.
//
// The hit ntuples store trackID and parentID, but with a shower generator a
// hit can be several generations away from the primary. Geant4 always starts
// a track after its parent, so the ancestor of the parent is already known
// when a new track begins.
class TrackingAction : public G4UserTrackingAction
{
public:
    TrackingAction() = default;
    ~TrackingAction() override = default;

    void PreUserTrackingAction(const G4Track* track) override;

    // trackID of the primary this track descends from; 0 if unknown
    G4int GetAncestor(G4int trackID) const;

    void Clear() { fAncestor.clear(); }

private:
    std::unordered_map<G4int, G4int> fAncestor;
};

#endif