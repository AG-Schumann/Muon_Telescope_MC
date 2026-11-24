#include "SteppingAction.hh"
#include "PhotonHit.hh"

#include "G4Step.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4VProcess.hh"
#include "G4LogicalVolume.hh"

#include <cstring>

void SteppingAction::UserSteppingAction(const G4Step* step)
{
    const G4Track* track = step->GetTrack();
    if (track->GetParticleDefinition() != G4OpticalPhoton::OpticalPhotonDefinition())
        return;

    const G4LogicalVolume* vertexLV = track->GetLogicalVolumeAtVertex();
    const G4int module = vertexLV ? PhotonModuleFromVolume(vertexLV->GetName()) : -1;
    if (module < 0) return;

    // Inside a PMT window the photon belongs to PhotonDetector, which kills
    // it on its first step there. That step still ends at the far face of
    // the window with a NoRINDEX status (vacuum behind it), which must not
    // be counted as a loss.
    const G4StepPoint* pre = step->GetPreStepPoint();
    if (PhotonOriginFromVolume(pre->GetPhysicalVolume()->GetLogicalVolume()->GetName()) == kOriginWindow)
        return;

    const G4StepPoint* post = step->GetPostStepPoint();
    const G4VProcess*  proc = post->GetProcessDefinedStep();

    if (proc && proc->GetProcessName() == "OpAbsorption") {
        ++fBulkAbsorbed[module];
        return;
    }

    if (post->GetStepStatus() != fGeomBoundary) return;

    if (!fBoundary) {
        auto pv = track->GetParticleDefinition()->GetProcessManager()->GetProcessList();
        for (std::size_t i = 0; i < pv->entries(); ++i) {
            if ((*pv)[i]->GetProcessName() == "OpBoundary") {
                fBoundary = static_cast<const G4OpBoundaryProcess*>((*pv)[i]);
                break;
            }
        }
        if (!fBoundary) return;
    }

    switch (fBoundary->GetStatus()) {
        case Absorption: ++fSurfaceAbsorbed[module]; break;
        case NoRINDEX:   ++fNoRindex[module];        break;
        default: break;
    }
}

void SteppingAction::Clear()
{
    std::memset(fBulkAbsorbed,    0, sizeof(fBulkAbsorbed));
    std::memset(fSurfaceAbsorbed, 0, sizeof(fSurfaceAbsorbed));
    std::memset(fNoRindex,        0, sizeof(fNoRindex));
}
