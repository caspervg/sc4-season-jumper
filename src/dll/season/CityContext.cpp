#include "CityContext.hpp"

#include <cISC4App.h>
#include <cISC4City.h>
#include <cRZBaseString.h>
#include <GZServPtrs.h>

namespace SeasonJumper
{
    std::string GetCurrentCityName()
    {
        const cISC4AppPtr app;
        if (!app) {
            return "unknown city";
        }

        cISC4City* const city = app->GetCity();
        if (!city) {
            return "unknown city";
        }

        cRZBaseString cityName;
        if (!city->GetCityName(cityName) || cityName.Strlen() == 0) {
            return "unnamed city";
        }

        return cityName.ToChar();
    }
}
