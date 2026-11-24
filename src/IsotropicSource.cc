// IsotropicSource.cc
#include "IsotropicSource.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cmath>
#include <cstdint>


IsotropicSource::IsotropicSource(std::uint64_t seed) : fSeed(seed)
{
    fMessenger = new IsotropicSourceMessenger(this);
}

IsotropicSource::~IsotropicSource()
{
    delete fMessenger;
}

void IsotropicSource::SetSurface(const G4String& type)
{
    if      (type == "plane")   fSurface = Surface::Plane;
    else if (type == "hsphere") fSurface = Surface::Hemisphere;
    else G4cerr << "IsotropicSource: unknown surface '" << type
                << "'. Valid: plane, hsphere." << G4endl;
}


G4ThreeVector
IsotropicSource::SampleCosineDirection(const G4ThreeVector& normal) const
{
    // Cosine law in the local frame: with pdf(theta) proportional to
    // cos(theta) sin(theta) the cumulative is sin^2(theta), so
    // sin(theta) = sqrt(u) samples it exactly, with no rejection loop.
    const G4double u    = G4UniformRand();
    const G4double sinT = std::sqrt(u);
    const G4double cosT = std::sqrt(1.0 - u);
    const G4double phi  = 2.0 * CLHEP::pi * G4UniformRand();

    // Build an orthonormal basis around the normal and rotate into it.
    G4ThreeVector w = normal.unit();
    G4ThreeVector a = (std::fabs(w.x()) > 0.9) ? G4ThreeVector(0, 1, 0)
                                               : G4ThreeVector(1, 0, 0);
    G4ThreeVector uAxis = w.cross(a).unit();
    G4ThreeVector vAxis = w.cross(uAxis);

    return sinT * std::cos(phi) * uAxis
         + sinT * std::sin(phi) * vAxis
         + cosT * w;
}

bool IsotropicSource::GetNextEvent(MuonEvent& event)
{
    G4ThreeVector pos, inwardNormal;

    if (fSurface == Surface::Plane) {
        pos = G4ThreeVector((G4UniformRand() - 0.5) * fSize,
                            (G4UniformRand() - 0.5) * fSize,
                            fHeight);
        inwardNormal = G4ThreeVector(0, 0, -1);
    } else {
        // Uniform on the upper hemisphere: cos(theta0) uniform in [0,1]
        // gives equal area per solid angle element.
        const G4double cosT0 = G4UniformRand();
        const G4double sinT0 = std::sqrt(1.0 - cosT0 * cosT0);
        const G4double phi0  = 2.0 * CLHEP::pi * G4UniformRand();

        const G4ThreeVector outward(sinT0 * std::cos(phi0),
                                    sinT0 * std::sin(phi0),
                                    cosT0);
        pos = fRadius * outward + G4ThreeVector(0, 0, fCentreZ);
        inwardNormal = -outward;
    }

    const G4ThreeVector dir = SampleCosineDirection(inwardNormal);

    event.x = pos.x();
    event.y = pos.y();
    event.z = pos.z();

    event.px = fMomentumGeV * dir.x();
    event.py = fMomentumGeV * dir.y();
    event.pz = fMomentumGeV * dir.z();

    // Zenith angle of the direction, measured from the downward (-z) axis
    event.theta = std::acos(-dir.z());
    event.phi   = std::atan2(dir.y(), dir.x());

    // Sea-level charge ratio mu+/mu- = 1.28
    event.charge = (G4UniformRand() < 128.0 / 228.0) ? +1 : -1;

    return true;
}

G4double IsotropicSource::GetGenerationArea() const
{
    if (fSurface == Surface::Plane) return (fSize * fSize) / (m * m);
    return (2.0 * CLHEP::pi * fRadius * fRadius) / (m * m);
}

void IsotropicSource::PrintConfiguration()
{
    G4cout << "\n##########################################################" << G4endl;
    G4cout << "Isotropic source (geometric acceptance mode)"                << G4endl;
    if (fSurface == Surface::Plane) {
        G4cout << "  Surface        : plane"                                << G4endl;
        G4cout << "  Size           : " << fSize/m << " x " << fSize/m << " m" << G4endl;
        G4cout << "  Height         : " << fHeight/m << " m"                << G4endl;
    } else {
        G4cout << "  Surface        : hemisphere"                           << G4endl;
        G4cout << "  Radius         : " << fRadius/m << " m"                << G4endl;
        G4cout << "  Centre z       : " << fCentreZ/m << " m"               << G4endl;
    }
    G4cout << "  Momentum       : " << fMomentumGeV << " GeV/c (fixed)"     << G4endl;
    G4cout << "  Area           : " << GetGenerationArea() << " m^2"        << G4endl;
    G4cout << "  G = (N_det/N_gen) * A_gen * pi   [m^2 sr]"                 << G4endl;
    G4cout << "  PRNG seed      : " << fSeed                               << G4endl;
    G4cout << "  Flux           : not defined in this mode (RunInfo: 0)"   << G4endl;
    G4cout << "##########################################################\n" << G4endl;
}



