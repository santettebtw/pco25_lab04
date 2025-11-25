//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$ 
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/ 
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$      
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$ 
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/ 


#include "locomotivebehavior.h"
#include "ctrain_handler.h"

namespace {
struct DirectionConfig {
    int contactBeforeEntry;
    int contactAfterEntry;
    int contactBeforeExit;
    int contactAfterExit;
    int inversionContact;
    int entrySwitchId;
    int entrySwitchState;
    int exitSwitchId;
    int exitSwitchState;
    int insideSwitchId;
    int insideSwitchState;
    SharedSectionInterface::Direction sharedDirection;
};

void applySwitch(int switchId, int switchState) {
    if (switchId > 0) {
        diriger_aiguillage(switchId, switchState, 0);
    }
}
}

void LocomotiveBehavior::run()
{
    //Initialisation de la locomotive
    loco.allumerPhares();
    loco.demarrer();
    loco.afficherMessage("Ready!");

    const bool isLocoA = (loco.numero() == 7);

    DirectionConfig forwardConfig{};
    DirectionConfig backwardConfig{};

    if (isLocoA) {
        forwardConfig = DirectionConfig{
            36,  // before entry
            17,  // after entry
            27,  // before exit
            9,   // after exit
            5,   // inversion
            24, DEVIE,  // entry switch
            18, DEVIE,  // exit switch
            15, DEVIE,  // inside constraint
            SharedSectionInterface::Direction::D1
        };
        backwardConfig = DirectionConfig{
            8,   // before entry (reverse)
            27,  // after entry
            17,  // before exit
            35,  // after exit
            34,  // inversion
            6, DEVIE,   // entry switch
            12, DEVIE,  // exit switch
            8, DEVIE,   // inside constraint
            SharedSectionInterface::Direction::D2
        };
    } else {
        forwardConfig = DirectionConfig{
            28,  // before entry
            24,  // after entry
            15,  // before exit
            10,  // after exit
            1,   // inversion
            16, DEVIE,      // entry switch
            8,  TOUT_DROIT, // exit switch
            12, TOUT_DROIT, // inside constraint
            SharedSectionInterface::Direction::D2
        };
        backwardConfig = DirectionConfig{
            4,  // before entry (reverse)
            15,  // after entry
            24,  // before exit
            22,  // after exit
            31,  // inversion
            7, DEVIE,        // entry switch
            15, TOUT_DROIT,  // exit switch
            18, TOUT_DROIT,  // inside constraint
            SharedSectionInterface::Direction::D1
        };
    }

    DirectionConfig current = forwardConfig;

    while (true) {
        // Approche de la section partagée
        attendre_contact(current.contactBeforeEntry);

        // Préparer l'entrée : aiguillage et réservation
        applySwitch(current.entrySwitchId, current.entrySwitchState);
        sharedSection->access(loco, current.sharedDirection);
        applySwitch(current.insideSwitchId, current.insideSwitchState);

        // La locomotive est dans la section, suivre le parcours
        attendre_contact(current.contactAfterEntry);
        attendre_contact(current.contactBeforeExit);

        // Prévenir la sortie de la zone partagée
        sharedSection->leave(loco, current.sharedDirection);
        applySwitch(current.exitSwitchId, current.exitSwitchState);

        // Attendre d'avoir dégagé le tronçon
        attendre_contact(current.contactAfterExit);
        sharedSection->release(loco);

        // Continuer jusqu'au point d'inversion
        attendre_contact(current.inversionContact);
        loco.inverserSens();

        // Passer au parcours retour
        current = (current.sharedDirection == forwardConfig.sharedDirection)
                      ? backwardConfig
                      : forwardConfig;
    }
}


void LocomotiveBehavior::printStartMessage()
{
    qDebug() << "[START] Thread de la loco" << loco.numero() << "lancé";
    loco.afficherMessage("Je suis lancée !");
}

void LocomotiveBehavior::printCompletionMessage()
{
    qDebug() << "[STOP] Thread de la loco" << loco.numero() << "a terminé correctement";
    loco.afficherMessage("J'ai terminé");
}
