#include "DetectorConstruction.hh"

#include "SensitiveDetector.hh"
#include "PhotonDetector.hh"

#include "G4SDManager.hh"
#include "G4SubtractionSolid.hh"
#include "G4UnionSolid.hh"
#include "G4Trd.hh"
#include "G4Tubs.hh"
#include "G4Transform3D.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4OpticalSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalBorderSurface.hh"

#include <algorithm>
#include <vector>

namespace {
// ── Geometry mode ────────────────────────────────────────────────
// 0 : original telescope, scintillator panels only.
// 1 : light guides only. The light-guide logical volumes keep the names
//     Panel0..Panel3, so the sensitive detectors, hit collections, ntuples
//     and the coincidence flags work without any other change.
// 2 : scintillator panels and light guides. The light guide of module i is
//     attached to the same sensitive detector as panel i; scintillator and
//     light-guide hits are separated offline by their x position.

// Light guide of the Scionix R280x10B280/2M modules (measured by hand):
// 7 cm long, 28 cm x 1 cm at the scintillator edge, 6 cm x 5 cm at the PMT.
// The thickness is assumed to be symmetric about the panel mid-plane.
const G4double kLGHalfLength     = 3.5  * cm;   // along x, towards the PMT
const G4double kLGHalfWidthIn    = 14.0 * cm;   // along y, at the scintillator edge
const G4double kLGHalfWidthOut   = 3.0  * cm;   // along y, at the PMT window
const G4double kLGHalfThickIn    = 0.5  * cm;   // along z, at the scintillator edge
const G4double kLGHalfThickOut   = 2.5  * cm;   // along z, at the PMT window

// All four PMTs are on the same side of the stack: -1 = -x side, +1 = +x side.
const G4int kLGSide = -1;

// ── Optical mode ─────────────────────────────────────────────────
// PMT window (ET 9214B behind the light guide). The useful photocathode
// diameter of 46 mm is the data-sheet value; it should be checked against
// the Scionix test sheet of the modules. The window is a glass cylinder
// flush against the 60 mm x 50 mm end face of the light guide.
const G4double kPMTCathodeRadius   = 23.0 * mm;
const G4double kPMTWindowHalfLength = 5.0 * mm;

// Photon energy range of all RINDEX tables, 300-650 nm. Cherenkov photons
// are only generated inside this range, so it is part of the result.
const G4double hc            = 1239.84193 * eV * nm;
const G4double kLambdaMin    = 300.0 * nm;
const G4double kLambdaMax    = 650.0 * nm;
const G4double kPhotonEnergyMin = hc / kLambdaMax;   // 1.91 eV
const G4double kPhotonEnergyMax = hc / kLambdaMin;   // 4.13 eV

// Tables are given here in increasing wavelength and converted to
// increasing photon energy, which is what G4MaterialPropertiesTable needs.
struct Spectrum { std::vector<G4double> energy, value; };
Spectrum FromWavelengths(const std::vector<G4double>& lambda_nm,
                         const std::vector<G4double>& value)
{
    Spectrum sp;
    for (std::size_t i = lambda_nm.size(); i-- > 0;) {
        sp.energy.push_back(hc / (lambda_nm[i] * nm));
        sp.value.push_back(value[i]);
    }
    return sp;
}
}


DetectorConstruction::DetectorConstruction(G4bool optical) : fOptical(optical) {
    fMessenger = new DetectorMessenger(this);
}
DetectorConstruction::~DetectorConstruction() {
    delete fMessenger;
}

