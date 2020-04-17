﻿// Copyright (c) 2017-2018 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_SERVICES_ASSETCONSENSUS_H
#define SYSCOIN_SERVICES_ASSETCONSENSUS_H


#include <primitives/transaction.h>
#include <services/asset.h>
class TxValidationState;
class CBlockIndexDB : public CDBWrapper {
public:
    CBlockIndexDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "blockindex", nCacheSize, fMemory, fWipe) {}
    
    bool ReadBlockHash(const uint256& txid, uint256& block_hash){
        return Read(txid, block_hash);
    } 
    bool FlushWrite(const std::vector<std::pair<uint256, uint256> > &blockIndex);
    bool FlushErase(const std::vector<uint256> &vecTXIDs);
};
class EthereumTxRoot {
    public:
    std::vector<unsigned char> vchBlockHash;
    std::vector<unsigned char> vchPrevHash;
    std::vector<unsigned char> vchTxRoot;
    std::vector<unsigned char> vchReceiptRoot;
    int64_t nTimestamp;
    
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {      
        READWRITE(vchBlockHash);
        READWRITE(vchPrevHash);
        READWRITE(vchTxRoot);
        READWRITE(vchReceiptRoot);
        READWRITE(nTimestamp);
    }
};
typedef std::unordered_map<uint32_t, EthereumTxRoot> EthereumTxRootMap;
class CEthereumTxRootsDB : public CDBWrapper {
public:
    CEthereumTxRootsDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "ethereumtxroots", nCacheSize, fMemory, fWipe) {
       Init();
    } 
    bool ReadTxRoots(const uint32_t& nHeight, EthereumTxRoot& txRoot) {
        return Read(nHeight, txRoot);
    } 
    void AuditTxRootDB(std::vector<std::pair<uint32_t, uint32_t> > &vecMissingBlockRanges);
    bool Init();
    bool Clear();
    bool PruneTxRoots(const uint32_t &fNewGethSyncHeight);
    bool FlushErase(const std::vector<uint32_t> &vecHeightKeys);
    bool FlushWrite(const EthereumTxRootMap &mapTxRoots);
};
typedef std::unordered_map<uint32_t, uint256> EthereumMintTxMap;
class CEthereumMintedTxDB : public CDBWrapper {
public:
    CEthereumMintedTxDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "ethereumminttx", nCacheSize, fMemory, fWipe) {
    } 
    bool FlushErase(const EthereumMintTxMap &mapMintKeys);
    bool FlushWrite(const EthereumMintTxMap &mapMintKeys);
};
extern std::unique_ptr<CBlockIndexDB> pblockindexdb;
extern std::unique_ptr<CEthereumTxRootsDB> pethereumtxrootsdb;
extern std::unique_ptr<CEthereumMintedTxDB> pethereumtxmintdb;
bool DisconnectAssetActivate(const CTransaction &tx, const uint256& txHash, AssetMap &mapAssets);
bool DisconnectAssetSend(const CTransaction &tx, const uint256& txHash, AssetMap &mapAssets);
bool DisconnectAssetUpdate(const CTransaction &tx, const uint256& txHash, AssetMap &mapAssets);
bool DisconnectAssetTransfer(const CTransaction &tx, const uint256& txHash, AssetMap &mapAssets);
bool DisconnectMintAsset(const CTransaction &tx, const uint256& txHash, EthereumMintTxMap &mapMintKeys);
bool DisconnectSyscoinTransaction(const CTransaction& tx, const uint256& txHash, CCoinsViewCache& view, AssetMap &mapAssets, EthereumMintTxMap &mapMintKeys);
bool CheckSyscoinMint(const bool &ibd, const CTransaction& tx, const uint256& txHash, TxValidationState &tstate, const bool &fJustCheck, const bool& bSanityCheck, const int& nHeight, const int64_t& nTime, const uint256& blockhash, EthereumMintTxMap &mapMintKeys);
bool CheckAssetInputs(const CTransaction &tx, const CAssetAllocation &theAssetAllocation, const uint256& txHash, TxValidationState &tstate, const bool &fJustCheck, const int &nHeight, const uint256& blockhash, AssetMap &mapAssets, const bool &bSanityCheck=false);
bool CheckSyscoinInputs(const CTransaction& tx, const CAssetAllocation &theAssetAllocation, const uint256& txHash, TxValidationState &tstate, const int &nHeight, const int64_t& nTime, EthereumMintTxMap &mapMintKeys);
bool CheckSyscoinInputs(const bool &ibd, const CTransaction& tx, const CAssetAllocation &theAssetAllocation, const uint256& txHash, TxValidationState &tstate, const bool &fJustCheck, const int &nHeight, const int64_t& nTime, const uint256 & blockHash, const bool &bSanityCheck, AssetMap &mapAssets, EthereumMintTxMap &mapMintKeys);
bool CheckAssetAllocationInputs(const CTransaction &tx, const CAssetAllocation &theAssetAllocation, const uint256& txHash, TxValidationState &tstate, const bool &fJustCheck, const int &nHeight, const uint256& blockhash, const bool &bSanityCheck = false);
bool FormatSyscoinErrorMessage(TxValidationState &state, const std::string errorMessage, bool bErrorNotInvalid = true, bool bConsensus = true);
#endif // SYSCOIN_SERVICES_ASSETCONSENSUS_H
