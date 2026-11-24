#ifndef MuonEvent_hh
#define MuonEvent_hh 1

#include "G4Types.hh"

struct MuonEvent
{
    // Initial position of the muon (mm, Geant4 internal length unit)
    G4double x = 0;
    G4double y = 0;
    G4double z = 0;

    // Initial momentum vector of the muon (GeV/c)
    // Full 3-vector, NOT a unit direction
    G4double px = 0;
    G4double py = 0;
    G4double pz = 0;

    // Charge of the muon (electric charge units): +1 for mu+, -1 for mu-
    G4int charge = 0;

    // Initial theta and phi angles (radians)
    G4double theta = 0; // Polar angle from the z-axis
    G4double phi   = 0; // Azimuthal angle in the x-y plane

    // PDG code. 0 = not available -> PrimaryGeneratorAction falls back to
    // mu+/mu- from `charge` (behaviour of all existing single-muon sources).
    G4int pdg = 0;

    // Arrival time relative to the earliest particle of the same shower (ns).
    // 0 for single-particle sources.
    G4double t_ns = 0;


};

#endif