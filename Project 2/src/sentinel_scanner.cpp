#include "core/sentinel_scanner.h"
#include <stdexcept>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(sentinel), pending_("") {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk)
{
    Out result{"", false};
    for (std::size_t i = 0; i < chunk.size(); i++)
    {
        char character = chunk[i];
        if (character == SentinelScanner::sentinel_[pending_.size()])
        {
            if (pending_.size() + 1 == sentinel_.size())
            {
                result.sentinel_found = true;
                pending_.clear();
                return result;
            }
            pending_ = pending_ + character;
        }
        else
        {
            std::string pot_candidate = pending_ + character;

            while (!pot_candidate.empty() && (SentinelScanner::sentinel_.compare(0, pot_candidate.size(), pot_candidate) != 0))
            {
                result.safe_text = result.safe_text + pot_candidate[0];
                pot_candidate.erase(0, 1);
            }
            pending_ = pot_candidate;
        }
    }
    return result;
}

SentinelScanner::Out SentinelScanner::flush()
{
    Out result{pending_, false};
    pending_.clear();
    return result;
}
