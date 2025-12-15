#ifndef SCRIP_MASTER_H
#define SCRIP_MASTER_H

#include <string>
#include <vector>
#include <unordered_map>

struct Scrip
{
    int token;
    std::string symbol;
    std::string exchange;
    std::string segment;
    std::string expiry;
    double strike;
    std::string optionType; // "CE", "PE", "XX"
    int lotSize;
    double tickSize;
};

class MasterRepository
{
public:
    // Fast lookup by Token
    std::unordered_map<int, Scrip> scripMap;

    // List for iteration
    std::vector<Scrip> allScrips;

    void addScrip(const Scrip &scrip)
    {
        allScrips.push_back(scrip);
        scripMap[scrip.token] = scrip;
    }

    const Scrip *getScrip(int token) const
    {
        auto it = scripMap.find(token);
        if (it != scripMap.end())
        {
            return &it->second;
        }
        return nullptr;
    }
};

#endif // SCRIP_MASTER_H
