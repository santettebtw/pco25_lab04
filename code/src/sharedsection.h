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
        // Si aucune loco n’est encore bloquée, on enregistre celle-ci
        if(!blocked)
            _loco = &loco;

        // Si la locomotive actuelle est celle enregistrée, on incrémente un compteur d’erreurs
        // (semble être un mécanisme de détection d’un comportement anormal)
        if(_loco == &loco)
            ++nberrors;

        // On acquiert le mutex pour protéger la section critique
        mutex.acquire();

        // Si l’accès est bloqué, cette loco doit attendre
        if(blocked){
            // On indique qu’elle est en attente
            iswaiting = true;
            // On libère le mutex avant de bloquer
            mutex.release();
            // On arrête physiquement la locomotive
            loco.arreter();
            // On attend sur le sémaphore pour pouvoir continuer
            sem.acquire();
        }

        // Une fois permise, on démarre la locomotive
        loco.demarrer();
        // On marque l’accès comme maintenant bloqué (une loco occupe la ressource)
        blocked  = true;
        // On enregistre la direction de cette locomotive
        _d = d;
        // On libère le mutex à la fin de la section critique
        mutex.release();

    }

    /**
     * @brief Notify the shared section that a Locomotive has left (not freed yed).
     * @param Locomotive who left
     * @param Direction of the locomotive
     */
    void leave(Locomotive& loco, Direction d) override {
        // Vérifie si la direction donnée ne correspond pas à
        // celle enregistrée pour la locomotive dans la section.
        // Si ce n’est pas la même direction, on incrémente le compteur d’erreurs.
        if(d != _d)
            ++nberrors;

        // Vérifie si la locomotive quittant la section
        // est la même que celle enregistrée comme occupant actuel.
        // Si c'est le cas, cela indique un comportement inattendu (erreur).
        if(_loco == &loco)
            ++nberrors;

    }

    /**
 * @brief Notify the shared section that it can now be accessed again (freed).
 * @param loco Locomotive who sent the notification
 */
    void release(Locomotive &loco) override {
        // On entre dans la section critique pour modifier l'état partagé
        mutex.acquire();

        // On indique que la section n’est plus bloquée :
        // elle peut être réoccupée par une autre locomotive
        blocked = false;

        // Si une locomotive était en attente d'accès,
        // on libère un thread bloqué sur le sémaphore
        if(iswaiting)
            sem.release();

        // On quitte la section critique
        mutex.release();
    }

    /**
 * @brief Stop all locomotives to access this shared section
 */
    void stopAll() override {
        // On entre dans la section critique pour modifier l’état partagé
        mutex.acquire();

        // On bloque l’accès à la section : aucune locomotive ne pourra entrer
        blocked = true;

        // Tentative d’acquérir à nouveau le mutex.
        // (Cela provoquera un blocage si le mutex n’est pas réentrant :
        //  la fonction se retrouvera bloquée ici en attendant elle-même.)
        mutex.acquire();
    }

    /**
     * @brief Return nbErrors
     * @return nbErrors
     */
    int nbErrors() override {
        // TODO
        // renvoie le nombre d'erreurs
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
