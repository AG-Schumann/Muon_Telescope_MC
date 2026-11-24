// CRYSource.cc
#include "CRYSource.hh"

#ifdef USE_CRY

#include "CRYSourceMessenger.hh"

#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include <cmath>
#include <sstream>

// CRY calls this function whenever it needs a random number. G4UniformRand()
// always uses the engine that is currently installed, so the seed given with
// -r drives CRY as well. Without this, CRY falls back to its own generator,
// which has a fixed seed and only 32768 distinct values.
static double CryRandom()
{
    return G4UniformRand();
}

CRYSource::CRYSource(std::uint64_t seed)
    : fSeed(seed)
{
    fMessenger = new CRYSourceMessenger(this);
}

CRYSource::~CRYSource()
{
    delete fMessenger;
    delete fGenerator;
    delete fSetup;
}

// Build the CRY generator from the current configuration.
// Called once, on the first shower request or from PrintConfiguration().
void CRYSource::Initialize()
{
    std::ostringstream setup;
    setup << "returnMuons "     << (fReturnMuons     ? 1 : 0) << " "
          << "returnElectrons " << (fReturnElectrons ? 1 : 0) << " "
          << "returnGammas "    << (fReturnGammas    ? 1 : 0) << " "
          << "returnNeutrons "  << (fReturnNeutrons  ? 1 : 0) << " "
          << "returnProtons "   << (fReturnProtons   ? 1 : 0) << " "
          << "returnPions "     << (fReturnPions     ? 1 : 0) << " "
          << "returnKaons 0 "
          << "altitude "        << fAltitude_m    << " "
          << "latitude "        << fLatitude_deg  << " "
          << "date "            << fDate          << " "
          << "subboxLength "    << fBoxLength_m;

    fSetup = new CRYSetup(setup.str(), fDataDir);

    // Must be set before the generator is built.
    fSetup->setRandomFunction(CryRandom);

    fGenerator = new CRYGenerator(fSetup);
}

G4bool CRYSource::AlreadyBuilt(const char* what) const
{
    if (fGenerator) {
        G4cout << "[CRYSource] The generator is already running, '" << what
               << "' is ignored. Put the command before /run/beamOn." << G4endl;
        return true;
    }
    return false;
}

// One CRY shower per Geant4 event
bool CRYSource::GetNextShower(std::vector<MuonEvent>& particles)
{
    if (!fGenerator) Initialize();

    particles.clear();
    fCRYParticles.clear();
    fGenerator->genEvent(&fCRYParticles);   // we own the particles afterwards

    // CRY times are absolute simulation times in seconds. Geant4 only cares
    // about times inside one event, so shift them to the earliest particle.
    G4double tFirst = 0.;
    if (!fCRYParticles.empty()) {
        tFirst = fCRYParticles[0]->t();
        for (size_t i = 1; i < fCRYParticles.size(); ++i) {
            if (fCRYParticles[i]->t() < tFirst) tFirst = fCRYParticles[i]->t();
        }
    }

    G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();

    for (size_t i = 0; i < fCRYParticles.size(); ++i) {
        CRYParticle* cryParticle = fCRYParticles[i];

        G4ParticleDefinition* definition =
            particleTable->FindParticle(cryParticle->PDGid());

        if (!definition) {
            G4cout << "[CRYSource] Unknown PDG code " << cryParticle->PDGid()
                   << ", particle skipped." << G4endl;
            delete cryParticle;
            continue;
        }

        // CRY gives kinetic energy in MeV; MuonEvent wants momentum in GeV/c.
        // p = sqrt(T*(T + 2m))
        const G4double kineticEnergy = cryParticle->ke();                 // MeV
        const G4double mass          = definition->GetPDGMass() / MeV;    // MeV
        const G4double momentum =
            std::sqrt(kineticEnergy * (kineticEnergy + 2. * mass)) / 1000.;  // GeV/c

        MuonEvent event;

        // CRY generates on the z = 0 plane and works in metres.
        event.x = cryParticle->x() * 1000.;   // m -> mm
        event.y = cryParticle->y() * 1000.;
        event.z = fPlaneZ_mm;

        // Direction cosines. CRY uses w < 0 for downward going particles.
        event.px = momentum * cryParticle->u();
        event.py = momentum * cryParticle->v();
        event.pz = momentum * cryParticle->w();

        event.charge = cryParticle->charge();
        event.pdg    = cryParticle->PDGid();
        event.theta  = std::acos(cryParticle->w());
        event.phi    = std::atan2(cryParticle->v(), cryParticle->u());
        event.t_ns   = (cryParticle->t() - tFirst) * 1.e9;   // s -> ns

        particles.push_back(event);
        delete cryParticle;
    }

    fCRYParticles.clear();
    ++fNShowers;
    return true;   // CRY never runs out of events
}

