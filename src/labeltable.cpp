#include "labeltable.hpp"
labelTable &labelTable::getInstance()
{
    static labelTable instance;
    return instance;
}
void labelTable::addLabel(const std::string &labelName, int pos)
{
    std::lock_guard<std::mutex> lk(mtx_); 
    labels.push_back(Label{labelName, pos, false});
}

std::optional<int> labelTable::getLabel(const std::string &name)
{
    auto it = std::find_if(labels.begin(), labels.end(),
        [&name](const Label& lbl) {
            return lbl.pos;
        });
    if (it != labels.end()) {
        return it->pos;
    } else {
        return std::nullopt;

    }
}
