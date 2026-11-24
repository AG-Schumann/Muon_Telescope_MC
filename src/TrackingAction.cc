#include "TrackingAction.hh"
#include "G4Track.hh"

void TrackingAction::PreUserTrackingAction(const G4Track* track)
{
    const G4int id = track->GetTrackID();
    const G4int parent = track->GetParentID();

    if (parent == 0) {
        fAncestor[id] = id;          // this track is a primary
    } else {
        auto it = fAncestor.find(parent);
        fAncestor[id] = (it != fAncestor.end()) ? it->second : parent;
    }
}

G4int TrackingAction::GetAncestor(G4int trackID) const
{
    auto it = fAncestor.find(trackID);
    return (it != fAncestor.end()) ? it->second : 0;
}