#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunBigIntSerializationTests(TestContext& ctx)
{
	std::ifstream file("tests/fixtures/phantasma_bigint_vectors.tsv");
	if (!file.is_open())
	{
		file.open("fixtures/phantasma_bigint_vectors.tsv");
	}
	if (!file.is_open())
	{
		Report(ctx, false, "BigInt fixture", "missing phantasma_bigint_vectors.tsv");
		return;
	}

	std::string line;
	bool header = true;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (header)
		{
			header = false;
			continue;
		}
		std::vector<std::string> cols;
		std::stringstream ss(line);
		std::string col;
		while (std::getline(ss, col, '\t'))
		{
			cols.push_back(col);
		}
		if (cols.size() < 3)
		{
			continue;
		}

		const std::string& number = cols[0];
		const ByteArray expectedPha = ParseDecBytes(cols[1]);
		const ByteArray expectedCsharp = ParseDecBytes(cols[2]);
		const BigInteger n = BigInteger::Parse(String(number.c_str()));
		const bool isNegative = !number.empty() && number[0] == '-';
		const bool isZero = number == "0";
		const std::string absText = isNegative ? number.substr(1) : number;

		const ByteArray phaBytes = n.ToSignedByteArray();
		Report(ctx, phaBytes == expectedPha, "BigInt PHA " + number);

		const BigInteger fromSigned = BigInteger::FromSignedArray(expectedPha);
		Report(ctx, fromSigned.ToString() == String(number.c_str()), "BigInt FromSignedArray " + number);

		const BigInteger fromSignedPtr = BigInteger::FromSignedArray(
			expectedPha.empty() ? nullptr : expectedPha.data(),
			(int)expectedPha.size());
		Report(ctx, fromSignedPtr.ToString() == String(number.c_str()), "BigInt FromSignedArray ptr " + number);

		Report(ctx, n.IsZero() == isZero, "BigInt IsZero " + number);
		Report(ctx, n.IsNegative() == isNegative, "BigInt IsNegative " + number);

		const BigInteger absVal = BigInteger::Abs(n);
		Report(ctx, absVal.ToString() == String(absText.c_str()), "BigInt Abs " + number);

		const BigInteger neg = -n;
		if (isZero)
		{
			Report(ctx, neg == n, "BigInt Negate zero " + number);
		}
		else
		{
			Report(ctx, neg != n, "BigInt Negate sign " + number);
			Report(ctx, (-neg).ToString() == String(number.c_str()), "BigInt Double negate " + number);
		}

		BinaryWriter writer;
		writer.WriteBigInteger(n);
		const ByteArray serialized = writer.ToArray();
		const bool lengthOk = serialized.size() == expectedPha.size() + 1 &&
			!serialized.empty() &&
			serialized[0] == (Byte)expectedPha.size();
		Report(ctx, lengthOk, "BigInt PHA length " + number);
		if (lengthOk)
		{
			ByteArray serializedBytes;
			serializedBytes.assign(serialized.begin() + 1, serialized.end());
			Report(ctx, serializedBytes == expectedPha, "BigInt PHA bytes " + number);
		}
		BinaryReader reader(serialized);
		BigInteger roundtripPha;
		reader.ReadBigInteger(roundtripPha);
		Report(ctx, roundtripPha.ToString() == String(number.c_str()), "BigInt PHA roundtrip " + number);

		ScriptBuilder sb;
		sb.EmitLoad(0, n);
		const ByteArray script = sb.ToScript();
		BinaryReader scriptReader(script);
		Byte opcode = 0;
		Byte reg = 0;
		Byte type = 0;
		scriptReader.Read(opcode);
		scriptReader.Read(reg);
		scriptReader.Read(type);
		(void)reg;
		Int64 len = 0;
		scriptReader.ReadVarInt(len);
		ByteArray csharpBytes;
		scriptReader.Read(csharpBytes, (int)len);
		Report(ctx, opcode == (Byte)Opcode::LOAD, "BigInt C# opcode " + number);
		Report(ctx, type == (Byte)VMType::Number, "BigInt C# type " + number);
		Report(ctx, csharpBytes == expectedCsharp, "BigInt C# " + number);
	}
}

} // namespace testcases
