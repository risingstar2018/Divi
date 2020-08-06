#include <script/StakingVaultScript.h>

#include <vector>
#include <script/script.h>

typedef std::vector<unsigned char> valtype;

#define STAKING_VAULT(ownerKeyHash,vaultKeyHash) \
    CScript() << OP_IF << ownerKeyHash \
    << OP_ELSE << OP_NOP10 << vaultKeyHash << OP_ENDIF \
    <<  OP_OVER << OP_HASH160 << OP_EQUALVERIFY << OP_CHECKSIG

CScript CreateStakingVaultScript(const valtype& ownerKeyHash, const valtype& vaultKeyHash)
{
    return STAKING_VAULT(ownerKeyHash,vaultKeyHash);
}
CScript GetStakingVaultScriptTemplate()
{
    return STAKING_VAULT(OP_PUBKEYHASH,OP_PUBKEYHASH);
}
bool GetStakingVaultPubkeyHashes(const CScript& scriptPubKey, std::pair<valtype,valtype>& pubkeyHashes)
{
    CScript copyScript = scriptPubKey;

    copyScript.erase(copyScript.begin()+24,copyScript.begin()+45);
    copyScript.insert(copyScript.begin()+24, OP_PUBKEYHASH);
    copyScript.erase(copyScript.begin()+1, copyScript.begin()+22);
    copyScript.insert(copyScript.begin()+1, OP_PUBKEYHASH);
    if(copyScript == GetStakingVaultScriptTemplate())
    {
        pubkeyHashes.first.resize(20);
        pubkeyHashes.first.assign(scriptPubKey.begin()+2,scriptPubKey.begin()+22);
        pubkeyHashes.second.resize(20);
        pubkeyHashes.second.assign(scriptPubKey.begin()+25,scriptPubKey.begin()+45);
        return true;
    }
    else
    {
        pubkeyHashes = std::pair<valtype,valtype>();
        return false;
    }
}