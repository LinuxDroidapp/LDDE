#include "ldde/launcher/launcher_item.hpp"
#include "ldde/launcher/launcher_icon_resolver.hpp"

namespace ldde::launcher {

LauncherItem LauncherItem::from_metadata(
    const application::ApplicationMetadata& meta,
    LauncherIconResolver* resolver) {
    LauncherItem item;
    item.id_ = meta.id();
    item.name_ = meta.name();
    item.generic_name_ = meta.generic_name();
    item.comment_ = meta.comment();
    item.icon_ref_ = meta.icon();
    item.categories_ = meta.categories();
    item.executable_ = meta.executable();
    item.exec_args_ = meta.exec_args();
    item.terminal_ = meta.terminal();
    item.desktop_entry_path_ = meta.source().path();
    item.startup_wm_class_ = meta.startup_wm_class();
    item.startup_notify_ = meta.startup_notify();

    if (resolver) {
        item.resolved_icon_path_ = resolver->resolve(item.icon_ref_);
    }

    return item;
}

void LauncherItem::resolve_icon(LauncherIconResolver& resolver, int preferred_size) {
    resolved_icon_path_ = resolver.resolve(icon_ref_, preferred_size);
}

} // namespace ldde::launcher
