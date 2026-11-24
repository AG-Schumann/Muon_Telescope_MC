// MuteFileSource.cc
#include "MuteFileSource.hh"

#include <fstream>
#include <sstream>
#include <cmath>

MuteFileSource::MuteFileSource(G4int charge) : fCharge(charge)
{
    if (charge != 1 && charge != -1) {
        G4cerr << "MuteFileSource: charge must be +1 or -1, got " << charge << G4endl;
    }
}

MuteFileSource::~MuteFileSource() {}

bool MuteFileSource::LoadFile(const G4String& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        G4cerr << "MuteFileSource: could not open file " << filename << G4endl;
        return false;
    }

    fEvents.clear();
    fCurrentEventIndex = 0;

    G4long badLines = 0;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        G4double x, y, z, dx, dy, dz, E_keV;
        iss >> x >> y >> z >> dx >> dy >> dz >> E_keV;
        if (iss.fail()) { ++badLines; continue; }

        // Direction: normalize defensively
        const G4double dmag = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (dmag <= 0.) { ++badLines; continue; }
        dx /= dmag; dy /= dmag; dz /= dmag;

        // Energy: file stores TOTAL energy in keV
        const G4double Etot_GeV = E_keV * 1.0e-6;
        if (Etot_GeV <= kMuonMassGeV) { ++badLines; continue; }   // unphysical

        const G4double p_GeV = std::sqrt(Etot_GeV*Etot_GeV
                                       - kMuonMassGeV*kMuonMassGeV);

        MuonEvent evt;
        // Position is already in mm, i.e. the Geant4 internal length unit
        evt.x = x;  evt.y = y;  evt.z = z;
        // Momentum 3-vector in GeV/c, matching the MuonEvent contract
        evt.px = p_GeV * dx;
        evt.py = p_GeV * dy;
        evt.pz = p_GeV * dz;
        evt.charge = fCharge;
        // Zenith angle measured from the downward (-z) direction
        evt.theta = std::acos(-dz);
        evt.phi   = std::atan2(dy, dx);

        fEvents.push_back(evt);
    }
    file.close();

    G4cout << "MuteFileSource: loaded " << fEvents.size() << " "
           << (fCharge > 0 ? "mu+" : "mu-") << " events from " << filename;
    if (badLines > 0) G4cout << " (" << badLines << " lines skipped)";
    G4cout << G4endl;

    return !fEvents.empty();
}

bool MuteFileSource::GetNextEvent(MuonEvent& event)
{
    if (fCurrentEventIndex >= static_cast<G4long>(fEvents.size())) return false;
    event = fEvents[fCurrentEventIndex];
    ++fCurrentEventIndex;
    return true;
}