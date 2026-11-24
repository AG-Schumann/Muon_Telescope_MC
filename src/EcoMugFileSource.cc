//EcoMugFileSource.cc

#include "EcoMugFileSource.hh"
#include <fstream>
#include <sstream>

#include "G4SystemOfUnits.hh"

EcoMugFileSource::EcoMugFileSource() {}
EcoMugFileSource::~EcoMugFileSource() {}


bool EcoMugFileSource::LoadFile(const G4String& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        G4cerr << "EcoMugFileSource: Could not open file " << filename << G4endl;
        return false;
    }

    fEvents.clear();
    fCurrentEventIndex = 0;
    std::string line;

    while (std::getline(file,line)) {
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        std::istringstream iss(line);
        MuonEvent evt;
        G4double theta, phi, p_dummy, energy_dummy;

        iss >> evt.x >> evt.y >> evt.z 
            >> evt.px >> evt.py >> evt.pz 
            >> p_dummy >> theta >> phi 
            >> evt.charge >> energy_dummy;

        if (iss.fail()) {
            G4cerr << "EcoMugFileSource: Failed to parse line: " << line << G4endl;
            continue; // Skip malformed lines
        }
        evt.x *= m; // Convert from m to Geant4 internal length unit (which is mm)
        evt.y *= m;
        evt.z *= m;
        fEvents.push_back(evt);

    };
    file.close();
    G4cout << "EcoMugFileSource: Loaded " << fEvents.size()
           << " events from " << filename << G4endl;
    return !fEvents.empty();   
}



bool EcoMugFileSource::GetNextEvent(MuonEvent& event)
{
    if (fCurrentEventIndex >= static_cast<G4int>(fEvents.size())) return false;

    event = fEvents[fCurrentEventIndex];
    ++fCurrentEventIndex;
    return true;
}

