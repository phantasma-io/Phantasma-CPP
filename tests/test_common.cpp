#include "test_common.h"

namespace testutil {
using namespace phantasma;
using namespace phantasma::carbon;

void Report(TestContext& ctx, bool ok, const std::string& name, const std::string& details)
{
	++ctx.total;
	if (!ok)
	{
		++ctx.failed;
		std::cerr << "FAIL: " << name;
		if (!details.empty())
		{
			std::cerr << " (" << details << ")";
		}
		std::cerr << std::endl;
	}
}

std::string ToUpper(const std::string& s)
{
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)toupper(c); });
	return out;
}

ByteArray HexToBytes(const std::string& hex)
{
	return Base16::Decode(hex.c_str(), (int)hex.size());
}

std::string BytesToHex(const ByteArray& bytes)
{
	const String s = Base16::Encode(bytes.empty() ? 0 : &bytes.front(), (int)bytes.size(), false);
	return std::string(s.begin(), s.end());
}

ByteArray BytesFromView(const ByteView& view)
{
	if (!view.length)
	{
		return {};
	}
	return ByteArray(view.bytes, view.bytes + view.length);
}

ByteArray ParseDecBytes(const std::string& dec)
{
	std::stringstream ss(dec);
	std::string token;
	ByteArray out;
	while (ss >> token)
	{
		const int value = std::stoi(token, nullptr, 10);
		if (value < 0 || value > 255)
		{
			throw std::runtime_error("invalid decimal byte in fixture");
		}
		out.push_back((Byte)value);
	}
	return out;
}

int64_t ParseNum(const std::string& s)
{
	int base = 10;
	size_t idx = 0;
	if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		base = 16;
		idx = 2;
	}
	return std::stoll(s.substr(idx), nullptr, base);
}

uint64_t ParseUNum(const std::string& s)
{
	int base = 10;
	size_t idx = 0;
	if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		base = 16;
		idx = 2;
	}
	return std::stoull(s.substr(idx), nullptr, base);
}

intx ParseIntx(const std::string& s)
{
	bool err = false;
	intx v = intx::FromString(s.c_str(), (uint32_t)s.size(), 10, &err);
	if (err)
	{
		throw std::runtime_error("bad intx");
	}
	return v;
}

int256 ToInt256(const BigInteger& bi)
{
	const ByteArray bytes = bi.ToSignedByteArray();
	return int256::FromBytes(ByteView{ bytes.data(), bytes.size() });
}

std::vector<std::string> SplitCsv(const std::string& s)
{
	if (s.empty())
	{
		return {};
	}
	std::vector<std::string> out;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, ','))
	{
		out.push_back(item);
	}
	return out;
}

std::vector<ByteArray> ParseArrBytes2D(const std::string& s)
{
	// format: [[01,02],[03,04]]
	if (s.size() < 4 || s.front() != '[' || s[1] != '[' || s[s.size() - 2] != ']' || s[s.size() - 1] != ']')
	{
		return {};
	}
	const std::string inner = s.substr(2, s.size() - 4);
	std::vector<ByteArray> out;
	size_t start = 0;
	while (start < inner.size())
	{
		size_t end = inner.find("],[", start);
		const std::string seg = inner.substr(start, end == std::string::npos ? std::string::npos : end - start);
		std::string flat;
		for (char c : seg)
		{
			if (c != ',')
			{
				flat.push_back(c);
			}
		}
		out.push_back(HexToBytes(flat));
		if (end == std::string::npos)
		{
			break;
		}
		start = end + 3;
	}
	return out;
}

std::vector<Row> LoadRows(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		file.open("tests/" + path);
	}
	std::string line;
	std::vector<Row> rows;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		// strip UTF-8 BOM if present
		if (!line.empty() && (unsigned char)line[0] == 0xEF)
		{
			line = line.substr(3);
		}
		std::vector<std::string> cols;
		std::stringstream ss(line);
		std::string col;
		while (std::getline(ss, col, '\t'))
		{
			cols.push_back(col);
		}
		if (cols.size() == 5 && (cols[0] == "BI" || cols[0] == "INTX"))
		{
			rows.push_back(Row{ cols[0], cols[1], cols[2], cols[3], cols[4] });
		}
		else if (cols.size() == 3)
		{
			rows.push_back(Row{ cols[0], cols[1], cols[2], {}, {} });
		}
	}
	return rows;
}

const std::string& SampleIcon()
{
	static const std::string icon = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0naHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmcnIHZpZXdCb3g9JzAgMCAyNCAyNCc+PHBhdGggZmlsbD0nI0Y0NDMzNicgZD0nTTcgNGg1YTUgNSAwIDAxMCAxMEg5djZIN3pNOSA2djZoM2EzIDMgMCAwMDAtNnonLz48L3N2Zz4=";
	return icon;
}

Bytes32 ToBytes32(const ByteArray& bytes)
{
	Bytes32 out{};
	if (!bytes.empty())
	{
		out = View(bytes.data(), bytes.data() + bytes.size());
	}
	return out;
}

VmNamedVariableSchema MakeSchema(const char* name, VmType type, const VmStructSchema* structSchema)
{
	VmNamedVariableSchema schema{};
	schema.name = SmallString(name);
	schema.schema.type = type;
	if (structSchema)
	{
		schema.schema.structure = *structSchema;
	}
	return schema;
}

VmStructSchema MakeStructSchema(VmNamedVariableSchema* fields, uint32_t count, bool allowDynamicExtras)
{
	return VmStructSchema::Sort(count, fields, allowDynamicExtras);
}

ByteArray BuildConsensusSingleVoteScript()
{
	const std::string wif = "L5UEVHBjujaR1721aZM5Zm5ayjDyamMZS9W35RE9Y9giRkdf3dVx";
	const PhantasmaKeys keys = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
	const int gasLimit = 10000;
	const int gasPrice = 210000;
	const char* subject = "system.nexus.protocol.version";

	ScriptBuilder sb;
	// Keep TS argument order (gasLimit, gasPrice) to match the shared vector.
	return sb
		.AllowGas(keys.GetAddress(), Address(), gasLimit, gasPrice)
		.CallContract("consensus", "SingleVote", keys.GetAddress().Text(), subject, 0)
		.SpendGas(keys.GetAddress())
		.EndScript();
}

} // namespace testutil