G4VPhysicalVolume* DetectorConstruction::Construct() {

    // After /detector/rebuild the surface tables would still point to the
    // volumes of the previous geometry.
    if (fOptical) {
        G4LogicalSkinSurface::CleanSurfaceTable();
        G4LogicalBorderSurface::CleanSurfaceTable();
    }

    //Rotations
    G4RotationMatrix* rotationMatrixX = new G4RotationMatrix();
    rotationMatrixX->rotateX(90.*deg);

    G4RotationMatrix* rotationMatrixY = new G4RotationMatrix();
    rotationMatrixY->rotateY(90.*deg);

    G4RotationMatrix* rotationMatrixZ = new G4RotationMatrix();
    rotationMatrixZ->rotateZ(90.*deg);


    G4NistManager* nist = G4NistManager::Instance();

    // ── Materials ───────────────────────────────────────────────
    G4Material* vacuum      = nist->FindOrBuildMaterial("G4_Galactic");
    G4Material* air         = nist->FindOrBuildMaterial("G4_AIR");
    G4Material* aluminum    = nist->FindOrBuildMaterial("G4_Al");
    G4Material* scintMat    = nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");   // Plastic scintillator, polyvinyltoluene base (EJ-200 / BC-408 equivalent)
    G4Material* lead        = nist->FindOrBuildMaterial("G4_Pb");
    G4Material* concrete    = nist->FindOrBuildMaterial("G4_CONCRETE");
    G4Material* lightGuideMat = nist->FindOrBuildMaterial("G4_PLEXIGLASS");             // PMMA (acrylic) light guide, n = 1.49
    G4Material* windowMat   = nist->FindOrBuildMaterial("G4_Pyrex_Glass");               // PMT window, borosilicate

    // ── Optical properties ───────────────────────────────────────
    // Created in the first Construct() whatever the geometry mode: the
    // Cherenkov and scintillation tables are built at initialisation from
    // the materials that have a property table at that moment, and mode 2
    // is only reached later with /detector/rebuild.
    if (fOptical) BuildOpticalMaterials(scintMat, lightGuideMat, windowMat);


    G4Material* worldMaterial;
    G4cout << "DetectorConstruction::Construct() - Materials defined." << G4endl;
    if (fWorldMaterialIsAir) {
        G4cout << "World material is set to AIR." << G4endl;
        worldMaterial = air;
    } else {
        G4cout << "World material is set to VACUUM." << G4endl;
        worldMaterial = vacuum;
    }

    const G4bool withLightGuide = (fGeometryMode != 0);
    const G4bool withScint      = (fGeometryMode != 1);
    const G4bool withOptics     = (fOptical && fGeometryMode == 2);   // PMT windows, surfaces
    if (fOptical && fGeometryMode == 1) {
        G4Exception("DetectorConstruction::Construct()", "OpticalMode1", JustWarning,
                    "Optical mode (-p) needs geometry mode 2; mode 1 has no scintillator.");
    }
    G4cout << "DetectorConstruction::Construct() - geometry mode " << fGeometryMode
           << (fGeometryMode == 0 ? " (scintillator panels only)" :
               fGeometryMode == 1 ? " (light guides only, named Panel0..3)" :
                                    " (scintillator panels + light guides)") << G4endl;

    // ── World volume ─────────────────────────────────────────────
    G4double worldHalfSize = 14.0 * m;
    G4Box* worldBox = new G4Box("World", worldHalfSize, worldHalfSize, worldHalfSize);
    G4LogicalVolume* worldLV = new G4LogicalVolume(worldBox, worldMaterial, "World");
    G4VPhysicalVolume* worldPV = new G4PVPlacement(
        nullptr,            // no rotation
        G4ThreeVector(),    // origin
        worldLV,
        "World",
        nullptr,            // no mother volume
        false, 0, true      // checkOverlaps = true
    );

    // Outer Frame
    G4double frameThicknessHL = 2.0 * cm; // 2 cm thick
    G4double outLegHL = 58.0 / 2.0 * cm; // 58 cm long
    G4double outFrameSideHL = (119.0/2.0) * cm;
    G4double outFrameTopHL =  62.0 / 2.0 * cm;
    G4double outerFrameGap = 48.1 /2.0 * cm; // Outer frame inner gap
    G4double topGap = 0.7 * cm; // gap between the top frame and the inner top bar
    G4double bottomGap = 1.2 * cm; // gap between the bottom frame and the inner bottom bar
    // Real size: 0.280 x 0.280 x 0.01 m
    G4double panelHX = 0.140 * m;   // 0.280/2
    G4double panelHY = 0.140 * m;
    G4double panelHZ = 0.005 * m;   // 0.01/2

    // Panel pair geometry, measured on the assembled telescope
    // Defined here because the light-guide slabs of the inner volume need them.
    // separation_*_2 is the clear gap between the FACING SURFACES of the two
    // panels of a pair; the panel placements below add one half-thickness on
    // each side, so the centre-to-centre distance is the gap plus 1 cm.
    G4double separation_up_2   = 0.105 * m;   // P0 - P1
    G4double separation_down_2 = 0.095 * m;   // P2 - P3

    G4double panel_boxUpHZ   = ((separation_up_2   + 4*panelHZ) / 2);
    G4double panel_boxDownHZ = ((separation_down_2 + 4*panelHZ) / 2);

    // Distance from the rotation pivot to the CENTRE of each pair, measured
    // directly, with the same convention for both pairs.
    G4double panel_boxUpPivotZ   = 0.3825 * m;
    G4double panel_boxDownPivotZ = 0.3925 * m;

    // Pair boxes: with light guides they must also contain the guides, which
    // stick out by 7 cm in x and are up to 5 cm thick.
    const G4double pairBoxHX = withLightGuide ? panelHX + 2*kLGHalfLength + 1.0*cm : panelHX;
    const G4double pairBoxHY = withLightGuide ? panelHY + 1.0*mm : panelHY;
    const G4double topBoxHZ  = withLightGuide
        ? std::max(panel_boxUpHZ,   separation_up_2/2   + panelHZ + kLGHalfThickOut + 2.0*mm)
        : panel_boxUpHZ;
    const G4double botBoxHZ  = withLightGuide
        ? std::max(panel_boxDownHZ, separation_down_2/2 + panelHZ + kLGHalfThickOut + 2.0*mm)
        : panel_boxDownHZ;


    G4double teleWX = outFrameSideHL*1.25; // telescope world
    G4double teleWY = outFrameSideHL*1.25; // telescope world
    G4double teleWZ = outFrameSideHL + 2*frameThicknessHL ; // telescope world
    G4Box* telescopeWorldBox = new G4Box("TelescopeWorld", teleWX, teleWY, teleWZ);
    G4LogicalVolume* telescopeWorldLV = new G4LogicalVolume(telescopeWorldBox, worldMaterial, "TelescopeWorld");
    G4VPhysicalVolume* telescopeWorldPV = new G4PVPlacement(
        nullptr,            // no rotation
        G4ThreeVector(0,0,teleWZ),    // origin
        telescopeWorldLV,
        "TelescopeWorld",
        worldLV,           // placed inside the world
        false, 0, true      // checkOverlaps = true
    );


    G4Box* leg = new G4Box("Leg1", outLegHL, frameThicknessHL, frameThicknessHL);
    G4Box* outFrameSide = new G4Box("OutFrameSide", outFrameSideHL, frameThicknessHL, frameThicknessHL);
    G4Box* outFrameTop = new G4Box("OutFrameTop", outFrameTopHL, frameThicknessHL, frameThicknessHL);

    G4LogicalVolume* leg1LV = new G4LogicalVolume(leg, aluminum, "Leg1");
    G4LogicalVolume* leg2LV = new G4LogicalVolume(leg, aluminum, "Leg2");
    G4LogicalVolume* outFrameSide1LV = new G4LogicalVolume(outFrameSide, aluminum, "OutFrameSide1");
    G4LogicalVolume* outFrameSide2LV = new G4LogicalVolume(outFrameSide, aluminum, "OutFrameSide2");
    G4LogicalVolume* outFrameTopLV = new G4LogicalVolume(outFrameTop, aluminum, "OutFrameTop");

    G4VPhysicalVolume* leg1PV = new G4PVPlacement(
        nullptr,
        G4ThreeVector(0, outerFrameGap, -teleWZ + frameThicknessHL), // left leg
        leg1LV,
        "Leg1",
        telescopeWorldLV,
        false, 0, true
    );
    G4VPhysicalVolume* leg2PV = new G4PVPlacement(
        nullptr,
        G4ThreeVector(0, -outerFrameGap, -teleWZ+frameThicknessHL), // right leg
        leg2LV,
        "Leg2",
        telescopeWorldLV,
        false, 0, true
    );
    G4VPhysicalVolume* outFrameSide1PV = new G4PVPlacement(
        rotationMatrixY, // rotated by 90 deg about y
        G4ThreeVector(0, outerFrameGap,0), //
        outFrameSide1LV,
        "OutFrameSide1",
        telescopeWorldLV,
        false, 0, true
    );
    G4VPhysicalVolume* outFrameSide2PV = new G4PVPlacement(
        rotationMatrixY, // rotated by 90 deg about y
        G4ThreeVector(0, -outerFrameGap, 0), //
        outFrameSide2LV,
        "OutFrameSide2",
        telescopeWorldLV,
        false, 0, true
    );
    G4VPhysicalVolume* outFrameTopPV = new G4PVPlacement(
        rotationMatrixZ,
        G4ThreeVector(0, 0, outFrameSideHL + frameThicknessHL), // Top frame, located in top of side frames
        outFrameTopLV,
        "OutFrameTop",
        telescopeWorldLV,
        false, 0, true
    );

    G4RotationMatrix* pivotRotationMatrix = new G4RotationMatrix(); // Call from messenger
    // Zenith tilt (about y)
    pivotRotationMatrix->rotateY(fTiltZenithAngle); // Call from messenger
    // Azimuth tilt (about z), optional
    pivotRotationMatrix->rotateZ(fTiltAzimuthAngle); // Call from messenger

    // Inner Frame
    G4double rotationPivotUpZ = 54.7 * cm; // Rotation pivot point height for top bar
    G4double rotationPivotDownZ = 58.0 * cm; // Rotation pivot point height for bottom bar
    G4double rotationPointZheight = outFrameSideHL + (2*frameThicknessHL);
    G4double innerVolumeHX = (panelHX*1.1); // Inner volume for pannels and inner frame
    G4double innerVolumeHY = (outerFrameGap - frameThicknessHL); // Inner volume for pannels and inner frame
    G4double innerVolumeHZ = ((2*rotationPivotDownZ) + (frameThicknessHL * 4)) / 2  ; // Inner volume for pannels and inner frame, safe margin included

    G4Box* innerVolumeBox = new G4Box("InnerVolume", innerVolumeHX, innerVolumeHY, innerVolumeHZ);
    // The parts of the inner volume that would overlap the boundary of the
    // telescope world are cut away with the boxes below.

    G4double topcutoutSize = 2*cm; // half height of the first cut box
    G4Box* topCutBox_1 = new G4Box("TopCutBox_1", innerVolumeHX*1.1, innerVolumeHY*1.1, topcutoutSize);
    G4ThreeVector cutPosition = G4ThreeVector(0, 0, innerVolumeHZ - topcutoutSize/2); // Position the cut box at the top of the inner volume, where it would overlap with the telescope world

    G4SubtractionSolid* topCut_1 = new G4SubtractionSolid(
    "InnerVolume",
    innerVolumeBox,
    topCutBox_1,
    nullptr,
    cutPosition);

    G4Box* topCutBox_2 = new G4Box("TopCutBox_2", innerVolumeHX*1.1, innerVolumeHY*1.1, topcutoutSize*4);
    G4ThreeVector cutPosition_2 = G4ThreeVector(innerVolumeHX + frameThicknessHL*2, 0, innerVolumeHZ - topcutoutSize*2);
    G4SubtractionSolid* topCut_2 = new G4SubtractionSolid(
    "InnerVolume",
    topCut_1,
    topCutBox_2,
    nullptr,
    cutPosition_2);

    G4Box* topCutBox_3 = new G4Box("TopCutBox_3", innerVolumeHX*1.1, innerVolumeHY*1.1, topcutoutSize*4);

    G4ThreeVector cutPosition_3 = G4ThreeVector(-innerVolumeHX - frameThicknessHL*2, 0, innerVolumeHZ - topcutoutSize*2);
    G4SubtractionSolid* topCut_3 = new G4SubtractionSolid(
    "InnerVolume",
    topCut_2,
    topCutBox_3,
    nullptr,
    cutPosition_3);


    G4Box* bottomCutBox = new G4Box("BottomCutBox",innerVolumeHY/2 + frameThicknessHL,  innerVolumeHY*1.1, 6*frameThicknessHL);
    G4ThreeVector bottomCutPosition_1 = G4ThreeVector(innerVolumeHX, 0, -(innerVolumeHZ)); // Position the cut box at the bottom of the inner volume, where it would overlap with the telescope world

    G4SubtractionSolid* bottomCutBoxSolid_1 = new G4SubtractionSolid(
    "InnerVolume",
    topCut_3,
    bottomCutBox,
    nullptr,
    bottomCutPosition_1);

    G4ThreeVector bottomCutPosition_2 = G4ThreeVector(-innerVolumeHX, 0, -(innerVolumeHZ));

    G4SubtractionSolid* innerVolumeSolid = new G4SubtractionSolid(
    "InnerVolume",
    bottomCutBoxSolid_1,
    bottomCutBox,
    nullptr,
    bottomCutPosition_2);

    // The light guides stick out of the original inner volume (half-width
    // 15.4 cm) on the PMT side. Two slabs at the heights of the panel pairs
    // are added so that the guides fit. The farthest slab corner is ~53 cm
    // from the pivot, well inside the telescope world (63.5 cm) for every
    // zenith angle, and the slabs stay clear of the inner frame bars.
    G4VSolid* innerVolumeFinal = innerVolumeSolid;
    if (withLightGuide) {
        G4Box* lgSlabUp   = new G4Box("LGSlabUp",   pairBoxHX + 0.5*cm, pairBoxHY + 0.5*cm, topBoxHZ + 0.5*cm);
        G4Box* lgSlabDown = new G4Box("LGSlabDown", pairBoxHX + 0.5*cm, pairBoxHY + 0.5*cm, botBoxHZ + 0.5*cm);
        G4UnionSolid* withSlabUp = new G4UnionSolid("InnerVolume", innerVolumeSolid, lgSlabUp,
                                                    nullptr, G4ThreeVector(0, 0,  panel_boxUpPivotZ));
        innerVolumeFinal = new G4UnionSolid("InnerVolume", withSlabUp, lgSlabDown,
                                            nullptr, G4ThreeVector(0, 0, -panel_boxDownPivotZ));
    }



    G4LogicalVolume* innerVolumeLV = new G4LogicalVolume(innerVolumeFinal, vacuum, "InnerVolume");

    G4VPhysicalVolume* innerVolumePV = new G4PVPlacement(
        pivotRotationMatrix, // rotation about the pivot point
        G4ThreeVector(0, 0, rotationPointZheight - teleWZ), // Inner volume, located at rotation point height
        innerVolumeLV,
        "InnerVolume",
        telescopeWorldLV,
        false, 0, true
    );

    G4double innerSideBar = (rotationPivotUpZ + rotationPivotDownZ) / 2; // Inner frame side bars;
    G4double innerTopBar = 44.0 / 2.0 * cm; // Inner frame top bar
    G4double innerBottomBar = innerTopBar; // Inner frame bottom bar

    G4Box* innerSideBarBox1 = new G4Box("InnerSideBar", innerSideBar, frameThicknessHL, frameThicknessHL);
    G4Box* innerSideBarBox2 = new G4Box("InnerSideBar", innerSideBar, frameThicknessHL, frameThicknessHL);
    G4Box* innerTopBarBox = new G4Box("InnerTopBar",innerTopBar, frameThicknessHL, frameThicknessHL);
    G4Box* innerBottomBarBox = new G4Box("InnerBottomBar",innerBottomBar, frameThicknessHL, frameThicknessHL);

    G4LogicalVolume* innerSideBar1LV = new G4LogicalVolume(innerSideBarBox1, aluminum, "InnerSideBar1");
    G4LogicalVolume* innerSideBar2LV = new G4LogicalVolume(innerSideBarBox2, aluminum, "InnerSideBar2");
    G4LogicalVolume* innerTopBarLV = new G4LogicalVolume(innerTopBarBox, aluminum, "InnerTopBar");
    G4LogicalVolume* innerBottomBarLV = new G4LogicalVolume(innerBottomBarBox, aluminum, "InnerBottomBar");


    G4double sideBarCenterZ = (rotationPivotUpZ - rotationPivotDownZ) / 2.0;


    G4VPhysicalVolume* innerSideBar1PV = new G4PVPlacement(
        rotationMatrixY, // rotated by 90 deg about y
        G4ThreeVector(0, outerFrameGap - (frameThicknessHL*2), sideBarCenterZ), // Inner side bar 1
        innerSideBar1LV,
        "InnerSideBar1",
        innerVolumeLV,
        false, 0, true
    );
    G4VPhysicalVolume* innerSideBar2PV = new G4PVPlacement(
        rotationMatrixY, // rotated by 90 deg about y
        G4ThreeVector(0, -(outerFrameGap - (frameThicknessHL*2)), sideBarCenterZ), // Inner side bar 2
        innerSideBar2LV,
        "InnerSideBar2",
        innerVolumeLV,
        false, 0, true
    );
    G4VPhysicalVolume* innerTopBarPV = new G4PVPlacement(
        rotationMatrixZ, // rotated by 90 deg about z
        G4ThreeVector(0, 0, (rotationPivotUpZ + frameThicknessHL)), // Inner top bar
        innerTopBarLV,
        "InnerTopBar",
        innerVolumeLV,
        false, 0, true
    );
    G4VPhysicalVolume* innerBottomBarPV = new G4PVPlacement(
        rotationMatrixZ, // rotated by 90 deg about z
        G4ThreeVector(0, 0, -(rotationPivotDownZ + frameThicknessHL)), // Inner bottom bar
        innerBottomBarLV,
        "InnerBottomBar",
        innerVolumeLV,
        false, 0, true
    );

    // ── Panel solid & logical ────────────────────────────────────


    G4Box* panelBox = new G4Box("ScintPanel", panelHX, panelHY, panelHZ);

    // Light guide: a G4Trd whose local z axis is the guide length.
    //   local x (width 28 -> 6 cm)     -> telescope y
    //   local y (thickness 1 -> 5 cm)  -> telescope z
    //   local z (+z = PMT end)         -> telescope kLGSide * x
    G4Trd* lightGuideSolid = new G4Trd("LightGuide",
                                       kLGHalfWidthIn, kLGHalfWidthOut,
                                       kLGHalfThickIn, kLGHalfThickOut,
                                       kLGHalfLength);

    // Mode 1: the light guide takes the place (and the name) of the panel.
    if (fGeometryMode == 1) {
        fPanel0 = new G4LogicalVolume(lightGuideSolid, lightGuideMat, "Panel0");
        fPanel1 = new G4LogicalVolume(lightGuideSolid, lightGuideMat, "Panel1");
        fPanel2 = new G4LogicalVolume(lightGuideSolid, lightGuideMat, "Panel2");
        fPanel3 = new G4LogicalVolume(lightGuideSolid, lightGuideMat, "Panel3");
    } else {
        fPanel0 = new G4LogicalVolume(panelBox, scintMat, "Panel0");
        fPanel1 = new G4LogicalVolume(panelBox, scintMat, "Panel1");
        fPanel2 = new G4LogicalVolume(panelBox, scintMat, "Panel2");
        fPanel3 = new G4LogicalVolume(panelBox, scintMat, "Panel3");
    }

    // Mode 2: separate light-guide volumes next to the scintillator panels.
    G4LogicalVolume* lightGuideLV[4] = {nullptr, nullptr, nullptr, nullptr};
    if (fGeometryMode == 2) {
        for (G4int i = 0; i < 4; ++i) {
            lightGuideLV[i] = new G4LogicalVolume(lightGuideSolid, lightGuideMat,
                                                  "LightGuide" + std::to_string(i));
        }
    }


    // PMT windows, mode 2 with -p only. G4Tubs axis is local z, rotated to x.
    G4Tubs* windowSolid = nullptr;
    G4RotationMatrix windowRot;
    windowRot.rotateY(90.*deg);
    const G4double windowX = kLGSide*(panelHX + 2*kLGHalfLength + kPMTWindowHalfLength);
    for (G4int i = 0; i < 4; ++i) fWindowLV[i] = nullptr;
    if (withOptics) {
        windowSolid = new G4Tubs("PMTWindow", 0., kPMTCathodeRadius, kPMTWindowHalfLength,
                                 0., 360.*deg);
        for (G4int i = 0; i < 4; ++i) {
            fWindowLV[i] = new G4LogicalVolume(windowSolid, windowMat,
                                               "PMTWindow" + std::to_string(i));
        }
    }


    G4Box* topPanelBox = new G4Box("TopPanelBox", pairBoxHX, pairBoxHY, topBoxHZ);
    G4Box* bottomPanelBox = new G4Box("BottomPanelBox", pairBoxHX, pairBoxHY, botBoxHZ);

    G4LogicalVolume* topPanelBoxLV = new G4LogicalVolume(topPanelBox, vacuum, "TopPanelBox");
    G4LogicalVolume* bottomPanelBoxLV = new G4LogicalVolume(bottomPanelBox, vacuum, "BottomPanelBox");

    G4VPhysicalVolume* topPanelBoxPV = new G4PVPlacement(
        nullptr,
        G4ThreeVector(0, 0, panel_boxUpPivotZ), // Top panel box, located at the top of inner volume
        topPanelBoxLV,
        "TopPanelBox",
        innerVolumeLV,
        false, 0, true
    );
    G4VPhysicalVolume* bottomPanelBoxPV = new G4PVPlacement(
        nullptr,
        G4ThreeVector(0, 0, -panel_boxDownPivotZ), // Bottom panel box, located at the bottom of inner volume
        bottomPanelBoxLV,
        "BottomPanelBox",
        innerVolumeLV,
        false, 0, true
    );


    // Placing panels (and/or light guides) inside their respective boxes.
    // zc is the height of the panel mid-plane inside the pair box.
    // The physical volumes are kept for the optical border surfaces.
    G4LogicalVolume* panelLV[4] = {fPanel0, fPanel1, fPanel2, fPanel3};
    G4VPhysicalVolume* panelPV[4]  = {nullptr, nullptr, nullptr, nullptr};
    G4VPhysicalVolume* guidePV[4]  = {nullptr, nullptr, nullptr, nullptr};
    G4VPhysicalVolume* windowPV[4] = {nullptr, nullptr, nullptr, nullptr};
    auto placeModule = [&](G4int i, G4LogicalVolume* pairLV, G4double zc) {
        const G4String idx = std::to_string(i);
        // Side of guide and PMT window of this module (rotation test: one module flipped)
        const G4int side = (i == fFlippedGuide) ? -kLGSide : kLGSide;
        G4RotationMatrix lightGuideRot;
        lightGuideRot.rotateZ(90.*deg);
        lightGuideRot.rotateY(side*90.*deg);
        const G4double lightGuideX = side*(panelHX + kLGHalfLength);
        const G4double moduleWindowX = side*(panelHX + 2*kLGHalfLength + kPMTWindowHalfLength);
        if (withScint) {
            panelPV[i] = new G4PVPlacement(nullptr, G4ThreeVector(0, 0, zc),
                              panelLV[i], "Panel" + idx, pairLV, false, 0, true);
        }
        if (withLightGuide) {
            G4LogicalVolume* lv    = (fGeometryMode == 1) ? panelLV[i] : lightGuideLV[i];
            const G4String  pvName = (fGeometryMode == 1) ? "Panel" + idx : "LightGuide" + idx;
            guidePV[i] = new G4PVPlacement(G4Transform3D(lightGuideRot, G4ThreeVector(lightGuideX, 0, zc)),
                              lv, pvName, pairLV, false, 0, true);
        }
        if (withOptics) {
            windowPV[i] = new G4PVPlacement(G4Transform3D(windowRot, G4ThreeVector(moduleWindowX, 0, zc)),
                              fWindowLV[i], "PMTWindow" + idx, pairLV, false, 0, true);
        }
    };

    placeModule(0, topPanelBoxLV,     (separation_up_2/2 + panelHZ));     // Panel_0, upper panel of the top box
    placeModule(1, topPanelBoxLV,    -(separation_up_2/2 + panelHZ));     // Panel_1, lower panel of the top box
    placeModule(2, bottomPanelBoxLV,  (separation_down_2/2 + panelHZ));   // Panel_2, upper panel of the bottom box
    placeModule(3, bottomPanelBoxLV, -(separation_down_2/2 + panelHZ));   // Panel_3, lower panel of the bottom box

    if (withOptics) BuildOpticalSurfaces(panelLV, lightGuideLV, panelPV, guidePV, windowPV);

    if (fLeadEnabled && withLightGuide) {
        // The enlarged pair boxes reach above Panel 2, where the lead would sit.
        G4Exception("DetectorConstruction::Construct()", "LeadWithLightGuide", JustWarning,
                    "Lead shielding is not placed in the light-guide geometry modes.");
    }
    if (fLeadEnabled && !withLightGuide) {
        const G4double leadHX = 0.150 * m;   // 30 cm
        const G4double leadHY = 0.150 * m;   // 30 cm
        const G4double leadHZ = fLeadThickness / 2.;

        // z of Panel 2's top surface, relative to the pivot
        const G4double p2Top = -(panel_boxDownPivotZ - separation_down_2/2 - 2*panelHZ);

        G4Box* leadBox = new G4Box("LeadShielding", leadHX, leadHY, leadHZ);
        G4LogicalVolume* leadLV = new G4LogicalVolume(leadBox, lead, "LeadShielding");
        G4VisAttributes* yellowVis = new G4VisAttributes(G4Colour::Yellow()); // yellow
        leadLV->SetVisAttributes(yellowVis);

        new G4PVPlacement(
            nullptr,
            G4ThreeVector(0, 0, p2Top + fLeadGap + leadHZ),
            leadLV, "LeadShielding", innerVolumeLV, false, 0, true);

        G4cout << "\n =============================================" << G4endl;
        G4cout << "  ----------- Lead Shielding -------------------" << G4endl;
        G4cout << "  lead shielding : " << fLeadThickness / cm << " cm, "
               << "top surface at " << (p2Top + fLeadGap + 2*leadHZ) / m << " m" << G4endl;
        G4cout << "\n =============================================" << G4endl;
    }

    // Printed so the geometry can be checked against the tape measure without
    // having to look at the ntuples first.
    G4cout << "\n =============================================" << G4endl;
    G4cout << "  ----------- Detector Geometry ----------------" << G4endl;
    G4cout << "\n--- Panel geometry (relative to pivot) ---" << G4endl;
    G4cout << "  P0 centre : " << (panel_boxUpPivotZ + separation_up_2/2 + panelHZ)/cm << " cm" << G4endl;
    G4cout << "  P1 centre : " << (panel_boxUpPivotZ - separation_up_2/2 - panelHZ)/cm << " cm" << G4endl;
    G4cout << "  P2 centre : " << -(panel_boxDownPivotZ - separation_down_2/2 - panelHZ)/cm << " cm" << G4endl;
    G4cout << "  P3 centre : " << -(panel_boxDownPivotZ + separation_down_2/2 + panelHZ)/cm << " cm" << G4endl;
    G4cout << "  outer centre-to-centre : "
        << (panel_boxUpPivotZ + separation_up_2/2 + panelHZ
            + panel_boxDownPivotZ + separation_down_2/2 + panelHZ)/cm << " cm" << G4endl;
    G4cout << "  outer envelope         : "
        << (panel_boxUpPivotZ + separation_up_2/2 + 2*panelHZ
            + panel_boxDownPivotZ + separation_down_2/2 + 2*panelHZ)/cm << " cm" << G4endl;
    if (withLightGuide) {
        G4cout << "\n--- Light guides (PMMA) ---" << G4endl;
        G4cout << "  x range   : " << (kLGSide*panelHX)/cm << " to "
               << (kLGSide*(panelHX + 2*kLGHalfLength))/cm << " cm" << G4endl;
        G4cout << "  width     : " << 2*kLGHalfWidthIn/cm << " -> " << 2*kLGHalfWidthOut/cm << " cm" << G4endl;
        G4cout << "  thickness : " << 2*kLGHalfThickIn/cm << " -> " << 2*kLGHalfThickOut/cm << " cm" << G4endl;
    }
    if (withOptics) {
        G4cout << "\n--- Optical mode ---" << G4endl;
        G4cout << "  PMT window: r = " << kPMTCathodeRadius/mm << " mm, length "
               << 2*kPMTWindowHalfLength/mm << " mm, centre x = " << windowX/mm << " mm" << G4endl;
        G4cout << "  wrapping  : " << fWrapping << G4endl;
        G4cout << "  PMMA grade: " << fPmmaGrade << G4endl;
        G4cout << "  photon energy range: " << kPhotonEnergyMin/eV << " - "
               << kPhotonEnergyMax/eV << " eV (" << kLambdaMin/nm << " - " << kLambdaMax/nm << " nm)" << G4endl;
    }
    G4cout << "\n =============================================" << G4endl;


    // ── Roof / floor slabs ───────────────────────────────────────
    // Concrete above the telescope: one slab (the roof of the single-storey
    // laboratory) or fRoofSlabs slabs (the floors above the room in the
    // high-rise building). fRoofHeight is the height of the LOWER surface of
    // the lowest slab above the floor, which is z = 0 in this geometry; the
    // slabs are stacked upwards with a clear gap fRoofSlabGap.
    //
    // For a horizontal slab that is wide compared with the acceptance and a
    // flux that is uniform over the generation plane, neither the height of
    // the slabs nor the gaps change the rate: a muon crosses t/cos(theta) of
    // concrete wherever the slab is, and a lateral shift of the track by
    // multiple scattering is compensated by the tracks shifted in from the
    // neighbourhood. The gaps can therefore be set to zero, which keeps the
    // generation plane low and small. The generation surface must lie above
    // the highest slab (checked for every vertex in PrimaryGeneratorAction).
    fRoofTop    = -DBL_MAX;
    fOverburden = 0.;
    if (fRoofEnabled) {
        const G4double roofHalfXY = fRoofHalfSize;   // must cover the acceptance at the slab heights
        const G4double roofHalfZ  = fRoofThickness / 2.;
        fRoofTop    = fRoofHeight + fRoofSlabs*fRoofThickness + (fRoofSlabs - 1)*fRoofSlabGap;
        fOverburden = fRoofSlabs * fRoofThickness * concrete->GetDensity();

        if (fRoofTop > worldHalfSize || roofHalfXY > worldHalfSize) {
            G4ExceptionDescription msg;
            msg << "The slabs (top at " << fRoofTop/m << " m, half size " << roofHalfXY/m
                << " m) do not fit into the world (half size " << worldHalfSize/m << " m).";
            G4Exception("DetectorConstruction::Construct()", "RoofOutsideWorld",
                        FatalException, msg);
        }

        G4Box* roofBox = new G4Box("HallRoof", roofHalfXY, roofHalfXY, roofHalfZ);
        G4LogicalVolume* roofLV = new G4LogicalVolume(roofBox, concrete, "HallRoof");

        for (G4int i = 0; i < fRoofSlabs; ++i) {
            const G4double zLow = fRoofHeight + i*(fRoofThickness + fRoofSlabGap);
            new G4PVPlacement(
                nullptr,
                G4ThreeVector(0, 0, zLow + roofHalfZ),
                roofLV,
                "HallRoof",
                worldLV,
                false, i, true      // copy number = slab index, 0 = lowest
            );
        }

        G4VisAttributes* cyanVis = new G4VisAttributes(G4Colour::Cyan()); // cyan
        roofLV->SetVisAttributes(cyanVis);

        G4cout << "\n--- Concrete above the telescope (" << concrete->GetName() << ", "
               << concrete->GetDensity()/(g/cm3) << " g/cm3) ---" << G4endl;
        G4cout << "  slabs          : " << fRoofSlabs << " x " << fRoofThickness/cm << " cm, gap "
               << fRoofSlabGap/cm << " cm, half size " << roofHalfXY/m << " m" << G4endl;
        G4cout << "  lower surface  : " << fRoofHeight/m << " m,  upper surface: "
               << fRoofTop/m << " m" << G4endl;
        G4cout << "  areal density  : " << fOverburden/(g/cm2) << " g/cm2 (vertical)" << G4endl;
        G4cout << "  -> the generation surface must lie above z = " << fRoofTop/m << " m" << G4endl;
    }

        // ── Visualisation ────────────────────────────────────────────
    auto blueColor = G4Colour(0.0, 0.0, 1.0, 0.4); // blue
    auto redColor = G4Colour(1.0, 0.0, 0.0, 0.4);  // red
    G4VisAttributes* blueVis = new G4VisAttributes(blueColor); // blue
    G4VisAttributes* redVis = new G4VisAttributes(redColor); // red
    G4VisAttributes* greenVis = new G4VisAttributes(G4Colour::Green()); // green
    G4VisAttributes* yellowVis = new G4VisAttributes(G4Colour::Yellow()); // yellow
    G4VisAttributes* cyanVis = new G4VisAttributes(G4Colour::Cyan()); // cyan



    worldLV->SetVisAttributes(G4VisAttributes::GetInvisible());
    innerVolumeLV->SetVisAttributes(G4VisAttributes::GetInvisible());
    telescopeWorldLV->SetVisAttributes(G4VisAttributes::GetInvisible());
    topPanelBoxLV->SetVisAttributes(greenVis);
    bottomPanelBoxLV->SetVisAttributes(greenVis);

    fPanel0->SetVisAttributes(redVis);
    fPanel1->SetVisAttributes(redVis);
    fPanel2->SetVisAttributes(redVis);
    fPanel3->SetVisAttributes(redVis);
    for (G4int i = 0; i < 4; ++i) {
        if (lightGuideLV[i]) lightGuideLV[i]->SetVisAttributes(blueVis);
        if (fWindowLV[i])    fWindowLV[i]->SetVisAttributes(yellowVis);
    }


    return worldPV;
}

