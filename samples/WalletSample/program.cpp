//__pragma(optimize("", off))
#include <cstdint>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>

#define PHANTASMA_IMPLEMENTATION

#if defined(_WIN32)
#define SODIUM_STATIC
#include "../../include/Adapters/PhantasmaAPI_cpprest.h"
#include "../../include/PhantasmaAPI.h"
#include "../../include/Adapters/PhantasmaAPI_sodium.h"
#pragma comment(lib, "libsodium.lib")
#else
#define PHANTASMA_CHAR char
#define PHANTASMA_STRING std::string
#include "../../include/Adapters/PhantasmaAPI_rapidjson.h"
#include "../../include/Adapters/PhantasmaAPI_curl.h"
#include "../../include/PhantasmaAPI.h"
#include "../../include/Adapters/PhantasmaAPI_openssl.h"
#endif

#include "../../include/Carbon/Carbon.h"
#include "../../include/Carbon/DataBlockchain.h"
#include "../../include/Carbon/Tx.h"
#include "../../include/Blockchain/Transaction.h"
#include "../../include/Domain/Event.h"
#include "../../include/Cryptography/KeyPair.h"
#include "../../include/VM/ScriptBuilder.h"
#include "../../include/Utils/RpcUtils.h"


using namespace std;
using namespace phantasma;
namespace carbon = phantasma::carbon;
namespace cbc = phantasma::carbon::Blockchain;

#if defined(_WIN32)
using ConsoleOut = std::wostream;
using ConsoleIn = std::wistream;
inline ConsoleOut& Cout() { return std::wcout; }
inline ConsoleIn& Cin() { return std::wcin; }
#else
using ConsoleOut = std::ostream;
using ConsoleIn = std::istream;
inline ConsoleOut& Cout() { return std::cout; }
inline ConsoleIn& Cin() { return std::cin; }
#endif

void Write(int i)
{
	Cout() << i;
}

void Write(const char* text)
{
#if defined(_WIN32)
	Write(FromUTF8(text));
#else
	Cout() << text;
#endif
}

void Write(const wchar_t* text)
{
#if defined(_WIN32)
	Cout() << text;
#else
	const std::wstring wide{text};
	Cout() << std::string(wide.begin(), wide.end());
#endif
}

void Write(const String& text)
{
	Cout() << text;
}

void WriteLine()
{
	Cout() << std::endl;
}

template<class T>
void WriteLine(const T& t)
{
	Write(t);
	WriteLine();
}

template<class T, class... Args>
void WriteLine(T a, Args... b)
{
	Write(a);
	WriteLine(b...);
}

String ReadLine()
{
	String str;
	std::getline(Cin(), str);
	if (!Cin())
	{
		Cin().clear();
	}
	return str;
}

static int GetTokenDecimals(const String& tokenSymbol, const vector<rpc::Token>& tokens)
{
	for(const auto& token : tokens)
		if( token.symbol == tokenSymbol )
			return token.decimals;
	return 0;
}
	
static String GetChainName(const String& chainAddress, const vector<rpc::Chain>& chains)
{
	for(const auto& chain : chains)
		if( chain.address == chainAddress )
			return chain.name;
	return {};
}

class Program
{
	String _nexus;
	String _host;
	HttpClient _http;
	rpc::PhantasmaAPI _phantasmaApiService;

	rpc::Account _account;
	PhantasmaKeys _key;
	vector<rpc::Chain> _chains;
	vector<rpc::Token> _tokens;
public:
	Program(const String& nexus, const String& host)
		: _nexus(nexus)
		, _host(host)
#if defined(_WIN32)
		, _http(_host.c_str())
#else
		, _http(_host)
#endif
		, _phantasmaApiService(_http)
	{
		WriteLine("Welcome to Phantasma Wallet sample.");
		WriteLine("Initializing...");
		WriteLine("Fetching data...");

		_chains = _phantasmaApiService.GetChains();
		_tokens = _phantasmaApiService.GetTokens(true);

		WriteLine("Enter your WIF:");

		bool loggedIn = false;

		while (!loggedIn)
		{
			try
			{
				SecureString wif = SecureString{ReadLine()};
				_key = PhantasmaKeys::FromWIF(wif); //KeyPair.Generate();
				loggedIn = true;
			}
			catch (std::exception&)
			{
				WriteLine("Incorrect wif, enter again:");
			}
		}
	}

