# Rapport — PCO25 Lab04

## Architecture
- Deux threads `LocomotiveBehavior` (locos 7 et 42) pilotent chacun un parcours cyclique avec inversion de sens.
- Une unique `SharedSection` sérialise l’accès au tronçon commun via `PcoSemaphore`.
- `emergency_stop()` coupe immédiatement les deux locomotives et notifie la section partagée (`stopAll`).

## Section partagée
- `access()` mémorise la locomotive/direction en attente, arrête la loco si la section est occupée puis la relance dès que le sémaphore est libéré.
- `leave()` enregistre la sortie physique et déclenche `release()` immédiatement seulement si la prochaine locomotive arrive en sens opposé.
- `release()` impose un délai post-sortie pour les trains qui se suivent dans le même sens (`isReleased` évite les doubles libérations) et libère le sémaphore.
- `stopAll()` pose un drapeau d’urgence et libère le sémaphore pour que toute attente se termine proprement ; `nbErrors()` comptabilise les séquences invalides.

## Parcours et aiguillages
- Chaque locomotive alterne deux configurations (`forwardConfig`, `backwardConfig`) qui définissent à la fois les contacts clefs et les aiguillages à positionner.
- Boucle principale : contact d’approche → aiguillage d’entrée → `access()` → contacts internes → `leave()` → contact post-sortie → `release()` → contact d’inversion → `loco.inverserSens()`.

### Loco rouge (n°7)
- **Sens base**  
  - Contacts : approche 36, entrée 17, pré-sortie 27, post-sortie 9, inversion 5.  
  - Aiguillages : entrée 24 (dévié), sortie 18 (dévié), contrainte 15 (dévié pour ne pas sortir sur la mauvaise boucle).
- **Sens inverse**  
  - Contacts : approche 8, après entrée 27, pré-sortie 17, post-sortie 35, inversion 34.  
  - Aiguillages : entrée 6 (dévié), sortie 12 (dévié), contrainte 8 (dévié).

### Loco bleue (n°42)
- **Sens base**  
  - Contacts : approche 28, entrée 24, pré-sortie 15, post-sortie 10, inversion 1.  
  - Aiguillages : entrée 16 (dévié), sortie 8 (tout droit), contrainte 12 (tout droit).
- **Sens inverse**  
  - Contacts : approche 4, entrée 15, pré-sortie 24, post-sortie 22, inversion 31.  
  - Aiguillages : entrée 7 (dévié), sortie 15 (tout droit), contrainte 18 (tout droit).

## Arrêt d’urgence
- `emergency_stop()` conserve des pointeurs globaux sur les locomotives et la section partagée ; il coupe la traction (`arreter()`) puis appelle `stopAll()` pour éviter tout redémarrage intempestif sans couper la maquette entière.

## Vérification
- Tests unitaires de base +
  - `OppositeDirections_ReleaseIsImmediate` s’assure qu’un train en sens opposé est relâché avant l’appel explicite à `release()`.
  - `StopAll_UnblocksWaitingAccess` confirme que `stopAll()` libère les locomotives bloquées dans `access()`.
- Vérification manuelle : observation des contacts 36/17/27/9 (loco 7) et 28/24/15/10 (loco 42), plus inversion aux contacts 5/34 et 1/31.

## Principaux choix
1. **Sémaphore unique + état mémorisé** pour appliquer la règle sens opposé/sens identique.
2. **Libération différée** afin d’éviter un stop/redémarrage immédiat en cas de même direction.
3. **Configurations paramétrées** (`DirectionConfig`) pour centraliser contacts et aiguillages.
4. **Arrêt centralisé** : `emergency_stop()` agit sur le matériel et la synchronisation sans appeler `mettre_maquette_hors_service()`.
