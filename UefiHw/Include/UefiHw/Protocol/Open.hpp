#pragma once

#include <UefiHw/Protocol/Opened.hpp>
#include <stdexcept>

namespace UefiHw::Protocol
{
	template <RegisteredProtocol T>
	[[nodiscard]]
	auto OpenByDriver(
		EFI_HANDLE Protocol,
		EFI_HANDLE Agent,
		EFI_HANDLE Controller
	) -> OpenedProtocol<T>
	{
		if (!Protocol || !Agent || !Controller)
		{
			throw std::invalid_argument(
				"OpenByDriver requires protocol, agent and controller handles"
			);
		}

		EFI_GUID Guid = Traits<T>::Guid;
		void* Interface{};

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.OpenProtocol(
						Protocol,
						&Guid,
						&Interface,
						Agent,
						Controller,
						EFI_OPEN_PROTOCOL_BY_DRIVER
					);
				}
			),
			"OpenProtocol"
		);

		OpenedProtocol<T> Result(
			static_cast<T*>(Interface),
			Protocol,
			Agent,
			Controller
		);

		if (!Interface)
		{
			ContractViolation(
				"OpenProtocol returned null"
			);
		}

		return Result;
	}

	template <RegisteredProtocol T>
	[[nodiscard]]
	auto TakeOver(
		EFI_HANDLE Protocol,
		EFI_HANDLE Agent
	) -> OpenedProtocol<T, EFI_OPEN_PROTOCOL_EXCLUSIVE>
	{
		if (!Protocol || !Agent)
		{
			throw std::invalid_argument(
				"TakeOver requires protocol and agent handles"
			);
		}
		auto Previous = UefiHw::Controller::DriverConnections::Capture(
			Protocol, Traits<T>::Guid
		);

		for (const auto& Entry : Previous.Opens())
		{
			if ((Entry.Attributes & EFI_OPEN_PROTOCOL_EXCLUSIVE) != 0 || (Entry.AgentHandle == Agent && (Entry.Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) != 0))
			{
				throw FirmwareError
				{
					ErrorStage::Firmware,
					EFI_ACCESS_DENIED,
					"OpenProtocol takeover ownership conflict"
				};
			}
		}

		EFI_GUID Guid = Traits<T>::Guid;
		void* Interface{};

		const auto Outcome = UefiCpp::Firmware::TryCall(
			[&](
				EFI_BOOT_SERVICES& BootServices
			) noexcept -> EFI_STATUS
			{
				return BootServices.OpenProtocol(
					Protocol,
					&Guid,
					&Interface,
					Agent,
					nullptr,
					EFI_OPEN_PROTOCOL_EXCLUSIVE
				);
			}
		);

		const auto Status = FirmwareStatus(
			Outcome,
			"OpenProtocol takeover"
		);

		if (Status != EFI_SUCCESS)
		{
			try
			{
				Previous.Reconnect();
			}
			catch (...)
			{
				FailCurrentCleanup(
					"Protocol takeover rollback"
				);
			}

			RequireSuccess(
				Status,
				"OpenProtocol takeover"
			);
		}

		OpenedProtocol<T, EFI_OPEN_PROTOCOL_EXCLUSIVE> Result(
			static_cast<T*>(Interface),
			Protocol,
			Agent,
			nullptr
		);

		Result.PreviousDrivers.emplace(
			std::move(Previous)
		);

		if (!Interface)
		{
			ContractViolation(
				"OpenProtocol returned null"
			);
		}

		return Result;
	}
}
