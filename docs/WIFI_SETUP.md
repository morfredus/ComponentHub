# Configuration WiFi et premier démarrage

Ce document détaille comment l'ESP32 obtient ses identifiants WiFi, dans
quel ordre, et comment se connecter à l'appareil pour les saisir quand
aucun réseau n'est encore enregistré. Pour la vue d'ensemble du projet, voir
[README.md](../README.md) ; pour l'implémentation, voir
`src/services/wifi_manager/`.

## Hiérarchie de configuration

Au démarrage, `WiFiManager::begin()` essaie, dans cet ordre, jusqu'à
trouver une source qui fonctionne :

1. **Réseaux enregistrés en NVS** (mémoire flash persistante, namespace
   `wifi`). C'est la source normale en usage courant : un réseau ajouté
   une fois (via le portail ou l'interface web) survit aux redémarrages
   et aux mises à jour de firmware.
2. **`WIFI_DEFAULT_SSID` / `WIFI_DEFAULT_PASSWORD`** définis dans
   `include/secrets.h`, **seulement si aucun réseau n'est encore en NVS**.
   Pratique en développement pour ne pas repasser par le portail à chaque
   flash d'une carte de test.
3. **Portail de configuration** (point d'accès WiFi) : démarré
   automatiquement si aucun réseau enregistré ni `secrets.h` ne permet de
   se connecter.

Cette hiérarchie est volontairement dégressive : NVS prime toujours sur
`secrets.h`, qui ne sert que de filet de sécurité pour le développement
local - il n'est jamais nécessaire en usage normal.

## Créer `include/secrets.h` (développement uniquement)

`include/secrets.h` est ignoré par Git (il ne doit jamais être committé).
Pour le créer :

```bash
cp include/secrets_example.h include/secrets.h
```

Puis éditez les deux constantes avec votre réseau WiFi de test :

```cpp
#define WIFI_DEFAULT_SSID     "MonWifi"
#define WIFI_DEFAULT_PASSWORD "MonMotDePasse"
```

Ce fichier est optionnel : si vous ne le créez pas, le framework compile
et fonctionne normalement, il saute simplement l'étape 2 ci-dessus et
passe directement au portail de configuration si NVS est vide.

## Premier démarrage sans `secrets.h` ni réseau enregistré : le portail

Si l'ESP32 ne trouve aucun réseau connu (NVS vide et pas de `secrets.h`,
ou réseaux enregistrés hors de portée), il bascule en point d'accès et
sert un portail de configuration :

1. **Connectez-vous depuis votre téléphone ou ordinateur** au réseau WiFi
   nommé `WIFI_PORTAL_AP_NAME` (défini dans `include/project_config.h`,
   visible aussi dans les logs série au démarrage : `Portail de
   configuration actif - SSID "..." `).
2. **Ouvrez un navigateur** sur `http://192.168.4.1` (l'IP du point
   d'accès, déclenchée automatiquement par la redirection captive sur la
   plupart des téléphones/PC, sinon ouvrez l'adresse manuellement).
3. La page liste les réseaux WiFi détectés à proximité (scan automatique
   à l'ouverture) : choisissez le vôtre dans la liste, ou saisissez son
   nom manuellement s'il n'apparaît pas (SSID caché, par exemple).
4. Saisissez le mot de passe et validez. L'ESP32 enregistre le réseau en
   NVS, redémarre automatiquement, et tente de s'y connecter au prochain
   démarrage selon la hiérarchie ci-dessus (étape 1).
5. Si la connexion réussit, le point d'accès de configuration disparaît :
   l'ESP32 est désormais sur votre réseau normal, à l'adresse attribuée
   par votre routeur (visible dans les logs série, ou via `/system` une
   fois l'interface web accessible).

Si la connexion échoue (mauvais mot de passe, réseau hors de portée),
l'ESP32 retombe automatiquement sur le portail au redémarrage suivant -
aucune intervention physique sur la carte n'est nécessaire, il suffit de
recommencer la procédure depuis l'étape 1.

## Ajouter ou retirer un réseau une fois connecté

Une fois l'ESP32 connecté à un réseau (portail ou `secrets.h`), d'autres
réseaux peuvent être ajoutés ou retirés sans repasser par le portail,
via l'API `WiFiManager` (`addNetwork()` / `removeNetwork()` / `savedNetworks()`)
exposée par les modules qui en ont besoin. Le portail captif lui-même
reste disponible comme méthode de secours si tous les réseaux enregistrés
deviennent indisponibles.

## Résumé pratique

| Situation | Comportement |
|---|---|
| Premier flash, sans `secrets.h`, NVS vide | Portail de configuration (point d'accès) |
| Premier flash, avec `secrets.h` rempli | Connexion directe au réseau de `secrets.h` |
| Réseau déjà enregistré en NVS (via portail ou interface web) | Connexion directe, `secrets.h` ignoré |
| Réseau enregistré hors de portée, pas d'autre réseau NVS valide | Portail de configuration au redémarrage |

## Accéder à l'interface : `componenthub.local` (mDNS) ou adresse IP

Une fois connecté, l'appareil est joignable de deux façons :

- **Par son IP** (ex. `http://192.168.1.46`) — toujours fiable, quelle que
  soit la plateforme. L'IP s'affiche dans les logs série au démarrage et sur
  la page `/system`.
- **Par son nom mDNS** `http://componenthub.local` (nom défini par
  `MDNS_HOSTNAME` dans `include/project_config.h`) — pratique car stable même
  si l'IP change, mais la résolution des noms `.local` dépend du système
  **client** :
  - **iOS / macOS / Android récents / Linux (avec Avahi)** : fonctionne
    nativement.
  - **Windows** : la résolution `.local` est peu fiable sans le service
    **Bonjour** d'Apple (installé avec iTunes ou « Bonjour Print Services »).
    Si `componenthub.local` ne répond pas alors que l'IP fonctionne, ce n'est
    **pas un défaut de l'appareil** — utilisez l'IP, ou installez Bonjour.

Autrement dit : si l'accès par IP fonctionne mais pas par `.local`, le
firmware va bien ; c'est la résolution mDNS côté PC qui est en cause. Le
firmware ré-annonce proprement son nom à chaque reconnexion WiFi
(`MdnsManager::start`).
