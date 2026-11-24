    // EcoMugDirectSource.cc
    #include "EcoMugDirectSource.hh"
    #include "EcoMugDirectSourceMessenger.hh"

    #include "G4SystemOfUnits.hh"
    #include "globals.hh"

    #include <cmath>

    using namespace std;




    EcoMugDirectSource::EcoMugDirectSource(std::uint64_t seed)
    {
        // Default geometry: Sky (flat horizontal surface)
        // Sky is the simplest and fastest option; it can be changed from the macro.
        fMug.SetUseSky();

        // Default sky size: 1m × 1m (EcoMug raw value, interpreted as metres)
        // The GeMSE_muon panels are ~30 cm wide, so 1 m x 1 m is a reasonable start.
        fMug.SetSkySize({{1.0, 1.0}});
        fMug.SetSkyCenterPosition({{0., 0., 0.}});
        
        if (seed > 0) {
            fMug.SetSeed(seed);
            fSeed = seed;
            G4cout << "  EcoMug PRNG seed: " << seed << G4endl;
        }

        G4cout << "  Default surface : sky (1m × 1m, centered at origin)"           << G4endl;
        G4cout << "  Default momentum: [0.01, 1000] GeV/c"                          << G4endl;
        G4cout << "  Default theta   : [0, π/2] rad (nadir)"                        << G4endl;
        G4cout << "  Configure via macro: /muonSource/ecomug/..."                   << G4endl;
        G4cout << "###############################################################" << G4endl;

        // The momentum, theta and phi ranges keep the EcoMug defaults:
        //   - Momentum: [0.01, 1000] GeV/c
        //   - Theta:    [0, π/2] (nadir, internal convention)
        //   - Phi:      [0, 2π]
        // They cover the bulk of the atmospheric muon spectrum.
        // They can be changed from the macro (SetMomentumRange / SetThetaRange).

        // Messenger: owned by this class, deleted in the destructor
        fMessenger = new EcoMugDirectSourceMessenger(this);
    }

    EcoMugDirectSource::~EcoMugDirectSource()
    {

        delete fMessenger;
    }

    // IMuonSource interface

    bool EcoMugDirectSource::GetNextEvent(MuonEvent& event)
    {
        // EcoMug generates one cosmic muon
        fMug.Generate();

        // Position
        // EcoMug returns raw values (read as metres, since EMUnits::m = 1.0)
        // MuonEvent expects the Geant4 internal length unit (mm), hence * m
        auto pos = fMug.GetGenerationPosition();
        event.x = pos[0] * m;
        event.y = pos[1] * m;
        event.z = pos[2] * m;

        // Momentum vector

        // EcoMug gives spherical coordinates: (|p|, theta, phi)
        //   - theta: angle between the outgoing direction after Generate() and the z axis (+z up)
        //
        // The formula of the paper is used directly (no extra sign, theta is already outgoing):
        //   px = |p| sin(θ) cos(φ)
        //   py = |p| sin(θ) sin(φ)
        //   pz = |p| cos(θ)

        G4double pTot  = fMug.GetGenerationMomentum();   // GeV raw
        G4double theta = fMug.GetGenerationTheta();      // rad
        event.theta = theta; // stored in MuonEvent directly from EcoMug
        
        G4double phi   = fMug.GetGenerationPhi();        // rad
        event.phi = phi; // stored in MuonEvent directly from EcoMug
        
        event.px = pTot * sin(theta) * cos(phi);
        event.py = pTot * sin(theta) * sin(phi);
        event.pz = pTot * cos(theta);

        // Charge
        // EcoMug GetCharge() returns an int (+1 or -1)
        event.charge = fMug.GetCharge();

        // the number of events is set by /run/beamOn N
        
        return true;
    }

    // Setter API (called by the messenger)

    // Unit conversion (Geant4 -> EcoMug):
    //   - Length:   G4 internal (mm) / m       ->  EcoMug raw value in m
    //   - Momentum: G4 (GeV unit, =1.0) / GeV  ->  EcoMug raw value in GeV
    //   - Angle:    G4 (rad, =1.0) / rad       ->  EcoMug raw value in rad
    // For angles and momenta the numbers do not change, but the conversion is written out.

    void EcoMugDirectSource::SetSurfaceType(const G4String& type)
    {
        if (type == "sky") {
            fMug.SetUseSky();
            fSurfaceType = "sky";
        } else if (type == "cylinder") {
            fMug.SetUseCylinder();
            fSurfaceType = "cylinder";
        } else if (type == "hsphere") {
            fMug.SetUseHSphere();
            fSurfaceType = "hsphere";
        
        } else if (type == "target") {
            fMug.SetUseTargetSphere();
            fSurfaceType = "target";
        }
        else {
            G4cerr << "EcoMugDirectSource: Unknown surface type '" << type
                << "'. Valid: `sky`, `cylinder`, `hsphere`, `target`." << G4endl;
        }
    }


    // Sky

    void EcoMugDirectSource::SetSkySizeX(G4double lx)
    {
        G4double currentLy = fMug.GetSkySize(1);  // raw value in m
        fMug.SetSkySize({{lx / m, currentLy}});
    }

    void EcoMugDirectSource::SetSkySizeY(G4double ly)
    {
        G4double currentLx = fMug.GetSkySize(0);
        fMug.SetSkySize({{currentLx, ly / m}});
    }

    void EcoMugDirectSource::SetSkyCenter(const G4ThreeVector& center)
    {
        fMug.SetSkyCenterPosition({{
            center.x() / m,
            center.y() / m,
            center.z() / m
        }});
    }

    // HSphere
    void EcoMugDirectSource::SetHSphereRadius(G4double radius)
    {
        fMug.SetHSphereRadius(radius / m);
    }

    void EcoMugDirectSource::SetHSphereCenter(const G4ThreeVector& center)
    {
        fMug.SetHSphereCenterPosition({{
            center.x() / m,
            center.y() / m,
            center.z() / m
        }});
    }

    // Cylinder
    void EcoMugDirectSource::SetCylinderRadius(G4double radius)
    {
        fMug.SetCylinderRadius(radius / m);
    }

    void EcoMugDirectSource::SetCylinderHeight(G4double height)
    {
        fMug.SetCylinderHeight(height / m);
    }

    void EcoMugDirectSource::SetCylinderCenter(const G4ThreeVector& center)
    {
        fMug.SetCylinderCenterPosition({{
            center.x() / m,
            center.y() / m,
            center.z() / m
        }});
    }

    // Target Disk
    void EcoMugDirectSource::SetTargetSphereRadius(G4double radius)
    {
        fMug.SetTargetSphereRadius(radius / m);
    }

    void EcoMugDirectSource::SetTargetSphereCenter(const G4ThreeVector& c)
    {
        fMug.SetTargetSphereCenterPosition({{c.x()/m, c.y()/m, c.z()/m}});
    }


    // Physical parameter ranges
    void EcoMugDirectSource::SetMinMomentum(G4double pmin)
    {
        fMug.SetMinimumMomentum(pmin / GeV);
    }

    void EcoMugDirectSource::SetMaxMomentum(G4double pmax)
    {
        fMug.SetMaximumMomentum(pmax / GeV);
    }

    void EcoMugDirectSource::SetMinTheta(G4double tmin)
    {
        fMug.SetMinimumTheta(tmin / rad);
    }

    void EcoMugDirectSource::SetMaxTheta(G4double tmax)
    {
        fMug.SetMaximumTheta(tmax / rad);
    }

    G4double EcoMugDirectSource::GetSkySizeX()        { return fMug.GetSkySize(0) * m; }
    G4double EcoMugDirectSource::GetSkySizeY()        { return fMug.GetSkySize(1) * m; }
    G4double EcoMugDirectSource::GetHSphereRadius()   { return fMug.GetHSphereRadius() * m; }
    G4double EcoMugDirectSource::GetCylinderRadius()  { return fMug.GetCylinderRadius() * m; }
    G4double EcoMugDirectSource::GetCylinderHeight()  { return fMug.GetCylinderHeight() * m; }
    G4double EcoMugDirectSource::GetMinMomentum()     { return fMug.GetMinimumMomentum() * GeV; }
    G4double EcoMugDirectSource::GetMaxMomentum()     { return fMug.GetMaximumMomentum() * GeV; }
    G4double EcoMugDirectSource::GetMinTheta()        { return fMug.GetMinimumTheta() * rad; }
    G4double EcoMugDirectSource::GetMaxTheta()        { return fMug.GetMaximumTheta() * rad; }

    GenRateResult EcoMugDirectSource::GetAverageGenRate(G4int npoints) {
        double rate, error;
        fMug.GetAverageGenRateAndError(rate, error, npoints);
        return {rate, error};  // returned as a struct
    }


    void EcoMugDirectSource::SetIsotropicMode(G4bool enable)
    {
        if (enable) {
            fMug.SetDifferentialFlux([](double, double) { return 1.0; });
            fIsotropicMode = true;
            G4cout << "EcoMugDirectSource: Isotropic mode ON" << G4endl;
        } else {
            fIsotropicMode = false;
            G4cout << "EcoMugDirectSource: Isotropic mode OFF" << G4endl;
        }
    }

    void EcoMugDirectSource::PrintConfiguration()
    {
        auto r = GetAverageGenRate(1000000);
        fCachedFlux    = r.rate;
        fCachedFluxErr = r.error;
        G4double area  = GetGenerationArea();   // already in m^2
        G4double t = fMug.GetEstimatedTime(1000000); // estimated live time for 1 million generated muons

        G4cout << "\n##########################################################" << G4endl;
        G4cout << "EcoMug generation configuration"                             << G4endl;
        G4cout << "  EcoMug version : " << EcoMugVersion << G4endl;
        G4cout << "  Surface area   : " << area << " m²"                        << G4endl;
        G4cout << "  Flux (norm.)   : " << r.rate << " ± " << r.error << " Hz/m²" << G4endl;
        G4cout << "  Total gen rate : " << r.rate * area << " Hz"               << G4endl;
        G4cout << "  Seed           : " << fSeed << G4endl;
        G4cout << "  Live time/1e6  : " << t << " s (per 1e6 generated muons)" << G4endl;
        G4cout << "  Momentum range : [" << GetMinMomentum()/GeV << ", " << GetMaxMomentum()/GeV << "] GeV/c" << G4endl;
        G4cout << "  Theta range    : [" << GetMinTheta() << ", " << GetMaxTheta() << "] rad (nadir)" << G4endl;
        G4cout << "  Surface type   : " << fSurfaceType << G4endl;
        G4cout << "##########################################################\n" << G4endl;
    }

    void EcoMugDirectSource::SetSeed(std::uint64_t seed)
    {
        fMug.SetSeed(seed);   // EcoMug checks seed > 0 itself
        if (seed > 0) fSeed = seed;
    }