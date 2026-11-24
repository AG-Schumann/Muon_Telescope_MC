#ifndef IMuonSource_hh
#define IMuonSource_hh 1

#include "globals.hh"
#include "MuonEvent.hh"
#include <vector>


class IMuonSource {
public:
    virtual ~IMuonSource() = default;
    virtual bool GetNextEvent(MuonEvent& event) = 0;
    // Shower-type sources return several particles per event. The default
    // implementation just wraps GetNextEvent(), so the existing single-muon
    // sources do not need to be changed.
    virtual bool GetNextShower(std::vector<MuonEvent>& particles)
    {
        particles.clear();
        MuonEvent event;
        if (!GetNextEvent(event)) return false;
        particles.push_back(event);
        return true;
    }

    // Optional metadata, written to the RunInfo ntuple
    // Sources that cannot provide a value keep the defaults; the analysis
    // side should treat 0 as "not available".
    virtual G4String GetSourceLabel()    const { return "unknown"; }
    virtual G4String GetSurfaceLabel()   const { return "n/a"; }
    virtual G4double GetGenerationArea() const { return 0.; }  // m^2
    virtual G4double GetNormalizedFlux() const { return 0.; }  // Hz/m^2
    virtual G4double GetFluxError()      const { return 0.; }  // Hz/m^2
    virtual std::uint64_t GetSeed()      const { return 0; }
    virtual G4double GetLiveTime(G4int /*nGenerated*/) { return 0.; }
    virtual void PrintConfiguration() {}   // default: no-op

    
};


#endif