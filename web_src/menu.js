(function () {
  var page = location.pathname;

  // Menu principal volontairement plat (pas de sous-menu déroulant) : une
  // interface "pilotable" (souris/tactile aujourd'hui, bouton rotatif plus
  // tard) se prête mal aux menus qui s'ouvrent au survol/clic — une suite de
  // liens et de pages faites de cartouches (voir web_src/settings.html et
  // system.html) reste navigable au tour de molette, sans état "ouvert/fermé"
  // supplémentaire à gérer.
  var links = [
    ["/", "Labo"], ["/inventory", "Inventaire"], ["/projects", "Projets"],
    ["/settings", "Paramètres"], ["/system", "Système"]
  ];

  var nav = document.createElement("nav");
  function addLink(l) {
    var a = document.createElement("a");
    a.href = l[0];
    a.textContent = l[1];
    if (page === l[0]) a.className = "active";
    nav.appendChild(a);
  }
  links.forEach(addLink);

  var header = document.createElement("header");
  var h1 = document.createElement("h1");
  if (document.title) { h1.textContent = document.title; } else { fetch("/api/system").then(function (r) { return r.json(); }).then(function (data) { h1.textContent = data.project; }); }
  header.appendChild(h1);
  header.appendChild(nav);

  // Bandeau supérieur figé (sticky) : en-tête de navigation + zone de statut
  // inline empilés. Reste collé en haut de la fenêtre au défilement, pour que
  // les confirmations/messages soient visibles même quand on agit en bas
  // d'une longue liste (ex. supprimer un composant). La zone de statut y est
  // ajoutée plus bas (elle est créée après la sous-navigation).
  var topbar = document.createElement("div");
  topbar.className = "app-topbar";
  topbar.appendChild(header);
  document.body.insertBefore(topbar, document.body.firstChild);

  // --- Sous-navigation par section (Paramètres / Système) ---
  // "Paramètres" et "Système" ne sont pas de simples pages mais des sections
  // regroupant plusieurs sous-pages. Sur la page racine comme sur chacune de
  // ses sous-pages, on affiche les cartouches de la section — celle de la page
  // courante mise en évidence — pour rester navigable entre pages sœurs sans
  // revenir en arrière (y compris, à terme, au bouton rotatif). Centralisé ici
  // plutôt que dupliqué dans chaque page HTML.
  var systemSection = { root: "/system", pages: [
    ["/files", "Fichiers"], ["/ota", "Mise à jour"], ["/logs", "Logs"]
  ] };
  var sections = [
    { root: "/settings", pages: [
      ["/locations", "Emplacements"], ["/categories", "Catégories"], ["/import-export", "Import / Export"]
    ] },
    systemSection
  ];

  function findSection(p) {
    if (p === "/debug") return systemSection;   // Debug rattaché à Système (voir sonde plus bas)
    for (var i = 0; i < sections.length; i++) {
      var s = sections[i];
      if (p === s.root) return s;
      for (var j = 0; j < s.pages.length; j++) if (s.pages[j][0] === p) return s;
    }
    return null;
  }

  var currentSection = findSection(page);
  if (currentSection) {
    // Marque le lien de menu principal (Paramètres/Système) actif même quand
    // on est sur une sous-page, qui n'est pas elle-même un lien de premier niveau.
    var mainLink = nav.querySelector('a[href="' + currentSection.root + '"]');
    if (mainLink) mainLink.className = "active";

    // Barre d'onglets (et non de gros boutons) : plus compacte, laisse la
    // place au contenu de la sous-page, et reste un rang d'éléments
    // focalisables (navigable au bouton rotatif à terme).
    var tabs = document.createElement("div");
    tabs.className = "section-tabs";

    var addSectionTab = function (l) {
      var a = document.createElement("a");
      a.href = l[0];
      a.textContent = l[1];
      a.className = "section-tab" + (page === l[0] ? " active" : "");
      tabs.appendChild(a);
    };
    currentSection.pages.forEach(addSectionTab);

    var main = document.querySelector("main");
    if (main) main.insertBefore(tabs, main.firstChild);

    if (currentSection === systemSection) {
      // Onglet Debug ajouté seulement si le module BootLog est actif
      // (ENABLE_BOOT_LOG) — sonde de /api/bootlog, absente sinon.
      fetch("/api/bootlog").then(function (r) { if (r.ok) addSectionTab(["/debug", "Debug"]); }).catch(function () {});
    }
  }

  // --- Pied de page (nom du projet + version), sur toutes les pages ---
  var footer = document.createElement("footer");
  footer.className = "app-footer";
  document.body.appendChild(footer);
  fetch("/api/system").then(function (r) { return r.json(); }).then(function (data) {
    footer.textContent = data.project + " v" + data.version;
  }).catch(function () {});

  // --- Zone de statut inline partagée (toutes les pages) ---
  // Placée dans le bandeau supérieur figé (topbar), juste sous la navigation :
  // messages, confirmations (remplacent alert/confirm) et progression restent
  // ainsi toujours visibles, même en bas d'une longue liste. Masquée quand
  // vide (display:none) pour ne pas laisser de bande grise sous l'en-tête.
  var statusBar = document.createElement("div");
  statusBar.id = "app-status";
  statusBar.className = "app-status";
  statusBar.innerHTML =
    '<span class="app-status-text"></span>' +
    '<div class="app-status-progress" hidden><div class="app-status-progress-fill"></div></div>' +
    '<span class="app-status-actions"></span>';
  topbar.appendChild(statusBar);

  // Hauteur du bandeau figé exposée en variable CSS `--topbar-h`, pour
  // empiler d'autres éléments sticky juste en dessous (barre d'outils et
  // en-têtes de tableau de l'inventaire). Recalculée à chaque changement de
  // hauteur du bandeau (apparition/disparition d'un message, retour à la
  // ligne du menu) et au redimensionnement de la fenêtre.
  function syncTopbarHeight() {
    document.documentElement.style.setProperty("--topbar-h", topbar.offsetHeight + "px");
  }
  syncTopbarHeight();
  if (window.ResizeObserver) new ResizeObserver(syncTopbarHeight).observe(topbar);
  window.addEventListener("resize", syncTopbarHeight);

  var statusText = statusBar.querySelector(".app-status-text");
  var statusProgress = statusBar.querySelector(".app-status-progress");
  var statusProgressFill = statusBar.querySelector(".app-status-progress-fill");
  var statusActions = statusBar.querySelector(".app-status-actions");
  var hideTimer = null;

  function clearAutoHide() {
    if (hideTimer) { clearTimeout(hideTimer); hideTimer = null; }
  }

  function resetBar(extraClass) {
    clearAutoHide();
    statusBar.className = "app-status visible" + (extraClass ? " " + extraClass : "");
    statusProgress.hidden = true;
    statusActions.innerHTML = "";
  }

  function show(message, type, autoHideMs) {
    resetBar(type);
    statusText.textContent = message;
    if (autoHideMs) hideTimer = setTimeout(hide, autoHideMs);
  }

  function hide() {
    clearAutoHide();
    statusBar.classList.remove("visible");
  }

  function progress(fraction, label) {
    resetBar("progress");
    statusText.textContent = label || "";
    statusProgress.hidden = false;
    statusProgressFill.style.width = Math.max(0, Math.min(100, fraction * 100)) + "%";
  }

  function confirmInline(message, onConfirm, onCancel) {
    resetBar("confirm");
    statusText.textContent = message;
    var yes = document.createElement("button");
    yes.type = "button";
    yes.textContent = "Confirmer";
    var no = document.createElement("button");
    no.type = "button";
    no.textContent = "Annuler";
    yes.addEventListener("click", function () { hide(); if (onConfirm) onConfirm(); });
    no.addEventListener("click", function () { hide(); if (onCancel) onCancel(); });
    statusActions.appendChild(yes);
    statusActions.appendChild(no);
  }

  window.AppStatus = {
    info: function (msg) { show(msg, "info", 4000); },
    success: function (msg) { show(msg, "success", 4000); },
    error: function (msg) { show(msg, "error"); },
    progress: progress,
    confirm: confirmInline,
    hide: hide
  };
})();