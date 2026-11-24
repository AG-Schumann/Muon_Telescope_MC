#ifndef IsotropicSource_hh
#define IsotropicSource_hh 1

#include "IMuonSource.hh"
#include "G4ThreeVector.hh"
#include "G4UImessenger.hh"
#include "G4String.hh"
#include "G4SystemOfUnits.hh"

#include <cstdint>

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithADoubleAndUnit;
class IsotropicSourceMessenger;

// Generates muons for a pure geometric acceptance calculation.
//
// Two generation surfaces are available:
//
//   Plane      a horizontal square of side fSize, centred at (0, 0, fHeight)
//   Hemisphere the upper half of a sphere of radius fRadius, centred at
//              (0, 0, fCentreZ)
//
// In both cases the direction is sampled from the cosine law about the LOCAL
// surface normal, i.e. pdf proportional to cos(theta_local) dOmega over the
// full hemisphere around that normal. With that sampling the weight of every
// surface element is exactly pi, so
//
//     G = (N_det / N_gen) * A_gen * pi        [m^2 sr]
//
// is an unbiased estimator of the geometric factor, regardless of which
// surface is used. Note that the direction must NOT be restricted to globally
// downward-going: on the hemisphere the local normal tilts away from vertical,
// and cutting the local cosine hemisphere would change the weight per element
// and break the normalisation. Upward-going muons simply never reach the
// panels and so do not contribute to N_det.
//
// The momentum is fixed and high enough that scattering and stopping are
// negligible, so the result depends on the geometry alone.

class IsotropicSource : public IMuonSource {
public:
    enum class Surface { Plane, Hemisphere };

    explicit IsotropicSource(std::uint64_t seed = 0);
    ~IsotropicSource() override;

    bool GetNextEvent(MuonEvent& event) override;

    // Setters (called by the messenger, before /run/beamOn)
    void SetSurface(const G4String& type);
    void SetSize(G4double size)         { fSize = size; }
    void SetHeight(G4double height)     { fHeight = height; }
    void SetRadius(G4double radius)     { fRadius = radius; }
    void SetCentreZ(G4double z)         { fCentreZ = z; }
    void SetMomentum(G4double pGeV)     { fMomentumGeV = pGeV; }

    // Metadata
    G4String GetSourceLabel()  const override { return "Isotropic"; }
    G4String GetSurfaceLabel() const override {
        return (fSurface == Surface::Plane) ? "flat" : "hsphere";
    }
    G4double GetGenerationArea() const override;   // m^2

    // No flux is defined in this mode: the source samples geometry, not an
    // atmospheric spectrum. Zero in RunInfo marks the run as "no rate
    // available"; the analysis branches on the generator label instead.
    G4double GetNormalizedFlux() const override { return 0.; }
    G4double GetFluxError()      const override { return 0.; }
    std::uint64_t GetSeed()      const override { return fSeed; }

    void PrintConfiguration() override;

private:
    G4ThreeVector SampleCosineDirection(const G4ThreeVector& normal) const;

    Surface  fSurface     = Surface::Plane;
    G4double fSize        = 1.0 * CLHEP::m;
    G4double fHeight      = 1.2 * CLHEP::m;
    G4double fRadius      = 1.4 * CLHEP::m;
    G4double fCentreZ     = 0.;
    G4double fMomentumGeV = 10.;
    std::uint64_t fSeed   = 0;

    IsotropicSourceMessenger* fMessenger;   // owned
};


// UI messenger for IsotropicSource. Kept in the same file because it is only
// five commands and has no use outside this class.
//
//   /muonSource/isotropic/surface   plane | hsphere
//   /muonSource/isotropic/size      <value> <unit>   (plane side)
//   /muonSource/isotropic/height    <value> <unit>   (plane height)
//   /muonSource/isotropic/radius    <value> <unit>   (hemisphere radius)
//   /muonSource/isotropic/centreZ   <value> <unit>   (hemisphere centre)
//   /muonSource/isotropic/momentum  <value> <unit>
class IsotropicSourceMessenger : public G4UImessenger {
public:
    explicit IsotropicSourceMessenger(IsotropicSource* source);
    ~IsotropicSourceMessenger() override;
    void SetNewValue(G4UIcommand* command, G4String newValue) override;

private:
    IsotropicSource* fSource;               // not owned
    G4UIdirectory*             fDir;
    G4UIcmdWithAString*        fSurfaceCmd;
    G4UIcmdWithADoubleAndUnit* fSizeCmd;
    G4UIcmdWithADoubleAndUnit* fHeightCmd;
    G4UIcmdWithADoubleAndUnit* fRadiusCmd;
    G4UIcmdWithADoubleAndUnit* fCentreZCmd;
    G4UIcmdWithADoubleAndUnit* fMomentumCmd;
};

#endif