void DetectorConstruction::ConstructSDandField()
{
    auto sdManager = G4SDManager::GetSDMpointer();
    G4LogicalVolume* panels[4] = {fPanel0, fPanel1, fPanel2, fPanel3};

    for (G4int i = 0; i < 4; ++i) {
        const G4String sdName  = "Panel" + std::to_string(i) + "SD";
        const G4String colName = "Panel" + std::to_string(i) + "HitsCollection";

        // After /detector/rebuild this method runs again with freshly created
        // logical volumes. Registering a second detector under the same name
        // would leave the new panels without a working hits collection, so an
        // existing detector is reused when one is already registered.
        auto sd = sdManager->FindSensitiveDetector(sdName, false);
        if (!sd) {
            sd = new SensitiveDetector(sdName, colName);
            sdManager->AddNewDetector(sd);
        }
        SetSensitiveDetector(panels[i], sd);

        // Mode 2: the light guide of module i reports into the same hit
        // collection as panel i. The newest volume of that name is taken, so
        // this also works after /detector/rebuild.
        if (fGeometryMode == 2) {
            auto lgLV = G4LogicalVolumeStore::GetInstance()->GetVolume(
                "LightGuide" + std::to_string(i), false, true);
            if (lgLV) SetSensitiveDetector(lgLV, sd);
        }
    }

    // Optical mode: one photon detector for the four PMT windows. It is
    // registered in every geometry mode so that its (possibly empty) hits
    // collection exists whenever -p is set.
    if (fOptical) {
        auto photonSD = sdManager->FindSensitiveDetector("PhotonSD", false);
        if (!photonSD) {
            photonSD = new PhotonDetector("PhotonSD", "PhotonHitsCollection");
            sdManager->AddNewDetector(photonSD);
        }
        for (G4int i = 0; i < 4; ++i) {
            if (fWindowLV[i]) SetSensitiveDetector(fWindowLV[i], photonSD);
        }
    }
}


