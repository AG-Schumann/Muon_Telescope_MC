"""Vertical four-fold rate with and without the concrete floors.

The vacuum run and the concrete runs are made with the same generation plane
and the same seed (overburden_scan.sh), so event i is the same EcoMug muon in
every file. The ratio of the rates is therefore taken event by event.

Usage in a notebook:

    import overburden_analysis as ob
    vac = ob.load_run("Overburden/vacuum.root")
    con = ob.load_run("Overburden/slabs5_t25.root")
    ob.summary(vac, con)
"""
import numpy as np
import uproot
from uncertainties import ufloat
from scipy.integrate import quad, cumulative_trapezoid

# Most probable deposit of each panel in four-fold events of the vacuum
# reference run (Landau x Gauss, Section MC_Deposit), MeV. The threshold is a
# property of the detector, so the same values in MeV are used for the runs
# with concrete, whose muon spectrum at the telescope is softer.
E_MPV = np.array([1.718, 1.694, 1.688, 1.711])


def ecomug_horizontal_flux(pmin=0.1, pmax=1000.):
    """Flux through a horizontal plane, m^-2 s^-1, from the EcoMug
    parametrisation J = 1600 (p+2.68)^-3.175 p^0.279 cos^n(theta),
    n = max(0.1, 2.856 - 0.655 ln p): int dp int dOmega J cos(theta)."""
    f = lambda p: 1600*(p + 2.68)**-3.175 * p**0.279
    n = lambda p: max(0.1, 2.856 - 0.655*np.log(p))
    edges = [pmin] + [x for x in (3.6946, 67.197) if pmin < x < pmax] + [pmax]
    return sum(quad(lambda p: f(p)*2*np.pi/(n(p) + 2), a, b, limit=400)[0]
               for a, b in zip(edges[:-1], edges[1:]))


def load_run(path, thresholds=0.5*E_MPV, pmin=0.1, pmax=1000.):
    """Read one run. Returns a dict with
    n_gen, area_m2, T_s (live time from the exact flux), overburden_gcm2,
    edep (n_gen x 4 deposits in MeV), sel4 (four-fold above the thresholds),
    any4 (four-fold with every deposit counted) and the primaries."""
    with uproot.open(path) as f:
        info = f["RunInfo"].arrays(library="np")
        n_gen = int(info["n_generated"][0])
        area = float(info["gen_area_m2"][0])
        surface = str(info["surface"][0])
        overburden = float(info["overburden_gcm2"][0]) if "overburden_gcm2" in info else np.nan
        edep = np.zeros((n_gen, 4))
        for i in range(4):
            h = f[f"Panel{i}Hits"].arrays(["eventID", "edep"], library="np")
            edep[:, i] = np.bincount(h["eventID"], weights=h["edep"], minlength=n_gen)
        prim = f["Primaries"].arrays(["eventID", "px", "py", "pz", "x0", "y0", "z0", "theta_deg"],
                                     library="np")
    if surface != "sky":
        raise ValueError(f"{path}: surface '{surface}'; the exact flux below is for a horizontal plane")
    phi = ecomug_horizontal_flux(pmin, pmax)        # m^-2 s^-1
    T = n_gen / (phi * area)                        # s
    thr = np.asarray(thresholds)
    return dict(path=path, n_gen=n_gen, area_m2=area, T_s=T, overburden_gcm2=overburden,
                edep=edep, sel4=np.all(edep > thr, axis=1), any4=np.all(edep > 0, axis=1),
                prim=prim)


def rate(run, key="sel4"):
    """Four-fold rate in Hz with its Poisson uncertainty."""
    n = int(run[key].sum())
    return ufloat(n, np.sqrt(n)) / run["T_s"]


def paired_ratio(vac, con, key="sel4"):
    """R_con / R_vac from the same muons. a: four-fold in both runs,
    b: only in vacuum, c: only with concrete; a, b, c are independent Poisson
    numbers, r = (a+c)/(a+b), error by linear propagation."""
    if vac["n_gen"] != con["n_gen"] or not np.array_equal(vac["prim"]["pz"], con["prim"]["pz"]):
        raise ValueError("the two runs do not contain the same muons (seed, plane, n_gen?)")
    v, c_ = vac[key], con[key]
    a = np.sum(v & c_); b = np.sum(v & ~c_); c = np.sum(~v & c_)
    r = (a + c) / (a + b)
    var = ((b - c)**2 * a + (a + c)**2 * b + (a + b)**2 * c) / (a + b)**4
    return ufloat(r, np.sqrt(var)), (int(a), int(b), int(c))


def edge_check(run, key="sel4", half_size=1.2):
    """Largest |x0|, |y0| (m) of the four-fold muons on the generation plane
    and the fraction within 0.2 m of the edge. Should be ~0."""
    m = run[key]
    x = run["prim"]["x0"][m] / 1000.; y = run["prim"]["y0"][m] / 1000.
    r = np.maximum(np.abs(x), np.abs(y))
    return r.max() if r.size else np.nan, np.mean(r > half_size - 0.2) if r.size else np.nan


def summary(vac, *cons, key="sel4"):
    rv = rate(vac, key)
    print(f"vacuum      : N4 = {vac[key].sum():6d}  R = {rv:.3f} Hz   (T = {vac['T_s']:.1f} s)")
    for con in cons:
        rc = rate(con, key)
        r, (a, b, c) = paired_ratio(vac, con, key)
        xmax, fedge = edge_check(con, key)
        print(f"{con['overburden_gcm2']:6.1f} g/cm2: N4 = {con[key].sum():6d}  R = {rc:.3f} Hz   "
              f"R/R_vac = {r:.4f}   (a, b, c = {a}, {b}, {c};  max|x0,y0| = {xmax:.2f} m, "
              f"edge fraction {fedge:.4f})")