	void RunConsole()
	{
		bool logout = false;
		while (!logout)
		{
			WriteLine();
			WriteLine();
			WriteLine("MENU");
			WriteLine("1 - show address");
			WriteLine("2 - show balance and account name");
			WriteLine("3 - show detailed balance");
			WriteLine("4 - register name");
			WriteLine("5 - send tokens");
			WriteLine("6 - send tokens (Carbon)");
			WriteLine("7 - list last 10 transactions");
			WriteLine("8 - logout");
			String strOption = ReadLine();
			int option = std::stoi(strOption);
			WriteLine();
			switch (option)
			{
			case 1:
				WriteLine(_key.GetAddress().ToString());
				break;
			case 2:
				ShowBalance();
				break;
			case 3:
				ShowBalance(true);
				break;
			case 4:
				RegisterName();
				break;
			case 5:
				CrossChainTransfer();
				break;
			case 6:
				CarbonTransfer();
				break;
			case 7:
				ListTransactions();
				break;
			case 8:
				logout = true;
				_account = rpc::Account();
				break;
			}

			WriteLine();
		}
	}

	void ListTransactions()
	{
		auto txs = _phantasmaApiService.GetAddressTransactions(_key.GetAddress().ToString().c_str(), 1, 10);
		for(const auto& tx : txs.txs)
		{
			WriteLine(GetTxDescription(tx, _chains, _tokens).c_str());
		}
	}

	void ShowBalance(bool detailed = false)
	{
		_account = _phantasmaApiService.GetAccount(_key.GetAddress().ToString().c_str());
		const auto& name = _account.name;
		WriteLine();
		WriteLine("Address Name: ", name.c_str());
		WriteLine();
		if (_account.balances.empty())
		{
			WriteLine("No funds");
		}
		else
		{
			if (_tokens.empty()) _tokens = _phantasmaApiService.GetTokens(true);
			for(const auto& balanceSheet : _account.balances)
			{
				WriteLine("********************");
				WriteLine("Token: ", balanceSheet.symbol.c_str());
				WriteLine("Chain: ", balanceSheet.chain.c_str());
				WriteLine("Amount: ", DecimalConversion(BigInteger::Parse(balanceSheet.amount), balanceSheet.decimals).c_str());
				if (detailed)
				{
					const rpc::Token* tokenInfo = nullptr;
					for (const auto& tk : _tokens)
					{
						if (tk.symbol == balanceSheet.symbol)
						{
							tokenInfo = &tk;
							break;
						}
					}
					if (!tokenInfo)
					{
						try
						{
							_tokens.push_back(_phantasmaApiService.GetToken(balanceSheet.symbol.c_str(), true));
							tokenInfo = &_tokens.back();
						}
						catch (const std::exception& ex)
						{
							WriteLine("Warn: failed to fetch token info: ", ex.what());
						}
					}
					if (tokenInfo)
					{
						if (!tokenInfo->carbonId.empty())
						{
							WriteLine("Carbon Id: ", tokenInfo->carbonId.c_str());
						}
						if (!tokenInfo->burnedSupply.empty())
						{
							WriteLine("Burned: ", DecimalConversion(BigInteger::Parse(tokenInfo->burnedSupply), tokenInfo->decimals).c_str());
						}
						if (!tokenInfo->metadata.empty())
						{
							WriteLine("Metadata:");
							for (const auto& prop : tokenInfo->metadata)
							{
								WriteLine("\t", prop.Key.c_str(), ": ", prop.Value.c_str());
							}
						}
						if (!tokenInfo->series.empty())
						{
							WriteLine("Series:");
							for (const auto& series : tokenInfo->series)
							{
								WriteLine("\tID: ", series.seriesID, " carbonTokenId: ", series.carbonTokenId.c_str(), " carbonSeriesId: ", series.carbonSeriesId.c_str());
								if (!series.burnedSupply.empty())
								{
									WriteLine("\tBurned: ", series.burnedSupply.c_str());
								}
								if (!series.metadata.empty())
								{
									for (const auto& prop : series.metadata)
									{
										WriteLine("\t\t", prop.Key.c_str(), ": ", prop.Value.c_str());
									}
								}
							}
						}
					}
				}

				for( const auto& id : balanceSheet.ids )
				{
					const auto& tokenData = _phantasmaApiService.GetTokenData(balanceSheet.symbol.c_str(), id.c_str());
					WriteLine("\tID: ", tokenData.ID.c_str(), " - RAM: ", tokenData.ram.c_str(), " ROM: ", tokenData.rom.c_str());
				}
				WriteLine();
			}
		}
	}

