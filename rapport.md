# Rapport - PCO25 Lab04

## 1. Architecture globale

Deux threads `LocomotiveBehavior` (locos 7 et 42) partagent une instance unique de `SharedSection`. Les contacts fournis par le simulateur synchronisent les déplacements, tandis que les aiguillages sont pilotés depuis chaque thread juste avant d'entrer/sortir du tronçon commun. L'arrêt d'urgence (`emergency_stop`) agit simultanément sur les locos et notifie la section partagée.

## 2. Section partagée (`sharedsection.h`)

```65:205:code/src/sharedsection.h
void SharedSection::access(...) {
    ...
    if (isOccupied) {
        waitingLoco = &loco;
        waitingDirection = d;
        mutex.release();
        loco.arreter();
        sem.acquire();
        ...
    }
    isOccupied = true;
    currentLoco = &loco;
    currentDirection = d;
    hasAccess = true;
    isReleased = false;
    loco.demarrer();
}
```

a) `access()` bloque via un sémaphore (`sem`) tant que la section est occupée. La loco en attente est arrêtée et sa direction mémorisée pour appliquer les règles "opposé/même sens".

```110:180:code/src/sharedsection.h
void SharedSection::leave(...) {
    ...
    lastLeftDirection = currentDirection;
    isOccupied = false;
    hasAccess = false;
    currentLoco = nullptr;
    isReleased = false;
    bool shouldReleaseImmediately = (waitingLoco != nullptr &&
                                     waitingDirection != lastLeftDirection);
    mutex.release();
    if (shouldReleaseImmediately) {
        release(loco);
    }
}
```

b) `leave()` ne libère pas directement la section : la sortie physique est enregistrée, puis `release()` est déclenché immédiatement uniquement si la loco en attente roule en sens opposé.

```150:190:code/src/sharedsection.h
void SharedSection::release(...) {
    ...
    if (isReleased) return;
    if (!isOccupied && waitingLoco != nullptr) {
        bool isOppositeDirection = (waitingDirection != lastLeftDirection);
        if (isOppositeDirection || !isReleased) {
            isReleased = true;
            sem.release();
        }
    } else {
        isReleased = true;
    }
}
```

c) `release()` assure le délai post-sortie pour les sens identiques : la libération n'est effective qu'après le contact attendu, ce qui évite l'effet "stop/redémarrage immédiat".

`stopAll()` positionne un drapeau `emergencyStop` et relâche le sémaphore pour que toute loco bloquée s'arrête proprement. `nbErrors()` comptabilise les séquences incorrectes (`access` double, `leave` hors ordre, etc.).

## 3. Parcours cycliques (`locomotivebehavior.cpp`)

```49:128:code/src/locomotivebehavior.cpp
if (isLocoA) {
    forwardConfig = {36,17,27,9,5, 24,DEVIE, 18,DEVIE, 15,DEVIE, D1};
    backwardConfig = {8,27,17,35,34, 6,DEVIE, 12,DEVIE, 8,DEVIE, D2};
} else {
    forwardConfig = {28,24,15,10,1, 16,DEVIE, 8,TOUT_DROIT, 12,TOUT_DROIT, D2};
    backwardConfig = {4,15,24,22,31, 7,DEVIE, 15,TOUT_DROIT, 18,TOUT_DROIT, D1};
}
...
attendre_contact(current.contactBeforeEntry);
applySwitch(current.entrySwitchId,...);
sharedSection->access(loco, current.sharedDirection);
...
attendre_contact(current.contactAfterExit);
sharedSection->release(loco);
attendre_contact(current.inversionContact);
loco.inverserSens();
current = (current.sharedDirection == forwardConfig.sharedDirection)
              ? backwardConfig
              : forwardConfig;
```

Chaque locomotive alterne deux configurations : sens "base" et sens "inverse". Chaque configuration fixe les contacts (approche, entrée, sortie, inversion) et les aiguillages à commuter :
- Loco 7 : entrée via 24 (DEVIE) / sortie via 18 (DEVIE) en sens base, et 6/12 pour le retour.
- Loco 42 : entrée via 16 (DEVIE) / sortie via 8 (TOUT_DROIT) en sens base, puis 7 / 15 pour le retour.

Pendant l'occupation, des aiguillages supplémentaires sont forcés (`insideSwitchId`) afin de respecter les contraintes du plan de voie (ex. loco 7 base ⇒ aiguillage 15 dévié).

## 4. Arrêt d'urgence (`cppmain.cpp`)

```31:47:code/src/cppmain.cpp
void emergency_stop() {
    afficher_message("\\nSTOP!");
    if (g_locoA) g_locoA->arreter();
    if (g_locoB) g_locoB->arreter();
    if (g_sharedSection) {
        g_sharedSection->stopAll();
    }
}
```

L'arrêt d'urgence mémorise un pointeur global vers la section partagée : on stoppe immédiatement les deux locomotives puis on notifie `SharedSection::stopAll()` pour débloquer toute attente.

## 5. Tests et validation

- `ctest` / `make unit_tests` : exécute trois tests GoogleTest fournis (`SharedSection.TwoSameDirection`, `ConsecutiveAccess`, `LeaveWrongDirection`).
- Vérification manuelle sur le simulateur : observation des contacts 36/17/27/9 (loco 7) et 28/24/15/10 (loco 42) pour s'assurer que `leave()` et `release()` sont déclenchés aux bons moments, et que l'inversion se produit aux contacts 5/34 et 1/31.

## 6. Choix d'implémentation

1. **Sémaphore unique** : une seule locomotive peut attendre car il n'y a que deux threads. Le sémaphore `sem` suffit, couplé à `waitingLoco` / `waitingDirection` pour retrouver l'état.
2. **Libération différée** : `release()` ne délivre le sémaphore qu'après un contact supplémentaire lorsque la loco suivante roule dans le même sens → confort des passagers.
3. **Configurations paramétrées** : les parcours sont décrits par des structures `DirectionConfig` afin d'éviter la duplication de code entre les deux locomotives et leurs deux sens.
4. **Arrêt d'urgence centralisé** : `emergency_stop()` agit à la fois sur les locomotives et sur la section partagée afin de garantir l'absence de redémarrage intempestif.

