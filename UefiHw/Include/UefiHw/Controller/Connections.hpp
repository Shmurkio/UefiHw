#pragma once

#include <Uefi.h>
#include <span>
#include <vector>

namespace UefiHw::Controller
{
	auto Disconnect(
		EFI_HANDLE Controller,
		EFI_HANDLE Driver = nullptr,
		EFI_HANDLE Child = nullptr
	) -> void;

	auto Connect(
		EFI_HANDLE Controller,
		std::span<const EFI_HANDLE> PreferredDrivers = {},
		bool Recursive = true
	) -> void;

	class DriverConnections final
	{
	public:
		[[nodiscard]]
		static auto Capture(
			EFI_HANDLE Controller,
			EFI_GUID Protocol
		) -> DriverConnections;

		[[nodiscard]]
		auto Opens() const noexcept -> std::span<const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY>;

		auto Reconnect() -> void;

	private:
		DriverConnections() = default;
		EFI_HANDLE ControllerHandle{};
		EFI_GUID ProtocolGuid{};
		std::vector<EFI_OPEN_PROTOCOL_INFORMATION_ENTRY> OriginalOpens;
		std::vector<EFI_HANDLE> Drivers;
	};
}
