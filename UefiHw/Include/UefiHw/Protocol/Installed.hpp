#pragma once

#include <UefiHw/Protocol/Traits.hpp>
#include <UefiHw/Firmware.hpp>
#include <memory>
#include <utility>

namespace UefiHw::Protocol
{
	template <RegisteredProtocol T> class InstalledProtocol;

	template <RegisteredProtocol T>
	[[nodiscard]]
	auto Install(
		EFI_HANDLE Handle,
		std::unique_ptr<T> Interface
	) -> InstalledProtocol<T>;

	template <RegisteredProtocol T> class InstalledProtocol final
	{
	public:
		~InstalledProtocol() noexcept
		{
			try
			{
				Close();
			}
			catch (...)
			{
				FailCurrentCleanup(
					"UninstallProtocolInterface"
				);
			}
		}
		InstalledProtocol(const InstalledProtocol&) = delete;
		auto operator=(const InstalledProtocol&) -> InstalledProtocol& = delete;

		InstalledProtocol(
			InstalledProtocol&& Other
		) noexcept : InterfaceOwner(std::move(Other.InterfaceOwner)), ProtocolHandle(std::exchange(Other.ProtocolHandle, nullptr))
		{
		}

		auto operator=(InstalledProtocol&&) -> InstalledProtocol& = delete;

		[[nodiscard]]
		auto Get() const noexcept -> T*
		{
			return InterfaceOwner.get();
		}

		[[nodiscard]]
		auto Handle() const noexcept -> EFI_HANDLE
		{
			return ProtocolHandle;
		}

		[[nodiscard]]
		auto IsInstalled() const noexcept -> bool
		{
			return ProtocolHandle != nullptr;
		}

		auto Close() -> void
		{
			if (!ProtocolHandle)
			{
				return;
			}

			EFI_GUID Guid = Traits<T>::Guid;

			RequireSuccess(
				UefiCpp::Firmware::TryCall(
					[&](
						EFI_BOOT_SERVICES& BootServices
					) noexcept -> EFI_STATUS
					{
						return BootServices.UninstallProtocolInterface(ProtocolHandle, &Guid, InterfaceOwner.get());
					}
				),
				"UninstallProtocolInterface"
			);

			ProtocolHandle = nullptr;
			InterfaceOwner.reset();
		}

	private:
		template <RegisteredProtocol U>
		friend auto Install(
			EFI_HANDLE,
			std::unique_ptr<U>
		) -> InstalledProtocol<U>;

		InstalledProtocol(
			EFI_HANDLE Handle,
			std::unique_ptr<T> Interface
		) noexcept : InterfaceOwner(std::move(Interface)), ProtocolHandle(Handle)
		{
		}

		std::unique_ptr<T> InterfaceOwner{};
		EFI_HANDLE ProtocolHandle{};
	};
}
