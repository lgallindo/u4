/*
 * $Id$
 */

#include "vc6.h"

#include "aura.h"

Aura::Aura() : type(NONE), duration(0) {}

int Aura::getDuration() const         { return duration; }
Aura::Type Aura::getType() const      { return type; }
bool Aura::isActive() const           { return duration > 0; }

void Aura::setDuration(int d) {
    duration = d;
    setChanged();
    notifyObservers(NULL);
}

void Aura::set(Type t, int d) {
    type = t;
    duration = d;
    setChanged();
    notifyObservers(NULL);
}

void Aura::setType(Type t) {
    type = t;
    setChanged();
    notifyObservers(NULL);
}

bool Aura::operator==(const Type &t) const    { return type == t; }
bool Aura::operator!=(const Type &t) const    { return !operator==(t); }

void Aura::passTurn() {
    if (duration > 0) {
        duration--;
        
        if (duration == 0) {
            type = NONE;

            setChanged();
            notifyObservers(NULL);
        }
    }
}
