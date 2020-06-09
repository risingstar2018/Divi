#include <SuperblockHelpers.h>

#include <chainparams.h>
#include <BlockRewards.h>
#include <spork.h>
#include <timedata.h>
#include <chain.h>

extern CChain chainActive;

// Legacy methods
CAmount BlockSubsidy(int nHeight, const CChainParams& chainParameters)
{
    if(nHeight == 0) {
        return 50 * COIN;
    } else if (nHeight == 1) {
        return chainParameters.premineAmt;
    }

    CAmount nSubsidy = std::max(
        1250 - 100* std::max(nHeight/chainParameters.SubsidyHalvingInterval() -1,0),
        250)*COIN;
    
    return nSubsidy;
}
CAmount Legacy::GetFullBlockValue(int nHeight, const CChainParams& chainParameters)
{
    if(sporkManager.IsSporkActive(SPORK_15_BLOCK_VALUE)) {
        MultiValueSporkList<BlockSubsiditySporkValue> vBlockSubsiditySporkValues;
        CSporkManager::ConvertMultiValueSporkVector(sporkManager.GetMultiValueSpork(SPORK_15_BLOCK_VALUE), vBlockSubsiditySporkValues);
        auto nBlockTime = chainActive[nHeight] ? chainActive[nHeight]->nTime : GetAdjustedTime();
        BlockSubsiditySporkValue activeSpork = CSporkManager::GetActiveMultiValueSpork(vBlockSubsiditySporkValues, nHeight, nBlockTime);

        if(activeSpork.IsValid() && 
            (activeSpork.nActivationBlockHeight % chainParameters.SubsidyHalvingInterval()) == 0 )
        {
            // we expect that this value is in coins, not in satoshis
            return activeSpork.nBlockSubsidity * COIN;
        }
    }

    return BlockSubsidy(nHeight, chainParameters);
}

CBlockRewards Legacy::GetBlockSubsidity(int nHeight, const CChainParams& chainParameters)
{
    CAmount nSubsidy = Legacy::GetFullBlockValue(nHeight,chainParameters);

    if(nHeight <= chainParameters.LAST_POW_BLOCK()) {
        return CBlockRewards(nSubsidy, 0, 0, 0, 0, 0);
    }

    CAmount nLotteryPart = (nHeight >= chainParameters.GetLotteryBlockStartBlock()) ? (50 * COIN) : 0;

    assert(nSubsidy >= nLotteryPart);
    nSubsidy -= nLotteryPart;

    auto helper = [nHeight,&chainParameters,nSubsidy,nLotteryPart](
        int nStakePercentage, 
        int nMasternodePercentage,
        int nTreasuryPercentage, 
        int nProposalsPercentage, 
        int nCharityPercentage) 
    {
        auto helper = [nSubsidy](int percentage) {
            return (nSubsidy * percentage) / 100;
        };

        return CBlockRewards(
            helper(nStakePercentage), 
            helper(nMasternodePercentage), 
            helper(nTreasuryPercentage), 
            helper(nCharityPercentage),
            nLotteryPart, 
            helper(nProposalsPercentage));
    };

    if(sporkManager.IsSporkActive(SPORK_13_BLOCK_PAYMENTS)) {
        MultiValueSporkList<BlockPaymentSporkValue> vBlockPaymentsValues;
        CSporkManager::ConvertMultiValueSporkVector(sporkManager.GetMultiValueSpork(SPORK_13_BLOCK_PAYMENTS), vBlockPaymentsValues);
        auto nBlockTime = chainActive[nHeight] ? chainActive[nHeight]->nTime : GetAdjustedTime();
        BlockPaymentSporkValue activeSpork = CSporkManager::GetActiveMultiValueSpork(vBlockPaymentsValues, nHeight, nBlockTime);

        if(activeSpork.IsValid() &&
            (activeSpork.nActivationBlockHeight % chainParameters.SubsidyHalvingInterval()) == 0 ) {
            // we expect that this value is in coins, not in satoshis
            return helper(
                activeSpork.nStakeReward,
                activeSpork.nMasternodeReward,
                activeSpork.nTreasuryReward, 
                activeSpork.nProposalsReward, 
                activeSpork.nCharityReward);
        }
    }


    return helper(38, 45, 16, 0, 1);
}


bool Legacy::IsValidLotteryBlockHeight(int nBlockHeight,const CChainParams& chainParams)
{
    return nBlockHeight >= chainParams.GetLotteryBlockStartBlock() &&
            ((nBlockHeight % chainParams.GetLotteryBlockCycle()) == 0);
}

bool Legacy::IsValidTreasuryBlockHeight(int nBlockHeight,const CChainParams& chainParams)
{
    return nBlockHeight >= chainParams.GetTreasuryPaymentsStartBlock() &&
            ((nBlockHeight % chainParams.GetTreasuryPaymentsCycle()) == 0);
}

int64_t Legacy::GetTreasuryReward(const CBlockRewards &rewards, const CChainParams& chainParameters)
{
    return rewards.nTreasuryReward*chainParameters.GetTreasuryPaymentsCycle();
}

