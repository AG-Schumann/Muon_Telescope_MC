// PhysicsList.cc
#include "PhysicsList.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"

PhysicsList::PhysicsList(G4bool optical) : FTFP_BERT(0)
{
    // FTFP_BERT ships G4EmStandardPhysics (option0). Swap it for the most
    // accurate EM configuration, which matters for the soft electron and
    // gamma component coming from CRY.
    ReplacePhysics(new G4EmStandardPhysics_option4());

    if (!optical) return;

    // Cherenkov, scintillation, bulk absorption and boundary processes. The
    // Cherenkov and scintillation tables are built at initialisation from the
    // materials that carry a G4MaterialPropertiesTable at that moment, so
    // DetectorConstruction creates all optical materials in its first
    // Construct() call. G4OpticalPhysics attaches G4EmSaturation to
    // G4Scintillation, so the Birks constant of the scintillator is used.
    RegisterPhysics(new G4OpticalPhysics());

    auto params = G4OpticalParameters::Instance();
    params->SetCerenkovMaxPhotonsPerStep(100);
    params->SetCerenkovMaxBetaChange(10.0);
    params->SetCerenkovTrackSecondariesFirst(true);
    params->SetScintTrackSecondariesFirst(true);
    params->SetScintFiniteRiseTime(true);
}

PhysicsList::~PhysicsList() {}
