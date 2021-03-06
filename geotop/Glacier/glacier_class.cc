/*
 * @brief Snow Data implementation
 */

#include "glacier_class.h"

Glacier::Glacier(double novalue, size_t total_pixel)
{
        melted = GeoVector<double>(total_pixel + 1);
        subl = GeoVector<double>(total_pixel + 1);
}

//FIXME: Horrible hack needed to cope with legacy code structure
void Glacier::allocate_data(double novalue, size_t total_pixel)
{
        melted.resize(total_pixel + 1);
        subl.resize(total_pixel + 1);
}