# Straight-line model: no scattering, no secondaries, no threshold.
# A muon is counted if its straight line crosses both outer panels (zero
# thickness, Eq. Geometric_Factor_Rect) and its CSDA range exceeds the
# concrete along the line plus the three panels above P3.
_ME, _MMU, _K = 0.51099895, 105.6583755, 0.307075          # MeV, MeV, MeV cm2/g
# PDG "Concrete, Portland": Z/A, I (eV), Sternheimer C, x0, x1, a, k
CONCRETE = dict(ZA=0.50274, I_eV=135.2, C=3.9464, x0=0.1301, x1=3.0466, a=0.07515, k=3.5467)


def bethe(p, ZA, I_eV, C, x0, x1, a, k):
    """Mean energy loss of a muon (Bethe, density effect), MeV cm2/g; p in MeV/c."""
    bg = p / _MMU; g = np.sqrt(1 + bg**2); b2 = bg**2 / g**2
    wmax = 2*_ME*bg**2 / (1 + 2*g*_ME/_MMU + (_ME/_MMU)**2)
    x = np.log10(bg)
    delta = np.where(x >= x1, 2*np.log(10)*x - C,
                     np.where(x >= x0, 2*np.log(10)*x - C + a*np.clip(x1 - x, 0, None)**k, 0.))
    return _K*ZA/b2 * (0.5*np.log(2*_ME*bg**2*wmax/(I_eV*1e-6)**2) - b2 - delta/2)


def _csda_table(material=CONCRETE):
    p = np.logspace(np.log10(5.), 6., 6000)                  # MeV/c
    dEdp = p / np.sqrt(p**2 + _MMU**2)
    return p, cumulative_trapezoid(dEdp / bethe(p, **material), p, initial=0)   # g/cm2


_trapz = getattr(np, "trapezoid", None) or np.trapz        # numpy < 2.0 has only trapz


def transmission_model(X_gcm2, X_panels=3.4, Z=88.5, side=28., N=60, pmin=0.1, pmax=1000.):
    """Four-fold rate under X g/cm2 of concrete relative to no concrete,
    straight lines. X_panels: the three panels above P3 (3.1 g/cm2 PVT,
    about 3.4 g/cm2 concrete-equivalent by dE/dx). Returns an array."""
    p_tab, R_tab = _csda_table()
    xi = (np.arange(N) + 0.5) * side / N
    XI, ETA = np.meshgrid(xi, xi, indexing="ij")
    r2 = (Z**2 + XI**2 + ETA**2)[..., None]
    cos = Z / np.sqrt(r2)                                     # (N, N, 1)
    pg = np.logspace(np.log10(pmin), np.log10(pmax), 1200)   # GeV/c
    f = 1600*(pg + 2.68)**-3.175 * pg**0.279
    n = np.maximum(0.1, 2.856 - 0.655*np.log(pg))
    w = 4*Z**(n + 2) * (side - XI[..., None])*(side - ETA[..., None]) / r2**((n + 4)/2)
    integrand = f * w * pg                                    # d/d(ln p)
    def rate(X):
        if X <= 0:
            pcut = np.zeros_like(cos)
        else:
            pcut = np.interp((X + X_panels)/cos, R_tab, p_tab) / 1000.   # GeV/c
        y = np.where(pg >= pcut, integrand, 0.)
        return _trapz(y, np.log(pg), axis=-1).sum()
    r0 = rate(0.)
    return np.array([rate(X)/r0 for X in np.atleast_1d(X_gcm2)])


# Vertical sea-level muon spectra, m^-2 s^-1 sr^-1 (GeV/c)^-1, p in GeV/c.
_MMU_GEV = _MMU / 1000.


def ecomug_vertical(p):
    """EcoMug default parametrisation (ADAMO), theta = 0."""
    return 1600*(p + 2.68)**-3.175 * p**0.279


def gaisser_vertical(p, cos_theta=1.0):
    """Gaisser formula as given in the PDG cosmic-ray review; valid only where
    muon decay is negligible, E > 100 GeV / cos(theta)."""
    E = np.sqrt(p**2 + _MMU_GEV**2)
    dNdE = 0.14*E**-2.7*(1/(1 + 1.1*E*cos_theta/115) + 0.054/(1 + 1.1*E*cos_theta/850))
    return dNdE*1e4 * p/E                                   # cm^-2 -> m^-2, dE -> dp


def guan_vertical(p, cos_theta=1.0):
    """Modified Gaisser formula of Guan et al., arXiv:1509.06176 (fit to world
    data, low energies and all zenith angles)."""
    P1, P2, P3, P4, P5 = 0.102573, -0.068287, 0.958633, 0.040725, 0.817285
    c = cos_theta
    cs = np.sqrt((c**2 + P1**2 + P2*c**P3 + P4*c**P5) / (1 + P1**2 + P2 + P4))
    E = np.sqrt(p**2 + _MMU_GEV**2)
    dNdE = 0.14*(E*(1 + 3.64/(E*cs**1.29)))**-2.7 * (1/(1 + 1.1*E*cs/115) + 0.054/(1 + 1.1*E*cs/850))
    return dNdE*1e4 * p/E


# Hebbeker & Timmermans, world average of the vertical spectrum
# (arXiv:hep-ph/0102042): p (GeV/c) -> (flux, error), m^-2 s^-1 sr^-1 GeV^-1
HEBBEKER_TIMMERMANS = {10: (1.19, 0.08), 100: (2.59e-3, 0.19e-3), 1000: (1.52e-6, 0.26e-6)}


def integral_vertical(spectrum, pmin, pmax=1e4):
    """Integral vertical intensity above pmin, m^-2 s^-1 sr^-1."""
    return quad(spectrum, pmin, pmax, limit=500)[0]