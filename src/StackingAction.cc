#include "StackingAction.hh"
#include "PhotonHit.hh"

#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4VProcess.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"

#include <cstring>

G4ClassificationOfNewTrack StackingAction::ClassifyNewTrack(const G4Track* track)
{
    if (track->GetParticleDefinition() != G4OpticalPhoton::OpticalPhotonDefinition())
        return fUrgent;

    // G4Cerenkov and G4Scintillation give the secondary the pre-step
    // touchable of the parent, so GetVolume() is the volume of creation.
    // GetLogicalVolumeAtVertex() is only set once tracking starts.
    const G4VPhysicalVolume* pv = track->GetVolume();
    const G4VProcess* creator   = track->GetCreatorProcess();
    if (pv && creator) {
        const G4String& lvName = pv->GetLogicalVolume()->GetName();
        const G4int module  = PhotonModuleFromVolume(lvName);
        const G4int origin  = PhotonOriginFromVolume(lvName);
        const G4int process = PhotonProcessFromName(creator->GetProcessName());
        if (module >= 0 && origin >= 0 && process >= 0)
            ++fProduced[module][origin][process];
    }
    return fUrgent;
}

void StackingAction::Clear()
{
    std::memset(fProduced, 0, sizeof(fProduced));
}
