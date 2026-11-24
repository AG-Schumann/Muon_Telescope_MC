"""
sim_tables.py -- per-panel numbers of the simulation for the thesis

The steps of LG_energy_photons.ipynb, applied to the runs of run_scans.sh:

  A. energy deposit in the scintillator of each panel, all hits and four-fold
     events; most probable value from a Moyal (Landau) fit
  B. momentum of the four-fold muons, MIP band
  C. light guide: geometric path, deposit, Cherenkov photons, pulse in units
     of the muon signal, fractions above the thresholds as a function of c
  D. rates at zenith 0 and over the scan
  E. LaTeX tables and macros

A-C use the vertical runs (theta_0.root) of
    ecomug_mode0  scintillators only
    ecomug_mode2  scintillators + light guides
    ecomug_flip1  as mode 2, guide of panel 1 on the other side
Panels 0 (top) ... 3 (bottom) = lab P1, P0, P3, P2.
D uses scan_analysis.py (same folder).
"""

from __future__ import annotations

import os
import warnings

import numpy as np
import pandas as pd
import uproot
from scipy import integrate
from scipy.optimize import brentq, minimize
from scipy.stats import moyal as _moyal

import scan_analysis as sa

# constants (as in LG_energy_photons.ipynb)
MMU, ME = 105.6583755, 0.51099895            # MeV
K = 0.307075                                 # 4 pi N_A r_e^2 m_e c^2, MeV cm^2/mol (PDG)
Y_SC = 10_000                                # EJ-200 light yield, photons/MeV
N_PMMA = 1.49
LAMBDA_NM = (300, 500)                       # photocathode range, nm
DE_EV = 1239.84 / LAMBDA_NM[0] - 1239.84 / LAMBDA_NM[1]
CHER_K = 370.0                               # alpha/(hbar c), photons/(eV cm)
PVT = dict(ZA=0.54141, I=64.7e-6, rho=1.023, Cbar=3.1997, X0=0.1464, X1=2.4855, a=0.16101, k=3.2393)
PMMA = dict(ZA=0.53937, I=74.0e-6, rho=1.190, Cbar=3.3297, X0=0.1824, X1=2.6681, a=0.11433, k=3.3836)

# geometry at zenith 0 (DetectorConstruction, optical-light-collection branch)
SC_HALF, SC_HALF_T = 140.0, 5.0              # mm
LG_X0, LG_LEN = 140.0, 70.0                  # guide from |x| = 140 to 210 mm
LG_HW = (140.0, 30.0)                        # half width (y), scintillator end -> PMT end
LG_HT = (5.0, 25.0)                          # half thickness (z)
LG_SIDE = -1                                 # kLGSide: guide on the -x side

# thresholds in units of the most probable signal, panels top -> bottom (lab P1, P0, P3, P2)
THRESHOLDS = {
    "lo":  np.array([0.145, 0.097, 0.108, 0.097]),   # -30 mV, effective (tab::threshold_energy)
    "adj": np.array([0.52, 0.43, 0.48, 0.49]),       # adjusted branch, erf fits (Section Counter_Waveforms)
    "110": np.array([0.353, 0.390, 0.347, 0.310]),   # -110 mV (lab scaler, TBS)
    "half": np.full(4, 0.5),                          # nominal value of the adjusted threshold
}
A_ADJ_VERTICAL = np.array([0.504, 0.43, 0.499, 0.479])   # same mV values / vertical A_MPV
MEASURED = {"adj": (0.866, 0.007), "110": (0.875, 0.005)}   # R_x / R_-30 at zenith 0
C_VALUES = {2.75: "standard PMMA", 3.6: "UV-transmitting PMMA"}   # c from the optical branch
LAB_LINES, LAB_NOLG = "Straight lines", "Geant4, no guides"


# physics
def density_delta(bg, m):
    X = np.log10(bg)
    d = 2*np.log(10)*X - m["Cbar"]
    d = np.where((X >= m["X0"]) & (X < m["X1"]), d + m["a"]*np.clip(m["X1"] - X, 0, None)**m["k"], d)
    return np.where(X < m["X0"], 0.0, d)


def bethe(bg, m, M=MMU):
    """Mean mass stopping power, MeV cm^2/g (PDG Eq. 34.5)."""
    g = np.sqrt(1 + bg**2); b2 = bg**2/g**2
    wmax = 2*ME*bg**2/(1 + 2*g*ME/M + (ME/M)**2)
    return K*m["ZA"]/b2*(0.5*np.log(2*ME*bg**2*wmax/m["I"]**2) - b2 - density_delta(bg, m)/2)


def landau_mpv(bg, m, x):
    """Most probable loss (MeV) in x g/cm^2 (Landau-Vavilov-Bichsel, PDG Eq. 34.12)."""
    g = np.sqrt(1 + bg**2); b2 = bg**2/g**2
    xi = K/2*m["ZA"]*x/b2
    return xi*(np.log(2*ME*bg**2/m["I"]) + np.log(xi/m["I"]) + 0.2 - b2 - density_delta(bg, m))


def landau_kappa(bg, x, m=PVT):
    """Vavilov kappa = xi / W_max; kappa < 0.01 is the Landau regime."""
    g = np.sqrt(1 + bg**2); b2 = bg**2/g**2
    wmax = 2*ME*bg**2/(1 + 2*g*ME/MMU + (ME/MMU)**2)
    return K/2*m["ZA"]*x/b2/wmax


def cherenkov_per_cm(beta, n=N_PMMA):
    """Cherenkov photons per cm in the photocathode range."""
    return CHER_K*np.clip(1 - 1/(n*np.asarray(beta))**2, 0, None)*DE_EV


def beta_from_T(T):
    g = np.asarray(T)/MMU + 1
    return np.sqrt(1 - 1/g**2)


def bg_from_T(T):
    return np.sqrt((np.asarray(T)/MMU + 1)**2 - 1)


X_SC = PVT["rho"]*1.0                                 # 1 cm EJ-200, g/cm^2
_BG = np.logspace(-0.5, 4, 3000)
_MPV_CURVE = landau_mpv(_BG, PVT, X_SC)
MPV_LV_MIN = float(_MPV_CURVE.min())
MPV_LV_PLATEAU = float(landau_mpv(1e3, PVT, X_SC))
BG_MIP = brentq(lambda b: landau_mpv(b, PVT, X_SC) - 1.1*MPV_LV_MIN, 0.5, _BG[np.argmin(_MPV_CURVE)])


