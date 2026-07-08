/**
 * Module — Interface de base pour tout module métier du framework.
 *
 * Le framework (core + services) ne connaît jamais le métier : il fournit
 * des services (WiFi, web, stockage, OTA, logs...) et appelle les modules
 * métier via cette interface neutre.
 */

#pragma once
#include <Arduino.h>

class WebRouter;  // forward declaration — voir api/api_router

class Module {
public:
    virtual ~Module() = default;

    // Nom court utilisé pour les logs (Log::i(name(), ...))
    virtual const char* name() const = 0;

    // Initialisation — appelée une fois, après que tous les services
    // (WiFi, stockage, etc.) soient prêts.
    virtual void begin() {}

    // Boucle principale — appelée à chaque itération de loop(), non bloquante.
    virtual void loop() {}

    // Enregistrement des routes HTTP propres au module (optionnel).
    virtual void registerRoutes(WebRouter& router) { (void)router; }
};
