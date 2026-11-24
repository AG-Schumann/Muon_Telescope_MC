// MuteFileSource.hh
#ifndef MuteFileSource_hh
#define MuteFileSource_hh 1

#include "IMuonSource.hh"
#include <vector>

// Reads muon events produced by the MUTE-based generator of the collaboration.
//
// File format (whitespace separated, one event per line):
//   x  y  z  dx  dy  dz  E
//     x, y, z    : generation position [mm], on a hemisphere of R = 2 m
//     dx, dy, dz : direction cosines (UNIT vector - the "px py pz" header is
//                  misleading, these are not momentum components)
//     E          : TOTAL energy [keV]
//
// mu+ and mu- are provided in separate files, so the charge is not stored in
// the file and must be given in the constructor.
class MuteFileSource : public IMuonSource
{
public:
    explicit MuteFileSource(G4int charge);
    ~MuteFileSource() override;

    bool LoadFile(const G4String& filename);
    bool GetNextEvent(MuonEvent& event) override;

    G4long GetTotalEvents() const { return static_cast<G4long>(fEvents.size()); }

private:
    std::vector<MuonEvent> fEvents;
    G4long fCurrentEventIndex = 0;
    G4int  fCharge            = 0;

    static constexpr G4double kMuonMassGeV = 0.1056583755;
};

#endif