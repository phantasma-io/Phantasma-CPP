#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include "DomainSettings.h"

namespace phantasma {

struct ValidationUtils
{
	static constexpr const char* ANONYMOUS_NAME = "anonymous";
	static constexpr const char* GENESIS_NAME = "genesis";
	static constexpr const char* ENTRY_CONTEXT_NAME = "entry";
	static constexpr const char* NULL_NAME = "null";

	static const PHANTASMA_VECTOR<String>& PrefixNames()
	{
		// Keep the reserved prefix list in sync with C#/TS ValidationUtils.
		static const PHANTASMA_VECTOR<String> names = {
			"phantasma", "neo", "ethereum", "bitcoin", "litecoin", "eos",
			"decentraland", "elastos", "loopring", "grin", "nuls",
			"bancor", "ark", "nos", "bluzelle", "satoshi", "gwei", "nacho",
			"oracle", "oracles", "dex", "exchange", "wallet", "account",
			"airdrop", "giveaway", "free", "mail", "dapp", "charity", "address", "system",
			"coin", "token", "nexus", "deposit", "phantom", "cityofzion", "coz",
			"huobi", "binance", "kraken", "kucoin", "coinbase", "switcheo", "bittrex", "bitstamp",
			"bithumb", "okex", "hotbit", "bitmart", "bilaxy", "vitalik", "nakamoto",
		};
		return names;
	}

	static const PHANTASMA_VECTOR<String>& ReservedNames()
	{
		// Keep the reserved names list in sync with C#/TS ValidationUtils.
		static const PHANTASMA_VECTOR<String> names = {
			"ripple", "tether", "tron", "chainchanged", "libra", "loom", "enigma", "wax",
			"monero", "dash", "tezos", "cosmos", "maker", "ontology", "dogecoin", "zcash", "vechain",
			"qtum", "omise", "holo", "nano", "augur", "waves", "icon", "dai", "bitshares",
			"siacoin", "komodo", "zilliqa", "steem", "enjin", "aelf", "nash", "stratis",
			"windows", "osx", "ios", "android", "google", "yahoo", "facebook", "alibaba", "ebay",
			"apple", "amazon", "microsoft", "samsung", "verizon", "walmart", "ibm", "disney",
			"netflix", "alibaba", "tencent", "baidu", "visa", "mastercard", "instagram", "paypal",
			"adobe", "huawei", "vodafone", "dell", "uber", "youtube", "whatsapp", "snapchat", "pinterest",
			"gamecenter", "pixgamecenter", "seal", "crosschain", "blacat",
			"bitladon", "bitcoinmeester", "ico", "ieo", "sto", "kyc",
		};
		return names;
	}

	static bool IsReservedIdentifier(const String& name)
	{
		if (name == DomainSettings::InfusionName)
		{
			return true;
		}

		if (name == NULL_NAME)
		{
			return true;
		}

		for (const auto& prefix : PrefixNames())
		{
			if (name.size() >= prefix.size() && name.compare(0, prefix.size(), prefix) == 0)
			{
				return true;
			}
		}

		for (const auto& reserved : ReservedNames())
		{
			if (name == reserved)
			{
				return true;
			}
		}

		return false;
	}

	static bool IsValidIdentifier(const String& name)
	{
		if (name.empty())
		{
			return false;
		}

		if (name.size() < 3 || name.size() > 15)
		{
			return false;
		}

		if (name == ANONYMOUS_NAME || name == GENESIS_NAME || name == ENTRY_CONTEXT_NAME)
		{
			return false;
		}

		for (size_t i = 0; i < name.size(); ++i)
		{
			const char c = name[i];
			if (c >= 'a' && c <= 'z') continue;
			if (c == '_') continue;
			if (i > 0 && c >= '0' && c <= '9') continue;
			return false;
		}

		return true;
	}

	static bool IsValidTicker(const String& name)
	{
		if (name.empty())
		{
			return false;
		}

		if (name.size() < 2 || name.size() > 5)
		{
			return false;
		}

		for (size_t i = 0; i < name.size(); ++i)
		{
			const char c = name[i];
			if (c >= 'A' && c <= 'Z') continue;
			return false;
		}

		return true;
	}
};

} // namespace phantasma
