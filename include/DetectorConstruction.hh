#ifndef DetectorConstruction_hh
#define DetectorConstruction_hh 1

#include "G4VUserDetectorConstruction.hh"
#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"
#include "globals.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4NistManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SubtractionSolid.hh"


#include "SensitiveDetector.hh"
#include "DetectorMessenger.hh"

class G4Material;
class G4MaterialPropertiesTable;

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
    // optical = true (the -p flag): optical material properties in every
    // geometry mode, PMT windows and optical surfaces in mode 2.
    explicit DetectorConstruction(G4bool optical = false);
    ~DetectorConstruction();

    virtual G4VPhysicalVolume* Construct() override;

    virtual void ConstructSDandField() override;

    // Access to the scoring volumes

    const G4LogicalVolume* GetPanel0LV() const { return fPanel0; }
    const G4LogicalVolume* GetPanel1LV() const { return fPanel1; }
    const G4LogicalVolume* GetPanel2LV() const { return fPanel2; }
    const G4LogicalVolume* GetPanel3LV() const { return fPanel3; }

    // Angle setters (called by the messenger)
    void SetZenithAngle(G4double angle)   { fTiltZenithAngle = angle; }
    void SetAzimuthAngle(G4double angle)  { fTiltAzimuthAngle = angle; }

    void SetRoofEnabled(G4bool enable)      { fRoofEnabled = enable; }
    void SetRoofThickness(G4double t)       { fRoofThickness = t; }
    void SetRoofHeight(G4double h)          { fRoofHeight = h; }
    void SetRoofHalfSize(G4double s)        { fRoofHalfSize = s; }
    // Overburden of several floors: fRoofSlabs slabs of fRoofThickness each,
    // stacked upwards from fRoofHeight with a clear gap fRoofSlabGap.
    void SetRoofSlabs(G4int n)              { fRoofSlabs = n; }
    void SetRoofSlabGap(G4double g)         { fRoofSlabGap = g; }
    // Height of the upper surface of the highest slab; -DBL_MAX without roof.
    // Every primary vertex must lie above it (checked in PrimaryGeneratorAction).
    G4double GetRoofTop() const             { return fRoofTop; }
    // Total areal density of the slabs along the vertical (0 without roof).
    G4double GetOverburden() const          { return fOverburden; }

    void SetLeadEnabled(G4bool e)     { fLeadEnabled = e; }
    void SetLeadThickness(G4double t) { fLeadThickness = t; }
    void SetLeadGap(G4double g)       { fLeadGap = g; }  

    void SetWorldMaterialIsAir(G4bool isAir) { fWorldMaterialIsAir = isAir; }
    // Geometry mode: 0 = scintillator panels only, 1 = light guides only
    // (named Panel0..3), 2 = scintillator panels + light guides
    void  SetGeometryMode(G4int mode) { fGeometryMode = mode; }
    void  SetFlippedGuide(G4int i)    { fFlippedGuide = i; }
    G4int GetGeometryMode() const     { return fGeometryMode; }

    // Optical mode (-p). The wrapping and the PMMA grade take effect at the
    // next Construct(), i.e. after /detector/rebuild.
    G4bool GetOptical() const                      { return fOptical; }
    void SetWrapping(const G4String& name)         { fWrapping = name; }
    void SetPmmaGrade(const G4String& name)        { fPmmaGrade = name; }
    void SetStorePhotons(G4bool store)             { fStorePhotons = store; }
    G4bool GetStorePhotons() const                 { return fStorePhotons; }
    
    // Angle getters
    G4double GetZenithAngle() const  { return fTiltZenithAngle; }
    G4double GetAzimuthAngle() const { return fTiltAzimuthAngle; }

private:
    void BuildOpticalMaterials(G4Material* scint, G4Material* guide, G4Material* window);
    void SetPmmaAbsorption(G4MaterialPropertiesTable* mpt);
    void BuildOpticalSurfaces(G4LogicalVolume* panelLV[4], G4LogicalVolume* guideLV[4],
                              G4VPhysicalVolume* panelPV[4], G4VPhysicalVolume* guidePV[4],
                              G4VPhysicalVolume* windowPV[4]);

    G4LogicalVolume* fPanel0 = nullptr;
    G4LogicalVolume* fPanel1 = nullptr;
    G4LogicalVolume* fPanel2 = nullptr;
    G4LogicalVolume* fPanel3 = nullptr;
    
    G4double fTiltZenithAngle   = 0.*deg;
    G4double fTiltAzimuthAngle  = 0.*deg;
    
    DetectorMessenger* fMessenger = nullptr;
    
    G4bool   fRoofEnabled   = false;
    G4double fRoofThickness = 30 * cm;
    G4double fRoofHeight    = 1.6 * m;   // the crane-hall roof is at 10.5 m (lower surface)
    G4double fRoofHalfSize  = 0.5 * m;
    G4int    fRoofSlabs     = 1;
    G4double fRoofSlabGap   = 0.;
    G4double fRoofTop       = -DBL_MAX;   // set in Construct()
    G4double fOverburden    = 0.;         // set in Construct()

    G4bool   fLeadEnabled   = false;
    G4double fLeadThickness = 10 * cm;
    G4double fLeadGap       = 8 * cm;

    G4bool   fWorldMaterialIsAir = false;  // true: air, false: vacuum
    
    G4int fGeometryMode = 0;
    G4int fFlippedGuide = -1;   // panel whose guide + PMT are on the other side (-1: none)

    G4bool   fOptical      = false;
    G4String fWrapping     = "alfoil";     // alfoil | tyvek | black
    G4String fPmmaGrade    = "standard";   // standard | uvt
    G4bool   fStorePhotons = false;        // fill the PhotonHits ntuple
    G4LogicalVolume* fWindowLV[4] = {nullptr, nullptr, nullptr, nullptr};


};

#endif
