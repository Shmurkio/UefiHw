#pragma once

#include <UefiHw/Protocol/Installed.hpp>
#include <stdexcept>

namespace UefiHw::Protocol
{
	template <RegisteredProtocol T>
	[[nodiscard]]
	auto Install(
		EFI_HANDLE Handle,
		std::unique_ptr<T> Interface
	) -> InstalledProtocol<T>
	{
		if (!Interface)
		{
			throw std::invalid_argument(
				"Null protocol interface"
			);
		}

		EFI_GUID Guid = Traits<T>::Guid;
		auto InstalledHandle = Handle;

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.InstallProtocolInterface(
						&InstalledHandle,
						&Guid,
						EFI_NATIVE_INTERFACE,
						Interface.get()
					);
				}
			),
			"InstallProtocolInterface"
		);

		if (!InstalledHandle)
		{
			FailCleanup(
				{
					ErrorStage::Contract,
					EFI_COMPROMISED_DATA,
					"InstallProtocolInterface null handle"
				}
			);
		}

		return InstalledProtocol<T>
		{
			InstalledHandle,
			std::move(Interface)
		};
	}

	template <RegisteredProtocol T>
	[[nodiscard]]
	auto Install(
		std::unique_ptr<T> Interface
	) -> InstalledProtocol<T>
	{
		return Install<T>(
			nullptr,
			std::move(Interface)
		);
	}
}