# loading (zenith 0)
def load_detail(path):
    """Per-event arrays of a vertical run, one row per event with a deposit.

    E_sc, E_lg            (N, 4)  deposits in scintillator / light guide (MeV)
    E_prim_sc, E_prim_lg  (N, 4)  part of it from the primary muon (parentID 0)
    T_in_sc, T_in_lg      (N, 4)  kinetic energy of the muon at its first step there (MeV, nan if none)
    p (N,) MeV/c, u (3, N) direction, x0, y0, z0 (N,) mm   (generated muon)
    zc (4,) panel centres (mm), n_gen
    """
    if os.path.basename(path).startswith("theta_") and sa.theta_of(path) != 0:
        raise ValueError("load_detail is for the vertical runs (theta_0.root)")
    cols = ["eventID", "parentID", "edep", "x", "z", "time", "kineticEnergy"]
    with uproot.open(path) as f:
        n_gen = int(f["RunInfo"].arrays(["n_generated"], library="np")["n_generated"].sum())
        steps = [f[f"Panel{i}Hits"].arrays(cols, library="np") for i in range(4)]
        pr = f["Primaries"].arrays(["eventID", "primaryID", "px", "py", "pz", "x0", "y0", "z0"],
                                   library="np")
    ids = np.unique(np.concatenate([s["eventID"] for s in steps]))
    n = len(ids)
    out = {k: np.zeros((n, 4)) for k in ("E_sc", "E_lg", "E_prim_sc", "E_prim_lg")}
    out.update({k: np.full((n, 4), np.nan) for k in ("T_in_sc", "T_in_lg")})
    zc = np.zeros(4)
    for i, s in enumerate(steps):
        idx = np.searchsorted(ids, s["eventID"])
        g = np.abs(s["x"]) > SC_HALF + 1e-3          # pre-step point (as scan_analysis)
        zs = s["z"][~g]
        zc[i] = 0.5*(zs.min() + zs.max())
        prim = s["parentID"] == 0
        for reg, m in (("sc", ~g), ("lg", g)):
            out[f"E_{reg}"][:, i] = np.bincount(idx[m], s["edep"][m], minlength=n)
            sel = m & prim
            out[f"E_prim_{reg}"][:, i] = np.bincount(idx[sel], s["edep"][sel], minlength=n)
            e, t, T = idx[sel], s["time"][sel], s["kineticEnergy"][sel]
            if e.size:
                o = np.lexsort((t, e)); e, T = e[o], T[o]
                first = np.r_[True, e[1:] != e[:-1]]
                out[f"T_in_{reg}"][e[first], i] = T[first]
    first = pr["primaryID"] == 1
    j = np.searchsorted(ids, pr["eventID"][first])
    ok = (j < n) & (ids[np.minimum(j, n - 1)] == pr["eventID"][first])
    P = {k: np.full(n, np.nan) for k in ("px", "py", "pz", "x0", "y0", "z0")}
    for k in P:
        P[k][j[ok]] = pr[k][first][ok]
    p = np.sqrt(P["px"]**2 + P["py"]**2 + P["pz"]**2)            # GeV/c
    out.update(p=p*1e3, u=np.vstack([P["px"], P["py"], P["pz"]])/p,
               x0=P["x0"], y0=P["y0"], z0=P["z0"], eventID=ids, zc=zc, n_gen=n_gen)
    return out


def four_fold(d, guides=False):
    """Any deposit in all four modules (scintillator, or scintillator + guide)."""
    E = d["E_sc"] + d["E_lg"] if guides else d["E_sc"]
    return np.all(E > 0, axis=1)


# A. fits of the deposit spectrum
# Landau's density phi(lambda) = 1/pi int_0^inf exp(-t ln t - lambda t) sin(pi t) dt, mode at -0.22278.
# The deposit Delta of a muon in a thin layer follows phi((Delta - Delta_0)/xi)/xi.
_LAM = np.concatenate([np.linspace(-3.8, 20, 2400), np.geomspace(20.05, 2000, 400)])
with warnings.catch_warnings():                      # quad warns about round-off far in the tail (phi < 1e-6)
    warnings.simplefilter("ignore", integrate.IntegrationWarning)
    _PHI = np.array([integrate.quad(lambda t, l=l: np.exp(-t*np.log(t) - l*t)*np.sin(np.pi*t) if t > 0 else 0.0,
                                    0, np.inf, limit=400)[0]/np.pi for l in _LAM])
LANDAU_MODE = -0.22278
_GRID = np.arange(0.0, 10.0001, 0.002)               # MeV


def landau_std(lam):
    lam = np.asarray(lam, float)
    return np.where(lam > _LAM[-1], 1/np.maximum(lam, 1)**2, np.interp(lam, _LAM, _PHI, left=0.0))


def langau(x, mpv_l, xi, sigma):
    """Landau (mode mpv_l, width xi) convolved with a Gaussian (sigma), density at x (MeV)."""
    f = landau_std((_GRID - (mpv_l - LANDAU_MODE*xi))/xi)/xi
    dx = _GRID[1] - _GRID[0]
    if sigma > 2*dx:
        k = np.arange(-int(5*sigma/dx), int(5*sigma/dx) + 1)*dx
        w = np.exp(-0.5*(k/sigma)**2)
        f = np.convolve(f, w/w.sum(), "same")
    return np.interp(x, _GRID, f)


def _mode(f_grid):
    j = int(np.clip(np.argmax(f_grid), 1, len(f_grid) - 2))
    y0, y1, y2 = f_grid[j - 1], f_grid[j], f_grid[j + 1]
    return _GRID[j] + 0.5*(y0 - y2)/(y0 - 2*y1 + y2)*(_GRID[1] - _GRID[0])


def _hessian(f, x, h):
    x = np.asarray(x, float); n = x.size
    H = np.zeros((n, n))
    for i in range(n):
        for j in range(i, n):
            ei, ej = np.eye(n)[i]*h[i], np.eye(n)[j]*h[j]
            H[i, j] = H[j, i] = (f(x + ei + ej) - f(x + ei - ej) - f(x - ei + ej) + f(x - ei - ej))/(4*h[i]*h[j])
    return H


def _window(E, lo, hi, bw):
    E = np.asarray(E); E = E[E > 0]
    h, edges = np.histogram(E, bins=np.arange(0.3, 6.0 + bw, bw))
    k = int(np.argmax(h)); pk = 0.5*(edges[k] + edges[k + 1])
    a, b = lo*pk, hi*pk
    return E[(E > a) & (E < b)], a, b, pk


def _chi2(x, a, b, bw, cdf):
    """Pearson chi^2 of the bw histogram in the window (bins with >= 5 expected entries)."""
    hb, eb = np.histogram(x, bins=np.arange(a, b + 1e-9, bw))
    c = cdf(eb)
    expct = x.size*np.diff(c)/(c[-1] - c[0])
    ok = expct >= 5
    return float(np.sum((hb[ok] - expct[ok])**2/expct[ok])), int(ok.sum())


