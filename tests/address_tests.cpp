#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunAddressTests(TestContext& ctx)
{
	const std::string wif = "L5UEVHBjujaR1721aZM5Zm5ayjDyamMZS9W35RE9Y9giRkdf3dVx";
	const std::string expected = "P2KFEyFevpQfSaW8G4VjSmhWUZXR4QrG9YQR1HbMpTUCpCL";
	const PhantasmaKeys keys = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
	const bool fromWifOk = std::string(keys.GetAddress().Text().c_str()) == expected;
	Report(ctx, fromWifOk, "Address from WIF");
	const Address addr = Address::FromText(expected.c_str(), (int)expected.size());
	const bool fromTextOk = std::string(addr.Text().c_str()) == expected;
	Report(ctx, fromTextOk, "Address from text");
	const Address fromKey = Address::FromKey(keys);
	const bool fromKeyOk = std::string(fromKey.Text().c_str()) == expected;
	Report(ctx, fromKeyOk, "Address from key");
	const Address nullAddr;
	Report(ctx, nullAddr.IsNull(), "Address null");
	const bool nullLabelOk = nullAddr.ToString() == PHANTASMA_LITERAL("[Null address]");
	Report(ctx, nullLabelOk, "Address null label");
}

} // namespace testcases
