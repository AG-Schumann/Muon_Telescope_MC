#include "RunAction.hh"

#include "G4Run.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"

#include "IMuonSource.hh"
#include "DetectorConstruction.hh"
#include "G4RunManager.hh"

RunAction::RunAction(const G4String& outputFilename,
                     IMuonSource* source,
                     const G4String& generatorFlag,
                     const G4String& sourceFile,
                     G4bool optical)
    : fOutputFilename(outputFilename),
      fSource(source),
      fGeneratorFlag(generatorFlag),
      fSourceFile(sourceFile),
      fOptical(optical)
{}
RunAction::~RunAction() {}

void RunAction::BeginOfRunAction(const G4Run* /*run*/)
{
    auto analysisManager = G4AnalysisManager::Instance();
    if (fSource) fSource->PrintConfiguration();
    analysisManager->SetDefaultFileType("root");
    analysisManager->SetVerboseLevel(0);

    // Output file (the extension is added automatically)
    analysisManager->OpenFile(fOutputFilename);

    // Ntuples and their columns


    for (G4int i = 0; i<4; i++){
    // Ntuple_i: Panel_i hits
    G4String name = "Panel" + std::to_string(i) + "Hits";
    G4String title = "Hits in Panel_" + std::to_string(i);
    analysisManager->CreateNtuple(name, title);
    analysisManager->CreateNtupleIColumn("eventID");
    analysisManager->CreateNtupleIColumn("trackID");
    analysisManager->CreateNtupleIColumn("parentID");
    analysisManager->CreateNtupleIColumn("pdg");
    analysisManager->CreateNtupleDColumn("edep");
    analysisManager->CreateNtupleDColumn("x"); // mm
    analysisManager->CreateNtupleDColumn("y"); // mm
    analysisManager->CreateNtupleDColumn("z"); // mm
    analysisManager->CreateNtupleDColumn("time");
    analysisManager->CreateNtupleDColumn("kineticEnergy");   // 9
    analysisManager->CreateNtupleIColumn("primaryID");       // 10
    analysisManager->FinishNtuple();
    }

    // Ntuple 4: Coincidence
    analysisManager->CreateNtuple("Coincidence", "Coincidence summary");
    analysisManager->CreateNtupleIColumn("eventID"); // 0
    analysisManager->CreateNtupleIColumn("coin_4fold"); // 1
    analysisManager->CreateNtupleIColumn("coin_top");       // 2
    analysisManager->CreateNtupleIColumn("coin_bottom");    // 3
    analysisManager->CreateNtupleIColumn("coin_outer");     // 4
    analysisManager->CreateNtupleIColumn("coincidence");
    analysisManager->FinishNtuple();


    // Ntuple 5: Primaries
    analysisManager->CreateNtuple("Primaries", "Generated particles");
    analysisManager->CreateNtupleIColumn("eventID");        // 0
    analysisManager->CreateNtupleIColumn("primaryID");      // 1
    analysisManager->CreateNtupleIColumn("pdg");            // 2
    analysisManager->CreateNtupleDColumn("energy");         // 3
    analysisManager->CreateNtupleDColumn("px");             // 4
    analysisManager->CreateNtupleDColumn("py");             // 5
    analysisManager->CreateNtupleDColumn("pz");             // 6
    analysisManager->CreateNtupleDColumn("x0");             // 7
    analysisManager->CreateNtupleDColumn("y0");             // 8
    analysisManager->CreateNtupleDColumn("z0");             // 9
    analysisManager->CreateNtupleDColumn("theta_deg");      // 10
    analysisManager->CreateNtupleDColumn("phi_deg");        // 11
    analysisManager->CreateNtupleDColumn("t_ns");           // 12
    analysisManager->FinishNtuple();

    // Ntuple 6: RunInfo, one row per run, written at EndOfRunAction
    analysisManager->CreateNtuple("RunInfo", "Run configuration and normalization");
    analysisManager->CreateNtupleIColumn("n_generated");    // 0
    analysisManager->CreateNtupleSColumn("generator");      // 1
    analysisManager->CreateNtupleSColumn("surface");        // 2
    analysisManager->CreateNtupleSColumn("source_file");    // 3
    analysisManager->CreateNtupleDColumn("gen_area_m2");    // 4
    analysisManager->CreateNtupleDColumn("flux_hz_m2");     // 5
    analysisManager->CreateNtupleDColumn("flux_err");       // 6
    analysisManager->CreateNtupleDColumn("seed");           // 7  (D: avoids a uint64 overflow)
    analysisManager->CreateNtupleDColumn("live_time_s");   // 8
    analysisManager->CreateNtupleDColumn("overburden_gcm2"); // 9  concrete slabs, vertical; 0 = none
    analysisManager->FinishNtuple();

    if (!fOptical) return;

    // Ntuple 7: OpticalSummary, one row per event, optical mode only.
    // Photon counts per module i: produced (StackingAction), detected at the
    // PMT window (PhotonDetector), lost (SteppingAction). Suffixes: sc =
    // created in the scintillator, lg = in the light guide, win = in the
    // PMT window; Scint = scintillation, Cher = Cherenkov.
    // Column index of module i, entry k (0..11): 1 + 12*i + k.
    analysisManager->CreateNtuple("OpticalSummary", "Optical photons per event and module");
    analysisManager->CreateNtupleIColumn("eventID");                     // 0
    for (G4int i = 0; i < 4; ++i) {
        const G4String m = std::to_string(i);
        analysisManager->CreateNtupleIColumn("prodScint_sc" + m);        // k = 0
        analysisManager->CreateNtupleIColumn("prodCher_sc" + m);         // 1
        analysisManager->CreateNtupleIColumn("prodCher_lg" + m);         // 2
        analysisManager->CreateNtupleIColumn("prodCher_win" + m);        // 3
        analysisManager->CreateNtupleIColumn("detScint_sc" + m);         // 4
        analysisManager->CreateNtupleIColumn("detCher_sc" + m);          // 5
        analysisManager->CreateNtupleIColumn("detCher_lg" + m);          // 6
        analysisManager->CreateNtupleIColumn("detCher_win" + m);         // 7
        analysisManager->CreateNtupleDColumn("tFirst" + m);              // 8  ns, -1 if none
        analysisManager->CreateNtupleIColumn("absBulk" + m);             // 9  OpAbsorption
        analysisManager->CreateNtupleIColumn("absSurf" + m);             // 10 absorbed by the wrapping
        analysisManager->CreateNtupleIColumn("noRindex" + m);            // 11 killed: next material has no RINDEX
    }
    analysisManager->FinishNtuple();

    // Ntuple 8: PhotonHits, one row per detected photon, filled only with
    // /detector/optics/storePhotons true.
    analysisManager->CreateNtuple("PhotonHits", "Detected optical photons");
    analysisManager->CreateNtupleIColumn("eventID");        // 0
    analysisManager->CreateNtupleIColumn("module");         // 1
    analysisManager->CreateNtupleIColumn("origin");         // 2  0 scintillator, 1 light guide, 2 PMT window
    analysisManager->CreateNtupleIColumn("process");        // 3  0 Scintillation, 1 Cerenkov
    analysisManager->CreateNtupleDColumn("energy_eV");      // 4
    analysisManager->CreateNtupleDColumn("wavelength_nm");  // 5
    analysisManager->CreateNtupleDColumn("time_ns");        // 6  arrival at the window
    analysisManager->CreateNtupleDColumn("x0_mm");          // 7  creation point
    analysisManager->CreateNtupleDColumn("y0_mm");          // 8
    analysisManager->CreateNtupleDColumn("z0_mm");          // 9
    analysisManager->FinishNtuple();
}