def fit_langau(E, lo=0.6, hi=2.0, bw=0.05):
    """Most probable value of a deposit spectrum from an unbinned maximum-likelihood fit
    of Landau x Gauss in the window lo..hi x the peak of a histogram with bw-wide bins,
    the function normalised in the window. mu is the maximum of the fitted function.
    Returns dict(kind, mu, err, xi, sigma, mpv_l, n_fit, chi2, ndf, peak, window)."""
    x, a, b, pk = _window(E, lo, hi, bw)
    win = (_GRID >= a) & (_GRID <= b)

    def grid(q):
        return langau(_GRID, q[0], np.exp(q[1]), 0.003 + np.exp(q[2]))

    def nll(q):
        f = grid(q)
        return -np.sum(np.log(np.maximum(np.interp(x, _GRID, f), 1e-300))) + \
            x.size*np.log(integrate.trapezoid(f[win], _GRID[win]))

    r = minimize(nll, (pk, np.log(0.08), np.log(0.05)), method="Nelder-Mead",
                 options=dict(xatol=1e-6, fatol=1e-6, maxiter=4000))
    q = r.x
    cov = np.linalg.pinv(_hessian(nll, q, (1e-4, 1e-3, 1e-3)))
    h = np.array([2e-3, 2e-2, 2e-2])
    jac = np.array([(_mode(grid(q + np.eye(3)[i]*h[i])) - _mode(grid(q - np.eye(3)[i]*h[i])))/(2*h[i])
                    for i in range(3)])
    f = grid(q)
    cum = np.concatenate([[0], np.cumsum(0.5*(f[1:] + f[:-1])*np.diff(_GRID))])
    chi2, nb = _chi2(x, a, b, bw, lambda e: np.interp(e, _GRID, cum))
    return dict(kind="langau", mu=float(_mode(f)), err=float(np.sqrt(max(jac @ cov @ jac, 0))),
                xi=float(np.exp(q[1])), sigma=float(0.003 + np.exp(q[2])), mpv_l=float(q[0]),
                n_fit=int(x.size), chi2=chi2, ndf=nb - 3, peak=pk, window=(a, b))


def fit_moyal(E, lo=0.6, hi=2.0, bw=0.05):
    """As fit_langau with the Moyal function exp(-(z + e^-z)/2), z = (E - mu)/s
    (the approximation of the Landau distribution used in LG_energy_photons.ipynb)."""
    x, a, b, pk = _window(E, lo, hi, bw)

    def nll(q):
        mu, s = q[0], np.exp(q[1])
        return -np.sum(_moyal.logpdf(x, mu, s)) + x.size*np.log(_moyal.cdf(b, mu, s) - _moyal.cdf(a, mu, s))

    r = minimize(nll, (pk, np.log(0.12)), method="Nelder-Mead",
                 options=dict(xatol=1e-7, fatol=1e-7, maxiter=4000))
    cov = np.linalg.pinv(_hessian(nll, r.x, (1e-4, 1e-3)))
    mu, s = float(r.x[0]), float(np.exp(r.x[1]))
    chi2, nb = _chi2(x, a, b, bw, lambda e: _moyal.cdf(e, mu, s))
    return dict(kind="moyal", mu=mu, err=float(np.sqrt(cov[0, 0])), s=s, n_fit=int(x.size),
                chi2=chi2, ndf=nb - 2, peak=pk, window=(a, b))


fit_mpv = fit_langau


def fit_curve(fit, x, bw):
    """Fitted function in counts per bw-wide bin (for plots), inside its window."""
    a, b = fit["window"]
    x = np.asarray(x, float)
    if fit["kind"] == "moyal":
        pdf = _moyal.pdf(x, fit["mu"], fit["s"])/(_moyal.cdf(b, fit["mu"], fit["s"]) - _moyal.cdf(a, fit["mu"], fit["s"]))
    else:
        g = (_GRID >= a) & (_GRID <= b)
        f = langau(_GRID, fit["mpv_l"], fit["xi"], fit["sigma"])
        pdf = np.interp(x, _GRID, f)/integrate.trapezoid(f[g], _GRID[g])
    return np.where((x >= a) & (x <= b), fit["n_fit"]*bw*pdf, np.nan)


# straight-track geometry
def chord(d, m, i, region="lg", side=LG_SIDE, n_s=501, chunk=4000):
    """Geometric path (cm) of the straight primary track in the scintillator ("sc")
    or the light guide ("lg") of panel i, for the events m (bool mask)."""
    idx = np.nonzero(m)[0]
    out = np.zeros(len(idx))
    half_t = SC_HALF_T if region == "sc" else LG_HT[1]
    with np.errstate(divide="ignore", invalid="ignore"):
        for a in range(0, len(idx), chunk):
            k = idx[a:a + chunk]
            x0, y0, z0 = d["x0"][k], d["y0"][k], d["z0"][k]
            ux, uy, uz = d["u"][:, k]
            z = d["zc"][i] + np.linspace(-half_t, half_t, n_s)[:, None]
            t = (z - z0)/uz
            x, y = x0 + ux*t, y0 + uy*t
            if region == "sc":
                inside = (np.abs(x) <= SC_HALF) & (np.abs(y) <= SC_HALF)
            else:
                xs = side*x
                s = np.clip((xs - LG_X0)/LG_LEN, 0, 1)
                hw = LG_HW[0] + (LG_HW[1] - LG_HW[0])*s
                ht = LG_HT[0] + (LG_HT[1] - LG_HT[0])*s
                inside = (xs >= LG_X0) & (xs <= LG_X0 + LG_LEN) & (np.abs(y) <= hw) & (np.abs(z - d["zc"][i]) <= ht)
            out[a:a + chunk] = inside.sum(axis=0)*(2*half_t/(n_s - 1))/np.abs(uz)/10.0
    return np.where(np.isfinite(out), out, 0.0)


# A. deposit table
def deposit_table(d0):
    """Per panel, all hits and four-fold events of a scintillator-only run.
    Thresholds are taken in units of the four-fold MPV of the panel, as in the lab.
    Returns (table, mpv4, fits)."""
    four = four_fold(d0)
    fits4 = [fit_langau(d0["E_sc"][four, i]) for i in range(4)]
    mpv4 = np.array([f["mu"] for f in fits4])
    rows, fits = [], {}
    for name, sel in (("all hits", np.ones(len(four), bool)), ("four-fold", four)):
        for i in range(4):
            m = sel & (d0["E_sc"][:, i] > 0)
            E = d0["E_sc"][m, i]
            f = fits4[i] if name == "four-fold" else fit_langau(E)
            fm = fit_moyal(E)
            fits[(name, i)] = (f, fm)
            L = chord(d0, m, i, "sc")
            T = d0["T_in_sc"][m, i]
            T = np.where(np.isfinite(T), T, np.sqrt(d0["p"][m]**2 + MMU**2) - MMU)
            Lm = float(np.median(L[L > 0]))
            row = dict(panel=i, selection=name, N=E.size, MPV=f["mu"], MPV_err=f["err"], xi=f["xi"],
                       sigma=f["sigma"], chi2=f["chi2"], ndf=f["ndf"],
                       MPV_moyal=fm["mu"], MPV_moyal_err=fm["err"], s_moyal=fm["s"],
                       chi2_moyal=fm["chi2"], ndf_moyal=fm["ndf"],
                       median=np.median(E), mean=E.mean(),
                       L_median=Lm, MPV_LV=float(landau_mpv(np.median(bg_from_T(T)), PVT, PVT["rho"]*Lm)),
                       N_sc_MPV=Y_SC*f["mu"], N_sc_median=Y_SC*np.median(E))
            for key, a in THRESHOLDS.items():
                row[f"f_{key}"] = np.mean(E >= a[i]*mpv4[i])
            rows.append(row)
    return pd.DataFrame(rows), mpv4, fits


