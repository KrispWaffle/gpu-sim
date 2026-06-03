#include "labeltable.hpp"

void LabelTable::addLabel(const std::string& labelName, int pos) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& l : labels) {
        if (l.labelName == labelName) return;
    }
    labels.push_back(Label{labelName, pos});
}

std::optional<int> LabelTable::getLabel(const std::string& name) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = std::find_if(labels.begin(), labels.end(),
        [&name](const Label& lbl) { return lbl.labelName == name; });
    if (it != labels.end()) return it->pos;
    return std::nullopt;
}

void LabelTable::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    labels.clear();
}
