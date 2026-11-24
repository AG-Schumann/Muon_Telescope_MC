#ifndef StackingAction_hh
#define StackingAction_hh 1

#include "G4UserStackingAction.hh"
#include "globals.hh"

// Counts the optical photons created in each module, by origin volume and
// creator process, as they are handed to the stack. EventAction resets the
// counters at the start of every event and writes them to the
// OpticalSummary ntuple.
class StackingAction : public G4UserStackingAction
{
public:
    StackingAction() = default;
    ~StackingAction() override = default;

    G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override;

    // module 0..3, origin and process codes from PhotonHit.hh
    G4int GetProduced(G4int module, G4int origin, G4int process) const
    {
        return fProduced[module][origin][process];
    }
    void Clear();

private:
    G4int fProduced[4][3][2] = {};
};

#endif