def k_cal(d0):
    """Geometric / energy-based path in the scintillator (four-fold, per panel),
    L_E = E_prim / (<dE/dx>_min rho). Corrects the energy-based path for the Landau shape."""
    four = four_fold(d0)
    cal = []
    for i in range(4):
        m = four & (d0["E_prim_sc"][:, i] > 0)
        L_geo = chord(d0, m, i, "sc")
        L_est = d0["E_prim_sc"][m, i]/(bethe(4.0, PVT)*PVT["rho"])
        g = L_geo > 0.5
        cal.append(np.median(L_geo[g]/L_est[g]))
    return float(np.mean(cal)), np.array(cal)


# B. MIP band
def mip_table(d0):
    """Momentum of the four-fold muons and the MIP band (MPV within 10 % of its minimum)."""
    four = four_fold(d0)
    p = d0["p"][four]
    rows = []
    for i in range(4):
        T = d0["T_in_sc"][four, i]; T = T[np.isfinite(T)]
        rows.append(dict(panel=i, T_entry_median=np.median(T)/1e3, T_entry_min=T.min(),
                         f_mip_entry=np.mean(bg_from_T(T) > BG_MIP)))
    Esum = d0["E_sc"][four].sum(axis=1)
    pc = np.percentile(p, (1, 5, 16, 50, 84, 95, 99))/1e3
    info = dict(n_four=int(four.sum()), p_percentiles=dict(zip((1, 5, 16, 50, 84, 95, 99), pc)),
                p_min=float(p.min()), T_min=float(np.sqrt(p.min()**2 + MMU**2) - MMU),
                f_mip=float(np.mean(p > BG_MIP*MMU)), p_mip=BG_MIP*MMU,
                inclination=float(np.mean(1/np.abs(d0["u"][2][four]))),
                Esum_median=float(np.median(Esum)), Esum_mean=float(Esum.mean()), Esum_min=float(Esum.min()))
    bg4 = np.median(p)/MMU
    info["MPV_LV_four"] = float(landau_mpv(bg4, PVT, X_SC*info["inclination"]))
    info["kappa"] = float(landau_kappa(bg4, X_SC))
    return pd.DataFrame(rows), info


# C. light guide
def guide_crossings(d2, flip_panel=None, kcal=1.0):
    """One row per panel reached only through its guide in a four-fold event
    (any deposit in all four modules, no deposit in this scintillator), as in tab::lg_runs."""
    four = four_fold(d2, guides=True)
    rows = []
    for i in range(4):
        m = four & (d2["E_lg"][:, i] > 0) & (d2["E_sc"][:, i] == 0)
        if not m.any():
            continue
        side = -LG_SIDE if i == flip_panel else LG_SIDE
        L = chord(d2, m, i, "lg", side)
        T = d2["T_in_lg"][m, i]
        T = np.where(np.isfinite(T), T, np.sqrt(d2["p"][m]**2 + MMU**2) - MMU)
        beta = beta_from_T(T)
        rows.append(pd.DataFrame(dict(
            panel=i, eventID=d2["eventID"][m], E_lg=d2["E_lg"][m, i], E_prim_lg=d2["E_prim_lg"][m, i],
            L_geo=L, L_est=kcal*d2["E_prim_lg"][m, i]/(bethe(4.0, PMMA)*PMMA["rho"]),
            T_in=T, beta=beta, N_ch=cherenkov_per_cm(beta)*L,
            N_ch_scan=sa.NCH_PER_CM*d2["E_lg"][m, i]/sa.DEDX_PMMA)))   # model used in scan_analysis
    return pd.concat(rows, ignore_index=True)


def k_classes(d2):
    """Four-fold events (any deposit) by the number k of panels reached only through the guide."""
    four = four_fold(d2, guides=True)
    k = ((d2["E_lg"] > 0) & (d2["E_sc"] == 0)).sum(axis=1)
    return np.array([int(np.sum(four & (k == kk))) for kk in range(5)])


def pulse(cross, mpv4, c):
    """Pulse of each guide crossing in units of the muon signal of its panel, a = c N_ch / N_sc(MPV)."""
    return c*cross.N_ch.to_numpy()/(Y_SC*np.asarray(mpv4)[cross.panel.to_numpy()])


def guide_table(cross, mpv4, c=2.75):
    """Per panel and in total: path, deposit, photons, pulse at c, fractions above the thresholds."""
    a = pulse(cross, mpv4, c)
    groups = [(str(i), (cross.panel == i).to_numpy()) for i in sorted(cross.panel.unique())]
    rows = []
    for name, m in groups + [("all", np.ones(len(cross), bool))]:
        d = cross[m]; g = (d.L_geo > 0).to_numpy()
        thr = {k: v[d.panel.to_numpy()] for k, v in THRESHOLDS.items()}
        row = dict(panel=name, crossings=int(m.sum()), f_no_path=float(np.mean(~g)),
                   E_lg_median=d.E_lg.median(), L_geo_median=d.L_geo[g].median(),
                   L_est_median=d.L_est[g].median(), beta_median=d.beta.median(),
                   N_ch_median=d.N_ch[g].median(), N_ch_mean=d.N_ch[g].mean(),
                   N_ch_90=d.N_ch[g].quantile(0.9),
                   scan_over_geo=float(np.median(d.N_ch_scan[g]/d.N_ch[g])),
                   ratio_median=float(np.median(d.N_ch[g]/(Y_SC*np.asarray(mpv4)[d.panel[g]]))),
                   a_median=float(np.median(a[m][g])))
        for k in THRESHOLDS:
            row[f"p_{k}"] = float(np.mean(a[m] >= thr[k]))
        rows.append(row)
    return pd.DataFrame(rows)


def c_curve(cross, mpv4, p_target=0.6, c_grid=None):
    """Fractions of guide crossings above each threshold as a function of c."""
    c_grid = np.logspace(-0.5, 1.5, 400) if c_grid is None else c_grid
    r = (cross.N_ch/(Y_SC*np.asarray(mpv4)[cross.panel])).to_numpy()
    thr = {k: v[cross.panel.to_numpy()] for k, v in THRESHOLDS.items()}
    p = {k: np.array([np.mean(r*c >= t) for c in c_grid]) for k, t in thr.items()}
    c_fit = float(np.interp(p_target, p["lo"], c_grid))
    return dict(c_grid=c_grid, p=p, c_fit=c_fit, p_target=p_target,
                at_fit={k: float(np.interp(c_fit, c_grid, v)) for k, v in p.items()},
                at_c={c: {k: float(np.interp(c, c_grid, v)) for k, v in p.items()} for c in C_VALUES})


