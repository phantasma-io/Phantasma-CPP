#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <vector>

namespace phantasma {

struct DomainSettings
{
	static constexpr int LatestKnownProtocol = 19;
	static constexpr int Phantasma20Protocol = 7;
	static constexpr int Phantasma30Protocol = 8;

	static constexpr int MaxTxPerBlock = 1024;
	static constexpr int MaxOracleEntriesPerBlock = 5120;
	static constexpr int MaxEventsPerBlock = 2048;
	static constexpr int MaxEventsPerTx = 8096;
	static constexpr int MaxTriggerLoop = 5;
	static constexpr int MAX_TOKEN_DECIMALS = 18;

	static constexpr int DefaultMinimumGasFee = 100000;
	static constexpr int InitialValidatorCount = 4;

	static constexpr const char* FuelTokenSymbol = "KCAL";
	static constexpr const char* FuelTokenName = "Phantasma Energy";
	static constexpr int FuelTokenDecimals = 10;

	static constexpr const char* NexusMainnet = "mainnet";
	static constexpr const char* NexusTestnet = "testnet";

	static constexpr const char* StakingTokenSymbol = "SOUL";
	static constexpr const char* StakingTokenName = "Phantasma Stake";
	static constexpr int StakingTokenDecimals = 8;

	static constexpr const char* FiatTokenSymbol = "USD";
	static constexpr const char* FiatTokenName = "Dollars";
	static constexpr int FiatTokenDecimals = 8;

	static constexpr const char* RewardTokenSymbol = "CROWN";
	static constexpr const char* RewardTokenName = "Phantasma Crown";

	static constexpr const char* LiquidityTokenSymbol = "LP";
	static constexpr const char* LiquidityTokenName = "Phantasma Liquidity";
	static constexpr int LiquidityTokenDecimals = 8;

	static constexpr const char* FuelPerContractDeployTag = "nexus.contract.cost";
	static constexpr const char* FuelPerTokenDeployTag = "nexus.token.cost";
	static constexpr const char* FuelPerOrganizationDeployTag = "nexus.organization.cost";

	static const PHANTASMA_VECTOR<String>& SystemTokens()
	{
		static const PHANTASMA_VECTOR<String> tokens = {
			FuelTokenSymbol,
			StakingTokenSymbol,
			FiatTokenSymbol,
			RewardTokenSymbol,
			LiquidityTokenSymbol
		};
		return tokens;
	}

	static constexpr const char* RootChainName = "main";

	static constexpr const char* ValidatorsOrganizationName = "validators";
	static constexpr const char* MastersOrganizationName = "masters";
	static constexpr const char* StakersOrganizationName = "stakers";
	static constexpr const char* PhantomForceOrganizationName = "phantom_force";

	static constexpr const char* PlatformName = "phantasma";

	static constexpr int ArchiveMinSize = 64;
	static constexpr int ArchiveMaxSize = 104857600;

	static constexpr const char* InfusionName = "infusion";

	static constexpr int NameMaxLength = 255;
	static constexpr int UrlMaxLength = 2048;
	static constexpr int ArgsMax = 64;
	static constexpr int AddressMaxSize = 34;
	static constexpr int ScriptMaxSize = 32767;
	static constexpr int FieldMaxLength = 80;
	static constexpr int FieldMinLength = 1;
};

} // namespace phantasma
