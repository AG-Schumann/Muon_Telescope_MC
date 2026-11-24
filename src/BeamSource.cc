// BeamSource.cc
#include "BeamSource.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"
#include <cmath>


BeamSource::BeamSource()
{
    fMessenger = new BeamSourceMessenger(this);
}

BeamSource::~BeamSource()
{
    delete fMessenger;
}

void BeamSource::SetParticle(const G4String& name)
{
    auto def = G4ParticleTable::GetParticleTable()->FindParticle(name);
    if (!def) {
        G4cerr << "BeamSource: unknown particle '" << name << "', keeping "
               << fParticle << G4endl;
        return;
    }
    fParticle = name;
    fPDG      = def->GetPDGEncoding();
}

bool BeamSource::GetNextEvent(MuonEvent& event)
{
    // MuonEvent positions are in mm and momenta in GeV/c (see MuonEvent.hh).
    event.x = fPosition.x() / mm;
    event.y = fPosition.y() / mm;
    event.z = fPosition.z() / mm;

    event.px = fMomentumGeV * fDirection.x();
    event.py = fMomentumGeV * fDirection.y();
    event.pz = fMomentumGeV * fDirection.z();

    event.pdg    = fPDG;
    event.charge = (fPDG == 13 || fPDG == 11) ? -1 : +1;

    // Zenith angle of the direction, measured from the downward (-z) axis
    event.theta = std::acos(-fDirection.z());
    event.phi   = std::atan2(fDirection.y(), fDirection.x());
    event.t_ns  = 0.;

    return true;
}

void BeamSource::PrintConfiguration()
{
    G4cout << "\n##########################################################" << G4endl;
    G4cout << "Beam source (pencil beam, position scans)"                    << G4endl;
    G4cout << "  Particle       : " << fParticle << " (PDG " << fPDG << ")" << G4endl;
    G4cout << "  Momentum       : " << fMomentumGeV << " GeV/c"             << G4endl;
    G4cout << "  Position       : " << fPosition/mm << " mm"                << G4endl;
    G4cout << "  Direction      : " << fDirection                           << G4endl;
    G4cout << "  Flux           : not defined in this mode (RunInfo: 0)"   << G4endl;
    G4cout << "##########################################################\n" << G4endl;
}



#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWith3VectorAndUnit.hh"
#include "G4UIcmdWith3Vector.hh"

BeamSourceMessenger::BeamSourceMessenger(BeamSource* source)
    : fSource(source)
{
    fDir = new G4UIdirectory("/muonSource/beam/");
    fDir->SetGuidance("Pencil beam for light-collection position scans.");

    fParticleCmd = new G4UIcmdWithAString("/muonSource/beam/particle", this);
    fParticleCmd->SetGuidance("Particle name (mu-, mu+, e-, ...).");
    fParticleCmd->SetParameterName("name", false);
    fParticleCmd->SetDefaultValue("mu-");
    fParticleCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fMomentumCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/beam/momentum", this);
    fMomentumCmd->SetGuidance("Fixed beam momentum.");
    fMomentumCmd->SetParameterName("p", false);
    fMomentumCmd->SetUnitCategory("Energy");
    fMomentumCmd->SetDefaultUnit("GeV");
    fMomentumCmd->SetRange("p > 0");
    fMomentumCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fPositionCmd = new G4UIcmdWith3VectorAndUnit("/muonSource/beam/position", this);
    fPositionCmd->SetGuidance("Start point of the beam.");
    fPositionCmd->SetParameterName("x", "y", "z", false);
    fPositionCmd->SetUnitCategory("Length");
    fPositionCmd->SetDefaultUnit("mm");
    fPositionCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fDirectionCmd = new G4UIcmdWith3Vector("/muonSource/beam/direction", this);
    fDirectionCmd->SetGuidance("Direction of the beam (normalised internally).");
    fDirectionCmd->SetParameterName("dx", "dy", "dz", false);
    fDirectionCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

BeamSourceMessenger::~BeamSourceMessenger()
{
    delete fParticleCmd;
    delete fMomentumCmd;
    delete fPositionCmd;
    delete fDirectionCmd;
    delete fDir;
}

void BeamSourceMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if      (command == fParticleCmd)  fSource->SetParticle(newValue);
    else if (command == fMomentumCmd)  fSource->SetMomentum(fMomentumCmd->GetNewDoubleValue(newValue) / GeV);
    else if (command == fPositionCmd)  fSource->SetPosition(fPositionCmd->GetNew3VectorValue(newValue));
    else if (command == fDirectionCmd) {
        const G4ThreeVector dir = fDirectionCmd->GetNew3VectorValue(newValue);
        if (dir.mag() > 0.) fSource->SetDirection(dir);
        else G4cerr << "BeamSourceMessenger: zero direction ignored" << G4endl;
    }
    else {
        G4cerr << "BeamSourceMessenger: unhandled command "
               << command->GetCommandPath() << G4endl;
    }
}