# D. rates
def _counts(ev, mpv4, c, thresholds=None):
    """Four-fold flags of one run with the Cherenkov model (c = 0: scintillator only)."""
    thresholds = thresholds or THRESHOLDS
    a = sa.panel_amplitude(ev, mpv4, c)
    out = {"any": np.all((ev["E_sc"] + ev["E_lg"]) > 0, axis=1).astype(float)}
    for k, t in thresholds.items():
        out[k] = np.all(a >= t, axis=1).astype(float)
    return out


def rate_row(ev, mpv4, c, T, thresholds=None):
    f = _counts(ev, mpv4, c, thresholds)
    row = {}
    for k, v in f.items():
        row[f"R_{k}"], row[f"R_{k}_err"] = v.sum()/T, np.sqrt(v.sum())/T
    for k in ("adj", "110"):
        row[f"r_{k}"], row[f"r_{k}_err"] = sa.ratio([f[k]], [f["lo"]])
    return row


def vertical_rates(scan_root, mpv4, c_values=tuple(C_VALUES), flip="ecomug_flip1", thresholds=None):
    """Four-fold rates at zenith 0: straight lines, Geant4 without and with guides.
    Returns (table, live time)."""
    rows = [dict(model=LAB_LINES, R_any=float(sa.line_model([0.])[0]))]
    ev0 = sa.load_events(os.path.join(scan_root, "ecomug_mode0", "theta_0.root"))
    T = sa.ecomug_live_time(ev0["n_gen"], 0.1)
    rows.append(dict(model=LAB_NOLG, c=0.0, **rate_row(ev0, mpv4, 0.0, T, thresholds)))
    for scan, lab in (("ecomug_mode2", "Guides"), (flip, "Panel 1 turned")):
        path = os.path.join(scan_root, scan, "theta_0.root")
        if not os.path.exists(path):
            continue
        ev = sa.load_events(path)
        for c in c_values:
            rows.append(dict(model=f"{lab}, $c = {c:g}$", c=c, **rate_row(ev, mpv4, c, T, thresholds)))
    rows.append(dict(model="Measured", r_adj=MEASURED["adj"][0], r_adj_err=MEASURED["adj"][1],
                     r_110=MEASURED["110"][0], r_110_err=MEASURED["110"][1]))
    return pd.DataFrame(rows), T


def angle_table(scan_root, mpv4, c=2.75, a_cry=0.5):
    """One row per zenith angle: straight lines, Geant4 (mode 0), ratio, R_adj/R_-30
    normal and turned with the Cherenkov model, paired turned/normal; CRY muons / EcoMug
    and CRY all / CRY muons with all four scintillator deposits above a_cry x E_MPV
    (as in the CRY figure of the scan analysis)."""
    def get(name):
        p = os.path.join(scan_root, name)
        return sa.load_scan(p) if os.path.isdir(p) else {}
    m0, m2, fl, cm, ca = (get(n) for n in ("ecomug_mode0", "ecomug_mode2", "ecomug_flip1",
                                           "cry_muons", "cry_all"))
    th = np.array(sorted(m0))
    lines = sa.line_model(th)
    pair = sa.cherenkov_paired(scan_root, "ecomug_mode2", "ecomug_flip1", mpv4,
                               THRESHOLDS["lo"], THRESHOLDS["adj"], c) if m2 and fl else None
    half = {"half": np.full(4, a_cry)}

    def r_half(ev, T):
        n = _counts(ev, mpv4, 0.0, half)["half"].sum()          # scintillator only
        return n/T, np.sqrt(n)/T

    rows = []
    for t, rl in zip(th, lines):
        T = sa.ecomug_live_time(m0[t]["n_gen"], 0.1)
        n4 = np.all(m0[t]["E_sc"] > 0, axis=1).sum()
        row = dict(theta=t, R_lines=rl, R_g4=n4/T, R_g4_err=np.sqrt(n4)/T,
                   g4_over_lines=n4/T/rl, g4_over_lines_err=np.sqrt(n4)/T/rl)
        for key, runs in (("normal", m2), ("turned", fl)):
            if t in runs:
                f = _counts(runs[t], mpv4, c)
                row[f"r_{key}"], row[f"r_{key}_err"] = sa.ratio([f["adj"]], [f["lo"]])
        if pair is not None and t in set(pair.theta):
            q = pair[pair.theta == t].iloc[0]
            row["turned_over_normal"], row["turned_over_normal_err"] = q.ratio, q.err
        if t in m2:
            row["R_eco_half"], row["R_eco_half_err"] = r_half(m2[t], sa.ecomug_live_time(m2[t]["n_gen"], 0.1))
        for key, runs in (("cry_mu", cm), ("cry_all", ca)):
            if t in runs:
                row[f"R_{key}_half"], row[f"R_{key}_half_err"] = r_half(runs[t], runs[t]["live_time_s"])
        for name, a, b in (("cry_mu_over_eco", "cry_mu", "eco"), ("cry_all_over_mu", "cry_all", "cry_mu")):
            if f"R_{a}_half" in row and f"R_{b}_half" in row and row[f"R_{b}_half"] > 0:
                ra, ea, rb, eb = row[f"R_{a}_half"], row[f"R_{a}_half_err"], row[f"R_{b}_half"], row[f"R_{b}_half_err"]
                row[name] = ra/rb
                row[f"{name}_err"] = ra/rb*np.hypot(ea/ra if ra > 0 else 0, eb/rb)
        rows.append(row)
    return pd.DataFrame(rows)


def cry_plane_flux(path, pmin=0.1):
    """Muon flux (m^-2 s^-1) of a CRY run through its horizontal generation square,
    muons with p >= pmin GeV/c. The side of the square is read from the macro next
    to the file (/muonSource/cry/boxLength), else from RunInfo gen_area_m2."""
    with uproot.open(path) as f:
        info = f["RunInfo"].arrays(["live_time_s", "gen_area_m2"], library="np")
        area = float(info["gen_area_m2"][0])
        T = float(info["live_time_s"].sum())
        n = 0
        for ch in f["Primaries"].iterate(["pdg", "px", "py", "pz"], step_size="200 MB", library="np"):
            p = np.sqrt(ch["px"]**2 + ch["py"]**2 + ch["pz"]**2)
            n += int(np.sum((np.abs(ch["pdg"]) == 13) & (p >= pmin)))
    mac = path[:-5] + ".mac"
    if os.path.exists(mac):
        for line in open(mac):
            if line.startswith("/muonSource/cry/boxLength"):
                L = float(line.split()[1]); unit = line.split()[2] if len(line.split()) > 2 else "m"
                area = (L*{"m": 1.0, "cm": 0.01, "mm": 0.001}[unit])**2
    return n/(area*T), np.sqrt(n)/(area*T), dict(n=n, area_m2=area, T=T)


