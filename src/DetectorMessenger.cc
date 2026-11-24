#include "DetectorMessenger.hh"
#include "DetectorConstruction.hh"
#include "G4RunManager.hh"

DetectorMessenger::DetectorMessenger(DetectorConstruction* detectorConstruction)
    : G4UImessenger(), fDetectorConstruction(detectorConstruction)
{
    // /detector/ directory
    fDetectorDir = new G4UIdirectory("/detector/");
    fDetectorDir->SetGuidance("Detector geometry control commands");
    
    // /detector/zenith command
    fZenithAngleCmd = new G4UIcmdWithADoubleAndUnit("/detector/zenith", this);
    fZenithAngleCmd->SetGuidance("Set detector tilt angle in zenith direction");
    fZenithAngleCmd->SetGuidance("Usage: /detector/zenith <angle> <unit>");
    fZenithAngleCmd->SetParameterName("angle", false); // parameter required
    fZenithAngleCmd->SetUnitCategory("Angle");
    fZenithAngleCmd->SetDefaultValue(0.0);
    fZenithAngleCmd->SetRange("angle >= -90. && angle <= 90.");
    
    // /detector/azimuth command
    fAzimuthAngleCmd = new G4UIcmdWithADoubleAndUnit("/detector/azimuth", this);
    fAzimuthAngleCmd->SetGuidance("Set detector tilt angle in azimuth direction");
    fAzimuthAngleCmd->SetGuidance("Usage: /detector/azimuth <angle> <unit>");
    fAzimuthAngleCmd->SetParameterName("angle", false);
    fAzimuthAngleCmd->SetUnitCategory("Angle");
    fAzimuthAngleCmd->SetDefaultValue(0.0);
    fAzimuthAngleCmd->SetRange("angle >= 0. && angle < 360.");

    // Hall roof
    // The telescope stands on the floor of an open crane hall; the roof is the
    // only material overhead. These commands allow the roof to be switched on
    // and off without rebuilding the application.
    fRoofCmd = new G4UIcmdWithABool("/detector/roof", this);
    fRoofCmd->SetGuidance("Enable or disable the concrete hall roof.");
    fRoofCmd->SetParameterName("enable", false);
    fRoofCmd->SetDefaultValue(false);

    fRoofThicknessCmd = new G4UIcmdWithADoubleAndUnit("/detector/roofThickness", this);
    fRoofThicknessCmd->SetGuidance("Thickness of one concrete slab (G4_CONCRETE, 2.3 g/cm3).");
    fRoofThicknessCmd->SetParameterName("thickness", false);
    fRoofThicknessCmd->SetUnitCategory("Length");
    fRoofThicknessCmd->SetDefaultUnit("cm");
    fRoofThicknessCmd->SetRange("thickness > 0.");

    fRoofHeightCmd = new G4UIcmdWithADoubleAndUnit("/detector/roofHeight", this);
    fRoofHeightCmd->SetGuidance("Height of the LOWER surface of the lowest slab above the floor (z = 0).");
    fRoofHeightCmd->SetParameterName("height", false);
    fRoofHeightCmd->SetUnitCategory("Length");
    fRoofHeightCmd->SetDefaultUnit("m");
    fRoofHeightCmd->SetRange("height > 0.");

    fRoofHalfSizeCmd = new G4UIcmdWithADoubleAndUnit("/detector/roofHalfSize", this);
    fRoofHalfSizeCmd->SetGuidance("Half side length of the (square) slabs in x and y.");
    fRoofHalfSizeCmd->SetParameterName("width", false);
    fRoofHalfSizeCmd->SetUnitCategory("Length");
    fRoofHalfSizeCmd->SetDefaultUnit("m");
    fRoofHalfSizeCmd->SetRange("width > 0.");

    // Several floors above the room (high-rise building). The slabs are
    // stacked upwards from /detector/roofHeight; the generation surface has
    // to lie above the highest one.
    fRoofSlabsCmd = new G4UIcmdWithAnInteger("/detector/roofSlabs", this);
    fRoofSlabsCmd->SetGuidance("Number of concrete slabs (floors) above the telescope (default 1).");
    fRoofSlabsCmd->SetParameterName("n", false);
    fRoofSlabsCmd->SetRange("n >= 1");

    fRoofSlabGapCmd = new G4UIcmdWithADoubleAndUnit("/detector/roofSlabGap", this);
    fRoofSlabGapCmd->SetGuidance("Clear gap between neighbouring slabs (default 0).");
    fRoofSlabGapCmd->SetParameterName("gap", false);
    fRoofSlabGapCmd->SetUnitCategory("Length");
    fRoofSlabGapCmd->SetDefaultUnit("m");
    fRoofSlabGapCmd->SetRange("gap >= 0.");


    // Lead shielding
    fLeadCmd = new G4UIcmdWithABool("/detector/lead", this);
    fLeadCmd->SetGuidance("Enable or disable the lead shielding.");
    fLeadCmd->SetParameterName("enable", false);
    fLeadCmd->SetDefaultValue(false);

    fLeadThicknessCmd = new G4UIcmdWithADoubleAndUnit("/detector/leadThickness", this);
    fLeadThicknessCmd->SetGuidance("Thickness of the lead shielding.");
    fLeadThicknessCmd->SetParameterName("thickness", false);
    fLeadThicknessCmd->SetUnitCategory("Length");
    fLeadThicknessCmd->SetDefaultUnit("cm");
    fLeadThicknessCmd->SetRange("thickness > 0.");

    fLeadGapCmd = new G4UIcmdWithADoubleAndUnit("/detector/leadGap", this);
    fLeadGapCmd->SetGuidance("Gap between the lead shielding and the detector.");
    fLeadGapCmd->SetParameterName("gap", false);
    fLeadGapCmd->SetUnitCategory("Length");
    fLeadGapCmd->SetDefaultUnit("cm");
    fLeadGapCmd->SetRange("gap > 0.");

    // World Material
    fWorldMaterialIsAirCmd = new G4UIcmdWithABool("/detector/worldMaterialIsAir", this);
    fWorldMaterialIsAirCmd->SetGuidance("Set the world material to air (true) or vacuum (false).");
    fWorldMaterialIsAirCmd->SetParameterName("isAir", false);
    fWorldMaterialIsAirCmd->SetDefaultValue(false);

    // /detector/rebuild command
    fRebuildCmd = new G4UIcmdWithoutParameter("/detector/rebuild", this);
    fRebuildCmd->SetGuidance("Rebuild detector geometry after angle changes");
    fRebuildCmd->SetGuidance("WARNING: This will reset the simulation!");

    // /detector/geometryMode command
    fGeometryModeCmd = new G4UIcmdWithAnInteger("/detector/geometryMode", this);
    fGeometryModeCmd->SetGuidance("Select the panel geometry:");
    fGeometryModeCmd->SetGuidance("  0 : scintillator panels only (default)");
    fGeometryModeCmd->SetGuidance("  1 : light guides only, named Panel0..3");
    fGeometryModeCmd->SetGuidance("  2 : scintillator panels + light guides");
    fGeometryModeCmd->SetGuidance("After /run/initialize, follow with /detector/rebuild.");
    fGeometryModeCmd->SetParameterName("mode", false);
    fGeometryModeCmd->SetRange("mode >= 0 && mode <= 2");
    fGeometryModeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fFlipGuideCmd = new G4UIcmdWithAnInteger("/detector/flipLightGuide", this);
    fFlipGuideCmd->SetGuidance("Panel (0 = top ... 3) whose light guide and PMT are put on the other side; -1 = none.");
    fFlipGuideCmd->SetParameterName("panel", false);
    fFlipGuideCmd->SetRange("panel >= -1 && panel <= 3");
    fFlipGuideCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // Optical mode (-p)
    // The wrapping is applied as a skin surface in Construct(), and the PMMA
    // grade replaces the ABSLENGTH entry of the light-guide material in
    // Construct(). Both therefore take effect after /detector/rebuild.
    fOpticsDir = new G4UIdirectory("/detector/optics/");
    fOpticsDir->SetGuidance("Optical light-collection settings (-p).");

    fWrappingCmd = new G4UIcmdWithAString("/detector/optics/wrapping", this);
    fWrappingCmd->SetGuidance("Reflector under the black tape of the panels and light guides:");
    fWrappingCmd->SetGuidance("  alfoil : specular, R = 0.90, air gap (default)");
    fWrappingCmd->SetGuidance("  tyvek  : diffuse, R = 0.95, air gap");
    fWrappingCmd->SetGuidance("  black  : R = 0.05, only total internal reflection survives");
    fWrappingCmd->SetGuidance("Follow with /detector/rebuild.");
    fWrappingCmd->SetParameterName("name", false);
    fWrappingCmd->SetCandidates("alfoil tyvek black");
    fWrappingCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fPmmaGradeCmd = new G4UIcmdWithAString("/detector/optics/pmmaGrade", this);
    fPmmaGradeCmd->SetGuidance("Bulk absorption of the PMMA light guide:");
    fPmmaGradeCmd->SetGuidance("  standard : UV-absorbing acrylic, cut-off near 380 nm (default)");
    fPmmaGradeCmd->SetGuidance("  uvt      : UV-transmitting acrylic");
    fPmmaGradeCmd->SetGuidance("Follow with /detector/rebuild.");
    fPmmaGradeCmd->SetParameterName("name", false);
    fPmmaGradeCmd->SetCandidates("standard uvt");
    fPmmaGradeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fStorePhotonsCmd = new G4UIcmdWithABool("/detector/optics/storePhotons", this);
    fStorePhotonsCmd->SetGuidance("Write one row per detected photon to the PhotonHits ntuple (large).");
    fStorePhotonsCmd->SetParameterName("store", false);
    fStorePhotonsCmd->SetDefaultValue(false);
    fStorePhotonsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

DetectorMessenger::~DetectorMessenger()
{
    delete fZenithAngleCmd;
    delete fAzimuthAngleCmd;
    delete fRebuildCmd;
    delete fDetectorDir;
    delete fRoofCmd;
    delete fRoofThicknessCmd;
    delete fRoofHeightCmd;
    delete fRoofHalfSizeCmd;
    delete fRoofSlabsCmd;
    delete fRoofSlabGapCmd;
    delete fLeadCmd;
    delete fLeadThicknessCmd;
    delete fLeadGapCmd;
    delete fWorldMaterialIsAirCmd;
    delete fGeometryModeCmd;
    delete fFlipGuideCmd;
    delete fWrappingCmd;
    delete fPmmaGradeCmd;
    delete fStorePhotonsCmd;
    delete fOpticsDir;
}

void DetectorMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if (command == fZenithAngleCmd) {
        G4double angle = fZenithAngleCmd->GetNewDoubleValue(newValue);
        fDetectorConstruction->SetZenithAngle(angle);
        G4cout << "###################################################"<< G4endl;
        G4cout << ">>> Zenith angle set to " << angle / deg << " deg" << G4endl;
    }
    else if (command == fAzimuthAngleCmd) {
        G4double angle = fAzimuthAngleCmd->GetNewDoubleValue(newValue);
        fDetectorConstruction->SetAzimuthAngle(angle);
        G4cout << "###################################################"<< G4endl;
        G4cout << ">>> Azimuth angle set to " << angle / deg << " deg" << G4endl;
    }
    else if (command == fRoofCmd) {
        G4bool enable = fRoofCmd->GetNewBoolValue(newValue);
        fDetectorConstruction->SetRoofEnabled(enable);
        G4cout << "###################################################" << G4endl;
        G4cout << ">>> Hall roof " << (enable ? "enabled" : "disabled") << G4endl;
    }
    else if (command == fRoofThicknessCmd) {
        G4double t = fRoofThicknessCmd->GetNewDoubleValue(newValue);
        fDetectorConstruction->SetRoofThickness(t);
        G4cout << ">>> Roof thickness set to " << t / cm << " cm" << G4endl;
    }
    else if (command == fRoofHeightCmd) {
        G4double h = fRoofHeightCmd->GetNewDoubleValue(newValue);
        fDetectorConstruction->SetRoofHeight(h);
        G4cout << ">>> Roof lower surface set to " << h / m << " m" << G4endl;
    }
    else if (command == fRoofHalfSizeCmd) {
        G4double s = fRoofHalfSizeCmd->GetNewDoubleValue(newValue);
        fDetectorConstruction->SetRoofHalfSize(s);
        G4cout << ">>> Roof half size set to " << s / m << " m" << G4endl;
    }
    else if (command == fRoofSlabsCmd) {
        G4int n = fRoofSlabsCmd->GetNewIntValue(newValue);
        fDetectorConstruction->SetRoofSlabs(n);
        G4cout << ">>> Number of concrete slabs set to " << n << G4endl;
    }
    else if (command == fRoofSlabGapCmd) {
        G4double g = fRoofSlabGapCmd->GetNewDoubleValue(newValue);
        fDetectorConstruction->SetRoofSlabGap(g);
        G4cout << ">>> Gap between slabs set to " << g / m << " m" << G4endl;
    }
    else if (command == fLeadCmd) {
        G4bool enable = fLeadCmd->GetNewBoolValue(newValue);
        fDetectorConstruction->SetLeadEnabled(enable);
        G4cout << "###################################################" << G4endl;
        G4cout << ">>> Lead shielding " << (enable ? "enabled" : "disabled") << G4endl;
    }
    else if (command == fLeadThicknessCmd) {
        G4double t = fLeadThicknessCmd->GetNewDoubleValue(newValue);
        fDetectorConstruction->SetLeadThickness(t);
        G4cout << ">>> Lead thickness set to " << t / cm << " cm" << G4endl;
    }
    else if (command == fLeadGapCmd) {
        G4double g = fLeadGapCmd->GetNewDoubleValue(newValue);
        fDetectorConstruction->SetLeadGap(g);
        G4cout << ">>> Lead gap set to " << g / cm << " cm" << G4endl;
    }
    else if (command == fWorldMaterialIsAirCmd) {
        G4bool isAir = fWorldMaterialIsAirCmd->GetNewBoolValue(newValue);
        fDetectorConstruction->SetWorldMaterialIsAir(isAir);
        G4cout << "###################################################" << G4endl;
        G4cout << ">>> World material set to " << (isAir ? "AIR" : "VACUUM") << G4endl;
    }
    else if (command == fFlipGuideCmd) {
        fDetectorConstruction->SetFlippedGuide(fFlipGuideCmd->GetNewIntValue(newValue));
    }
    else if (command == fGeometryModeCmd) {
        G4int mode = fGeometryModeCmd->GetNewIntValue(newValue);
        fDetectorConstruction->SetGeometryMode(mode);
        G4cout << "###################################################" << G4endl;
        G4cout << ">>> Geometry mode set to " << mode << G4endl;
    }

    else if (command == fWrappingCmd) {
        fDetectorConstruction->SetWrapping(newValue);
        G4cout << ">>> Wrapping set to " << newValue << " (applied at /detector/rebuild)" << G4endl;
    }
    else if (command == fPmmaGradeCmd) {
        fDetectorConstruction->SetPmmaGrade(newValue);
        G4cout << ">>> PMMA grade set to " << newValue << " (applied at /detector/rebuild)" << G4endl;
    }
    else if (command == fStorePhotonsCmd) {
        G4bool store = fStorePhotonsCmd->GetNewBoolValue(newValue);
        fDetectorConstruction->SetStorePhotons(store);
        G4cout << ">>> PhotonHits ntuple " << (store ? "enabled" : "disabled") << G4endl;
    }
    else if (command == fRebuildCmd) {
        G4cout << "------------------------------------" << G4endl;
        G4cout << ">>> Rebuilding detector geometry..."  << G4endl;

        auto runManager = G4RunManager::GetRunManager();

        runManager->DefineWorldVolume(fDetectorConstruction->Construct());

        // DefineWorldVolume() does not re-attach the sensitive detectors: the new
        // logical volumes would silently produce no hits. ConstructSDandField()
        // must be called explicitly, and the navigator has to be told that the
        // geometry changed.
        fDetectorConstruction->ConstructSDandField();
        runManager->GeometryHasBeenModified();

        G4cout << ">>> Detector geometry rebuilt successfully!" << G4endl;
        G4cout << "###################################################" << G4endl;
    }
}