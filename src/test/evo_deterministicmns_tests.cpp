// Copyright (c) 2018-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <test/util/setup_common.h>

#include <script/interpreter.h>
#include <script/standard.h>
#include <script/sign.h>
#include <validation.h>
#include <base58.h>
#include <netbase.h>
#include <messagesigner.h>
#include <policy/policy.h>
#include <script/signingprovider.h>
#include <spork.h>

#include <evo/specialtx.h>
#include <evo/providertx.h>
#include <evo/deterministicmns.h>
#include <boost/test/unit_test.hpp>
typedef std::vector<std::pair<COutPoint, std::pair<int, CAmount>> > SimpleUTXOVec;

static SimpleUTXOVec BuildSimpleUTXOVec(const std::vector<CTransactionRef>& txs)
{
    SimpleUTXOVec utxos;
    for (size_t i = 0; i < txs.size(); i++) {
        auto& tx = *txs[i];
        for (size_t j = 0; j < tx.vout.size(); j++) {
            if(tx.vout[j].nValue > 0)
                utxos.emplace_back(COutPoint(tx.GetHash(), j), std::make_pair((int)i + 1, tx.vout[j].nValue));
        }
    }
    return utxos;
}

static std::vector<COutPoint> SelectUTXOs(SimpleUTXOVec& utxos, CAmount amount, CAmount& changeRet)
{
    changeRet = 0;
    std::vector<COutPoint> selectedUtxos;
    CAmount selectedAmount = 0;
    auto it = utxos.begin();
    bool bFound = false;
    while (it != utxos.end()) {
        if (::ChainActive().Height() - it->second.first < 101) {
            it++;
            continue;
        }
        selectedAmount += it->second.second;
        selectedUtxos.emplace_back(it->first);
        it = utxos.erase(it);
        bFound = true;
        if (selectedAmount >= amount) {
            changeRet = selectedAmount - amount;
            break;
        }
    }
    BOOST_ASSERT(bFound);
    return selectedUtxos;
}

static void FundTransaction(CMutableTransaction& tx, SimpleUTXOVec& utoxs, const CScript& scriptPayout, CAmount amount)
{
    CAmount change;
    auto inputs = SelectUTXOs(utoxs, amount, change);
    for (size_t i = 0; i < inputs.size(); i++) {
        tx.vin.emplace_back(CTxIn(inputs[i]));
    }
    tx.vout.emplace_back(CTxOut(amount, scriptPayout));
    if (change != 0) {
        tx.vout.emplace_back(CTxOut(change, scriptPayout));
    }
}

static void SignTransaction(CMutableTransaction& tx, const CKey& coinbaseKey)
{
    LOCK(cs_main);
    FillableSigningProvider tempKeystore;
    tempKeystore.AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());
    std::map<COutPoint, Coin> coins;
    for (size_t i = 0; i < tx.vin.size(); i++) {
        Coin coin;
        GetUTXOCoin(tx.vin[i].prevout, coin);
        coins.emplace(tx.vin[i].prevout, coin);
    }
    std::map<int, std::string> input_errors;
    BOOST_CHECK(SignTransaction(tx, &tempKeystore, coins, SIGHASH_ALL, input_errors));
}

static CMutableTransaction CreateProRegTx(SimpleUTXOVec& utxos, int port, const CScript& scriptPayout, const CKey& coinbaseKey, CKey& ownerKeyRet, CBLSSecretKey& operatorKeyRet)
{
    ownerKeyRet.MakeNewKey(true);
    operatorKeyRet.MakeNewKey();

    CProRegTx proTx;
    proTx.collateralOutpoint.n = 0;
    proTx.addr = LookupNumeric("1.1.1.1", port);
    proTx.keyIDOwner = ownerKeyRet.GetPubKey().GetID();
    proTx.pubKeyOperator = operatorKeyRet.GetPublicKey();
    proTx.keyIDVoting = ownerKeyRet.GetPubKey().GetID();
    proTx.scriptPayout = scriptPayout;
    proTx.nOperatorReward = 5000;

    CMutableTransaction tx;
    tx.nVersion = SYSCOIN_TX_VERSION_MN_REGISTER;
    FundTransaction(tx, utxos, scriptPayout, 100 * COIN);
    proTx.inputsHash = CalcTxInputsHash(CTransaction(tx));
    SetTxPayload(tx, proTx);
    SignTransaction(tx, coinbaseKey);

    return tx;
}

