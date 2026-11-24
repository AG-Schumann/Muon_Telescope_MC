// ActionInitialization.cc
#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"

#include "IMuonSource.hh"
#include "EcoMugFileSource.hh"
#include "EcoMugDirectSource.hh"
#include "MuteFileSource.hh"
#include "IsotropicSource.hh"
#include "BeamSource.hh"
#include "SourceType.hh"
#include "CRYSource.hh"
#include "TrackingAction.hh"
#include "StackingAction.hh"
#include "SteppingAction.hh"

#include "G4UnitsTable.hh"
#include "G4SystemOfUnits.hh"

ActionInitialization::ActionInitialization(SourceType sourceType,
                     const G4String& muonSourceFile,
                     const G4String& outputFilename,
                     std::uint64_t seed,
                     G4bool optical)
    : fSourceType(sourceType),
      fMuonSourceFile(muonSourceFile),
      fOutputFilename(outputFilename),
      fSeed(seed),
      fOptical(optical)
{}

ActionInitialization::~ActionInitialization() {}

void ActionInitialization::BuildForMaster() const
{

}

void ActionInitialization::Build() const
{
    // Source selection (set in main with the -g flag)
    // More details in SourceType.hh.
    
    IMuonSource* source = nullptr;
    SetUserAction(new RunAction(fOutputFilename, nullptr, "", ""));
    G4cout << "###############################################################" << G4endl;

    switch (fSourceType) {

        case SourceType::EcoMugDirect: {
            G4cout << "[ActionInitialization] EcoMugDirectSource (runtime generation)" << G4endl;
            G4cout << "  PRNG seed: " << fSeed << G4endl;
            source = new EcoMugDirectSource(fSeed);
            break;
        }

        case SourceType::EcoMugFile: {
            G4cout << "[ActionInitialization] EcoMugFileSource"   << G4endl;
            G4cout << "  Source file: " << fMuonSourceFile        << G4endl;
            auto* s = new EcoMugFileSource();
            if (!s->LoadFile(fMuonSourceFile)) {
                G4Exception("ActionInitialization::Build()", "SourceLoadFailed",
                            FatalException, "Could not load EcoMug source file.");
            }
            source = s;
            break;
        }
        
        #ifdef USE_CRY
        case SourceType::CRY:
            G4cout << "[ActionInitialization] CRYSource (cosmic-ray shower generator)" << G4endl;
            G4cout << "  PRNG seed: " << fSeed << G4endl;
            source = new CRYSource(fSeed);   // same seed argument as EcoMugDirectSource
            break;
        #else
                case SourceType::CRY:
                    G4Exception("ActionInitialization", "NoCRY", FatalException,
                                "This binary was built without CRY (cmake -DWITH_CRY=ON).");
                    break;
        #endif


        case SourceType::MutePlus:
        case SourceType::MuteMinus: {
            const G4int charge = (fSourceType == SourceType::MutePlus) ? +1 : -1;
            G4cout << "[ActionInitialization] MuteFileSource ("
                   << (charge > 0 ? "mu+" : "mu-") << ")"         << G4endl;
            G4cout << "  Source file: " << fMuonSourceFile        << G4endl;
            auto* s = new MuteFileSource(charge);
            if (!s->LoadFile(fMuonSourceFile)) {
                G4Exception("ActionInitialization::Build()", "SourceLoadFailed",
                            FatalException, "Could not load MUTE source file.");
            }
            source = s;
            break;
        }

        case SourceType::Isotropic: {
            G4cout << "[ActionInitialization] IsotropicSource (geometric acceptance)" << G4endl;
            source = new IsotropicSource(fSeed);
            break;
        }

        case SourceType::Beam: {
            G4cout << "[ActionInitialization] BeamSource (pencil beam, /muonSource/beam/)" << G4endl;
            source = new BeamSource();
            break;
        }
    }

    G4cout << "###############################################################" << G4endl;


    auto trackingAction = new TrackingAction();

    // Optical mode: photon production counts (stacking) and loss counts
    // (stepping), read out by EventAction.
    StackingAction* stackingAction = nullptr;
    SteppingAction* steppingAction = nullptr;
    if (fOptical) {
        stackingAction = new StackingAction();
        steppingAction = new SteppingAction();
        SetUserAction(stackingAction);
        SetUserAction(steppingAction);
    }

    SetUserAction(trackingAction);
    SetUserAction(new PrimaryGeneratorAction(source));
    SetUserAction(new RunAction(fOutputFilename, source,
                                SourceTypeToString(fSourceType), fMuonSourceFile, fOptical));
    SetUserAction(new EventAction(trackingAction, stackingAction, steppingAction));

}