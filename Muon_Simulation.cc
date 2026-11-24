// Muon_Simulation.cc
// Main function for the Muon Telescope_MC application

#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"
#include "PhysicsList.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "SourceType.hh"

#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "G4UItcsh.hh"
#include "G4UIterminal.hh"
#include "G4ios.hh"

#include <getopt.h>
#include <time.h>
#include <unistd.h>


void PrintUsage(const char* programName, const std::string& git_tag) {
    G4cout << "\nUsage: " << programName << " [OPTIONS]\n"
           << "\nOptions:\n"
           << "  -m <macro>   Macro file to execute (optional)\n"
           << "               If omitted, interactive visualization mode starts\n"
           << "  -g <gen>     Muon generator (default: ecomug)\n"
           << "                 ecomug       EcoMug, generated at runtime\n"
           << "                 isotropic    cosine-law source for geometric acceptance\n"  
           << "                 ecomug-file  pre-generated EcoMug file (needs -s)\n"
           << "                 cry          CRY cosmic-ray shower generator\n"
           << "                 mute+        MUTE mu+ file             (needs -s)\n"
           << "                 mute-        MUTE mu- file             (needs -s)\n"
           << "                 beam         pencil beam, set with /muonSource/beam/\n"
           << "  -s <file>    Muon source file, required by the file-based generators\n"
           << "  -o <name>    Output ROOT file basename (default: MuonTelescope)\n"
           << "  -r <seed>    Random seed (default: derived from the system clock)\n"
           << "  -i           Keep an interactive session open after the macro\n"
           << "  -p           Optical photons: G4OpticalPhysics, optical material\n"
           << "               properties, PMT windows and photon ntuples (geometry mode 2)\n"
           << "  -h           Show this help message\n"
           << "\nExamples:\n"
           << "  " << programName << " -m macros/run.mac -o results\n"
           << "  " << programName << " -m macros/isotropic.mac -o results -g isotropic\n"
           << "  " << programName << " -m macros/run.mac -o results -g mute+ -s data/mute_plus.dat\n"
           << "  " << programName << " -m macros/run.mac -o results -r 12345\n"
           << "  " << programName << " -m macros/optical_beam.mac -o beam -g beam -p\n"
           << "  " << programName << "                          (visualization_vrml creation mode)\n"
           << "\nMuon_Telescope_MC v" << git_tag << "\n"
           << G4endl;
}

// Converts the -g argument to a SourceType
bool ParseSourceType(const G4String& name, SourceType& type) {
    if      (name == "ecomug")      type = SourceType::EcoMugDirect;
    else if (name == "ecomug-file") type = SourceType::EcoMugFile;
    else if (name == "mute+")       type = SourceType::MutePlus;
    else if (name == "mute-")       type = SourceType::MuteMinus;
    else if (name == "isotropic")   type = SourceType::Isotropic;
    else if (name == "cry")         type = SourceType::CRY;
    else if (name == "beam")        type = SourceType::Beam;
    else return false;
    return true;
}