static CMutableTransaction CreateProUpServTx(SimpleUTXOVec& utxos, const uint256& proTxHash, const CBLSSecretKey& operatorKey, int port, const CKey& coinbaseKey)
{
    CProUpServTx proTx;
    proTx.proTxHash = proTxHash;
    proTx.addr = LookupNumeric("1.1.1.1", port);
    proTx.scriptOperatorPayout = GetScriptForDestination(PKHash(coinbaseKey.GetPubKey()));
    CMutableTransaction tx;
    tx.nVersion = SYSCOIN_TX_VERSION_MN_UPDATE_SERVICE;
    FundTransaction(tx, utxos, GetScriptForDestination(PKHash(coinbaseKey.GetPubKey())), 1 * COIN);
    proTx.inputsHash = CalcTxInputsHash(CTransaction(tx));
    proTx.sig = operatorKey.Sign(::SerializeHash(proTx));
    SetTxPayload(tx, proTx);
    SignTransaction(tx, coinbaseKey);

    return tx;
}

static CMutableTransaction CreateProUpRegTx(SimpleUTXOVec& utxos, const uint256& proTxHash, const CKey& mnKey, const CBLSPublicKey& pubKeyOperator, const CKeyID& keyIDVoting, const CScript& scriptPayout, const CKey& coinbaseKey)
{

    CProUpRegTx proTx;
    proTx.proTxHash = proTxHash;
    proTx.pubKeyOperator = pubKeyOperator;
    proTx.keyIDVoting = keyIDVoting;
    proTx.scriptPayout = scriptPayout;

    CMutableTransaction tx;
    tx.nVersion = SYSCOIN_TX_VERSION_MN_UPDATE_REGISTRAR;
    FundTransaction(tx, utxos, GetScriptForDestination(PKHash(coinbaseKey.GetPubKey())), 1 * COIN);
    proTx.inputsHash = CalcTxInputsHash(CTransaction(tx));
    CHashSigner::SignHash(::SerializeHash(proTx), mnKey, proTx.vchSig);
    SetTxPayload(tx, proTx);
    SignTransaction(tx, coinbaseKey);

    return tx;
}

static CMutableTransaction CreateProUpRevTx(SimpleUTXOVec& utxos, const uint256& proTxHash, const CBLSSecretKey& operatorKey, const CKey& coinbaseKey)
{
    CProUpRevTx proTx;
    proTx.proTxHash = proTxHash;

    CMutableTransaction tx;
    tx.nVersion = SYSCOIN_TX_VERSION_MN_UPDATE_REVOKE;
    FundTransaction(tx, utxos, GetScriptForDestination(PKHash(coinbaseKey.GetPubKey())), 1 * COIN);
    proTx.inputsHash = CalcTxInputsHash(CTransaction(tx));
    proTx.sig = operatorKey.Sign(::SerializeHash(proTx));
    SetTxPayload(tx, proTx);
    SignTransaction(tx, coinbaseKey);

    return tx;
}

template<typename ProTx>
static CMutableTransaction MalleateProTxPayout(const CMutableTransaction& tx)
{
    ProTx proTx;
    GetTxPayload(tx, proTx);

    CKey key;
    key.MakeNewKey(true);
    proTx.scriptPayout = GetScriptForDestination(PKHash(key.GetPubKey()));

    CMutableTransaction tx2 = tx;
    SetTxPayload(tx2, proTx);

    return tx2;
}

static CScript GenerateRandomAddress()
{
    CKey key;
    key.MakeNewKey(true);
    return GetScriptForDestination(PKHash(key.GetPubKey()));
}