// ── Optical materials ─────────────────────────────────────────────
// EJ-200 (Eljen data sheet): n = 1.58, bulk attenuation length 380 cm,
// 10000 photons/MeV, emission peak 425 nm, rise 0.9 ns, decay 2.1 ns.
// PMMA: n = 1.49; the absorption depends on the (unknown) grade, see
// SetPmmaAbsorption(). PMT window: borosilicate, n = 1.49.
void DetectorConstruction::BuildOpticalMaterials(G4Material* scint, G4Material* guide,
                                                 G4Material* window)
{
    const std::vector<G4double> range = {kPhotonEnergyMin, kPhotonEnergyMax};

    if (!scint->GetMaterialPropertiesTable()) {
        auto mpt = new G4MaterialPropertiesTable();
        mpt->AddProperty("RINDEX",    range, {1.58, 1.58});
        mpt->AddProperty("ABSLENGTH", range, {380.*cm, 380.*cm});

        // Emission spectrum, relative intensity. Approximate: digitised by
        // eye from the EJ-200 data sheet, to be replaced by a proper
        // digitisation.
        const Spectrum em = FromWavelengths(
            {380, 390, 400, 410, 420, 425, 430, 440, 450, 460, 470, 480, 490, 500, 520, 550},
            {0.00, 0.03, 0.18, 0.55, 0.92, 1.00, 0.97, 0.80, 0.58, 0.40, 0.27, 0.17, 0.10, 0.06, 0.02, 0.00});
        mpt->AddProperty("SCINTILLATIONCOMPONENT1", em.energy, em.value);
        mpt->AddConstProperty("SCINTILLATIONYIELD", 10000./MeV);
        mpt->AddConstProperty("RESOLUTIONSCALE", 1.0);
        mpt->AddConstProperty("SCINTILLATIONYIELD1", 1.0);
        mpt->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 2.1*ns);
        mpt->AddConstProperty("SCINTILLATIONRISETIME1", 0.9*ns);
        scint->SetMaterialPropertiesTable(mpt);

        // Birks constant, typical value for polyvinyltoluene. Used by
        // G4EmSaturation, which G4OpticalPhysics attaches to G4Scintillation.
        scint->GetIonisation()->SetBirksConstant(0.126*mm/MeV);
    }

    if (!guide->GetMaterialPropertiesTable()) {
        // Eljen / Scionix cast PMMA: n = 1.502 (436 nm), 1.492 (589 nm);
        // the end points follow the Cauchy fit n = 1.480 + 4200 nm^2 / lambda^2
        // through these two values.
        auto mpt = new G4MaterialPropertiesTable();
        const Spectrum n = FromWavelengths({300, 436, 589, 650}, {1.527, 1.502, 1.492, 1.490});
        mpt->AddProperty("RINDEX", n.energy, n.value);
        guide->SetMaterialPropertiesTable(mpt);
    }
    // The grade can change between rebuilds. Only ABSLENGTH is replaced; it
    // is read by G4OpAbsorption at every step, unlike RINDEX, from which the
    // Cherenkov and scintillation tables are built once at initialisation.
    SetPmmaAbsorption(guide->GetMaterialPropertiesTable());

    if (!window->GetMaterialPropertiesTable()) {
        auto mpt = new G4MaterialPropertiesTable();
        mpt->AddProperty("RINDEX",    range, {1.49, 1.49});
        mpt->AddProperty("ABSLENGTH", range, {100.*cm, 100.*cm});
        window->SetMaterialPropertiesTable(mpt);
    }
}

