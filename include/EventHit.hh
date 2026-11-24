#ifndef EventHit_hh
#define EventHit_hh 1

#include "G4VHit.hh"
#include "G4THitsCollection.hh"
#include "G4Allocator.hh"
#include "G4ThreeVector.hh"
#include "G4Types.hh"


class EventHit : public G4VHit
{
public:
    EventHit();
    ~EventHit();

    // Operators needed by the Geant4 hit allocator
    inline void* operator new(size_t);
    inline void  operator delete(void*);


    // Setters
    void SetEdep(G4double edep)               { fEdep = edep;}
    void SetPosition(G4ThreeVector pos)       { fPosition = pos;}
    void SetTime(G4double time)               { fTime = time;}
    void SetPDG(G4int pdg)                    { fPDG = pdg;}
    void SetTrackID(G4int id)                 { fTrackID = id;}
    void SetParentID(G4int id)                { fParentID = id;}
    void SetPanelName(const G4String& name)   { fPanelName = name;}
    void SetKineticEnergy(G4double e)         { fKineticEnergy = e; }
    G4double GetKineticEnergy() const         { return fKineticEnergy; }

    // Getters
    G4double      GetEdep()      const { return fEdep; }
    G4ThreeVector GetPosition()  const { return fPosition; }
    G4double      GetTime()      const { return fTime; }
    G4int         GetPDG()       const { return fPDG; }
    G4int         GetTrackID()   const { return fTrackID; }
    G4int         GetParentID()  const { return fParentID; }
    const G4String& GetPanelName() const { return fPanelName; }

private:
    G4double      fEdep      = 0.;
    G4ThreeVector fPosition;
    G4double      fTime      = 0.;
    G4int         fPDG       = 0;
    G4int         fTrackID   = 0;
    G4int         fParentID  = 0;
    G4String      fPanelName = "";
    G4double      fKineticEnergy = 0.;

};

using EventHitsCollection = G4THitsCollection<EventHit>;

extern G4ThreadLocal G4Allocator<EventHit>* EventHitAllocator;


inline void* EventHit::operator new(size_t)
{
    if (!EventHitAllocator)
        EventHitAllocator = new G4Allocator<EventHit>;
    return (void*)EventHitAllocator->MallocSingle();
}

inline void EventHit::operator delete(void* hit)
{
    EventHitAllocator->FreeSingle((EventHit*) hit);
}






#endif