static CDeterministicMNCPtr FindPayoutDmn(const CBlock& block)
{
    CDeterministicMNList mnList;
    deterministicMNManager->GetListAtChainTip(mnList);

    for (const auto& txout : block.vtx[0]->vout) {
        CDeterministicMNCPtr found;
        mnList.ForEachMN(true, [&](const CDeterministicMNCPtr& dmn) {
            if (found == nullptr && txout.scriptPubKey == dmn->pdmnState->scriptPayout) {
                found = dmn;
            }
        });
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

static bool CheckTransactionSignature(const CMutableTransaction& tx)
{
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const auto& txin = tx.vin[i];
        Coin coin;
        GetUTXOCoin(txin.prevout, coin);
        if (!VerifyScript(txin.scriptSig, coin.out.scriptPubKey, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker(&tx, i, coin.out.nValue))) {
            return false;
        }
    }
    return true;
}

BOOST_AUTO_TEST_SUITE(evo_dip3_activation_tests)

BOOST_FIXTURE_TEST_CASE(dip3_activation, TestChainDIP3BeforeActivationSetup)
{
    auto utxos = BuildSimpleUTXOVec(m_coinbase_txns);
    CKey ownerKey;
    CBLSSecretKey operatorKey;
    CScript addr = GenerateRandomAddress();
    auto tx = CreateProRegTx(utxos, 1, addr, coinbaseKey, ownerKey, operatorKey);
    std::vector<CMutableTransaction> txns = std::vector<CMutableTransaction>{tx};

    int nHeight;
    {
        LOCK(cs_main);
        nHeight = ::ChainActive().Height();
    }

    // We start one block before DIP3 activation, so mining a block with a DIP3 transaction should be no-op
    auto block = std::make_shared<CBlock>(CreateAndProcessBlock(txns, GetScriptForRawPubKey(coinbaseKey.GetPubKey())));
    {
        LOCK(cs_main);
        BOOST_ASSERT(::ChainActive().Height() == nHeight + 1);
        BOOST_ASSERT(block->GetHash() == ::ChainActive().Tip()->GetBlockHash());
    }
    CDeterministicMNList mnList;
    deterministicMNManager->GetListAtChainTip(mnList);
    BOOST_ASSERT(!mnList.HasMN(tx.GetHash()));

    // re-create reg tx prev one got mined as no-op
    tx = CreateProRegTx(utxos, 1, addr, coinbaseKey, ownerKey, operatorKey);
    txns = std::vector<CMutableTransaction>{tx};
    // Mining a block with a DIP3 transaction should succeed now
    block = std::make_shared<CBlock>(CreateAndProcessBlock(txns, GetScriptForRawPubKey(coinbaseKey.GetPubKey())));
    {
        LOCK(cs_main);
        deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
        BOOST_ASSERT(::ChainActive().Height() == nHeight + 2);
        BOOST_ASSERT(block->GetHash() == ::ChainActive().Tip()->GetBlockHash());
    }
    deterministicMNManager->GetListAtChainTip(mnList);
    BOOST_ASSERT(mnList.HasMN(tx.GetHash()));
}

BOOST_FIXTURE_TEST_CASE(dip3_protx, TestChainDIP3Setup)
{
    CKey sporkKey;
    sporkKey.MakeNewKey(true);
    sporkManager.SetSporkAddress(EncodeDestination(PKHash(sporkKey.GetPubKey())));
    sporkManager.SetPrivKey(EncodeSecret(sporkKey));

    auto utxos = BuildSimpleUTXOVec(m_coinbase_txns);

    int nHeight;
    {
        LOCK(cs_main);
        nHeight = ::ChainActive().Height();
    }
    int port = 1;

    std::vector<uint256> dmnHashes;
    std::map<uint256, CKey> ownerKeys;
    std::map<uint256, CBLSSecretKey> operatorKeys;

    // register one MN per block
    for (size_t i = 0; i < 6; i++) {
        CKey ownerKey;
        CBLSSecretKey operatorKey;
        auto tx = CreateProRegTx(utxos, port++, GenerateRandomAddress(), coinbaseKey, ownerKey, operatorKey);
        dmnHashes.emplace_back(tx.GetHash());
        ownerKeys.emplace(tx.GetHash(), ownerKey);
        operatorKeys.emplace(tx.GetHash(), operatorKey);
        {
            LOCK(cs_main);
            // also verify that payloads are not malleable after they have been signed
            // the form of ProRegTx we use here is one with a collateral included, so there is no signature inside the
            // payload itself. This means, we need to rely on script verification, which takes the hash of the extra payload
            // into account
            auto tx2 = MalleateProTxPayout<CProRegTx>(tx);
            TxValidationState dummyState;
            // Technically, the payload is still valid...
            BOOST_ASSERT(CheckProRegTx(CTransaction(tx), ::ChainActive().Tip(), dummyState, false));
            BOOST_ASSERT(CheckProRegTx(CTransaction(tx2), ::ChainActive().Tip(), dummyState, false));
            // But the signature should not verify anymore
            BOOST_ASSERT(CheckTransactionSignature(tx));
            BOOST_ASSERT(!CheckTransactionSignature(tx2));
        }

        CreateAndProcessBlock({tx}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        {
            LOCK(cs_main);
            deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());

            BOOST_ASSERT(::ChainActive().Height() == nHeight + 1);
        }
        CDeterministicMNList mnList;
        deterministicMNManager->GetListAtChainTip(mnList);
        BOOST_ASSERT(mnList.HasMN(tx.GetHash()));

        nHeight++;
    }
    int DIP0003EnforcementHeightBackup;
    {
        LOCK(cs_main);
        DIP0003EnforcementHeightBackup = Params().GetConsensus().DIP0003EnforcementHeight;
        const_cast<Consensus::Params&>(Params().GetConsensus()).DIP0003EnforcementHeight = ::ChainActive().Height() + 1;
        CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
    }
    nHeight++;

    // check MN reward payments
    for (size_t i = 0; i < 20; i++) {
        CDeterministicMNList mnList;
        deterministicMNManager->GetListAtChainTip(mnList);
        auto dmnExpectedPayee = mnList.GetMNPayee();

        CBlock block = CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        {
            LOCK(cs_main);
            deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
        }
        BOOST_ASSERT(!block.vtx.empty());

        auto dmnPayout = FindPayoutDmn(block);
        BOOST_ASSERT(dmnPayout != nullptr);
        BOOST_CHECK_EQUAL(dmnPayout->proTxHash.ToString(), dmnExpectedPayee->proTxHash.ToString());

        nHeight++;
    }

    // register multiple MNs per block
    for (size_t i = 0; i < 3; i++) {
        std::vector<CMutableTransaction> txns;
        for (size_t j = 0; j < 3; j++) {
            CKey ownerKey;
            CBLSSecretKey operatorKey;
            auto tx = CreateProRegTx(utxos, port++, GenerateRandomAddress(), coinbaseKey, ownerKey, operatorKey);
            dmnHashes.emplace_back(tx.GetHash());
            ownerKeys.emplace(tx.GetHash(), ownerKey);
            operatorKeys.emplace(tx.GetHash(), operatorKey);
            txns.emplace_back(tx);
        }
        CreateAndProcessBlock(txns, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        {
            LOCK(cs_main);
            deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
            BOOST_ASSERT(::ChainActive().Height() == nHeight + 1);
        }

        for (size_t j = 0; j < 3; j++) {
            CDeterministicMNList mnList;
            deterministicMNManager->GetListAtChainTip(mnList);
            BOOST_ASSERT(mnList.HasMN(txns[j].GetHash()));
        }

        nHeight++;
    }

    // test ProUpServTx
    auto tx = CreateProUpServTx(utxos, dmnHashes[0], operatorKeys[dmnHashes[0]], 1000, coinbaseKey);
    CreateAndProcessBlock({tx}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    {
        LOCK(cs_main);
        deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
        BOOST_ASSERT(::ChainActive().Height() == nHeight + 1);
    }
    nHeight++;
    CDeterministicMNList mnList;
    deterministicMNManager->GetListAtChainTip(mnList);
    auto dmn = mnList.GetMN(dmnHashes[0]);
    BOOST_ASSERT(dmn != nullptr && dmn->pdmnState->addr.GetPort() == 1000);

    // test ProUpRevTx
    tx = CreateProUpRevTx(utxos, dmnHashes[0], operatorKeys[dmnHashes[0]], coinbaseKey);
    CreateAndProcessBlock({tx}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    {
        LOCK(cs_main);
        deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
        BOOST_ASSERT(::ChainActive().Height() == nHeight + 1);
    }
    nHeight++;
    deterministicMNManager->GetListAtChainTip(mnList);
    dmn = mnList.GetMN(dmnHashes[0]);
    BOOST_ASSERT(dmn != nullptr && dmn->pdmnState->GetBannedHeight() == nHeight);

    // test that the revoked MN does not get paid anymore
    for (size_t i = 0; i < 20; i++) {
        deterministicMNManager->GetListAtChainTip(mnList);
        auto dmnExpectedPayee = mnList.GetMNPayee();
        BOOST_ASSERT(dmnExpectedPayee->proTxHash != dmnHashes[0]);

        CBlock block = CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        {
            LOCK(cs_main);
            deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
        }
        BOOST_ASSERT(!block.vtx.empty());

        auto dmnPayout = FindPayoutDmn(block);
        BOOST_ASSERT(dmnPayout != nullptr);
        BOOST_CHECK_EQUAL(dmnPayout->proTxHash.ToString(), dmnExpectedPayee->proTxHash.ToString());

        nHeight++;
    }

    // test reviving the MN
    CBLSSecretKey newOperatorKey;
    newOperatorKey.MakeNewKey();
    deterministicMNManager->GetListAtChainTip(mnList);
    dmn = mnList.GetMN(dmnHashes[0]);
    tx = CreateProUpRegTx(utxos, dmnHashes[0], ownerKeys[dmnHashes[0]], newOperatorKey.GetPublicKey(), ownerKeys[dmnHashes[0]].GetPubKey().GetID(), dmn->pdmnState->scriptPayout, coinbaseKey);
    {
        LOCK(cs_main);
        // check malleability protection again, but this time by also relying on the signature inside the ProUpRegTx
        auto tx2 = MalleateProTxPayout<CProUpRegTx>(tx);
        TxValidationState dummyState;
        BOOST_ASSERT(CheckProUpRegTx(CTransaction(tx), ::ChainActive().Tip(), dummyState, false));
        BOOST_ASSERT(!CheckProUpRegTx(CTransaction(tx2), ::ChainActive().Tip(), dummyState, false));
        BOOST_ASSERT(CheckTransactionSignature(tx));
        BOOST_ASSERT(!CheckTransactionSignature(tx2));
    }
    // now process the block
    CreateAndProcessBlock({tx}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    {
        LOCK(cs_main);
        deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
        BOOST_ASSERT(::ChainActive().Height() == nHeight + 1);
    }
    nHeight++;

    tx = CreateProUpServTx(utxos, dmnHashes[0], newOperatorKey, 100, coinbaseKey);
    CreateAndProcessBlock({tx}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    {
        LOCK(cs_main);
        deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
        BOOST_ASSERT(::ChainActive().Height() == nHeight + 1);
    }
    nHeight++;
    deterministicMNManager->GetListAtChainTip(mnList);
    dmn = mnList.GetMN(dmnHashes[0]);
    BOOST_ASSERT(dmn != nullptr && dmn->pdmnState->addr.GetPort() == 100);
    BOOST_ASSERT(dmn != nullptr && !dmn->pdmnState->IsBanned());

    // test that the revived MN gets payments again
    bool foundRevived = false;
    for (size_t i = 0; i < 20; i++) {
        deterministicMNManager->GetListAtChainTip(mnList);
        auto dmnExpectedPayee = mnList.GetMNPayee();
        if (dmnExpectedPayee->proTxHash == dmnHashes[0]) {
            foundRevived = true;
        }

        CBlock block = CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        {
            LOCK(cs_main);
            deterministicMNManager->UpdatedBlockTip(::ChainActive().Tip());
        }
        BOOST_ASSERT(!block.vtx.empty());

        auto dmnPayout = FindPayoutDmn(block);
        BOOST_ASSERT(dmnPayout != nullptr);
        BOOST_CHECK_EQUAL(dmnPayout->proTxHash.ToString(), dmnExpectedPayee->proTxHash.ToString());

        nHeight++;
    }
    BOOST_ASSERT(foundRevived);

    const_cast<Consensus::Params&>(Params().GetConsensus()).DIP0003EnforcementHeight = DIP0003EnforcementHeightBackup;
}
BOOST_AUTO_TEST_SUITE_END()
