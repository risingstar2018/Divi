// Copyright (c) 2020 The DIVI Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ForkActivation.h"

#include "chain.h"
#include "primitives/block.h"

#include <unordered_map>

#include <Settings.h>
#include <set>
extern Settings& settings;

namespace
{
constexpr int64_t unixTimestampForDec31stMidnight = 1609459199;
const std::set<Fork> manualOverrides = {Fork::StakingVaults,Fork::HardenedStakeModifier,Fork::UniformLotteryWinners};
/**
 * For forks that get activated at a certain block time, the associated
 * activation times.
 */
const std::unordered_map<Fork, int64_t,std::hash<int>> ACTIVATION_TIMES = {
  /* FIXME: Set real activation height for staking vaults once
     the schedule has been finalised.  */
  {Fork::TestByTimestamp, 1000000000},
  {Fork::StakingVaults, unixTimestampForDec31stMidnight},
  {Fork::HardenedStakeModifier, unixTimestampForDec31stMidnight},
  {Fork::UniformLotteryWinners, unixTimestampForDec31stMidnight},
};

} // anonymous namespace

ActivationState::ActivationState(const CBlockIndex* pi)
  : nTime(pi->nTime)
{}

ActivationState::ActivationState(const CBlockHeader& block)
  : nTime(block.nTime)
{}

bool ActivationState::IsActive(const Fork f) const
{
  constexpr char manualForkSettingLookup[] = "-manual_fork";
  if(settings.ParameterIsSet(manualForkSettingLookup) && manualOverrides.count(f)>0)
  {
    const int64_t timestampOverride = settings.GetArg(manualForkSettingLookup,0);
    return nTime >= timestampOverride;
  }
  const auto mit = ACTIVATION_TIMES.find(f);
  assert(mit != ACTIVATION_TIMES.end());
  return nTime >= mit->second;
}
