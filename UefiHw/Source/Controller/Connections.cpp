#include <UefiHw/Controller/Connections.hpp>
#include <UefiHw/Protocol/Information.hpp>
#include <UefiHw/Firmware.hpp>
#include <algorithm>
#include <stdexcept>

namespace UefiHw::Controller
{
	namespace
	{
		auto ConnectList(
			EFI_HANDLE Controller,
			EFI_HANDLE* Drivers,
			bool Recursive
		) -> EFI_STATUS
		{
			return FirmwareStatus(
				UefiCpp::Firmware::TryCall(
					[&](
						EFI_BOOT_SERVICES& BootServices
					) noexcept -> EFI_STATUS
					{
						return BootServices.ConnectController(
							Controller,
							Drivers,
							nullptr,
							Recursive ? TRUE : FALSE
						);
					}
				),
				"ConnectController"
			);
		}
	}

	auto Disconnect(
		EFI_HANDLE Controller,
		EFI_HANDLE Driver,
		EFI_HANDLE Child
	) -> void
	{
		if (!Controller)
		{
			throw std::invalid_argument(
				"Disconnect requires a controller"
			);
		}

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
					) noexcept -> EFI_STATUS
				{
					return BootServices.DisconnectController(
						Controller,
						Driver,
						Child
					);
				}
			),
			"DisconnectController"
		);
	}

	auto Connect(
		EFI_HANDLE Controller,
		std::span<const EFI_HANDLE> PreferredDrivers,
		bool Recursive
	) -> void
	{
		if (!Controller || std::ranges::find(PreferredDrivers, nullptr) != PreferredDrivers.end())
		{
			throw std::invalid_argument(
				"Connect requires non-null controller and driver handles"
			);
		}

		if (PreferredDrivers.empty())
		{
			RequireSuccess(
				ConnectList(Controller, nullptr, Recursive),
				"ConnectController"
			);

			return;
		}

		std::vector<EFI_HANDLE> Drivers(
			PreferredDrivers.begin(),
			PreferredDrivers.end()
		);

		Drivers.push_back(
			nullptr
		);

		RequireSuccess(
			ConnectList(Controller, Drivers.data(),Recursive),
			"ConnectController"
		);
	}

	auto DriverConnections::Capture(
		EFI_HANDLE Controller,
		EFI_GUID Protocol
	) -> DriverConnections
	{
		DriverConnections Result;
		Result.ControllerHandle = Controller;
		Result.ProtocolGuid = Protocol;

		Result.OriginalOpens = UefiHw::Protocol::OpenInformation(
			Controller,
			Protocol
		);

		for (const auto& Entry : Result.OriginalOpens)
		{
			if ((Entry.Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) != 0)
			{
				if (!Entry.AgentHandle)
				{
					ContractViolation(
						"OpenProtocolInformation returned null driver"
					);
				}
				if (std::ranges::find(Result.Drivers, Entry.AgentHandle) == Result.Drivers.end())
				{
					Result.Drivers.push_back(
						Entry.AgentHandle
					);
				}
			}
		}
		Result.Drivers.push_back(
			nullptr
		);

		return Result;
	}

	auto DriverConnections::Opens() const noexcept -> std::span<const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY>
	{
		return OriginalOpens;
	}

	auto DriverConnections::Reconnect() -> void
	{
		if (Drivers.size() <= 1)
		{
			return;
		}

		const auto Status = ConnectList(
			ControllerHandle,
			Drivers.data(),
			true
		);

		if (Status != EFI_SUCCESS && Status != EFI_NOT_FOUND)
		{
			RequireSuccess(
				Status,
				"ConnectController"
			);
		}

		const auto Current = UefiHw::Protocol::OpenInformation(
			ControllerHandle,
			ProtocolGuid
		);

		for (const auto Driver : Drivers)
		{
			if (Driver && !std::ranges::any_of(Current, [Driver](const auto& Entry)
				{
					return Entry.AgentHandle == Driver && (Entry.Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) != 0;
				}))
			{
				RequireSuccess(
					Status,
					"ConnectController"
				);

				throw FirmwareError
				{
					ErrorStage::Firmware,
					EFI_NOT_STARTED,
					"Reconnect original protocol driver"
				};
			}
		}
	}
}