int64_t Legacy::GetCharityReward(const CBlockRewards &rewards, const CChainParams& chainParameters)
{
    return rewards.nCharityReward*chainParameters.GetTreasuryPaymentsCycle();
}

int64_t Legacy::GetLotteryReward(const CBlockRewards &rewards, const CChainParams& chainParameters)
{
    // 50 coins every block for lottery
    return rewards.nLotteryReward*chainParameters.GetLotteryBlockCycle();
}


SuperblockHeightValidator::SuperblockHeightValidator(
    const CChainParams& chainParameters
    ): chainParameters_(chainParameters)
    , transitionHeight_(chainParameters_.GetLotteryBlockCycle()*chainParameters_.GetTreasuryPaymentsCycle())
    , superblockCycleLength_((chainParameters_.GetLotteryBlockCycle()+chainParameters_.GetTreasuryPaymentsCycle())/2)
{
}

bool SuperblockHeightValidator::IsValidLotteryBlockHeight(int nBlockHeight) const
{
    if(nBlockHeight < transitionHeight_)
    {
        return Legacy::IsValidLotteryBlockHeight(nBlockHeight,chainParameters_);
    }
    else
    {
        return ((nBlockHeight - transitionHeight_) % superblockCycleLength_) == 0;
    }
}
bool SuperblockHeightValidator::IsValidTreasuryBlockHeight(int nBlockHeight) const
{
    if(nBlockHeight < transitionHeight_)
    {
        return Legacy::IsValidTreasuryBlockHeight(nBlockHeight,chainParameters_);
    }
    else
    {
        return IsValidLotteryBlockHeight(nBlockHeight-1);
    }
}

int SuperblockHeightValidator::getTransitionHeight() const
{
    return transitionHeight_;
}

const CChainParams& SuperblockHeightValidator::getChainParameters() const
{
    return chainParameters_;
}

int SuperblockHeightValidator::GetTreasuryBlockPaymentCycle(int nBlockHeight) const
{
    return (nBlockHeight < transitionHeight_)? chainParameters_.GetTreasuryPaymentsCycle():
        ((nBlockHeight <= transitionHeight_+1)? chainParameters_.GetTreasuryPaymentsCycle()+1: superblockCycleLength_);
}
int SuperblockHeightValidator::GetLotteryBlockPaymentCycle(int nBlockHeight) const
{
    return (nBlockHeight < transitionHeight_)? chainParameters_.GetLotteryBlockCycle(): superblockCycleLength_;
}

BlockSubsidyProvider::BlockSubsidyProvider(
    const CChainParams& chainParameters,
    I_SuperblockHeightValidator& heightValidator
    ): chainParameters_(chainParameters)
    , heightValidator_(heightValidator)
{

}

void BlockSubsidyProvider::updateTreasuryReward(int nHeight, CBlockRewards& rewards,bool isTreasuryBlock) const
{
    CAmount& treasuryReward = *const_cast<CAmount*>(&rewards.nTreasuryReward);
    CAmount& charityReward = *const_cast<CAmount*>(&rewards.nCharityReward);
    if(!isTreasuryBlock) 
    {
        treasuryReward = 0;
        charityReward =0;
    }
    int treasuryBlockCycleLength = heightValidator_.GetTreasuryBlockPaymentCycle(nHeight);
    int priorTreasuryBlockHeight = nHeight - treasuryBlockCycleLength;
    CBlockRewards priorRewards = isTreasuryBlock? Legacy::GetBlockSubsidity(priorTreasuryBlockHeight,chainParameters_): rewards;
    int numberOfSubsidyIntervals = nHeight/chainParameters_.SubsidyHalvingInterval(); // must be at least 2;
    int priorRewardWeight = numberOfSubsidyIntervals*chainParameters_.SubsidyHalvingInterval() - priorTreasuryBlockHeight;
    int currentRewardWeight =  nHeight - numberOfSubsidyIntervals*chainParameters_.SubsidyHalvingInterval();

    treasuryReward = priorRewards.nTreasuryReward * priorRewardWeight + rewards.nTreasuryReward* currentRewardWeight;
    charityReward = priorRewards.nCharityReward * priorRewardWeight + rewards.nCharityReward* currentRewardWeight;
}


void BlockSubsidyProvider::updateLotteryReward(int nHeight, CBlockRewards& rewards,bool isLotteryBlock) const
{
    CAmount& lotteryReward = *const_cast<CAmount*>(&rewards.nLotteryReward);
    if(!isLotteryBlock) 
    {
        lotteryReward = 0;
    }
    int lotteryBlockCycleLength = heightValidator_.GetLotteryBlockPaymentCycle(nHeight);
    int priorLotteryBlockHeight = nHeight - lotteryBlockCycleLength;
    CBlockRewards priorRewards = isLotteryBlock? Legacy::GetBlockSubsidity(priorLotteryBlockHeight,chainParameters_): rewards;
    int numberOfSubsidyIntervals = nHeight/chainParameters_.SubsidyHalvingInterval(); // must be at least 2;
    int priorRewardWeight = numberOfSubsidyIntervals*chainParameters_.SubsidyHalvingInterval() - priorLotteryBlockHeight;
    int currentRewardWeight =  nHeight - numberOfSubsidyIntervals*chainParameters_.SubsidyHalvingInterval();

    lotteryReward = priorRewards.nLotteryReward * priorRewardWeight + rewards.nLotteryReward* currentRewardWeight;
}


