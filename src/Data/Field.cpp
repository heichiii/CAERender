#include "Field.h"
#include <algorithm>
void Field::computeRange()
{
    if (data.empty())
    {
        min_value = max_value = 0.0f;
        return;
    }
    min_value = *std::min_element(data.begin(), data.end());
    max_value = *std::max_element(data.begin(), data.end());
}
