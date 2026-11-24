#ifndef SourceType_hh
#define SourceType_hh 1

#include "globals.hh"


enum class SourceType {
    EcoMugDirect,   // runtime generation with EcoMug   (-g ecomug)
    EcoMugFile,     // pre-generated EcoMug file        (-g ecomug-file -s <file>)
    MutePlus,       // MUTE mu+ file                    (-g mute+       -s <file>)
    MuteMinus,      // MUTE mu- file                    (-g mute-       -s <file>)
    CRY,            // CRY cosmic-ray shower generator  (-g cry)
    Isotropic,      // Isotropic source                   (-g isotropic   -s <file>)
    Beam            // Pencil beam for position scans   (-g beam)
};

// Label written to the RunInfo ntuple; matches the -g flag values.
inline G4String SourceTypeToString(SourceType t) {
    switch (t) {
        case SourceType::EcoMugDirect: return "ecomug";
        case SourceType::EcoMugFile:   return "ecomug-file";
        case SourceType::MutePlus:     return "mute+";
        case SourceType::MuteMinus:    return "mute-";
        case SourceType::CRY:          return "cry";
        case SourceType::Isotropic:    return "isotropic";
        case SourceType::Beam:         return "beam";
    }
    return "unknown";
}

#endif