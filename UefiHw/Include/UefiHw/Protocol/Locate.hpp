#pragma once

#include <UefiHw/Protocol/View.hpp>
#include <UefiHw/Firmware.hpp>
#include <optional>
#include <stdexcept>

namespace UefiHw::Protocol
{
	template <RegisteredProtocol T>
	[[nodiscard]]
	auto Find() -> std::optional<View<T>>
	{
		EFI_GUID Guid = Traits<T>::Guid;
		void* Interface{};

		const auto Status = FirmwareStatus(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.LocateProtocol(
						&Guid,
						nullptr,
						&Interface
					);
				}
			),
			"LocateProtocol"
		);

		if (Status == EFI_NOT_FOUND)
		{
			return std::nullopt;
		}

		RequireSuccess(
			Status,
			"LocateProtocol"
		);

		if (!Interface)
		{
			ContractViolation(
				"LocateProtocol returned null"
			);
		}

		return View<T>
		{
			*static_cast<T*>(Interface)
		};
	}

	template <RegisteredProtocol T>
	[[nodiscard]]
	auto FindOnHandle(
		EFI_HANDLE Handle
	) -> std::optional<View<T>>
	{
		if (!Handle)
		{
			throw std::invalid_argument(
				"Null protocol handle"
			);
		}

		EFI_GUID Guid = Traits<T>::Guid;
		void* Interface{};

		const auto Status = FirmwareStatus(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.HandleProtocol(
						Handle,
						&Guid,
						&Interface
					);
				}
			),
			"HandleProtocol"
		);

		if (Status == EFI_UNSUPPORTED)
		{
			return std::nullopt;
		}

		RequireSuccess(
			Status,
			"HandleProtocol"
		);

		if (!Interface)
		{
			ContractViolation(
				"HandleProtocol returned null"
			);
		}

		return View<T>
		{
			*static_cast<T*>(Interface)
		};
	}
}
