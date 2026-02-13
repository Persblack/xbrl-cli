#include "taxonomie_config.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

TaxonomieConfig loadTaxonomieConfig(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        throw std::runtime_error("Taxonomie-Config nicht gefunden: " + jsonPath);
    }

    nlohmann::json j;
    file >> j;

    TaxonomieConfig config;
    config.version = j.value("version", "");
    config.namespaceUri = j.value("namespace", "");
    config.schemaRef = j.value("schemaRef", "");
    config.linkNamespace = j.value("linkNamespace", "http://www.xbrl.org/2003/linkbase");
    config.xlinkNamespace = j.value("xlinkNamespace", "http://www.w3.org/1999/xlink");

    // Load mapping (skip _comment keys)
    auto& mapping = j["mapping"];
    for (auto& [key, value] : mapping.items()) {
        if (key.substr(0, 8) == "_comment") {
            continue;
        }
        config.mapping[key] = value.get<std::string>();
    }

    // Load auffangpositionen (skip _comment keys)
    if (j.contains("auffangpositionen")) {
        auto& auffang = j["auffangpositionen"];
        for (auto& [key, value] : auffang.items()) {
            if (key.substr(0, 8) == "_comment") {
                continue;
            }
            AuffangPosition ap;
            ap.summeKey = value.value("summeKey", "");
            ap.auffangTag = value.value("auffangTag", "");
            if (value.contains("detailKeys")) {
                for (auto& dk : value["detailKeys"]) {
                    ap.detailKeys.push_back(dk.get<std::string>());
                }
            }
            config.auffangpositionen[key] = ap;
        }
    }

    // Load mussfelder
    if (j.contains("mussfelder")) {
        for (auto& mf : j["mussfelder"]) {
            config.mussfelder.push_back(mf.get<std::string>());
        }
    }

    // Load stammdaten mapping (skip _comment keys)
    if (j.contains("stammdatenMapping")) {
        auto& sm = j["stammdatenMapping"];
        for (auto& [key, value] : sm.items()) {
            if (key.substr(0, 8) == "_comment") {
                continue;
            }
            config.stammdatenMapping[key] = value.get<std::string>();
        }
    }

    spdlog::debug("Taxonomie-Config v{} geladen: {} Mappings, {} Auffangpositionen, {} Mussfelder",
                  config.version, config.mapping.size(), config.auffangpositionen.size(), config.mussfelder.size());

    return config;
}
