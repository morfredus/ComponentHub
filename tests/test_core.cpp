// Tests ctest du cœur (sans Qt) : client HTTP (chunked), JSON tolérant, Tar, NULL SQLite,
// enregistrement distant mal typé, aller-retour sauvegarde/restauration en WAL.
// Pas de framework : un CHECK qui échoue affiche la ligne et fait échouer le test.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include "AppContext.h"
#include "json_tolerant.h"
#include "net/HttpClient.h"
#include "platform/Backup.h"
#include "platform/Tar.h"
#include "test_util.h"
#include "util/sha256.h"

static void testChunked() {
    std::string out;
    CHECK(chnet::decodeChunked("4\r\nWiki\r\n5;ext=1\r\npedia\r\n0\r\n\r\n", out) && out == "Wikipedia");
    CHECK(!chnet::decodeChunked("4\r\nWi", out));             // tronqué
    CHECK(!chnet::decodeChunked("zz\r\nabc\r\n0\r\n\r\n", out)); // taille non hexadécimale
    CHECK(!chnet::decodeChunked("4\r\nWikiXX0\r\n\r\n", out));   // CRLF de bloc manquant

    // Réponse complète : chunked décodé, Content-Length inchangé.
    auto r = chnet::parseResponse(
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: Chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n");
    CHECK(r.ok && r.status == 200 && r.body == "abc");
    r = chnet::parseResponse("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello");
    CHECK(r.ok && r.body == "hello");
    r = chnet::parseResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n9\r\nabc");
    CHECK(!r.ok && r.error == "http.invalid_chunked");
}

static void testJsonTolerant() {
    const nlohmann::json j = nlohmann::json::parse(R"({"notes":null,"name":"x","qty":"abc","n":3})");
    CHECK(domain::jget(j, "notes", "d") == "d");           // null -> défaut
    CHECK(domain::jget(j, "name", "") == "x");
    CHECK(domain::jget(j, "qty", 7) == 7);                  // texte pour un nombre -> défaut
    CHECK(domain::jget(j, "n", std::string("z")) == "z");   // nombre pour un texte -> défaut
    CHECK(domain::jget(j, "absent", 5) == 5);
}

// --- Fabrique d'archives tar « à la main » pour les cas hostiles -------------------------------
struct RawEntry {
    std::string name;
    char type = '0';
    std::string content;
    bool badChecksum = false;
};

static std::string rawArchive(const std::vector<RawEntry>& entries, bool terminator = true) {
    std::string out;
    for (const auto& e : entries) {
        char h[512];
        std::memset(h, 0, 512);
        std::memcpy(h, e.name.data(), std::min<size_t>(e.name.size(), 100));
        std::snprintf(h + 100, 8, "%07o", 0644);
        std::snprintf(h + 124, 12, "%011o", (unsigned)e.content.size());
        h[156] = e.type;
        std::memcpy(h + 257, "ustar", 5);
        std::memset(h + 148, ' ', 8);
        unsigned sum = 0;
        for (int i = 0; i < 512; i++) sum += (unsigned char)h[i];
        std::snprintf(h + 148, 8, "%06o", e.badChecksum ? sum + 7 : sum);
        h[155] = ' ';
        out.append(h, 512);
        out += e.content;
        out.append((512 - e.content.size() % 512) % 512, '\0');
    }
    if (terminator) out.append(1024, '\0');
    return out;
}

static int extractRaw(const std::string& archive, const fs::path& out, std::vector<std::string>& errors) {
    const fs::path tarFile = out.parent_path() / (out.filename().string() + ".tar");
    writeFile(tarFile, archive);
    errors.clear();
    return chdesktop::tar::extract(tarFile.string(), out.string(), errors);
}

static bool hasError(const std::vector<std::string>& errors, const char* code) {
    for (const auto& e : errors) if (e.rfind(code, 0) == 0) return true;
    return false;
}

static void testTar() {
    const fs::path d = freshDir("ch_test_tar");
    writeFile(d / "aaaaaaa.txt", "bonjour");
    const std::string tarPath = (d / "x.tar").string();
    std::string err;
    CHECK(chdesktop::tar::pack(tarPath, d.string(), {".tmp"}, &err));
    CHECK(chdesktop::tar::looksLikeTar(tarPath));

    {   // nom sain : restauré
        const fs::path out = freshDir("ch_test_tar_out");
        std::vector<std::string> errors;
        CHECK(chdesktop::tar::extract(tarPath, out.string(), errors) == 1 && errors.empty());
        CHECK(readFile(out / "aaaaaaa.txt") == "bonjour");
    }

    // Noms hostiles : jamais écrits, ni dans le dossier cible ni au-dessus.
    const char* const bad[] = {"a:b.txt", "CON", "nul.txt", "COM1.log", "lpt9", "../evil.txt", "..evil",
                               "end.", "/abs.txt", "x\\y.txt", "a/../../b.txt", "C:/x.txt", "a//b.txt",
                               "./x.txt", "dir/CON.txt", "dir/nul", "dir/ends.", "a/b/c/d/e/f/g/h/i.txt"};
    for (const char* name : bad) {
        const fs::path base = freshDir("ch_test_tar_hostile");
        const fs::path out = base / "out";
        fs::create_directories(out);
        std::vector<std::string> errors;
        const int n = extractRaw(rawArchive({{name, '0', "PWNED"}}), out, errors);
        CHECK(n == 0);
        CHECK(hasError(errors, "tar.unsafe_entry"));
        CHECK(!fs::exists(base / "evil.txt") && !fs::exists(base / "b.txt") && !fs::exists(base / "..evil"));
        std::size_t files = 0;
        for (const auto& e : fs::recursive_directory_iterator(out)) if (e.is_regular_file()) files++;
        CHECK(files == 0);
    }
    // Liens, périphériques : refusés, et le contenu annoncé est sauté (l'entrée suivante reste lue).
    for (char type : {'1', '2', '3', '4', '6', 'x', 'g'}) {
        const fs::path out = freshDir("ch_test_tar_link");
        std::vector<std::string> errors;
        const int n = extractRaw(rawArchive({{"lien", type, "cible"}, {"ok.txt", '0', "ok"}}), out, errors);
        CHECK(n == 1 && hasError(errors, "tar.unsafe_entry") && readFile(out / "ok.txt") == "ok");
        CHECK(!fs::exists(out / "lien"));
    }
    {   // chemin imbriqué sain : accepté, dossiers créés
        const fs::path out = freshDir("ch_test_tar_nested");
        std::vector<std::string> errors;
        const int n = extractRaw(rawArchive({{"x/", '5', ""}, {"x/y/z.txt", '0', "profond"}}), out, errors);
        CHECK(n == 1 && errors.empty() && readFile(out / "x" / "y" / "z.txt") == "profond");
    }
    {   // somme de contrôle fausse : archive corrompue, rien d'écrit
        const fs::path out = freshDir("ch_test_tar_sum");
        std::vector<std::string> errors;
        CHECK(extractRaw(rawArchive({{"a.txt", '0', "x", true}}), out, errors) == 0);
        CHECK(hasError(errors, "tar.corrupt") && !fs::exists(out / "a.txt"));
    }
    {   // tronquée au milieu des données : détectée, fichier partiel supprimé
        const std::string full = rawArchive({{"a.txt", '0', std::string(1000, 'a')}});
        const fs::path out = freshDir("ch_test_tar_cut");
        std::vector<std::string> errors;
        CHECK(extractRaw(full.substr(0, 512 + 300), out, errors) == 0);
        CHECK(hasError(errors, "tar.write_failed") && !fs::exists(out / "a.txt"));
    }
    {   // tronquée au milieu d'un en-tête
        const std::string full = rawArchive({{"a.txt", '0', "x"}, {"b.txt", '0', "y"}});
        const fs::path out = freshDir("ch_test_tar_cuthdr");
        std::vector<std::string> errors;
        extractRaw(full.substr(0, 1024 + 100), out, errors);
        CHECK(hasError(errors, "tar.corrupt"));
    }
    {   // coupée proprement entre deux entrées, sans marque de fin : incomplète
        const std::string full = rawArchive({{"a.txt", '0', "x"}}, /*terminator=*/false);
        const fs::path out = freshDir("ch_test_tar_noend");
        std::vector<std::string> errors;
        extractRaw(full, out, errors);
        CHECK(hasError(errors, "tar.corrupt"));
    }
    {   // nom > 99 octets : pack refuse
        const fs::path d2 = freshDir("ch_test_tar_long");
        writeFile(d2 / (std::string(100, 'n') + ".txt"), "x");
        CHECK(!chdesktop::tar::pack((d / "long.tar").string(), d2.string(), {}, &err));
        CHECK(err.rfind("tar.name_too_long", 0) == 0 && !fs::exists(d / "long.tar"));
    }
    {   // profondeur excessive : pack refuse avec un message clair
        const fs::path d2 = freshDir("ch_test_tar_deep");
        fs::path p = d2;
        for (int i = 0; i < chdesktop::tar::kMaxDepth + 1; i++) p /= "d";
        writeFile(p / "f.txt", "x");
        CHECK(!chdesktop::tar::pack((d / "deep.tar").string(), d2.string(), {}, &err));
        CHECK(err.rfind("tar.too_deep", 0) == 0 && !fs::exists(d / "deep.tar"));
    }
    {   // plafond de taille totale
        const fs::path d2 = freshDir("ch_test_tar_big");
        writeFile(d2 / "a.bin", std::string(5000, 'a'));
        writeFile(d2 / "b.bin", std::string(5000, 'b'));
        CHECK(!chdesktop::tar::pack((d / "big.tar").string(), d2.string(), {}, &err, {}, 8000));
        CHECK(err.rfind("tar.too_large", 0) == 0 && !fs::exists(d / "big.tar"));
        CHECK(chdesktop::tar::pack((d / "big.tar").string(), d2.string(), {}, &err, {}, 10000));
    }
}

// Aller-retour d'un arbre : sous-dossiers imbriqués, binaires, dossier vide, noms accentués.
static void testTarTree() {
    const fs::path src = freshDir("ch_test_tree_src");
    const fs::path att = freshDir("ch_test_tree_att");
    std::mt19937 rng(42);
    auto randomBytes = [&](size_t n) {
        std::string s(n, '\0');
        for (auto& c : s) c = (char)(rng() & 0xFF);
        return s;
    };
    std::string all256;
    for (int i = 0; i < 256; i++) all256.push_back((char)i);

    writeFile(src / "racine.txt", "racine");
    writeFile(src / "ignore.tmp", "temporaire");
    writeFile(src / "a" / "b" / "c" / "profond.bin", all256);
    writeFile(src / "a" / "vide.txt", "");
    writeFile(src / "é à" / "ça.txt", "accents");
    fs::create_directories(src / "dossier-vide");
    writeFile(att / "h1", randomBytes(1500000));            // 1,5 Mio : plusieurs milliers de blocs
    writeFile(att / "h2", randomBytes(513));                // juste au-dessus d'un bloc
    writeFile(att / "h3", randomBytes(512));                // exactement un bloc
    writeFile(att / "h4.tmp.7", "ecriture atomique en cours");
    writeFile(att / "sous" / "h5", "imbriqué");
    std::error_code ec;
    fs::create_symlink(src / "racine.txt", src / "lien.txt", ec);   // peut échouer sous Windows : ignoré

    const fs::path tarPath = freshDir("ch_test_tree_tar") / "tree.tar";
    std::string err;
    CHECK(chdesktop::tar::pack(tarPath.string(), src.string(), {".tmp"}, &err,
                               {{"attachments", att.string()}}));
    const fs::path out = freshDir("ch_test_tree_out");
    std::vector<std::string> errors;
    CHECK(chdesktop::tar::extract(tarPath.string(), out.string(), errors) == 8);
    CHECK(errors.empty());
    CHECK(readFile(out / "racine.txt") == "racine");
    CHECK(readFile(out / "a" / "b" / "c" / "profond.bin") == all256);
    CHECK(fs::exists(out / "a" / "vide.txt") && readFile(out / "a" / "vide.txt").empty());
    CHECK(readFile(out / "é à" / "ça.txt") == "accents");
    CHECK(fs::is_directory(out / "dossier-vide"));
    CHECK(readFile(out / "attachments" / "h1") == readFile(att / "h1"));
    CHECK(readFile(out / "attachments" / "h2") == readFile(att / "h2"));
    CHECK(readFile(out / "attachments" / "h3") == readFile(att / "h3"));
    CHECK(readFile(out / "attachments" / "sous" / "h5") == "imbriqué");
    CHECK(!fs::exists(out / "ignore.tmp") && !fs::exists(out / "attachments" / "h4.tmp.7"));
    CHECK(!fs::exists(out / "lien.txt"));                  // les liens ne sont jamais archivés
    CHECK(!fs::exists(out / "tree.tar"));
}

static void testNullAndRemote() {
    const fs::path d = freshDir("ch_test_null");
    chdesktop::AppContext ctx(d.string());
    domain::Component c;
    c.name = "R1";
    c = ctx.inventory.saveComponent(c);
    // Valeur NULL en base : ne doit ni planter ni lever (std::string depuis nullptr).
    ctx.database().execSql("UPDATE components SET notes = NULL, supplier = NULL;");
    CHECK(ctx.components.findAll().size() == 1);

    // Enregistrement distant mal typé : appliqué avec des valeurs par défaut.
    domain::SyncRecord rec;
    rec.id = "remote-1";
    rec.type = "component";
    rec.meta.rev = 5;
    rec.meta.updatedAt = "2026-01-01T00:00:00Z";
    rec.data = nlohmann::json::parse(R"({"name":"Distant","notes":null,"quantity":"beaucoup","price":null})");
    CHECK(ctx.components.applyRemote(rec));
    auto got = ctx.components.findById("remote-1");
    CHECK(got && got->name == "Distant" && got->notes.empty() && got->quantity == 0);
}

static void testBackupRoundTrip() {
    const fs::path src = freshDir("ch_test_bk_src");
    const fs::path dst = freshDir("ch_test_bk_dst");
    const std::string tarPath = (fs::temp_directory_path() / "ch_test_bk.tar").string();
    std::error_code ec;
    fs::remove(tarPath, ec);

    {
        chdesktop::AppContext ctx(src.string());   // base ouverte en WAL pendant toute la sauvegarde
        for (int i = 0; i < 50; i++) {
            domain::Component c;
            c.name = "Composant " + std::to_string(i);
            c.notes = "note é €";
            ctx.inventory.saveComponent(c);
        }
        writeFile(src / "sync_state.json", "{\"deviceId\":\"d1\"}");
        std::string err;
        CHECK(chdesktop::backup::create(ctx.database(), src.string(), tarPath,
                                        {{"app_config.json", "{}"}}, err));
        CHECK(err.empty());
    }

    // L'archive ne doit contenir ni -wal ni -shm (extraction brute pour le vérifier).
    {
        const fs::path raw = freshDir("ch_test_bk_raw");
        std::vector<std::string> errors;
        chdesktop::tar::extract(tarPath, raw.string(), errors);
        CHECK(fs::exists(raw / "componenthub.sqlite"));
        CHECK(!fs::exists(raw / "componenthub.sqlite-wal") && !fs::exists(raw / "componenthub.sqlite-shm"));
        CHECK(fs::exists(raw / "sync_state.json") && fs::exists(raw / "app_config.json"));
    }

    // Restauration dans un dossier qui contient déjà une autre base (et un -wal orphelin).
    {
        chdesktop::AppContext other(dst.string());
        domain::Component c;
        c.name = "A ECRASER";
        other.inventory.saveComponent(c);
    }
    writeFile(dst / "componenthub.sqlite-wal", "orphelin");
    std::vector<std::string> warnings;
    std::string err;
    CHECK(chdesktop::backup::stageRestore(tarPath, dst.string(), warnings, err));
    CHECK(fs::exists(dst / "componenthub.sqlite"));   // pas encore touchée
    bool applied = false;
    CHECK(chdesktop::backup::applyPendingRestore(dst.string(), applied, err) && applied);
    CHECK(fs::exists(dst / "componenthub.sqlite.bak"));
    CHECK(!fs::exists(dst / chdesktop::backup::kPendingDir));
    chdesktop::AppContext restored(dst.string());
    auto all = restored.components.findAll();
    CHECK(all.size() == 50);
    bool accents = false;
    for (auto& c : all) accents = accents || c.notes == "note é €";
    CHECK(accents);
    CHECK(readFile(dst / "sync_state.json").find("d1") != std::string::npos);

    // Archive invalide : rien ne change.
    writeFile(fs::temp_directory_path() / "ch_test_junk.tar", std::string(2048, 'x'));
    CHECK(!chdesktop::backup::stageRestore((fs::temp_directory_path() / "ch_test_junk.tar").string(),
                                           dst.string(), warnings, err));
    CHECK(!fs::exists(dst / chdesktop::backup::kPendingDir));
}

// --- Sauvegarde / restauration avec pièces jointes ------------------------------------------
static std::string storeBlob(chdesktop::AppContext& ctx, const std::string& bytes) {
    const std::string h = chutil::sha256Hex(bytes);
    CHECK(ctx.attachments.put(h, bytes));
    return h;
}

static void testBackupAttachments() {
    std::string binary;
    for (int i = 0; i < 1000; i++) binary.push_back((char)(i * 7));
    const fs::path src = freshDir("ch_test_ba_src");
    const std::string tarPath = (fs::temp_directory_path() / "ch_test_ba.tar").string();
    std::string h1, h2, h3, hBad;
    {
        chdesktop::AppContext ctx(src.string());
        domain::Component c;
        c.name = "Avec pièces jointes";
        ctx.inventory.saveComponent(c);
        h1 = storeBlob(ctx, binary);
        h2 = storeBlob(ctx, "second blob");
        h3 = storeBlob(ctx, std::string(70000, 'z'));
        // Fichier nommé comme un hash mais au contenu différent : doit être écarté à la restauration.
        hBad = chutil::sha256Hex("autre chose");
        writeFile(src / "attachments" / hBad, "contenu qui ne correspond pas");
        writeFile(src / "attachments" / (h1 + ".tmp.3"), "ecriture en cours");   // jamais archivé
        std::string err;
        CHECK(chdesktop::backup::create(ctx.database(), src.string(), tarPath, {{"app_config.json", "{}"}}, err));
    }

    // Poste de destination : autre base ET autres pièces jointes, qui doivent être mises de côté.
    const fs::path dst = freshDir("ch_test_ba_dst");
    {
        chdesktop::AppContext other(dst.string());
        domain::Component c;
        c.name = "A ECRASER";
        other.inventory.saveComponent(c);
        storeBlob(other, "ancienne piece jointe");
    }
    const std::string oldBlobHash = chutil::sha256Hex("ancienne piece jointe");

    std::vector<std::string> warnings;
    std::string err;
    CHECK(chdesktop::backup::stageRestore(tarPath, dst.string(), warnings, err));
    CHECK(hasError(warnings, "backup.attachment_invalid"));                 // le faux hash est signalé
    const fs::path pending = dst / chdesktop::backup::kPendingDir;
    CHECK(fs::exists(pending / "attachments" / h1) && !fs::exists(pending / "attachments" / hBad));
    CHECK(!fs::exists(pending / "attachments" / (h1 + ".tmp.3")));
    CHECK(fs::exists(dst / "attachments" / oldBlobHash));                   // pas encore touchée
    bool applied = false;
    CHECK(chdesktop::backup::applyPendingRestore(dst.string(), applied, err) && applied);
    CHECK(readFile(dst / "attachments" / h1) == binary);
    CHECK(readFile(dst / "attachments" / h2) == "second blob");
    CHECK(readFile(dst / "attachments" / h3) == std::string(70000, 'z'));
    CHECK(!fs::exists(dst / "attachments" / oldBlobHash) && !fs::exists(dst / "attachments" / hBad));
    CHECK(readFile(dst / "attachments.bak" / oldBlobHash) == "ancienne piece jointe");   // mis de côté
    CHECK(fs::exists(dst / "componenthub.sqlite.bak") && !fs::exists(pending));
    {
        chdesktop::AppContext restored(dst.string());
        CHECK(restored.components.findAll().size() == 1 && restored.components.findAll()[0].name == "Avec pièces jointes");
    }

    // Archive ancienne (1.11 et avant) : pas de dossier attachments, avec un .bak parasite. Les
    // pièces jointes locales restent en place et le .bak local (copie de sécurité) n'est pas écrasé.
    {
        const fs::path legacySrc = freshDir("ch_test_ba_legacy");
        {
            chdesktop::AppContext ctx(freshDir("ch_test_ba_legacy_db").string());
            domain::Component c;
            c.name = "Ancienne sauvegarde";
            ctx.inventory.saveComponent(c);
            ctx.database().snapshotTo((legacySrc / "componenthub.sqlite").string());
        }
        writeFile(legacySrc / "sync_state.json", "{\"deviceId\":\"ancien\"}");
        writeFile(legacySrc / "componenthub.sqlite.bak", "BAK PARASITE");
        const std::string legacyTar = (fs::temp_directory_path() / "ch_test_ba_legacy.tar").string();
        CHECK(chdesktop::tar::pack(legacyTar, legacySrc.string()));

        warnings.clear();
        CHECK(chdesktop::backup::stageRestore(legacyTar, dst.string(), warnings, err));
        CHECK(!fs::exists(pending / "attachments"));
        CHECK(chdesktop::backup::applyPendingRestore(dst.string(), applied, err) && applied);
        CHECK(readFile(dst / "attachments" / h1) == binary);                // pièces jointes locales intactes
        CHECK(fs::exists(dst / "attachments.bak" / oldBlobHash));           // pas retouché
        CHECK(readFile(dst / "componenthub.sqlite.bak") != "BAK PARASITE");
        CHECK(readFile(dst / "sync_state.json").find("ancien") != std::string::npos);
        chdesktop::AppContext restored(dst.string());
        CHECK(restored.components.findAll().size() == 1 && restored.components.findAll()[0].name == "Ancienne sauvegarde");
    }

    // Archive hostile : base valide + entrées qui sortent du dossier. Restaurée sans ces entrées.
    {
        const fs::path base = freshDir("ch_test_ba_hostile");
        const fs::path d2 = base / "data";
        fs::create_directories(d2);
        const std::string dbBytes = readFile(dst / "componenthub.sqlite");
        const std::string raw = rawArchive({{"componenthub.sqlite", '0', dbBytes},
                                            {"../../evil.txt", '0', "PWNED"},
                                            {"attachments/../../evil2.txt", '0', "PWNED"},
                                            {"attachments/CON", '0', "PWNED"}});
        const std::string hostileTar = (base / "hostile.tar").string();
        writeFile(hostileTar, raw);
        warnings.clear();
        CHECK(chdesktop::backup::stageRestore(hostileTar, d2.string(), warnings, err));
        CHECK(hasError(warnings, "tar.unsafe_entry"));
        CHECK(!fs::exists(base / "evil.txt") && !fs::exists(base.parent_path() / "evil.txt") && !fs::exists(d2 / "evil2.txt"));
        CHECK(!fs::exists(d2 / chdesktop::backup::kPendingDir / "attachments" / "CON"));
    }

    // Archive tronquée : rejetée, dossier d'attente supprimé, données locales intactes.
    {
        const std::string full = readFile(tarPath);
        writeFile(fs::temp_directory_path() / "ch_test_ba_cut.tar", full.substr(0, full.size() / 2));
        warnings.clear();
        CHECK(!chdesktop::backup::stageRestore((fs::temp_directory_path() / "ch_test_ba_cut.tar").string(),
                                               dst.string(), warnings, err));
        CHECK(!fs::exists(pending) && fs::exists(dst / "attachments" / h1));
    }

    // Reprise après arrêt brutal : l'ancienne base a déjà été mise de côté (.bak) mais la nouvelle
    // n'est pas en place. Le .bak est la SEULE copie de l'ancienne base : il ne doit pas être supprimé.
    {
        const fs::path crash = freshDir("ch_test_ba_crash");
        {
            chdesktop::AppContext ctx(crash.string());
            domain::Component c;
            c.name = "Ancienne base";
            ctx.inventory.saveComponent(c);
        }
        warnings.clear();
        CHECK(chdesktop::backup::stageRestore(tarPath, crash.string(), warnings, err));
        fs::rename(crash / "componenthub.sqlite", crash / "componenthub.sqlite.bak");   // état « mi-chemin »
        std::error_code ec;
        fs::remove(crash / "componenthub.sqlite-wal", ec);
        fs::remove(crash / "componenthub.sqlite-shm", ec);
        CHECK(chdesktop::backup::applyPendingRestore(crash.string(), applied, err) && applied);
        CHECK(fs::exists(crash / "componenthub.sqlite.bak") && fs::exists(crash / "componenthub.sqlite"));
        chdesktop::AppContext restored(crash.string());
        CHECK(restored.components.findAll().size() == 1 && restored.components.findAll()[0].name == "Avec pièces jointes");
    }
}

int main() {
    testChunked();
    testJsonTolerant();
    testTar();
    testTarTree();
    testNullAndRemote();
    testBackupRoundTrip();
    testBackupAttachments();
    std::printf(g_failures ? "%d echec(s)\n" : "tous les tests passent\n", g_failures);
    return g_failures ? 1 : 0;
}
