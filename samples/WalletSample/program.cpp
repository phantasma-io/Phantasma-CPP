//__pragma(optimize("", off))
#define PHANTASMA_IMPLEMENTATION
#define SODIUM_STATIC

#include "../../include/Adapters/PhantasmaAPI_cpprest.h"
#include "../../include/PhantasmaAPI.h"
#include "../../include/Adapters/PhantasmaAPI_sodium.h"
#include "../../include/Blockchain/Transaction.h"
#include "../../include/Domain/Event.h"
#include "../../include/Cryptography/KeyPair.h"
#include "../../include/VM/ScriptBuilder.h"
#include "../../include/Utils/RpcUtils.h"

//Sorry, I haven't actually bundled a compiled version of libSodium with the project.
//You have to download/build libSodium yourself!
#pragma comment(lib, "libsodium.lib")


using namespace std;
using namespace phantasma;

void Write( int i )
{
	std::wcout << i;
}
void Write( const char* text )
{
	std::wcout << text;
}
void Write( const wchar_t* text )
{
	std::wcout << text;
}
void Write( const String& text )
{
	std::wcout << text.c_str();
}
void WriteLine()
{
	std::wcout << std::endl;
}
template<class T>
void WriteLine( const T& t )
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
#ifdef _UNICODE
	std::wcin >> str;
#else
	std::cin >> str;
#endif
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

static String GetChainAddress(const String& chainName, const vector<rpc::Chain>& chains)
{
	for(const auto& chain : chains)
		if( chain.name == chainName )
			return chain.address;
	return {};
}


class Program
{
	web::uri _host;
	web::http::client::http_client _http;
	rpc::PhantasmaAPI _phantasmaApiService;

	rpc::Account _account;
	PhantasmaKeys _key;
	vector<rpc::Chain> _chains;
	vector<rpc::Token> _tokens;
	String _nexus;
public:
	Program(const String& nexus, const String& host)
		: _host(host.c_str())
		, _http(_host)
		, _phantasmaApiService(_http)
		, _nexus(nexus)
	{
		WriteLine(U("Welcome to Phantasma Wallet sample."));
		WriteLine(U("Initializing..."));
		WriteLine(U("Fetching data..."));

		_chains = _phantasmaApiService.GetChains();
		_tokens = _phantasmaApiService.GetTokens(false);

		WriteLine(U("Enter your WIF:"));

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
				WriteLine(U("Incorrect wif, enter again:"));
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
			WriteLine("3 - register name");
			WriteLine("4 - send tokens");
			WriteLine("5 - list last 10 transactions");
			WriteLine("6 - logout");
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
				RegisterName();
				break;
			case 4:
				CrossChainTransfer();
				break;
			case 5:
				ListTransactions();
				break;
			case 6:
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

	void ShowBalance()
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
			if (_tokens.empty()) _tokens = _phantasmaApiService.GetTokens(false);
			for(const auto& balanceSheet : _account.balances)
			{
				WriteLine("********************");
				WriteLine("Token: ", balanceSheet.symbol.c_str());
				WriteLine("Chain: ", balanceSheet.chain.c_str());
				WriteLine("Amount: ", DecimalConversion(BigInteger::Parse(balanceSheet.amount), balanceSheet.decimals).c_str());

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

	void CrossChainTransfer()
	{
		if (_account.address.empty())
			_account = _phantasmaApiService.GetAccount(_key.GetAddress().ToString().c_str());
		if (!HaveTokenBalanceToTransfer())
		{
			WriteLine("No tokens to tranfer");
			return;
		}

		WriteLine("Select token and chain: ");

		for (int i = 0; i < _account.balances.size(); i++)
		{
			WriteLine(i + 1, " - ", _account.balances[i].symbol, " in ", _account.balances[i].chain, " chain");
		}

		int selectedTokenOption = std::stoi(ReadLine());
		if( selectedTokenOption < 1 || selectedTokenOption > _account.balances.size() )
		{
			WriteLine("Invalid selection");
			return;
		}
		const auto& token = _account.balances[selectedTokenOption - 1];

		WriteLine("Select destination chain:");

		for (int i = 0; i < _chains.size(); i++)
		{
			WriteLine(i + 1, " - ", _chains[i].name);
		}

		int selectedChainOption = std::stoi(ReadLine());
		if( selectedChainOption < 1 || selectedChainOption > _chains.size() )
		{
			WriteLine("Invalid selection");
			return;
		}
		const auto& destinationChain = _chains[selectedChainOption - 1];


		WriteLine("Enter amount: (max ", DecimalConversion(BigInteger::Parse(token.amount), token.decimals), ")");
		String amount = ReadLine();

		WriteLine("Enter destination address: ");
		String destinationAddress = ReadLine();

		if (!Address::IsValidAddress(destinationAddress))
		{
			WriteLine("Invalid address");
			return;
		}

		int cont = 1;
		if (token.chain == destinationChain.name)
		{
			SameChainTransfer(destinationAddress, amount, token.symbol, destinationChain.name);
		}
		else
		{
			WriteLine("TODO");
			//var listSteps = Helper.GetShortestPath(token.ChainName, destinationChain.Name, _chains);
			//if (listSteps.Count >= 2)
			//{
			//    while (listSteps.Count >= 2)
			//    {
			//        WriteLine($"Sending {cont} transaction of {listSteps.Count}");
			//        var txHash = CrossChainTransferToken(destinationAddress, listSteps[0].Name, listSteps[1].Name, token.Symbol,
			//            amount);
			//        var confirmationDto = _phantasmaApiService.GetConfirmations.SendRequestAsync(txHash);
			//        while (!confirmationDto.IsConfirmed) Task.Delay(100);
			//        WriteLine($"Settling block...");
			//        var settleTx = SettleBlock(listSteps[0].Address, confirmationDto.Hash, listSteps[1].Address);
			//        listSteps.RemoveAt(0);
			//        cont++;
			//    }
			//}
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
			.CallContract(U("token"), U("SettleBlock"), sourceChain, block)
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
			.CallContract(U("account"), U("RegisterName"), _key.GetAddress(), name)
			.SpendGas(_key.GetAddress())
			.EndScript();

		SignAndSendTx(script, U("main"));
	}

	String SignAndSendTx(const PHANTASMA_VECTOR<Byte>& script, const String& chain)
	{
		try
		{
			WriteLine("Sending transaction...");
			Transaction tx(_nexus.c_str(), chain.c_str(), script, Timestamp::Now() + Timespan::FromHours(1));
			tx.Sign(_key);
			String txResult = _phantasmaApiService.SendRawTransaction(Base16::Encode(tx.ToByteArray(true)).c_str());

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
	if (sodium_init() == -1)
	{
		WriteLine("Could not initialize sodium library");
		return 1;
	}

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
				nexus = U("simnet");
				host = U("http://localhost:7077/");
				break;
			case 2:
				nexus = U("testnet");
				host = U("http://testnet.phantasma.io:5101");
				break;
			case 3:
				nexus = U("mainnet");
				host = U("http://pharpc1.phantasma.io:7078");
				break;
			}

			WriteLine(U("Selected ") + nexus + U(" nexus with ") + host + U(" RPC"));

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
