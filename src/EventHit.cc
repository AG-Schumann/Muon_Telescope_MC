#include "EventHit.hh"

// Thread-local allocator instance
G4ThreadLocal G4Allocator<EventHit>* EventHitAllocator = nullptr;

EventHit::EventHit() {}
EventHit::~EventHit() {}
