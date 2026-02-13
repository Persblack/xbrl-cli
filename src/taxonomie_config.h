#pragma once

#include <map>
#include <string>
#include <vector>

// Auffangposition: catch-all position for a group
struct AuffangPosition {
    std::string summeKey;                   // Position ID of the group sum
    std::vector<std::string> detailKeys;    // Position IDs of detail positions
    std::string auffangTag;                 // XBRL tag for catch-all
};

// Taxonomie configuration loaded from JSON
struct TaxonomieConfig {
    std::string version;
    std::string namespaceUri;
    std::string schemaRef;
    std::string linkNamespace;
    std::string xlinkNamespace;

    // positionId -> XBRL tag name
    std::map<std::string, std::string> mapping;

    // groupName -> AuffangPosition
    std::map<std::string, AuffangPosition> auffangpositionen;

    // List of mandatory XBRL fields
    std::vector<std::string> mussfelder;

    // Stammdaten field -> XBRL tag
    std::map<std::string, std::string> stammdatenMapping;
};

// Load taxonomie configuration from JSON file
TaxonomieConfig loadTaxonomieConfig(const std::string& jsonPath);