	bool HaveTokenBalanceToTransfer()
	{
		for(const auto& token : _account.balances)
			if( BigInteger::Parse(token.amount) > BigInteger::Zero() )
				return true;
		return false;
	}

	bool TryGetCarbonTokenId(const String& symbol, uint64_t& carbonId)
	{
		rpc::PhantasmaError err{};
		try
		{
			const auto tokenInfo = _phantasmaApiService.GetToken(symbol.c_str(), true, &err);
			if (err.code != 0)
			{
				WriteLine("RPC error: ", err.message.c_str());
				return false;
			}

			if (tokenInfo.carbonId.empty())
			{
				WriteLine("RPC error: missing carbon id in token info");
				return false;
			}

			carbonId = std::stoull(tokenInfo.carbonId);
			return true;
		}
		catch (const std::invalid_argument&)
		{
			WriteLine("RPC error: invalid carbon id format");
		}
		catch (const std::exception& ex)
		{
			WriteLine("RPC error: ", ex.what());
		}
		return false;
	}

	bool TrySendCarbonTransaction(const cbc::TxMsg& msg, const PhantasmaKeys& keys, String& txHash)
	{
		rpc::PhantasmaError err{};
		try
		{
			txHash = SignAndSendCarbonTransaction(_phantasmaApiService, msg, keys, &err);
		}
		catch (const std::exception& ex)
		{
			WriteLine("RPC error: ", ex.what());
			return false;
		}
		if (err.code != 0)
		{
			WriteLine("RPC error: ", err.message.c_str());
			return false;
		}

		return !txHash.empty();
	}

	void CrossChainTransfer()
	{
		if (_account.address.empty())
			_account = _phantasmaApiService.GetAccount(_key.GetAddress().ToString().c_str());
		if (!HaveTokenBalanceToTransfer())
		{
			WriteLine("No tokens to transfer");
			return;
		}

		WriteLine("Select token and chain: ");

		for (size_t i = 0; i < _account.balances.size(); ++i)
		{
			WriteLine(i + 1, " - ", _account.balances[i].symbol, " in ", _account.balances[i].chain, " chain");
		}

		const int selectedTokenOption = std::stoi(ReadLine());
		if (selectedTokenOption < 1 || static_cast<size_t>(selectedTokenOption) > _account.balances.size())
		{
			WriteLine("Invalid selection");
			return;
		}
		const size_t selectedTokenIndex = static_cast<size_t>(selectedTokenOption - 1);
		const auto& token = _account.balances[selectedTokenIndex];

		// WriteLine("Select destination chain:");

		// for (int i = 0; i < _chains.size(); i++)
		// {
		//	WriteLine(i + 1, " - ", _chains[i].name);
		// }

		// int selectedChainOption = std::stoi(ReadLine());
		// if( selectedChainOption < 1 || selectedChainOption > _chains.size() )
		// {
		// 	WriteLine("Invalid selection");
		//	return;
		// }
		// const auto& destinationChain = _chains[selectedChainOption - 1];


		WriteLine("Enter amount: (max ", DecimalConversion(BigInteger::Parse(token.amount), token.decimals), ")");
		String amount = ReadLine();

		WriteLine("Enter destination address: ");
		String destinationAddress = ReadLine();

		if (!Address::IsValidAddress(destinationAddress))
		{
			WriteLine("Invalid address");
			return;
		}

		SameChainTransfer(destinationAddress, amount, token.symbol, "main");
	}

