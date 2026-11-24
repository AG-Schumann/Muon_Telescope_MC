// PhysicsList.hh

#ifndef PhysicsList_hh
#define PhysicsList_hh 1


#include "FTFP_BERT.hh"

class PhysicsList : public FTFP_BERT
{
public:
    // optical = true registers G4OpticalPhysics (the -p flag of the main
    // program). Without it the list is unchanged.
    explicit PhysicsList(G4bool optical = false);
    ~PhysicsList() override;
};


#endif