#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"

IsotropicSourceMessenger::IsotropicSourceMessenger(IsotropicSource* source)
    : fSource(source)
{
    fDir = new G4UIdirectory("/muonSource/isotropic/");
    fDir->SetGuidance("Cosine-law source for geometric acceptance studies.");

    fSurfaceCmd = new G4UIcmdWithAString("/muonSource/isotropic/surface", this);
    fSurfaceCmd->SetGuidance("Generation surface: plane or hsphere.");
    fSurfaceCmd->SetParameterName("type", false);
    fSurfaceCmd->SetCandidates("plane hsphere");
    fSurfaceCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSizeCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/isotropic/size", this);
    fSizeCmd->SetGuidance("Side length of the square generation plane.");
    fSizeCmd->SetParameterName("size", false);
    fSizeCmd->SetUnitCategory("Length");
    fSizeCmd->SetDefaultUnit("m");
    fSizeCmd->SetRange("size > 0");
    fSizeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fHeightCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/isotropic/height", this);
    fHeightCmd->SetGuidance("Height of the generation plane above the origin.");
    fHeightCmd->SetParameterName("height", false);
    fHeightCmd->SetUnitCategory("Length");
    fHeightCmd->SetDefaultUnit("m");
    fHeightCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fRadiusCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/isotropic/radius", this);
    fRadiusCmd->SetGuidance("Radius of the generation hemisphere.");
    fRadiusCmd->SetParameterName("radius", false);
    fRadiusCmd->SetUnitCategory("Length");
    fRadiusCmd->SetDefaultUnit("m");
    fRadiusCmd->SetRange("radius > 0");
    fRadiusCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fCentreZCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/isotropic/centreZ", this);
    fCentreZCmd->SetGuidance("z position of the hemisphere centre.");
    fCentreZCmd->SetParameterName("z", false);
    fCentreZCmd->SetUnitCategory("Length");
    fCentreZCmd->SetDefaultUnit("m");
    fCentreZCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fMomentumCmd = new G4UIcmdWithADoubleAndUnit("/muonSource/isotropic/momentum", this);
    fMomentumCmd->SetGuidance("Fixed muon momentum. High enough that scattering");
    fMomentumCmd->SetGuidance("and stopping are negligible, so that the result");
    fMomentumCmd->SetGuidance("depends on the geometry alone.");
    fMomentumCmd->SetParameterName("p", false);
    fMomentumCmd->SetUnitCategory("Energy");
    fMomentumCmd->SetDefaultUnit("GeV");
    fMomentumCmd->SetRange("p > 0");
    fMomentumCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

IsotropicSourceMessenger::~IsotropicSourceMessenger()
{
    delete fSurfaceCmd;
    delete fSizeCmd;
    delete fHeightCmd;
    delete fRadiusCmd;
    delete fCentreZCmd;
    delete fMomentumCmd;
    delete fDir;
}

void IsotropicSourceMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if      (command == fSurfaceCmd)  fSource->SetSurface(newValue);
    else if (command == fSizeCmd)     fSource->SetSize(fSizeCmd->GetNewDoubleValue(newValue));
    else if (command == fHeightCmd)   fSource->SetHeight(fHeightCmd->GetNewDoubleValue(newValue));
    else if (command == fRadiusCmd)   fSource->SetRadius(fRadiusCmd->GetNewDoubleValue(newValue));
    else if (command == fCentreZCmd)  fSource->SetCentreZ(fCentreZCmd->GetNewDoubleValue(newValue));
    else if (command == fMomentumCmd) fSource->SetMomentum(fMomentumCmd->GetNewDoubleValue(newValue) / GeV);
    else {
        G4cerr << "IsotropicSourceMessenger: unhandled command "
               << command->GetCommandPath() << G4endl;
    }
}