void RunAction::EndOfRunAction(const G4Run* run){
    auto analysisManager = G4AnalysisManager::Instance();
    const G4int nGen = run->GetNumberOfEvent();
    const G4double t = fSource ? fSource->GetLiveTime(nGen) : 0.;   // [s]

    // Fill the RunInfo ntuple
    analysisManager->FillNtupleIColumn(6, 0, nGen);
    analysisManager->FillNtupleSColumn(6, 1, fSource ? fSource->GetSourceLabel()  : "unknown");
    analysisManager->FillNtupleSColumn(6, 2, fSource ? fSource->GetSurfaceLabel() : "n/a");
    analysisManager->FillNtupleSColumn(6, 3, fSourceFile.empty() ? "n/a" : fSourceFile);
    analysisManager->FillNtupleDColumn(6, 4, fSource ? fSource->GetGenerationArea() : 0.);
    analysisManager->FillNtupleDColumn(6, 5, fSource ? fSource->GetNormalizedFlux() : 0.);
    analysisManager->FillNtupleDColumn(6, 6, fSource ? fSource->GetFluxError()      : 0.);
    analysisManager->FillNtupleDColumn(6, 7, fSource ? static_cast<G4double>(fSource->GetSeed()) : 0.);
    analysisManager->FillNtupleDColumn(6, 8, fSource ? t : 0.);
    const auto* detector = dynamic_cast<const DetectorConstruction*>(
        G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    analysisManager->FillNtupleDColumn(6, 9, detector ? detector->GetOverburden() / (g/cm2) : 0.);
    analysisManager->AddNtupleRow(6);
    G4cout << "[RunAction] fSource = " << (fSource ? "valid" : "NULL") << G4endl;
    if (fSource) G4cout << "  label = " << fSource->GetSourceLabel() << G4endl;

    analysisManager->Write();
    analysisManager->CloseFile();
}