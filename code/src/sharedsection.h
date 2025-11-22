//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/ 
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$      
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$ 
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/ 


#ifndef SHAREDSECTION_H
#define SHAREDSECTION_H

#include <QDebug>

#include <pcosynchro/pcosemaphore.h>

#ifdef USE_FAKE_LOCO
#  include "fake_locomotive.h"
#else
#  include "locomotive.h"
#endif

#ifndef USE_FAKE_LOCO
  #include "ctrain_handler.h"
#endif

#include "sharedsectioninterface.h"

/**
 * @brief La classe SharedSection implémente l'interface SharedSectionInterface qui
 * propose les méthodes liées à la section partagée.
 */
class SharedSection final : public SharedSectionInterface
{


public:

    /**
     * @brief SharedSection Constructeur de la classe qui représente la section partagée.
     * Initialisez vos éventuels attributs ici, sémaphores etc.
     */
    SharedSection() : sem(0), mutex(1), blocked(false),iswaiting(false), nberrors(0){
        // TODO
    }

    /**
     * @brief Request access to the shared section
     * @param Locomotive who asked access
     * @param Direction of the locomotive
     */
    void access(Locomotive& loco, Direction d) override {
        // TODO

        if(!blocked)
            _loco = &loco;

        if(_loco == &loco)
            ++nberrors;

        mutex.acquire();
        if(blocked){
            iswaiting = true;
            mutex.release();
            loco.arreter();
            sem.acquire();
        }
        loco.demarrer();
        blocked  = true;
        _d = d;
        mutex.release();

    }

    /**
     * @brief Notify the shared section that a Locomotive has left (not freed yed).
     * @param Locomotive who left
     * @param Direction of the locomotive
     */
    void leave(Locomotive& loco, Direction d) override {
        // TODO
        if(d != _d)
            ++nberrors;

        if(_loco == &loco)
            ++nberrors;

    }

    /**
     * @brief Notify the shared section that it can now be accessed again (freed).
     * @param Locomotive who sent the notification
     */
    void release(Locomotive &loco) override {
        // TODO
        mutex.acquire();
        blocked = false;

        if(iswaiting)
            sem.release();
        mutex.release();
    }

    /**
     * @brief Stop all locomotives to access this shared section
     */
    void stopAll() override {
        // TODO
        mutex.acquire();
        blocked = true;
        mutex.acquire();

    }

    /**
     * @brief Return nbErrors
     * @return nbErrors
     */
    int nbErrors() override {
        // TODO
        return nberrors;
    }

private:
    /*
     * Vous êtes libres d'ajouter des méthodes ou attributs
     * pour implémenter la section partagée.
     */
    Locomotive *_loco;
    PcoSemaphore sem;
    PcoSemaphore mutex;
    bool blocked;
    bool iswaiting;
    int nberrors;
    Direction _d;
};


#endif // SHAREDSECTION_H
