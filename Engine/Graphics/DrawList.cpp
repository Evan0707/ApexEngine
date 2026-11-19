#include "Graphics/DrawList.h"

#include <algorithm>

namespace apex::graphics {

void sortDrawItems(std::vector<DrawItem>& items) {
    std::ranges::stable_sort(items, [](const DrawItem& left, const DrawItem& right) {
        if (left.materialIndex != right.materialIndex) {
            return left.materialIndex < right.materialIndex;
        }
        return left.meshIndex < right.meshIndex;
    });
}

} // namespace apex::graphics
