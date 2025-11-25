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
    SharedSection() 
        : sem(0), 
          mutex(1), 
          isOccupied(false),
          currentDirection(Direction::D1),
          currentLoco(nullptr),
          hasAccess(false),
          errorCount(0),
          emergencyStop(false),
          waitingLoco(nullptr),
          waitingDirection(Direction::D1),
          lastLeftDirection(Direction::D1),
          isReleased(false)
    {
    }

    /**
     * @brief Request access to the shared section
     * @param Locomotive who asked access
     * @param Direction of the locomotive
     */
    void access(Locomotive& loco, Direction d) override {
        mutex.acquire();
        
        // erreur : access() appele deux fois sans leave()
        if (hasAccess && currentLoco == &loco) {
            errorCount++;
            mutex.release();
            return;
        }
        
        // si la section est occupée, attendre
        if (isOccupied) {
            // Enregistrer la locomotive en attente
            waitingLoco = &loco;
            waitingDirection = d;
            mutex.release();
            loco.arreter();  // Arrêter la locomotive
            sem.acquire();  // Attendre qu'une place se libère
            
            // verifier si arret d'urgence
            if (emergencyStop) {
                mutex.release();
                return;
            }
            
            mutex.acquire();
            waitingLoco = nullptr;
        }
        
        // Accès accordé
        isOccupied = true;
        currentLoco = &loco;
        currentDirection = d;
        hasAccess = true;
        isReleased = false;  // Réinitialiser le flag de libération
        loco.demarrer();  // Redémarrer si elle était arrêtée
        
        mutex.release();
    }

    /**
     * @brief Notify the shared section that a Locomotive has left (not freed yed).
     * @param Locomotive who left
     * @param Direction of the locomotive
     */
    void leave(Locomotive& loco, Direction d) override {
        mutex.acquire();
        
        // Détection d'erreurs
        if (!hasAccess || currentLoco != &loco) {
            errorCount++;
            mutex.release();
            return;
        }
        
        if (currentDirection != d) {
            errorCount++;
            mutex.release();
            return;
        }
        
        // La locomotive quitte physiquement la section
        lastLeftDirection = currentDirection;
        isOccupied = false;
        hasAccess = false;  // Marquer qu'on a quitté, mais pas encore libéré
        currentLoco = nullptr;
        isReleased = false;  // Pas encore libéré
        
        // Vérifier s'il y a une locomotive en attente avec direction opposée
        bool shouldReleaseImmediately = (waitingLoco != nullptr && 
                                        waitingDirection != lastLeftDirection);
        
        mutex.release();
        
        // Si sens opposé, libérer immédiatement pour permettre l'entrée
        if (shouldReleaseImmediately) {
            release(loco);
        }
        // Sinon, release() sera appelé après le contact suivant (même sens)
    }

    /**
     * @brief Notify the shared section that it can now be accessed again (freed).
     * @param Locomotive who sent the notification
     */
    void release(Locomotive &loco) override {
        mutex.acquire();
        
        // Détection d'erreur : release() sans leave() préalable
        if (hasAccess && currentLoco == &loco) {
            errorCount++;
            mutex.release();
            return;
        }
        
        // Si déjà libéré, ne rien faire (évite les doubles libérations)
        if (isReleased) {
            mutex.release();
            return;
        }
        
        // Libérer la section : permettre à une locomotive en attente d'entrer
        if (!isOccupied && waitingLoco != nullptr) {
            // Vérifier si direction opposée (libération immédiate) ou même sens
            bool isOppositeDirection = (waitingDirection != lastLeftDirection);
            
            if (isOppositeDirection || !isReleased) {
                isReleased = true;
                sem.release();
            }
        } else {
            isReleased = true;  // Marquer comme libéré même s'il n'y a pas d'attente
        }
        
        mutex.release();
    }

    /**
     * @brief Stop all locomotives to access this shared section
     */
    void stopAll() override {
        mutex.acquire();
        emergencyStop = true;
        
        // Libérer toutes les locomotives en attente (elles vérifieront emergencyStop)
        // On libère plusieurs fois pour s'assurer de libérer toutes les locomotives
        for (int i = 0; i < 10; i++) {
            sem.release();
        }
        
        mutex.release();
    }

    /**
     * @brief Return nbErrors
     * @return nbErrors
     */
    int nbErrors() override {
        mutex.acquire();
        int errors = errorCount;
        mutex.release();
        return errors;
    }

private:
    /*
     * Vous êtes libres d'ajouter des méthodes ou attributs
     * pour implémenter la section partagée.
     */
    PcoSemaphore sem;
    PcoSemaphore mutex;
    
    bool isOccupied;
    Direction currentDirection;
    Locomotive* currentLoco;
    bool hasAccess; // Suivre si la locomotive a fait access() sans leave()
    int errorCount; 
    bool emergencyStop;
    
    // Pour gérer les directions opposées
    Locomotive* waitingLoco; // Locomotive en attente
    Direction waitingDirection; // Direction de la locomotive en attente
    Direction lastLeftDirection; // Direction de la dernière locomotive qui a quitté
    bool isReleased;	// Flag pour savoir si la section a été libérée
};


#endif // SHAREDSECTION_H