	void CarbonTransfer()
	{
		if (_account.address.empty())
		{
			_account = _phantasmaApiService.GetAccount(_key.GetAddress().ToString().c_str());
		}
		if (!HaveTokenBalanceToTransfer())
		{
			WriteLine("No tokens to transfer");
			return;
		}

		WriteLine("Select token for Carbon transfer: ");
		for (size_t i = 0; i < _account.balances.size(); ++i)
		{
			WriteLine(i + 1, " - ", _account.balances[i].symbol, " in ", _account.balances[i].chain, " chain");
		}

		const int selectedTokenOption = std::stoi(ReadLine());
		if (selectedTokenOption < 1 || static_cast<size_t>(selectedTokenOption) > _account.balances.size())
		{
			WriteLine("Invalid selection");
			return;
		}
		const size_t selectedTokenIndex = static_cast<size_t>(selectedTokenOption - 1);
		const auto& token = _account.balances[selectedTokenIndex];

		uint64_t carbonTokenId = 0;
		if (!TryGetCarbonTokenId(token.symbol, carbonTokenId))
		{
			WriteLine("Unable to resolve Carbon token id for ", token.symbol.c_str());
			return;
		}

		if (_tokens.empty())
		{
			_tokens = _phantasmaApiService.GetTokens(false);
		}
		const int decimals = GetTokenDecimals(token.symbol, _tokens);
		WriteLine("Enter amount: (max ", DecimalConversion(BigInteger::Parse(token.amount), token.decimals), ")");
		const String amountInput = ReadLine();

		BigInteger bigAmount;
		try
		{
			bigAmount = DecimalConversion(amountInput, decimals);
		}
		catch (const std::exception& ex)
		{
			WriteLine("Invalid amount: ", ex.what());
			return;
		}

		if (bigAmount < BigInteger::Zero())
		{
			WriteLine("Amount must be positive");
			return;
		}

		uint64_t amount = 0;
		try
		{
			amount = std::stoull(bigAmount.ToString());
		}
		catch (const std::exception&)
		{
			WriteLine("Amount is too large for Carbon transaction");
			return;
		}

		WriteLine("Enter destination address: ");
		const String destinationAddress = ReadLine();

		if (!Address::IsValidAddress(destinationAddress))
		{
			WriteLine("Invalid address");
			return;
		}

		const Address destination = Address::FromText(destinationAddress);

		const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		cbc::TxMsg msg;
		msg.type = cbc::TxTypes::TransferFungible;
		msg.expiry = static_cast<int64_t>(nowMs + 60LL * 1000LL);
		msg.maxGas = carbon::FeeOptions().CalculateMaxGas();
		msg.maxData = 0;
		msg.gasFrom = carbon::Bytes32(_key.GetPublicKey());
		msg.payload = carbon::SmallString();
		msg.transferFt = cbc::TxMsgTransferFungible{
			carbon::Bytes32(destination.ToByteArray() + 2, 32),
			carbonTokenId,
			amount
		};

		String txHash;
		if (TrySendCarbonTransaction(msg, _key, txHash))
		{
			WriteLine("Carbon transaction sent. Tx hash: ", txHash.c_str());
		}
		else
		{
			WriteLine("Failed to send Carbon transaction");
		}
	}

	void SameChainTransfer(const String& addressTo, const String& amount, const String& tokenSymbol, const String& chain)
	{
		Address destinationAddress = Address::FromText(addressTo);

		int decimals = GetTokenDecimals(tokenSymbol, _tokens);
		auto bigIntAmount = DecimalConversion(amount, decimals);

		const auto& script = ScriptBuilder::BeginScript()
			.AllowGas(_key.GetAddress(), Address(), 100000, 9999)
			.TransferTokens(tokenSymbol, _key.GetAddress(), destinationAddress, bigIntAmount)
			.SpendGas(_key.GetAddress())
			.EndScript();

		SignAndSendTx(script, chain);
	}

