# Bien démarrer — cloner, compiler et lancer ComponentHub

Ce guide vous emmène de **zéro** à **l'application qui s'ouvre sur votre écran**,
même si vous n'avez **jamais compilé un programme**. Il couvre **Windows, Linux
et Raspberry Pi**.

> **Spoiler** : sous Linux et Raspberry Pi, l'installation des outils tient en
> **une seule commande** et la première compilation est souvent **plus rapide**
> que sous Windows. « Compiler soi-même » n'est pas réservé aux experts, et ce
> n'est pas plus compliqué sur Linux — au contraire.

Si vous voulez juste **utiliser** l'application une fois compilée, filez au
[mode d'emploi](USER_MANUAL.md).

---

## Ce qu'on va faire (le principe)

Un programme C++ n'est pas livré « prêt à double-cliquer » depuis le code source :
il faut le **compiler**, c'est-à-dire transformer le code en exécutable. Pour
cela il faut trois choses, quel que soit l'OS :

1. un **compilateur** C++ (traduit le code en programme) ;
2. **CMake** + **Ninja** (organisent et lancent la compilation) ;
3. la bibliothèque **Qt 6** (l'interface graphique) et **nlohmann-json** (lecture
   des fichiers de données).

Puis, toujours les mêmes deux commandes :

```sh
cmake --preset <nom>          # prépare la compilation
cmake --build --preset <nom>  # compile
```

`<nom>` vaut `mingw` (Windows), `linux` (PC Linux) ou `linux-arm64` (Raspberry
Pi). C'est tout. Voyons chaque OS.

---

## Étape commune : récupérer le code

Il faut **git** (l'outil qui télécharge le dépôt). Installez-le si besoin (voir
chaque section ci-dessous), puis :

```sh
git clone https://github.com/morfredus/ComponentHub.git ComponentHub
cd ComponentHub
```

Vous êtes maintenant dans le dossier du projet. Toutes les commandes qui suivent
se lancent **depuis ce dossier**.

---

## 🐧 Linux (PC x86_64) — le plus rapide

Testé sur Debian / Ubuntu / Linux Mint. Ouvrez un **terminal**.

### 1. Installer les outils (une seule commande)

```sh
sudo apt update
sudo apt install git cmake ninja-build g++ qt6-base-dev nlohmann-json3-dev
```

C'est **tout**. Vous avez le compilateur, CMake, Ninja, Qt 6 et nlohmann-json.

### 2. Récupérer le code

```sh
git clone https://github.com/morfredus/ComponentHub.git ComponentHub
cd ComponentHub
```

### 3. Compiler

```sh
cmake --preset linux
cmake --build --preset linux
```

### 4. Lancer

```sh
./build/ComponentHub
```

La fenêtre s'ouvre. 🎉 Voilà : de rien à l'application, en **quatre commandes**.

---

## 🍓 Raspberry Pi (Raspberry Pi OS 64 bits, ARM64)

**Exactement** la même chose que sous Linux, avec un preset différent. Utilisez
un Raspberry Pi OS **64 bits** (`uname -m` doit afficher `aarch64`).

```sh
sudo apt update
sudo apt install git cmake ninja-build g++ qt6-base-dev nlohmann-json3-dev

git clone https://github.com/morfredus/ComponentHub.git ComponentHub
cd ComponentHub

cmake --preset linux-arm64
cmake --build --preset linux-arm64
./build-arm64/ComponentHub
```

La compilation est un peu plus longue sur un Pi (processeur plus modeste), mais
la démarche est identique. Le même code, la même application, sur un ordinateur à
quelques dizaines d'euros.

---

## 🪟 Windows (MSYS2 / MinGW)

Sous Windows, il n'y a pas de gestionnaire de paquets intégré comme `apt` : on
installe **MSYS2**, qui en fournit un (`pacman`). Quelques étapes de plus, donc,
mais rien de difficile.

### 1. Installer MSYS2

Téléchargez et installez MSYS2 depuis <https://www.msys2.org> (bouton
d'installation en haut de la page, gardez le dossier par défaut `C:\msys64`).

### 2. Ouvrir le bon terminal

Dans le menu Démarrer, lancez **« MSYS2 MINGW64 »** (bien la variante *MINGW64*,
pas *MSYS* ni *UCRT64*). Une fenêtre de terminal s'ouvre.

### 3. Installer les outils (une commande)

```sh
pacman -S --needed git \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-qt6-base \
  mingw-w64-x86_64-nlohmann-json
```

Répondez « oui » (Entrée) aux questions d'installation.

### 4. Récupérer le code et compiler

```sh
git clone https://github.com/morfredus/ComponentHub.git ComponentHub
cd ComponentHub
cmake --preset mingw
cmake --build --preset mingw
```

### 5. Lancer

```sh
./build-mingw/ComponentHub.exe
```

Les DLL de Qt et de MinGW nécessaires sont **copiées automatiquement** à côté de
l'exécutable par la compilation : vous pouvez double-cliquer sur
`build-mingw\ComponentHub.exe` depuis l'Explorateur Windows.

> **Astuce VS Code (Windows)** : le dépôt fournit des tâches prêtes à l'emploi
> (`.vscode/tasks.json`) — **CMake: Build (MinGW)** et **ComponentHub: Run** —
> qui font les étapes 4 et 5 en un raccourci.

---

## Comparons honnêtement l'effort

| | Installer les outils | Compiler + lancer |
|---|---|---|
| **Linux / Raspberry Pi** | 1 commande `apt` | 3 commandes |
| **Windows** | installer MSYS2 + 1 commande `pacman` | 3 commandes |

La seule étape « en plus » sous Windows est l'installation de MSYS2 (parce que
Windows n'a pas de gestionnaire de paquets natif). Une fois cela fait, tout est
identique. Et sur une machine correcte, la compilation sous Linux est
généralement **plus rapide**. Bref : n'ayez aucune appréhension à essayer sous
Linux, y compris sur un Raspberry Pi.

---

## Ça ne marche pas ? Les cas courants

- **`cmake : command not found` (Linux)** — l'installation des paquets a échoué,
  relancez l'`apt install` de l'étape 1 et lisez les messages.
- **`cmake` introuvable (Windows)** — vous n'êtes pas dans le terminal **MSYS2
  MINGW64**, ou l'étape `pacman` a échoué. Rouvrez « MSYS2 MINGW64 » et
  recommencez l'étape 3.
- **`Could NOT find Qt6` pendant `cmake --preset`** — Qt n'est pas installé :
  vérifiez `qt6-base-dev` (Linux) ou `mingw-w64-x86_64-qt6-base` (Windows).
- **`Could NOT find nlohmann_json`** — installez `nlohmann-json3-dev` (Linux) ou
  `mingw-w64-x86_64-nlohmann-json` (Windows).
- **La fenêtre ne s'ouvre pas sous Linux (`could not load the Qt platform
  plugin "xcb"`)** — installez la dépendance d'exécution :
  `sudo apt install libxcb-cursor0`.
- **Je veux repartir de zéro** — supprimez le dossier de build
  (`build/`, `build-arm64/` ou `build-mingw/`) et relancez `cmake --preset …`.

---

## Et ensuite ?

- **Utiliser** l'application : [mode d'emploi](USER_MANUAL.md).
- **Fabriquer un paquet installable** (`.deb`, AppImage, ZIP Windows) et les
  détails multi-plateformes : [BUILD_DESKTOP.md](BUILD_DESKTOP.md).
- **Comprendre le code** pour le modifier : [ARCHITECTURE.md](ARCHITECTURE.md) et
  [CONTRIBUTING.md](../../CONTRIBUTING.md).

Bienvenue — vous venez peut-être de compiler votre premier programme. 🙂
