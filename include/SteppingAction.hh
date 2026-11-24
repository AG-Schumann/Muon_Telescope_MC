#ifndef SteppingAction_hh
#define SteppingAction_hh 1

#include "G4UserSteppingAction.hh"
#include "globals.hh"

class G4OpBoundaryProcess;

// Photon bookkeeping for the optical mode: counts, per module of origin, the
// optical photons lost by bulk absorption (OpAbsorption), by absorption at a
// wrapping surface, and by the NoRINDEX kill of G4OpBoundaryProcess. With the
// detected photons from PhotonDetector the sum must equal the produced
// photons of StackingAction, which is the check of the optical set-up.
class SteppingAction : public G4UserSteppingAction
{
public:
    SteppingAction() = default;
    ~SteppingAction() override = default;

    void UserSteppingAction(const G4Step* step) override;

    G4int GetBulkAbsorbed(G4int module)    const { return fBulkAbsorbed[module]; }
    G4int GetSurfaceAbsorbed(G4int module) const { return fSurfaceAbsorbed[module]; }
    G4int GetNoRindex(G4int module)        const { return fNoRindex[module]; }
    void Clear();

private:
    const G4OpBoundaryProcess* fBoundary = nullptr;   // looked up once

    G4int fBulkAbsorbed[4]    = {};
    G4int fSurfaceAbsorbed[4] = {};
    G4int fNoRindex[4]        = {};
};

#endif
