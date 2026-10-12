#pragma once

#include <UefiHw/Protocol/Traits.hpp>
#include <UefiHw/Firmware.hpp>
#include <UefiHw/Controller/Connections.hpp>
#include <optional>
#include <utility>

namespace UefiHw::Protocol
{
	template <RegisteredProtocol T, UINT32 OpenAttributes = EFI_OPEN_PROTOCOL_BY_DRIVER> class OpenedProtocol;
	template <RegisteredProtocol T>
	[[nodiscard]]
	auto OpenByDriver(
		EFI_HANDLE Protocol,
		EFI_HANDLE Agent,
		EFI_HANDLE Controller
	) -> OpenedProtocol<T>;

	template <RegisteredProtocol T>
	[[nodiscard]]
	auto TakeOver(
		EFI_HANDLE Protocol,
		EFI_HANDLE Agent
	) -> OpenedProtocol<T, EFI_OPEN_PROTOCOL_EXCLUSIVE>;

	template <RegisteredProtocol T, UINT32 OpenAttributes> class OpenedProtocol final
	{
		static_assert(
			OpenAttributes == EFI_OPEN_PROTOCOL_BY_DRIVER || OpenAttributes == EFI_OPEN_PROTOCOL_EXCLUSIVE
		);

	public:
		~OpenedProtocol() noexcept
		{
			try
			{
				Close();
			}
			catch (...)
			{
				FailCurrentCleanup(
					"CloseProtocol"
				);
			}
		}

		OpenedProtocol(const OpenedProtocol&) = delete;
		auto operator=(const OpenedProtocol&) -> OpenedProtocol& = delete;

		OpenedProtocol(
			OpenedProtocol&& Other
		) noexcept : InterfacePointer(std::exchange(Other.InterfacePointer, nullptr)), ProtocolHandle(std::exchange(Other.ProtocolHandle, nullptr)), AgentHandle(std::exchange(Other.AgentHandle, nullptr)), ControllerHandle(std::exchange(Other.ControllerHandle, nullptr)), PreviousDrivers(std::move(Other.PreviousDrivers))
		{
			Other.PreviousDrivers.reset();
		}

		auto operator=(OpenedProtocol&&) -> OpenedProtocol& = delete;

		[[nodiscard]]
		auto Get() const noexcept -> T*
		{
			return InterfacePointer;
		}

		[[nodiscard]]
		auto IsOpen() const noexcept -> bool
		{
			return ProtocolHandle != nullptr;
		}

		[[nodiscard]]
		auto Handle() const noexcept -> EFI_HANDLE
		{
			return ProtocolHandle;
		}

		[[nodiscard]]
		auto Agent() const noexcept -> EFI_HANDLE
		{
			return AgentHandle;
		}

		[[nodiscard]]
		auto Controller() const noexcept -> EFI_HANDLE
		{
			return ControllerHandle;
		}

		[[nodiscard]]
		static constexpr auto Attributes() noexcept -> UINT32
		{
			return OpenAttributes;
		}

		auto Close() -> void
		{
			if (ProtocolHandle)
			{
				EFI_GUID Guid = Traits<T>::Guid;

				RequireSuccess(
					UefiCpp::Firmware::TryCall(
						[&](
							EFI_BOOT_SERVICES& BootServices
						) noexcept -> EFI_STATUS
						{
							return BootServices.CloseProtocol(
								ProtocolHandle,
								&Guid,
								AgentHandle,
								ControllerHandle
							);
						}
					),
					"CloseProtocol"
				);

				InterfacePointer = nullptr;
				ProtocolHandle = nullptr;
				AgentHandle = nullptr;
				ControllerHandle = nullptr;
			}

			if (PreviousDrivers)
			{
				PreviousDrivers->Reconnect();
				PreviousDrivers.reset();
			}
		}

	private:
		template <RegisteredProtocol U>
		friend auto OpenByDriver(
			EFI_HANDLE,
			EFI_HANDLE,
			EFI_HANDLE
		) -> OpenedProtocol<U>;

		template <RegisteredProtocol U>
		friend auto TakeOver(
			EFI_HANDLE,
			EFI_HANDLE
		) -> OpenedProtocol<U, EFI_OPEN_PROTOCOL_EXCLUSIVE>;

		OpenedProtocol(
			T* Interface,
			EFI_HANDLE Protocol,
			EFI_HANDLE Agent,
			EFI_HANDLE Controller
		) noexcept : InterfacePointer(Interface), ProtocolHandle(Protocol), AgentHandle(Agent), ControllerHandle(Controller)
		{
		}

		T* InterfacePointer{};
		EFI_HANDLE ProtocolHandle{};
		EFI_HANDLE AgentHandle{};
		EFI_HANDLE ControllerHandle{};
		std::optional<UefiHw::Controller::DriverConnections> PreviousDrivers;
	};
}
