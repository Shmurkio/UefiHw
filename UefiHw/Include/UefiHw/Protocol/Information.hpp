#pragma once

#include <UefiHw/Protocol/Traits.hpp>
#include <vector>

namespace UefiHw::Protocol
{
	[[nodiscard]]
	auto OpenInformation(
		EFI_HANDLE Handle,
		EFI_GUID Guid
	) -> std::vector<EFI_OPEN_PROTOCOL_INFORMATION_ENTRY>;

	template <RegisteredProtocol T>
	[[nodiscard]]
	auto OpenInformation(
		EFI_HANDLE Handle
	) -> std::vector<EFI_OPEN_PROTOCOL_INFORMATION_ENTRY>
	{
		return OpenInformation(
			Handle,
			Traits<T>::Guid
		);
	}
}