// CRY returns a whole shower, so a single-particle call has no clean meaning.
bool CRYSource::GetNextEvent(MuonEvent&)
{
    G4Exception("CRYSource::GetNextEvent()", "CRY_SHOWER", FatalException,
                "CRY is a shower generator, use GetNextShower() instead.");
    return false;
}

// Setters
void CRYSource::SetDataDir(const G4String& dir)
{
    if (AlreadyBuilt("dataDir")) return;
    fDataDir = dir;
}

void CRYSource::SetBoxLength(G4double length)
{
    if (AlreadyBuilt("boxLength")) return;
    fBoxLength_m = length / m;
    if (fBoxLength_m > 300.) {
        G4cout << "[CRYSource] CRY allows at most 300 m, value clipped." << G4endl;
        fBoxLength_m = 300.;
    }
}

void CRYSource::SetPlaneZ(G4double z)
{
    if (AlreadyBuilt("planeZ")) return;
    fPlaneZ_mm = z / mm;
}

void CRYSource::SetAltitude(G4double altitude)
{
    if (AlreadyBuilt("altitude")) return;
    const G4double value = altitude / m;
    // CRY ships data tables only for these three altitudes.
    if (value != 0. && value != 2100. && value != 11300.) {
        G4cout << "[CRYSource] Altitude must be 0, 2100 or 11300 m. "
               << "Keeping " << fAltitude_m << " m." << G4endl;
        return;
    }
    fAltitude_m = value;
}

void CRYSource::SetLatitude(G4double latitudeDeg)
{
    if (AlreadyBuilt("latitude")) return;
    fLatitude_deg = latitudeDeg;
}

void CRYSource::SetDate(const G4String& date)
{
    if (AlreadyBuilt("date")) return;
    fDate = date;
}

void CRYSource::SetReturnMuons(G4bool value)     { if (!AlreadyBuilt("returnMuons"))     fReturnMuons = value; }
void CRYSource::SetReturnElectrons(G4bool value) { if (!AlreadyBuilt("returnElectrons")) fReturnElectrons = value; }
void CRYSource::SetReturnGammas(G4bool value)    { if (!AlreadyBuilt("returnGammas"))    fReturnGammas = value; }
void CRYSource::SetReturnNeutrons(G4bool value)  { if (!AlreadyBuilt("returnNeutrons"))  fReturnNeutrons = value; }
void CRYSource::SetReturnProtons(G4bool value)   { if (!AlreadyBuilt("returnProtons"))   fReturnProtons = value; }
void CRYSource::SetReturnPions(G4bool value)     { if (!AlreadyBuilt("returnPions"))     fReturnPions = value; }

// Normalization
// CRY counts the time needed to produce every primary it has sampled, including
// the ones that gave no particle inside the box, so this is the live time of
// the run. It keeps counting across runs: use one /run/beamOn per job.
G4double CRYSource::GetLiveTime(G4int /*nGenerated*/)
{
    if (!fGenerator) return 0.;
    return fGenerator->timeSimulated();   // s
}

G4double CRYSource::GetNormalizedFlux() const
{
    const G4double area = GetGenerationArea();
    if (!fGenerator || area <= 0.) return 0.;

    const G4double liveTime = fGenerator->timeSimulated();
    if (liveTime <= 0.) return 0.;

    return fNShowers / (liveTime * area);
}

G4double CRYSource::GetFluxError() const
{
    if (fNShowers <= 0) return 0.;
    return GetNormalizedFlux() / std::sqrt(static_cast<G4double>(fNShowers));
}

void CRYSource::PrintConfiguration()
{
    if (!fGenerator) Initialize();

    G4cout << "\n===== CRYSource configuration =====" << G4endl;
    G4cout << "  data directory : " << fDataDir              << G4endl;
    G4cout << "  box length     : " << fBoxLength_m   << " m"
           << "  (CRY table: "      << fGenerator->boxSizeUsed() << " m)" << G4endl;
    G4cout << "  generation z   : " << fPlaneZ_mm / 1000. << " m" << G4endl;
    G4cout << "  altitude       : " << fAltitude_m    << " m"   << G4endl;
    G4cout << "  latitude       : " << fLatitude_deg  << " deg" << G4endl;
    G4cout << "  date           : " << fDate                    << G4endl;
    G4cout << "  particles      : "
           << (fReturnMuons     ? "mu "  : "")
           << (fReturnElectrons ? "e "   : "")
           << (fReturnGammas    ? "gamma " : "")
           << (fReturnNeutrons  ? "n "   : "")
           << (fReturnProtons   ? "p "   : "")
           << (fReturnPions     ? "pi "  : "") << G4endl;
    G4cout << "  seed           : " << fSeed << G4endl;
    G4cout << "===================================\n" << G4endl;
}

#endif  // USE_CRY