// Bulk absorption length of the light guide, from the transmission curves
// of the Eljen / Scionix "Light guides and acrylic plastic" data sheet
// (cast acrylic, commercial grade vs. UVT, reference air), converted with
// L = -d / ln(T / (1-R)^2), (1-R)^2 = 0.923 for n = 1.49. The curves were
// digitised from the plot, so the values are good to about 20 %. Above
// 420 nm the plateau (91 %) is within the reading error of the Fresnel
// limit; there L is capped at 200 cm. Which grade the Scionix guides are
// made of is not known; Scionix lists both (PMMA / BC-802, UVT / BC-800).
//   standard : 19 mm commercial sheet, opaque below 380 nm
//   uvt      : 105 mm UVT sheet above 330 nm, 18 mm UVT sheet below
void DetectorConstruction::SetPmmaAbsorption(G4MaterialPropertiesTable* mpt)
{
    Spectrum abs;
    if (fPmmaGrade == "uvt") {
        abs = FromWavelengths({300, 310, 320, 330, 340, 350, 360, 370, 380, 390, 400, 650},
                              {2.*cm, 6.*cm, 10.*cm, 15.*cm, 22.*cm, 35.*cm, 58.*cm, 82.*cm,
                               120.*cm, 170.*cm, 200.*cm, 200.*cm});
    } else {
        if (fPmmaGrade != "standard") {
            G4Exception("DetectorConstruction::SetPmmaAbsorption()", "PmmaGrade", JustWarning,
                        ("Unknown PMMA grade '" + fPmmaGrade + "', using standard.").c_str());
        }
        abs = FromWavelengths({300, 370, 380, 385, 390, 395, 400, 405, 410, 420, 440, 460, 650},
                              {0.5*mm, 0.5*mm, 1.*mm, 6.*mm, 1.6*cm, 4.*cm, 10.*cm, 22.*cm,
                               46.*cm, 66.*cm, 150.*cm, 200.*cm, 200.*cm});
    }
    if (mpt->GetProperty("ABSLENGTH")) mpt->RemoveProperty("ABSLENGTH");
    mpt->AddProperty("ABSLENGTH", abs.energy, abs.value);
}


