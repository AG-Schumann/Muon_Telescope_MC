//EcoMugFileSource.hh

#ifndef EcoMugFileSource_hh
#define EcoMugFileSource_hh 1

#include "IMuonSource.hh"
#include <vector>


class EcoMugFileSource : public IMuonSource
{
public:
    EcoMugFileSource();
    ~EcoMugFileSource() override;
    
    bool LoadFile(const G4String& filename);
    bool GetNextEvent(MuonEvent& event) override;

private:
    std::vector<MuonEvent> fEvents;
    G4int fCurrentEventIndex;

};


#endif