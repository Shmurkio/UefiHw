#pragma once

#include <UefiHw/Protocol/Traits.hpp>
#include <vector>

namespace UefiHw::Protocol
{
	[[nodiscard]]
	auto EnumerateHandles(
		EFI_GUID Guid
	) -> std::vector<EFI_HANDLE>;

	template <RegisteredProtocol T>
	[[nodiscard]]
	auto FindHandles() -> std::vector<EFI_HANDLE>
	{
		return EnumerateHandles(
			Traits<T>::Guid
		);
	}
}
