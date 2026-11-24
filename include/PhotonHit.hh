#ifndef PhotonHit_hh
#define PhotonHit_hh 1

#include "G4VHit.hh"
#include "G4THitsCollection.hh"
#include "G4Allocator.hh"
#include "G4ThreeVector.hh"
#include "G4Types.hh"
#include "G4String.hh"

// Codes shared by the photon counting classes. The volume names follow the
// ones in DetectorConstruction: Panel<i>, LightGuide<i>, PMTWindow<i>.
enum PhotonOrigin  { kOriginScint = 0, kOriginGuide = 1, kOriginWindow = 2, kOriginOther = -1 };
enum PhotonProcess { kProcScint = 0, kProcCerenkov = 1, kProcOther = -1 };

inline G4int PhotonOriginFromVolume(const G4String& lvName)
{
    if (lvName.rfind("Panel", 0) == 0)      return kOriginScint;
    if (lvName.rfind("LightGuide", 0) == 0) return kOriginGuide;
    if (lvName.rfind("PMTWindow", 0) == 0)  return kOriginWindow;
    return kOriginOther;
}

// Module index 0..3 from the trailing digit of the volume name, -1 otherwise.
inline G4int PhotonModuleFromVolume(const G4String& lvName)
{
    if (PhotonOriginFromVolume(lvName) == kOriginOther) return -1;
    const char c = lvName.back();
    return (c >= '0' && c <= '3') ? (c - '0') : -1;
}

inline G4int PhotonProcessFromName(const G4String& processName)
{
    if (processName == "Scintillation") return kProcScint;
    if (processName == "Cerenkov")      return kProcCerenkov;
    return kProcOther;
}


// One detected optical photon (see PhotonDetector).
class PhotonHit : public G4VHit
{
public:
    PhotonHit();
    ~PhotonHit();

    inline void* operator new(size_t);
    inline void  operator delete(void*);

    void SetModule(G4int m)                   { fModule = m; }
    void SetOrigin(G4int o)                   { fOrigin = o; }
    void SetProcess(G4int p)                  { fProcess = p; }
    void SetEnergy(G4double e)                { fEnergy = e; }
    void SetTime(G4double t)                  { fTime = t; }
    void SetVertex(const G4ThreeVector& v)    { fVertex = v; }

    G4int         GetModule()  const { return fModule; }
    G4int         GetOrigin()  const { return fOrigin; }
    G4int         GetProcess() const { return fProcess; }
    G4double      GetEnergy()  const { return fEnergy; }   // Geant4 energy unit
    G4double      GetTime()    const { return fTime; }     // global time
    G4ThreeVector GetVertex()  const { return fVertex; }   // creation point

private:
    G4int         fModule  = -1;
    G4int         fOrigin  = kOriginOther;
    G4int         fProcess = kProcOther;
    G4double      fEnergy  = 0.;
    G4double      fTime    = 0.;
    G4ThreeVector fVertex;
};

using PhotonHitsCollection = G4THitsCollection<PhotonHit>;

extern G4ThreadLocal G4Allocator<PhotonHit>* PhotonHitAllocator;

inline void* PhotonHit::operator new(size_t)
{
    if (!PhotonHitAllocator)
        PhotonHitAllocator = new G4Allocator<PhotonHit>;
    return (void*)PhotonHitAllocator->MallocSingle();
}

inline void PhotonHit::operator delete(void* hit)
{
    PhotonHitAllocator->FreeSingle((PhotonHit*) hit);
}

#endif