# E. LaTeX
def num(v, nd=3):
    return "--" if v is None or not np.isfinite(v) else f"{v:.{nd}f}"


def pm(v, e, nd=3):
    if v is None or not np.isfinite(v):
        return "--"
    if e is None or not np.isfinite(e):
        return f"${v:.{nd}f}$"
    return rf"${v:.{nd}f} \pm {e:.{nd}f}$"


def tex_n(n):
    k = int(round(np.log10(n))) if n > 0 else 0
    return f"10^{k}" if n == 10**k else rf"{n/10**k:.2f}\times 10^{k}"


def latex_table(header, rows, caption, label, align=None, size=r"\small", colsep=None):
    """booktabs table; a row equal to "midrule" becomes \\midrule."""
    align = align or "l" + "c"*(len(header) - 1)
    out = [r"\begin{table}[htbp]", r"  \centering", f"  {size}"]
    if colsep:
        out.append(rf"  \setlength{{\tabcolsep}}{{{colsep}}}")
    out += [rf"  \caption{{{caption}}}", rf"  \label{{{label}}}",
            rf"  \begin{{tabular}}{{{align}}}", r"    \toprule",
            "    " + " & ".join(header) + r" \\", r"    \midrule"]
    for r in rows:
        out.append(r"    \midrule" if r == "midrule" else "    " + " & ".join(str(x) for x in r) + r" \\")
    out += [r"    \bottomrule", r"  \end{tabular}", r"\end{table}"]
    return "\n".join(out)


def latex_deposits(tab, n_gen):
    rows = []
    for sel in ("all hits", "four-fold"):
        if rows:
            rows.append("midrule")
        for _, r in tab[tab.selection == sel].iterrows():
            rows.append([sel if r.panel == 0 else "", int(r.panel), int(r.N), pm(r.MPV, r.MPV_err),
                         num(r.xi), num(r.sigma), f"{r.chi2:.0f}/{r.ndf}", pm(r.MPV_moyal, r.MPV_moyal_err),
                         f"{r.chi2_moyal:.0f}/{r.ndf_moyal}", num(r["median"], 2), num(r["mean"], 2),
                         num(r.L_median, 2), num(r.MPV_LV, 2)])
    return latex_table(
        ["", "Panel", "$N$", r"$E_{\mathrm{MPV}}$", r"$\xi$", r"$\sigma$", r"$\chi^2/n$",
         r"$E_{\mathrm{MPV}}^{\mathrm{Moyal}}$", r"$\chi^2/n$", "Median", "Mean", r"$\tilde{L}$",
         r"$\Delta_p(\tilde{L})$"], rows,
        rf"Energy deposit in the scintillator of each panel, ${tex_n(n_gen)}$ muons, vertical telescope, "
        r"scintillators only (MeV). $E_{\mathrm{MPV}}$: maximum of a Landau ($\xi$) $\otimes$ Gauss "
        r"($\sigma$) function fitted to the spectrum, $E_{\mathrm{MPV}}^{\mathrm{Moyal}}$: the same with a "
        r"Moyal function; $\chi^2/n$: Pearson $\chi^2$ and degrees of freedom of the fits in "
        r"0.05\,MeV bins. $\tilde{L}$: median geometric path of the muon in the panel (cm); "
        r"$\Delta_p(\tilde{L})$: Landau--Vavilov most probable loss for this path.",
        "tab::mc_deposits", "llr" + "c"*10, size=r"\footnotesize", colsep="2.5pt")


def latex_fractions(tab):
    rows = []
    for i in range(4):
        r = [i]
        for sel in ("all hits", "four-fold"):
            q = tab[(tab.selection == sel) & (tab.panel == i)].iloc[0]
            r += [num(q.f_lo, 4), num(q.f_adj, 4), num(q.f_110, 4)]
        rows.append(r)
    head = ["Panel"] + [rf"{t}" for t in (r"$-30$\,mV", "adjusted", r"$-110$\,mV")]*2
    t = latex_table(
        head, rows,
        r"Fraction of the scintillator deposits above the threshold of the panel, the thresholds in units "
        r"of the four-fold $E_{\mathrm{MPV}}$ of the panel: $-30$\,mV = 0.145, 0.097, 0.108, 0.097; "
        r"adjusted = 0.52, 0.43, 0.48, 0.49; $-110$\,mV = 0.353, 0.39, 0.347, 0.310 (panels 0 to 3).",
        "tab::mc_threshold_fractions", "c" + "c"*6, size=r"\small")
    return t.replace(r"    \toprule" + "\n",
                     r"    \toprule" + "\n" + r"    & \multicolumn{3}{c}{All hits} & \multicolumn{3}{c}{Four-fold} \\"
                     + "\n" + r"    \cmidrule(lr){2-4} \cmidrule(lr){5-7}" + "\n", 1)


def latex_guides(gt, kc_normal, kc_flip=None, c=2.75):
    rows = []
    for _, r in gt.iterrows():
        if r.panel == "all":
            rows.append("midrule")
        rows.append(["all" if r.panel == "all" else r.panel, int(r.crossings), num(r.E_lg_median, 2),
                     num(r.L_geo_median, 2), num(r.L_est_median, 2), num(r.beta_median, 4),
                     f"{r.N_ch_median:.0f}", f"{r.N_ch_90:.0f}", num(r.ratio_median), num(r.a_median),
                     num(r.p_lo, 2), num(r.p_adj, 2), num(r.p_half, 2)])
    extra = r" Four-fold events with $k = 0, \ldots, 4$ guide-only panels: " + ", ".join(map(str, kc_normal))
    if kc_flip is not None:
        extra += r"; with the guide of panel~1 turned: " + ", ".join(map(str, kc_flip))
    return latex_table(
        ["Panel", "Crossings", r"$E_{\mathrm{LG}}$", r"$L_{\mathrm{geo}}$", r"$L_{E}$", r"$\beta$",
         r"$N_{\mathrm{ch}}$", r"$N_{\mathrm{ch}}^{90\%}$", r"$N_{\mathrm{ch}}/N_{\mathrm{sc}}$",
         r"$a_{\mathrm{LG}}$", r"$p_{-30}$", r"$p_{\mathrm{adj}}$", r"$p_{0.5}$"], rows,
        r"Panels reached only through their light guide in four-fold events (vertical telescope, "
        r"scintillators and guides). Medians: deposit in the guide (MeV), geometric path of the straight "
        r"track and path from the deposit (cm), velocity of the muon at the entry, Cherenkov photons in "
        r"300--500\,nm ($N_{\mathrm{ch}}^{90\%}$: 90\,\% quantile), ratio to the scintillation photons "
        r"at the most probable deposit, and the pulse $a_{\mathrm{LG}} = c\,N_{\mathrm{ch}}/N_{\mathrm{sc}}$ "
        rf"with $c = {c:g}$ in units of the muon signal. $p$: fraction of the crossings above the threshold "
        r"of the panel ($p_{0.5}$: above half of the muon signal in every panel)." + extra + ".",
        "tab::mc_lightguide", "l" + "c"*12, size=r"\footnotesize", colsep="3pt")


