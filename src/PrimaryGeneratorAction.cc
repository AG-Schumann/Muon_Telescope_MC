#include "PrimaryGeneratorAction.hh"

#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4RunManager.hh"
#include "G4Exception.hh"

#include "DetectorConstruction.hh"


#include <cmath>



PrimaryGeneratorAction::PrimaryGeneratorAction(IMuonSource* source)
    : fMuonSource(source)
{
    fParticleGun = new G4ParticleGun(1);
}

PrimaryGeneratorAction::~PrimaryGeneratorAction(){
    delete fParticleGun;
}


void PrimaryGeneratorAction::GenerateTestPrimaries(G4Event* anEvent){

    // Define the particle type and energy
    // Define the particle type and energy
    // Particle type: Gamma -> for testing purposes
    G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
    G4String particleName = "mu-"; // Electron
    G4ParticleDefinition* muBeam = particleTable->FindParticle(particleName);


    fParticleGun->SetParticleEnergy(1.0 * MeV); // Energy of the particle
    fParticleGun->SetParticlePosition(G4ThreeVector(0,0,2.0 * m)); // Initial position of the particle, Start 2 meter away from the origin along the z-axis
    fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0,0,-1)); // Initial momentum direction of the particle, Pointing towards the origin
    fParticleGun->SetParticleDefinition(muBeam);
    fParticleGun->GeneratePrimaryVertex(anEvent); // Generate the primary vertex in the event


    // Fill the Primaries ntuple (ntuple 5)
    auto analysisManager = G4AnalysisManager::Instance();
    analysisManager->FillNtupleIColumn(5, 0, anEvent->GetEventID());
    analysisManager->FillNtupleIColumn(5, 1, fParticleGun->GetParticleDefinition()->GetPDGEncoding());
    analysisManager->FillNtupleDColumn(5, 2, fParticleGun->GetParticleEnergy() / MeV);
    G4ThreeVector momentumDir = fParticleGun->GetParticleMomentumDirection();
    analysisManager->FillNtupleDColumn(5, 3, momentumDir.x());
    analysisManager->FillNtupleDColumn(5, 4, momentumDir.y());
    analysisManager->FillNtupleDColumn(5, 5, momentumDir.z());

    G4ThreeVector pos = fParticleGun->GetParticlePosition();
    analysisManager->FillNtupleDColumn(5, 6, pos.x() / mm);
    analysisManager->FillNtupleDColumn(5, 7, pos.y() / mm);
    analysisManager->FillNtupleDColumn(5, 8, pos.z() / mm);
    
    analysisManager->AddNtupleRow(5);
}



void PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{

    std::vector<MuonEvent> particles;
    if (!fMuonSource->GetNextShower(particles) || particles.empty()) {
        G4cerr << "PrimaryGeneratorAction: No more events in source!" << G4endl;
        G4RunManager::GetRunManager()->AbortRun();
        return;
    }

    G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
    auto analysisManager = G4AnalysisManager::Instance();

    // With concrete slabs above the telescope every particle has to start
    // above the highest slab. A vertex below it would bypass the concrete
    // without leaving any other trace in the output (e.g. the 1.4 m
    // hemisphere with slabs above 1.4 m), so the run is stopped instead.
    const auto* detector = dynamic_cast<const DetectorConstruction*>(
        G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    const G4double roofTop = detector ? detector->GetRoofTop() : -DBL_MAX;

    // One primary vertex per particle: each particle in the shower keeps its own position and time

    for (size_t i = 0; i < particles.size(); ++i){

        const MuonEvent& evt = particles[i];

        if (evt.z * mm < roofTop) {
            G4ExceptionDescription msg;
            msg << "Primary vertex at z = " << evt.z * mm / m << " m lies below the upper surface "
                << "of the concrete slabs (z = " << roofTop / m << " m). Move the generation "
                << "surface above the slabs, e.g. /muonSource/ecomug/surface sky with "
                << "/muonSource/ecomug/skyCenter 0 0 <z > " << roofTop / m << "> m.";
            G4Exception("PrimaryGeneratorAction::GeneratePrimaries()", "VertexBelowRoof",
                        FatalException, msg);
            return;
        }

        G4ParticleDefinition* definition = nullptr;
        if (evt.pdg != 0) {
            definition = particleTable->FindParticle(evt.pdg);
        } else {
            definition = particleTable->FindParticle((evt.charge > 0) ? "mu+" : "mu-");
        }
        if (!definition) {
            G4ExceptionDescription msg;
            msg << "Unknown PDG code " << evt.pdg << " from the muon source.";
            G4Exception("PrimaryGeneratorAction::GeneratePrimaries()",
                        "UnknownPDG", FatalException, msg);
            return;
        }

        // Kinetic energy from momentum: E_kin = sqrt(p^2 + m^2) - m
        const G4double pTot_GeV = std::sqrt(evt.px*evt.px + evt.py*evt.py + evt.pz*evt.pz);
        const G4double pTot     = pTot_GeV * GeV;
        const G4double mass     = definition->GetPDGMass();
        const G4double kineticE = std::sqrt(pTot*pTot + mass*mass) - mass;

        G4ThreeVector momDir(evt.px, evt.py, evt.pz);
        if (momDir.mag() > 0) momDir = momDir.unit();

        fParticleGun->SetParticleDefinition(definition);
        fParticleGun->SetParticlePosition(G4ThreeVector(evt.x * mm, evt.y * mm, evt.z * mm));
        fParticleGun->SetParticleMomentumDirection(momDir);
        fParticleGun->SetParticleEnergy(kineticE);
        fParticleGun->SetParticleTime(evt.t_ns * ns);   // the gun keeps the last value
        fParticleGun->GeneratePrimaryVertex(anEvent);

        // Primaries ntuple: one row per particle
        const G4double theta_rad        = std::acos(evt.pz / pTot_GeV);
        const G4double theta_zenith_deg = (CLHEP::pi - theta_rad) / deg;
        const G4double phi_deg          = std::atan2(evt.py, evt.px) / deg;

        analysisManager->FillNtupleIColumn(5, 0, anEvent->GetEventID());
        analysisManager->FillNtupleIColumn(5, 1, static_cast<G4int>(i) + 1);  // primaryID: Geant4 numbers the primary tracks 1..N in vertex order
        analysisManager->FillNtupleIColumn(5, 2, definition->GetPDGEncoding());
        analysisManager->FillNtupleDColumn(5, 3, kineticE / MeV);
        analysisManager->FillNtupleDColumn(5, 4, evt.px);   // GeV/c
        analysisManager->FillNtupleDColumn(5, 5, evt.py);
        analysisManager->FillNtupleDColumn(5, 6, evt.pz);
        analysisManager->FillNtupleDColumn(5, 7, evt.x);    // mm
        analysisManager->FillNtupleDColumn(5, 8, evt.y);
        analysisManager->FillNtupleDColumn(5, 9, evt.z);
        analysisManager->FillNtupleDColumn(5, 10, theta_zenith_deg);
        analysisManager->FillNtupleDColumn(5, 11, phi_deg);
        analysisManager->FillNtupleDColumn(5, 12, evt.t_ns);
        analysisManager->AddNtupleRow(5);
    }
    

}