int main(int argc, char** argv) {
    // The Geant4 macro parser cannot read integers in some locales (e.g.
    // tr_TR). Setting the C locale here solves this for good.
    std::setlocale(LC_ALL, "C");



    std::string git_tag = "1.0.0"; // git tag of Muon_Telescope_MC goes here
    G4cout << "############################################################" << G4endl;
    G4cout << G4endl << "Welcome to Muon_Telescope_MC " << git_tag.c_str()<< G4endl;
    G4cout << "############################################################"<< G4endl;
 

    
    int  c            = 0;
    bool Macro        = false;
    bool Interactive  = false;
    bool Optical      = false;
    G4String MacroFilename;
    G4String MuonSourceFile = "";
    G4String OutputFile     = "";
    G4String GeneratorName  = "ecomug";
    long     Seed           = -1;          // <0: derived from the clock

    // Argument parsing
    // -m <macro>     : Macro file (optional, otherwise interactive vis mode)
    // -g <gen>       : Muon generator
    // -s <file>      : External muon source file, read by EcoMugFileSource
    //                  (omitted: EcoMugDirectSource runtime generation, default)
    // -o <name>      : Output ROOT file basename
    // -r <seed>      : Random seed (default: derived from the system clock)
    // -i             : Keep an interactive session open after the macro
    // -p             : Optical photon transport (see PhysicsList, DetectorConstruction)
    // -h             : Show this help message
    while ((c = getopt(argc, argv, "m:g:s:o:r:iph")) != -1) {
        switch (c) {
            case 'm': Macro = true; MacroFilename = optarg; break;
            case 'g': GeneratorName  = optarg; break;
            case 's': MuonSourceFile = optarg; break;
            case 'o': OutputFile     = optarg; break;
            case 'r': Seed = std::atol(optarg); break;
            case 'i': Interactive = true; break;
            case 'p': Optical = true; break;
            case 'h': PrintUsage(argv[0], git_tag); return 0;
            case '?':
                G4cerr << "Unknown option: -" << (char)optopt << G4endl;
                PrintUsage(argv[0], git_tag);
                return 1;
        }
    }


    // Check if output file was specified
    SourceType sourceType = SourceType::EcoMugDirect;
    if (!ParseSourceType(GeneratorName, sourceType)) {
        G4cerr << "\n###### ERROR: unknown generator '" << GeneratorName << "'"
               << "\n###### Valid values: ecomug | ecomug-file | mute+ | mute- | isotropic | cry | beam\n" << G4endl;
        return 1;
    }

    const bool needsFile = (sourceType != SourceType::EcoMugDirect &&
                        sourceType != SourceType::Isotropic &&
                        sourceType != SourceType::CRY &&
                        sourceType != SourceType::Beam);
    if (needsFile && !std::ifstream(MuonSourceFile).good()) {
        G4cerr << "\n###### ERROR: source file not found: " << MuonSourceFile
               << "\n" << G4endl;
        return 1;
    }
    if (!needsFile && MuonSourceFile != "") {
        G4cout << "\n###### WARNING: -s is ignored, generator 'ecomug' "
               << "generates muons at runtime.\n" << G4endl;
    }
    if (Macro && !std::ifstream(MacroFilename).good()) {
        G4cerr << "\n###### ERROR: macro file not found: " << MacroFilename
               << "\n" << G4endl;
        return 1;
    }
    if (OutputFile == "") {
        G4cout << "\n###### WARNING: no output file specified (-o), writing to "
               << "MuonTelescope.root in the current directory.\n" << G4endl;
        OutputFile = "MuonTelescope";
    }

    // Random seed
    // The seed is printed in every run; to repeat a run exactly, pass the
    // same value back with -r.
    if (Seed <= 0) Seed = static_cast<long>(std::time(nullptr));
        CLHEP::HepRandom::setTheSeed(Seed);
        G4cout << "\n###### Random seed: " << Seed
            << "   (Geant4 physics + EcoMug muon generation)"
            << "\n###### Reproduce with  -r  " << Seed
            << G4endl;

    // Run manager
    auto runManager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial);

    // The optical flag goes to all three: the physics list registers
    // G4OpticalPhysics, the detector adds the optical properties and the PMT
    // windows, and the actions add the photon counting and the extra ntuples.
    if (Optical) G4cout << "\n###### Optical photon transport enabled (-p)\n" << G4endl;
    runManager->SetUserInitialization(new DetectorConstruction(Optical));
    runManager->SetUserInitialization(new PhysicsList(Optical));
    runManager->SetUserInitialization(
        new ActionInitialization(sourceType, MuonSourceFile, OutputFile,
                             static_cast<std::uint64_t>(Seed), Optical));

    runManager->Initialize();

    auto visManager = new G4VisExecutive(argc, argv);
    visManager->Initialize();

    auto UImanager = G4UImanager::GetUIpointer();
    UImanager->ApplyCommand("/process/em/verbose 0");

    if (!Macro) {
        // Interactive visualisation mode
        auto ui = new G4UIExecutive(argc, argv);
        UImanager->ApplyCommand("/control/execute macros/visualization_vrml.mac");
        G4cout << "\n###### Visualization macro executed: "
               << "macros/visualization_vrml.mac\n" << G4endl;
        PrintUsage(argv[0], git_tag);
        ui->SessionStart();
        delete ui;
    }
    else {
        // Macro mode: batch by default
        UImanager->ApplyCommand("/control/execute " + MacroFilename);
        
        if (Interactive) {
            auto ui = new G4UIExecutive(argc, argv, "tcsh");
            ui->SessionStart();
            delete ui;
        }
    }

    delete visManager;
    delete runManager;
    return 0;
}