def latex_rates(rt, T, n_gen):
    def g(r, k):
        return pm(r.get(f"R_{k}", np.nan), r.get(f"R_{k}_err", np.nan))
    rows = []
    for _, r in rt.iterrows():
        rows.append([r.model, g(r, "any"), g(r, "lo"), g(r, "adj"), g(r, "110"),
                     pm(r.get("r_adj", np.nan), r.get("r_adj_err", np.nan)),
                     pm(r.get("r_110", np.nan), r.get("r_110_err", np.nan))])
    t = latex_table(
        ["", "Any", r"$-30$\,mV", "Adjusted", r"$-110$\,mV", r"Adj./$-30$", r"$-110$/$-30$"], rows,
        rf"Four-fold rates of the vertical telescope (Hz). Straight lines: four panels of zero thickness, "
        rf"same flux. Geant4: ${tex_n(n_gen)}$ EcoMug muons, live time {T:.1f}\,s, statistical "
        rf"uncertainties; without light guides, with light guides (Cherenkov model, $c = {list(C_VALUES)[0]:g}$ for standard "
        rf"and ${list(C_VALUES)[1]:g}$ for UV-transmitting PMMA), and with the guide of panel~1 on the other side. "
        r"``Any'': a deposit in all four modules. The ratios have the binomial uncertainty of the selection.",
        "tab::mc_rates_vertical", "l" + "c"*6,
        size=r"\footnotesize", colsep="2pt")
    return t.replace(r"    \toprule" + "\n",
                     r"    \toprule" + "\n" + r"    & \multicolumn{4}{c}{Four-fold rate $R_4$ (Hz)} & "
                     r"\multicolumn{2}{c}{Ratio of the rates} \\" + "\n" +
                     r"    \cmidrule(lr){2-5} \cmidrule(lr){6-7}" + "\n", 1)


def latex_angles(at, a_cry=0.5, c=2.75):
    rows = []
    for _, r in at.iterrows():
        rows.append([f"{r.theta:.0f}", num(r.R_lines), pm(r.R_g4, r.R_g4_err),
                     pm(r.g4_over_lines, r.g4_over_lines_err),
                     pm(r.get("r_normal", np.nan), r.get("r_normal_err", np.nan)),
                     pm(r.get("r_turned", np.nan), r.get("r_turned_err", np.nan)),
                     pm(r.get("turned_over_normal", np.nan), r.get("turned_over_normal_err", np.nan)),
                     pm(r.get("cry_mu_over_eco", np.nan), r.get("cry_mu_over_eco_err", np.nan), 2),
                     pm(r.get("cry_all_over_mu", np.nan), r.get("cry_all_over_mu_err", np.nan), 2)])
    t = latex_table(
        [r"$\theta_{\mathrm{tel}}$ ($^\circ$)", "Lines", "Geant4", "Geant4/lines", "normal",
         "turned", "turned/normal", r"$\mu$/EcoMug", r"all/$\mu$"], rows,
        r"Zenith-angle scan. Four-fold rates (Hz) of straight lines and of Geant4 with scintillators only "
        r"(EcoMug, $10^6$ muons per angle); ratio of the rates at the adjusted and the lowest threshold with "
        rf"the light guides (Cherenkov model, $c = {c:g}$), normal and with the guide of panel~1 turned, and "
        r"their ratio for the same muons; ratios of the four-fold rates with all four deposits above "
        rf"${a_cry:g}\,E_{{\mathrm{{MPV}}}}$: CRY muons to EcoMug, and all CRY particles to CRY muons.",
        "tab::mc_zenith_scan", "c"*9, size=r"\scriptsize", colsep="2.5pt")
    return t.replace(r"    \toprule" + "\n",
                     r"    \toprule" + "\n" + r"    & \multicolumn{3}{c}{Scintillators only} & "
                     r"\multicolumn{3}{c}{$R_{\mathrm{adj}}/R_{-30}$, light guides} & "
                     rf"\multicolumn{{2}}{{c}}{{CRY, ${a_cry:g}\,E_{{\mathrm{{MPV}}}}$}} \\" + "\n" +
                     r"    \cmidrule(lr){2-4} \cmidrule(lr){5-7} \cmidrule(lr){8-9}" + "\n", 1)


def latex_summary(items):
    """items: list of (quantity, value string)."""
    return latex_table(["Quantity", "Value"], [[q, v] for q, v in items],
                       r"Simulated properties of the vertical telescope used in the text.",
                       "tab::mc_summary", "lc")


def latex_macros(values):
    """\\newcommand for every value, so that the text follows the numbers."""
    return "\n".join(rf"\newcommand{{\{k}}}{{\ensuremath{{{v}}}}}" for k, v in values.items())