	String SettleBlock(const String& sourceChainAddress, const String& blockHash, const String& destinationChainAddress)
	{
		Address sourceChain = Address::FromText(sourceChainAddress);
		String destinationChainName = GetChainName(destinationChainAddress, _chains);

		Hash block = Hash::Parse(blockHash);

		auto settleTxScript = ScriptBuilder::BeginScript()
			.CallContract(PHANTASMA_LITERAL("token"), PHANTASMA_LITERAL("SettleBlock"), sourceChain, block)
			.AllowGas(_key.GetAddress(), Address(), 1, 9999)
			.SpendGas(_key.GetAddress())
			.EndScript();
		return SignAndSendTx(settleTxScript, destinationChainName);
	}

/*	String CrossChainTransferToken(const String& addressTo, const String& chainName, const String& destinationChain, const String& symbol, const String& amount)
	{
		String toChain = GetChainAddress(destinationChain, _chains);
		Address destinationAddress = Address::FromText(addressTo);
		int decimals = GetTokenDecimals(symbol, _tokens);
		auto bigIntAmount = DecimalConversion(amount, decimals);
		auto fee = DecimalConversion(U("0.0001"), 8);

		auto script = ScriptBuilder::BeginScript()
			.AllowGas(_key.Address(), Address(), 1, 9999)
			.CrossTransferToken(Address::FromText(toChain), symbol, _key.Address(),
				_key.Address(), fee)
			.CrossTransferToken(Address::FromText(toChain), symbol, _key.Address(),
				destinationAddress, bigIntAmount)
			.SpendGas(_key.Address())
			.EndScript();

		return SignAndSendTx(script, chainName);
	}*/

	void RegisterName()
	{
		if (!HaveTokenBalanceToTransfer())
		{
			WriteLine("Insuficient funds");
			return;
		}
		WriteLine("Enter name for address: ");
		String name = ReadLine();
		auto script = ScriptBuilder::BeginScript()
			.AllowGas(_key.GetAddress(), Address(), 100000, 9999)
			.CallContract(PHANTASMA_LITERAL("account"), PHANTASMA_LITERAL("RegisterName"), _key.GetAddress(), name)
			.SpendGas(_key.GetAddress())
			.EndScript();

		SignAndSendTx(script, PHANTASMA_LITERAL("main"));
	}

	String SignAndSendTx(const PHANTASMA_VECTOR<Byte>& script, const String& chain)
	{
		try
		{
			WriteLine("Sending transaction...");
			ByteArray payload;
			String txResult = SignAndSendTransaction(_phantasmaApiService, _key, _nexus.c_str(), chain.c_str(), script, payload);

			WriteLine("Transaction sent. Tx hash: ", txResult);
			return txResult;
		}
		catch (std::exception& ex)
		{
			WriteLine("Something happened. Error: ", ex.what());
			return {};
		}
	}
};

int main()
{
#if defined(_WIN32)
	if (sodium_init() == -1)
	{
		WriteLine("Could not initialize sodium library");
		return 1;
	}
#endif

	for(;;)
	{
		try
		{
			WriteLine("Please select your network");
			WriteLine("1 - local simnet");
			WriteLine("2 - testnet");
			WriteLine("3 - mainnet");
			String strOption = ReadLine();
			int option = std::stoi(strOption);
			WriteLine();

			String nexus;
			String host;
			switch (option)
			{
			case 1:
				nexus = PHANTASMA_LITERAL("simnet");
				host = PHANTASMA_LITERAL("http://localhost:5172/");
				break;
			case 2:
				nexus = PHANTASMA_LITERAL("testnet");
				host = PHANTASMA_LITERAL("https://testnet.phantasma.info");
				break;
			case 3:
				nexus = PHANTASMA_LITERAL("mainnet");
				host = PHANTASMA_LITERAL("https://pharpc1.phantasma.info");
				break;
			}

			WriteLine(String(PHANTASMA_LITERAL("Selected ")) + nexus + PHANTASMA_LITERAL(" nexus with ") + host + PHANTASMA_LITERAL(" RPC"));

			Program program(nexus, host);
			program.RunConsole();
			return 0;
		}
		catch (std::exception& ex)
		{
			WriteLine("An error occured: ", ex.what());
		}
	}
	return 1;
}
