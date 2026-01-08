#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include "../Carbon.h"

namespace phantasma::carbon {

enum class ListingType : uint8_t
{
	FixedPrice
};

struct TokenListing
{
	ListingType type = ListingType::FixedPrice;
	Bytes32 seller{};
	uint64_t quoteTokenId = 0;
	intx price{};
	int64_t startDate = 0;
	int64_t endDate = 0;
};

inline bool Read(TokenListing& out, ReadView& r)
{
	out.type = static_cast<ListingType>(Read1(r));
	return
		Read(out.seller, r) &&
		Read(out.quoteTokenId, r) &&
		Read(out.price, r) &&
		Read(out.startDate, r) &&
		Read(out.endDate, r);
}

inline void Write(const TokenListing& in, WriteView& w)
{
	Write1(static_cast<uint8_t>(in.type), w);
	Write(in.seller, w);
	Write(in.quoteTokenId, w);
	Write(in.price, w);
	Write(in.startDate, w);
	Write(in.endDate, w);
}

} // namespace phantasma::carbon
