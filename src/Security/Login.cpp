/*
 Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
 Licensed under AGPL-3.0 (or later), see <https://www.gnu.org/licenses/>.
 SPDX-License-Identifier: AGPL-3.0-or-later

 OFFLINE EDITION (BeamMP-Offline):
 This file has been rewritten to remove ALL communication with the BeamMP
 authentication backend (auth.beammp.com). No account, forum key, Discord
 or any other online identity service is required. The player simply picks
 a display name in-game; it is stored locally in a "player_name" file and
 sent to the server when connecting. See CONTEXT.md in the repo root.
*/

#include "Http.h"
#include "Logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
std::string PublicKey;
std::string PrivateKey;
extern bool LoginAuth;
extern std::string Username;
extern std::string UserRole;
extern int UserID;

static constexpr const char* PlayerNameFile = "player_name";

void UpdateKey(const char* newKey) {
    // In the offline edition this stores the locally chosen player name.
    if (newKey && newKey[0] != '\0') {
        PrivateKey = newKey;
        std::ofstream Key(PlayerNameFile);
        if (Key.is_open()) {
            Key << newKey;
            Key.close();
        } else
            fatal("Cannot write to disk!");
    } else if (fs::exists(PlayerNameFile)) {
        remove(PlayerNameFile);
    }
}

std::string GetFail(const std::string& R) {
    std::string DRet = R"({"success":false,"message":)";
    DRet += "\"" + R + "\"}";
    error(R);
    return DRet;
}

static std::string SanitizePlayerName(std::string Name) {
    // strip control characters, trim spaces, cap length (server caps at 32)
    Name.erase(std::remove_if(Name.begin(), Name.end(),
                   [](unsigned char c) { return c < 0x20 || c == 0x7F; }),
        Name.end());
    size_t Start = Name.find_first_not_of(" \t");
    if (Start == std::string::npos)
        return "";
    size_t End = Name.find_last_not_of(" \t");
    Name = Name.substr(Start, End - Start + 1);
    if (Name.size() > 32)
        Name.resize(32);
    return Name;
}

/// "username":"value","password":"value"
/// "Guest":"Name"
/// "pk":"private_key"
std::string Login(const std::string& fields) {
    if (fields == "LO") {
        Username = "";
        UserRole = "";
        UserID = -1;
        LoginAuth = false;
        UpdateKey(nullptr);
        return "";
    }
    // === OFFLINE MODE: no auth server is contacted. ===
    // The in-game UI sends {"username":"<name>"}; we validate it locally,
    // remember it, and report success immediately.
    info("Offline mode: setting local player name (no authentication server contacted)");
    std::string Name;
    try {
        nlohmann::json d = nlohmann::json::parse(fields, nullptr, false);
        if (!d.is_discarded() && d.is_object()) {
            if (d.contains("username") && d["username"].is_string()) {
                Name = d["username"].get<std::string>();
            } else if (d.contains("Guest") && d["Guest"].is_string()) {
                Name = d["Guest"].get<std::string>();
            }
        }
    } catch (const std::exception& e) {
        return GetFail(std::string("Failed to parse player name: ") + e.what());
    }

    Name = SanitizePlayerName(std::move(Name));
    if (Name.empty()) {
        // empty name = play as guest
        LoginAuth = true;
        Username = "";
        UserRole = "USER";
        UserID = 0;
        UpdateKey(nullptr);
        info("Offline mode: no name given, joining as Guest");
        return R"({"success":true,"message":"Offline mode: joining as Guest (no name set)"})";
    }

    LoginAuth = true;
    Username = Name;
    UserRole = "USER";
    UserID = 0;
    UpdateKey(Name.c_str());
    info("Offline mode: player name set to '" + Name + "'");
    return R"({"success":true,"message":"Offline mode: player name saved locally"})";
}

void CheckLocalKey() {
    // === OFFLINE MODE: restore the locally saved player name, never contact
    // any server. Always "logged in" — online identity simply does not exist.
    LoginAuth = true;
    UserRole = "USER";
    UserID = 0;
    Username = "";
    PublicKey = "";
    if (fs::exists(PlayerNameFile) && fs::file_size(PlayerNameFile) < 100) {
        std::ifstream Key(PlayerNameFile);
        if (Key.is_open()) {
            auto Size = fs::file_size(PlayerNameFile);
            std::string Buffer(Size, 0);
            Key.read(&Buffer[0], Size);
            Key.close();
            Username = SanitizePlayerName(std::move(Buffer));
            if (!Username.empty()) {
                info("Offline mode: using saved player name '" + Username + "'");
            }
        } else {
            warn("Could not open saved player name file!");
        }
    }
    if (Username.empty()) {
        debug("Offline mode: no saved player name, will join as Guest");
    }
}