// ── Optical surfaces ──────────────────────────────────────────────
// The modules are covered with black tape; what is under the tape is not
// known. The wrapping is a skin surface on each panel and light guide with
// a back-painted finish: the photon is reflected or absorbed at the
// surface and never enters the vacuum of the pair box, so the vacuum needs
// no RINDEX. The glued faces get border surfaces, which take precedence
// over the skin surfaces.
void DetectorConstruction::BuildOpticalSurfaces(G4LogicalVolume* panelLV[4],
                                                G4LogicalVolume* guideLV[4],
                                                G4VPhysicalVolume* panelPV[4],
                                                G4VPhysicalVolume* guidePV[4],
                                                G4VPhysicalVolume* windowPV[4])
{
    const std::vector<G4double> range = {kPhotonEnergyMin, kPhotonEnergyMax};

    // Wrapping: unified model, dielectric_dielectric with an air gap
    // (surface RINDEX 1.0) in front of a reflector of reflectivity R.
    // With a back-painted finish G4OpBoundaryProcess::DielectricDielectric()
    // calls ChooseReflection() for every reflection at the dielectric
    // surface, total internal reflection included; without the unified
    // constants the reflection would be Lambertian. The polished surface
    // therefore gets SPECULARSPIKECONSTANT = 1 (specular about the average
    // normal), the ground one SPECULARLOBECONSTANT = 1 (specular about a
    // micro-facet normal, spread sigma_alpha). The reflection off the paint
    // itself is specular for polishedbackpainted and Lambertian for
    // groundbackpainted.
    G4OpticalSurfaceFinish finish = polishedbackpainted;
    G4double reflectivity = 0.90;   // aluminium foil / aluminised Mylar
    G4double sigmaAlpha   = 0.0;
    G4double spike = 1.0, lobe = 0.0;
    if (fWrapping == "tyvek") {
        finish = groundbackpainted; reflectivity = 0.95; sigmaAlpha = 0.1;   // rad
        spike = 0.0; lobe = 1.0;
    } else if (fWrapping == "black") {
        reflectivity = 0.05;   // black tape only; total internal reflection survives
    } else if (fWrapping != "alfoil") {
        G4Exception("DetectorConstruction::BuildOpticalSurfaces()", "Wrapping", JustWarning,
                    ("Unknown wrapping '" + fWrapping + "', using alfoil.").c_str());
    }
    auto wrapSurface = new G4OpticalSurface("Wrapping", unified, finish, dielectric_dielectric);
    wrapSurface->SetSigmaAlpha(sigmaAlpha);
    auto wrapMPT = new G4MaterialPropertiesTable();
    wrapMPT->AddProperty("RINDEX",                range, {1.0, 1.0});
    wrapMPT->AddProperty("REFLECTIVITY",          range, {reflectivity, reflectivity});
    wrapMPT->AddProperty("SPECULARSPIKECONSTANT", range, {spike, spike});
    wrapMPT->AddProperty("SPECULARLOBECONSTANT",  range, {lobe, lobe});
    wrapMPT->AddProperty("BACKSCATTERCONSTANT",   range, {0.0, 0.0});
    wrapSurface->SetMaterialPropertiesTable(wrapMPT);

    // Optical cement (panel-guide) and grease (guide-window): polished
    // dielectric interfaces, Fresnel from the bulk indices only.
    auto cement = new G4OpticalSurface("OpticalCement", unified, polished, dielectric_dielectric);
    auto grease = new G4OpticalSurface("OpticalGrease", unified, polished, dielectric_dielectric);

    for (G4int i = 0; i < 4; ++i) {
        const G4String idx = std::to_string(i);
        new G4LogicalSkinSurface("PanelWrap" + idx, panelLV[i], wrapSurface);
        new G4LogicalSkinSurface("GuideWrap" + idx, guideLV[i], wrapSurface);

        new G4LogicalBorderSurface("PanelToGuide" + idx,  panelPV[i],  guidePV[i],  cement);
        new G4LogicalBorderSurface("GuideToPanel" + idx,  guidePV[i],  panelPV[i],  cement);
        new G4LogicalBorderSurface("GuideToWindow" + idx, guidePV[i],  windowPV[i], grease);
        new G4LogicalBorderSurface("WindowToGuide" + idx, windowPV[i], guidePV[i], grease);
    }
}