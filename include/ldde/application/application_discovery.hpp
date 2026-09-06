#pragma once

#include "ldde/core/error.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/application/application_discovery_policy.hpp"

namespace ldde::application {

class ApplicationDiscovery {
public:
    ApplicationDiscovery(ApplicationCatalog& catalog, ApplicationDiscoveryPolicy policy);

    [[nodiscard]] core::Status scan_and_refresh();

    [[nodiscard]] const ApplicationDiscoveryPolicy& policy() const noexcept { return policy_; }
    [[nodiscard]] ApplicationDiscoveryPolicy& policy() noexcept { return policy_; }
    [[nodiscard]] const ApplicationCatalog& catalog() const noexcept { return catalog_; }
    [[nodiscard]] ApplicationCatalog& catalog() noexcept { return catalog_; }

private:
    ApplicationCatalog& catalog_;
    ApplicationDiscoveryPolicy policy_;
};

} // namespace ldde::application

