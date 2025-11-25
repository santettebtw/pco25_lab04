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
    SharedSection() : sem(0), mutex(1), blocked(false){
        // TODO
    }

    /**
     * @brief Request access to the shared section
     * @param Locomotive who asked access
     * @param Direction of the locomotive
     */
    void access(Locomotive& loco, Direction d) override {
        // erreur : access() appele deux fois sans leave()
        if (hasAccess && currentLoco == &loco)
            errorCount++;
 
        // si la section est occupée, attendre
        if (isOccupied) {
            // Enregistrer la locomotive en attente
            waitingLoco = &loco;
            waitingDirection = d;
            mutex.release();
        }
    }

    /**
     * @brief Notify the shared section that a Locomotive has left (not freed yed).
     * @param Locomotive who left
     * @param Direction of the locomotive
     */
    void leave(Locomotive& loco, Direction d) override {
        mutex.acquire();
        
        if (!hasAccess || currentLoco != &loco) {
            errorCount++;
        }
        
        if (currentDirection != d) {
            errorCount++;
        }
        
        // la locomotive quitte physiquement la section
        lastLeftDirection = currentDirection;
        isOccupied = false;
        hasAccess = false;  // Marquer qu'on a quitte, mais pas encore libére
        currentLoco = nullptr;
        isReleased = false;  // Pas encore libéré
        
        // verifier s'il y a une locomotive en attente avec direction opposee
        bool shouldReleaseImmediately = (waitingLoco != nullptr && 
                                        waitingDirection != lastLeftDirection);
        
        mutex.release();
        
        // si sens oppose, liberer immediatement pour permettre l'entree
        if (shouldReleaseImmediately) {
            release(loco);
        }
    }

    /**
     * @brief Notify the shared section that it can now be accessed again (freed).
     * @param Locomotive who sent the notification
     */
    void release(Locomotive &loco) override {
        mutex.acquire();
        
        // erreur: release() sans leave() avant
        if (hasAccess && currentLoco == &loco) {
            errorCount++;
            mutex.release();
            return;
        }
        
        // Si déjà libéré, ne rien faire
        if (isReleased) {
            mutex.release();
            return;
        }

        // liberer la section
        if (!isOccupied && waitingLoco != nullptr) {
            // Vérifier si direction opposée (libération immédiate) ou même sens
            bool isOppositeDirection = (waitingDirection != lastLeftDirection);

            if (isOppositeDirection || !isReleased) {
                isReleased = true;
                sem.release();
            }
        } else {
            isReleased = true; // Marquer comme libéré même s'il n'y a pas d'attente
        }

        mutex.release();
    }

    /**
     * @brief Stop all locomotives to access this shared section
     */
    void stopAll() override {
        mutex.acquire();
        emergencyStop = true;

        sem.release();

        mutex.release();
    }

    /**
     * @brief Return nbErrors
     * @return nbErrors
     */
    int nbErrors() override {
        return errorCount;
    }

private:
    /*
     * Vous êtes libres d'ajouter des méthodes ou attributs
     * pour implémenter la section partagée.
     */
    PcoSemaphore sem;
    PcoSemaphore mutex;
    bool blocked;
};


#endif // SHAREDSECTION_H
