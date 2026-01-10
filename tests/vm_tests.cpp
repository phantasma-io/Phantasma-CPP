#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunVmObjectTests(TestContext& ctx)
{
#if __cplusplus >= 201703L
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::Bool);
		w.Write((uint8_t)1);
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		const bool ok = obj.DeserializeData(r) && obj.GetType() == VMType::Bool && obj.Data<bool>();
		Report(ctx, ok, "VMObject Bool");
	}
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::Bytes);
		const ByteArray payload = { (Byte)0x01, (Byte)0x02 };
		w.WriteByteArray(payload);
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		const bool ok = obj.DeserializeData(r) &&
			obj.GetType() == VMType::Bytes &&
			obj.Data<ByteArray>() == payload;
		Report(ctx, ok, "VMObject Bytes");
	}
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::String);
		w.WriteVarString(PHANTASMA_LITERAL("hello world"));
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		const bool ok = obj.DeserializeData(r) &&
			obj.GetType() == VMType::String &&
			obj.Data<String>() == PHANTASMA_LITERAL("hello world");
		Report(ctx, ok, "VMObject String");
	}
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::Number);
		w.WriteBigInteger(BigInteger(123));
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		const bool ok = obj.DeserializeData(r) &&
			obj.GetType() == VMType::Number &&
			obj.Data<BigInteger>().ToString() == PHANTASMA_LITERAL("123");
		Report(ctx, ok, "VMObject Number");
	}
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::Struct);
		w.WriteVarInt(1);
		w.Write((uint8_t)VMType::String);
		w.WriteVarString(PHANTASMA_LITERAL("name"));
		w.Write((uint8_t)VMType::Number);
		w.WriteBigInteger(BigInteger(7));
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		bool ok = obj.DeserializeData(r) && obj.GetType() == VMType::Struct;
		if (ok)
		{
			const VMStructure& data = obj.Data<VMStructure>();
			ok = data.size() == 1 &&
				data[0].first.GetType() == VMType::String &&
				data[0].first.Data<String>() == PHANTASMA_LITERAL("name") &&
				data[0].second.GetType() == VMType::Number &&
				data[0].second.Data<BigInteger>().ToString() == PHANTASMA_LITERAL("7");
		}
		Report(ctx, ok, "VMObject Struct");
	}
#else
	(void)ctx;
#endif
}

void RunVmDynamicVariableTests(TestContext& ctx)
{
	auto roundtrip = [&](const std::string& name, VmType type, const VmDynamicVariable& input, auto&& check) {
		try
		{
			ByteArray buffer;
			WriteView w(buffer);
			Write(type, input, nullptr, w);
			Allocator alloc;
			ReadView r(buffer.empty() ? nullptr : (void*)buffer.data(), buffer.size());
			VmDynamicVariable out{};
			const bool ok = Read(type, out, nullptr, r, alloc) && check(out);
			Report(ctx, ok, name);
		}
		catch (const std::exception& ex)
		{
			Report(ctx, false, name, ex.what());
		}
		catch (...)
		{
			Report(ctx, false, name, "unexpected exception");
		}
	};

	for (const uint8_t value : { (uint8_t)0, (uint8_t)1, (uint8_t)255 })
	{
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int8 " + std::to_string(value), VmType::Int8, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int8 && out.data.int8 == value;
		});
	}
	for (const int16_t value : { (int16_t)0, (int16_t)1, (int16_t)-32768, (int16_t)32767 })
	{
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int16 " + std::to_string(value), VmType::Int16, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int16 && (int16_t)out.data.int16 == value;
		});
	}
	for (const int32_t value : { (int32_t)0, (int32_t)1, (int32_t)-2147483648, (int32_t)2147483647 })
	{
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int32 " + std::to_string(value), VmType::Int32, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int32 && (int32_t)out.data.int32 == value;
		});
	}
	for (const uint64_t value : { (uint64_t)0, (uint64_t)1, std::numeric_limits<uint64_t>::max() })
	{
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int64 " + std::to_string((unsigned long long)value), VmType::Int64, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int64 && out.data.int64 == value;
		});
	}
	{
		const intx valueX = intx::FromString("1234567890123456789012345678901234567890", 0, 10, nullptr);
		const int256 value = valueX.Int256();
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int256", VmType::Int256, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int256 && out.data.int256.Signed().ToString() == value.ToString();
		});
	}
	{
		ByteArray bytes(32);
		for (size_t i = 0; i < bytes.size(); ++i)
		{
			bytes[i] = (Byte)i;
		}
		const VmDynamicVariable input(ByteView{ bytes.data(), bytes.size() });
		roundtrip("VmDynamic Bytes", VmType::Bytes, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Bytes && BytesFromView(out.data.bytes) == bytes;
		});
	}
	{
		const char* text = "hello world";
		const VmDynamicVariable input(text);
		roundtrip("VmDynamic String", VmType::String, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::String && std::string(out.data.string) == text;
		});
	}
	{
		Bytes16 bytes{};
		for (int i = 0; i < Bytes16::length; ++i)
		{
			bytes.bytes[i] = (Byte)i;
		}
		const VmDynamicVariable input(bytes);
		roundtrip("VmDynamic Bytes16", VmType::Bytes16, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Bytes16 && out.data.bytes16 == bytes;
		});
	}
	{
		Bytes32 bytes{};
		for (int i = 0; i < Bytes32::length; ++i)
		{
			bytes.bytes[i] = (Byte)i;
		}
		const VmDynamicVariable input(bytes);
		roundtrip("VmDynamic Bytes32", VmType::Bytes32, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Bytes32 && out.data.bytes32 == bytes;
		});
	}
	{
		Bytes64 bytes{};
		for (int i = 0; i < Bytes64::length; ++i)
		{
			bytes.bytes[i] = (Byte)i;
		}
		const VmDynamicVariable input(bytes);
		roundtrip("VmDynamic Bytes64", VmType::Bytes64, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Bytes64 && out.data.bytes64 == bytes;
		});
	}
}

} // namespace testcases