CBlockRewards BlockSubsidyProvider::GetBlockSubsidity(int nHeight) const
{
    CBlockRewards rewards = Legacy::GetBlockSubsidity(nHeight,chainParameters_);   
    updateTreasuryReward(nHeight,rewards, heightValidator_.IsValidTreasuryBlockHeight(nHeight));
    updateLotteryReward(nHeight,rewards, heightValidator_.IsValidLotteryBlockHeight(nHeight));
    return rewards;
}
CAmount BlockSubsidyProvider::GetFullBlockValue(int nHeight) const
{
    return Legacy::GetFullBlockValue(nHeight,chainParameters_);
}

SuperblockSubsidyProvider::SuperblockSubsidyProvider(
    const CChainParams& chainParameters, 
    I_SuperblockHeightValidator& heightValidator,
    I_BlockSubsidyProvider& blockSubsidyProvider
    ): chainParameters_(chainParameters)
    , heightValidator_(heightValidator)
    , blockSubsidyProvider_(blockSubsidyProvider)
{
}

CAmount SuperblockSubsidyProvider::GetTreasuryReward(int blockHeight) const
{
    CAmount totalReward = 0;
    if(blockHeight==0) return totalReward;
    if(heightValidator_.IsValidTreasuryBlockHeight(blockHeight))
    {
        return blockSubsidyProvider_.GetBlockSubsidity(blockHeight).nTreasuryReward;
    }
    return totalReward;
}

CAmount SuperblockSubsidyProvider::GetCharityReward(int blockHeight) const
{
    CAmount totalReward = 0;
    if(blockHeight==0) return totalReward;
    if(heightValidator_.IsValidTreasuryBlockHeight(blockHeight))
    {
        return blockSubsidyProvider_.GetBlockSubsidity(blockHeight).nCharityReward;
    }
    return totalReward;
}

CAmount SuperblockSubsidyProvider::GetLotteryReward(int blockHeight) const
{
    CAmount totalReward = 0;
    if(blockHeight==0) return totalReward;
    if(heightValidator_.IsValidLotteryBlockHeight(blockHeight))
    {
        return blockSubsidyProvider_.GetBlockSubsidity(blockHeight).nLotteryReward;
    }
    return totalReward;
}

SuperblockSubsidyContainer::SuperblockSubsidyContainer(
    const CChainParams& chainParameters
    ): chainParameters_(chainParameters)
    , heightValidator_(std::make_shared<SuperblockHeightValidator>(chainParameters_))
    , blockSubsidies_(std::make_shared<BlockSubsidyProvider>(chainParameters_,*heightValidator_))
    , superblockSubsidies_(chainParameters_,*heightValidator_,*blockSubsidies_)
{
}

const I_SuperblockHeightValidator& SuperblockSubsidyContainer::superblockHeightValidator() const
{
    return *heightValidator_;
}
const I_BlockSubsidyProvider& SuperblockSubsidyContainer::blockSubsidiesProvider() const
{
    return *blockSubsidies_;
}
const SuperblockSubsidyProvider& SuperblockSubsidyContainer::superblockSubsidiesProvider() const
{
    return superblockSubsidies_;
}

// Non-Legacy methods

bool IsValidLotteryBlockHeight(int nBlockHeight)
{
    SuperblockSubsidyContainer subsidiesContainer(Params());
    return subsidiesContainer.superblockHeightValidator().IsValidLotteryBlockHeight(nBlockHeight);
}

bool IsValidTreasuryBlockHeight(int nBlockHeight)
{
    SuperblockSubsidyContainer subsidiesContainer(Params());
    return subsidiesContainer.superblockHeightValidator().IsValidTreasuryBlockHeight(nBlockHeight);
}

CAmount GetTreasuryReward(int blockHeight)
{
    SuperblockSubsidyContainer subsidiesContainer(Params());
    return subsidiesContainer.superblockSubsidiesProvider().GetTreasuryReward(blockHeight);
}
CAmount GetCharityReward(int blockHeight)
{
    SuperblockSubsidyContainer subsidiesContainer(Params());
    return subsidiesContainer.superblockSubsidiesProvider().GetCharityReward(blockHeight);
}
CAmount GetLotteryReward(int blockHeight)
{
    SuperblockSubsidyContainer subsidiesContainer(Params());
    return subsidiesContainer.superblockSubsidiesProvider().GetLotteryReward(blockHeight);
}
CBlockRewards GetBlockSubsidity(int nHeight)
{
    SuperblockSubsidyContainer subsidiesContainer(Params());
    return subsidiesContainer.blockSubsidiesProvider().GetBlockSubsidity(nHeight);
}