def summary_and_macros(dep, mpv4, mip, kcal, gt, cc, rt, T, at=None, cry_flux=None):
    """Numbers used in the text: (list for latex_summary, dict for latex_macros)."""
    f4, fa = dep[dep.selection == "four-fold"], dep[dep.selection == "all hits"]
    g = gt[gt.panel == "all"].iloc[0]
    R = rt.set_index("model")
    def rr(model, k):
        if model not in R.index:
            return "--"
        r = R.loc[model]
        return f"{r[f'r_{k}']:.3f} \\pm {r[f'r_{k}_err']:.3f}"
    c_std, c_uvt = list(C_VALUES)[:2]
    std, uvt = f"Guides, $c = {c_std:g}$", f"Guides, $c = {c_uvt:g}$"
    g4 = R.loc[LAB_NOLG]
    kap = mip["kappa"]; e = int(np.floor(np.log10(kap)))
    m = {
        "MCmpvLVmin": f"{MPV_LV_MIN:.2f}", "MCmpvLVmax": f"{MPV_LV_PLATEAU:.2f}",
        "MCmpvLVfour": f"{mip['MPV_LV_four']:.2f}",
        "MCmpvFourMin": f"{f4.MPV.min():.2f}", "MCmpvFourMax": f"{f4.MPV.max():.2f}",
        "MCmpvAllMin": f"{fa.MPV.min():.2f}", "MCmpvAllMax": f"{fa.MPV.max():.2f}",
        "MCmpvMoyalMin": f"{f4.MPV_moyal.min():.2f}", "MCmpvMoyalMax": f"{f4.MPV_moyal.max():.2f}",
        "MCmpvShape": f"{np.mean(np.abs(f4.MPV - f4.MPV_moyal)):.3f}",
        "MCmpvShapeMax": f"{np.max(f4.MPV_moyal - f4.MPV):.3f}",
        "MCmpvErr": f"{f4.MPV_err.mean():.3f}",
        "MCpathAll": f"{fa.L_median.mean():.2f}", "MCinclination": f"{mip['inclination']:.3f}",
        "MCkappa": rf"{kap/10**e:.1f}\times 10^{{{e}}}",
        "MCnFour": f"{mip['n_four']}", "MCpMedian": f"{mip['p_percentiles'][50]:.2f}",
        "MCpFive": f"{mip['p_percentiles'][5]:.2f}", "MCpNinetyFive": f"{mip['p_percentiles'][95]:.1f}",
        "MCpMip": f"{mip['p_mip']:.0f}", "MCfMip": f"{100*mip['f_mip']:.1f}",
        "MCpMin": f"{mip['p_min']:.0f}", "MCesumMedian": f"{mip['Esum_median']:.2f}",
        "MCnScint": f"{np.mean(Y_SC*mpv4):.0f}", "MCkCal": f"{kcal:.3f}",
        "MCguideCross": f"{int(g.crossings)}", "MCguidePath": f"{g.L_geo_median:.2f}",
        "MCguideEdep": f"{g.E_lg_median:.2f}", "MCguideNch": f"{g.N_ch_median:.0f}",
        "MCguideRatio": f"{g.ratio_median:.3f}", "MCguidePulse": f"{g.a_median:.3f}",
        "MCcFit": f"{cc['c_fit']:.2f}", "MCpAdjAtFit": f"{cc['at_fit']['adj']:.3f}",
        "MCpLoStd": f"{cc['at_c'][c_std]['lo']:.2f}", "MCpLoUVT": f"{cc['at_c'][c_uvt]['lo']:.2f}",
        "MCliveTime": f"{T:.1f}", "MCrateLines": f"{R.loc[LAB_LINES, 'R_any']:.3f}",
        "MCrateGeant": f"{g4.R_any:.3f} \\pm {g4.R_any_err:.3f}",
        "MCrAdjNoLG": rr(LAB_NOLG, "adj"), "MCrAdjStd": rr(std, "adj"),
        "MCrAdjUVT": rr(uvt, "adj"), "MCrHiNoLG": rr(LAB_NOLG, "110"),
        "MCrHiStd": rr(std, "110"), "MCrHiUVT": rr(uvt, "110"),
        "MCrAdjTurnedStd": rr(f"Panel 1 turned, $c = {c_std:g}$", "adj"),
        "MCgeantOverLines": f"{g4.R_any/R.loc[LAB_LINES, 'R_any']:.3f}",
        "MCloGainStd": (f"{100*(R.loc[std, 'R_lo']/g4.R_lo - 1):.1f}" if std in R.index else "--"),
        "MCadjGainStd": (f"{100*(R.loc[std, 'R_adj']/g4.R_adj - 1):.1f}" if std in R.index else "--"),
        "MCxi": f"{K/2*PVT['ZA']*X_SC:.3f}",
        "MCfLoFourMin": f"{100*f4.f_lo.min():.1f}", "MCfAdjFourMin": f"{100*f4.f_adj.min():.1f}",
        "MCfHiFourMin": f"{100*f4.f_110.min():.1f}", "MCfAdjAllMin": f"{100*fa.f_adj.min():.1f}",
    }
    if at is not None and len(at) and at.theta.iloc[0] == 0:
        a0 = at.iloc[0]
        for k, name in (("cry_mu_over_eco", "MCcryMuOverEco"), ("cry_all_over_mu", "MCcryAllOverMu")):
            if np.isfinite(a0.get(k, np.nan)):
                m[name] = f"{a0[k]:.2f} \\pm {a0[k + '_err']:.2f}"
    if cry_flux is not None:
        m["MCcryFlux"] = f"{cry_flux[0]:.1f} \\pm {cry_flux[1]:.1f}"
        m["MCcryFluxDiff"] = f"{100*(1 - cry_flux[0]/126.67):.0f}"
    items = [
        (r"Landau--Vavilov $\Delta_p$, 1\,cm EJ-200, normal incidence (MeV)", f"{MPV_LV_MIN:.2f}--{MPV_LV_PLATEAU:.2f}"),
        (r"$E_{\mathrm{MPV}}$, four-fold events, Landau $\otimes$ Gauss (MeV)", f"{f4.MPV.min():.2f}--{f4.MPV.max():.2f}"),
        (r"$E_{\mathrm{MPV}}$, four-fold events, Moyal (MeV)", f"{f4.MPV_moyal.min():.2f}--{f4.MPV_moyal.max():.2f}"),
        (r"$E_{\mathrm{MPV}}$, all hits (MeV)", f"{fa.MPV.min():.2f}--{fa.MPV.max():.2f}"),
        (r"$\langle 1/\cos\theta \rangle$ of the four-fold muons", f"{mip['inclination']:.3f}"),
        (r"Vavilov $\kappa = \xi/W_{\max}$ at the median momentum", f"${m['MCkappa']}$"),
        (r"Four-fold muons: median momentum (GeV/$c$)", f"{mip['p_percentiles'][50]:.2f}"),
        (r"Four-fold muons: 5--95\,\% momentum (GeV/$c$)",
         f"{mip['p_percentiles'][5]:.2f}--{mip['p_percentiles'][95]:.1f}"),
        (rf"Four-fold muons in the MIP band ($p > {mip['p_mip']:.0f}$\,MeV/$c$)", f"{100*mip['f_mip']:.1f}\\,\\%"),
        (r"Sum of the four deposits, median (MeV)", f"{mip['Esum_median']:.2f}"),
        (r"Scintillation photons at $E_{\mathrm{MPV}}$", f"{np.mean(Y_SC*mpv4):.0f}"),
        (r"Guide-only panels in four-fold events", f"{int(g.crossings)}"),
        (r"\quad path in the guide, median (cm)", f"{g.L_geo_median:.2f}"),
        (r"\quad deposit in the guide, median (MeV)", f"{g.E_lg_median:.2f}"),
        (r"\quad Cherenkov photons, median", f"{g.N_ch_median:.0f}"),
        (r"\quad $N_{\mathrm{ch}}/N_{\mathrm{sc}}$, median", f"{g.ratio_median:.3f}"),
        (rf"$c$ for $p_{{-30}} = {cc['p_target']:g}$", f"{cc['c_fit']:.2f}"),
        (rf"$p_{{-30}}$ at $c = {c_std:g}$ / ${c_uvt:g}$", f"{cc['at_c'][c_std]['lo']:.2f} / {cc['at_c'][c_uvt]['lo']:.2f}"),
    ]
    return items, m