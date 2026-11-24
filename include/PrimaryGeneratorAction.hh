#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "globals.hh"

#include "IMuonSource.hh"


class G4ParticleGun;
class G4Event;

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
    public:
        PrimaryGeneratorAction(IMuonSource* source);
        ~PrimaryGeneratorAction() override;

        void GeneratePrimaries(G4Event* event) override;
        void GenerateTestPrimaries(G4Event* event);


        const G4ParticleGun* GetParticleGun() const { return fParticleGun; }

    private:
        G4ParticleGun* fParticleGun;
        IMuonSource* fMuonSource = nullptr;

};


#endif
