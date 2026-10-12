#pragma once

#include <UefiHw/Protocol/Traits.hpp>
#include <UefiHw/Firmware.hpp>
#include <array>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace UefiHw::Protocol
{
	template <RegisteredProtocol... T>
	requires(sizeof...(T) > 1)
	class InstalledProtocols final
	{
	public:
		explicit InstalledProtocols(
			EFI_HANDLE Handle,
			std::unique_ptr<T>... Interfaces
		) : Owners(std::move(Interfaces)...), ProtocolHandle(Handle)
		{
			const auto Valid = std::apply(
				[](
					const auto&... Owner
				) noexcept -> bool
				{
					return (static_cast<bool>(Owner) && ...);
				},
				Owners
			);

			if (!Valid)
			{
				throw std::invalid_argument(
					"Null grouped protocol interface"
				);
			}

			for (std::size_t First{}; First < Guids.size(); ++First)
			{
				for (auto Second = First + 1; Second < Guids.size(); ++Second)
				{
					const auto& A = Guids[First];
					const auto& B = Guids[Second];
					bool Equal = A.Data1 == B.Data1 && A.Data2 == B.Data2 && A.Data3 == B.Data3;

					for (std::size_t Index{}; Index < 8; ++Index)
					{
						Equal = Equal && A.Data4[Index] == B.Data4[Index];
					}

					if (Equal)
					{
						throw std::invalid_argument(
							"Duplicate grouped protocol GUID"
						);
					}
				}
			}

			RequireSuccess(
				Invoke(true),
				"InstallMultipleProtocolInterfaces"
			);

			if (!ProtocolHandle)
			{
				FailCleanup(
					{
						ErrorStage::Contract,
						EFI_COMPROMISED_DATA,
						"InstallMultiple null handle"
					}
				);
			}

			Published = true;
		}

		~InstalledProtocols() noexcept
		{
			try
			{
				Close();
			}
			catch (...)
			{
				FailCurrentCleanup(
					"UninstallMultipleProtocolInterfaces"
				);
			}
		}
		InstalledProtocols(const InstalledProtocols&) = delete;
		auto operator=(const InstalledProtocols&) -> InstalledProtocols& = delete;

		InstalledProtocols(
			InstalledProtocols&& Other
		) noexcept : Owners(std::move(Other.Owners)), ProtocolHandle(std::exchange(Other.ProtocolHandle, nullptr)), Published(std::exchange(Other.Published, false))
		{
		}

		auto operator=(InstalledProtocols&&) -> InstalledProtocols& = delete;

		template <typename U>
		[[nodiscard]]
		auto Get() const noexcept -> U*
		{
			return std::get<std::unique_ptr<U>>(Owners).get();
		}

		[[nodiscard]]
		auto Handle() const noexcept -> EFI_HANDLE
		{
			return ProtocolHandle;
		}

		auto Close() -> void
		{
			if (!Published)
			{
				return;
			}

			RequireSuccess(
				Invoke(false),
				"UninstallMultipleProtocolInterfaces"
			);

			Published = false;
			ProtocolHandle = nullptr;

			std::apply(
				[](
					auto&... Owner
				) noexcept -> void
				{
					(Owner.reset(), ...);
				},
				Owners
			);
		}

	private:
		using ArgumentTuple = decltype(std::tuple_cat(std::tuple<EFI_GUID*, T*>{}...));

		template <std::size_t... Index>
		auto Arguments(
			std::index_sequence<Index...>
		) noexcept -> ArgumentTuple
		{
			return std::tuple_cat(
				std::make_tuple(&Guids[Index], std::get<Index>(Owners).get())...
			);
		}
		auto Invoke(
			bool Install
		) noexcept -> UefiCpp::Firmware::Outcome<EFI_STATUS>
		{
			auto Pairs = Arguments(
				std::index_sequence_for<T...>{}
			);

			return UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return std::apply(
						[&](
							auto*... Argument
						) noexcept -> EFI_STATUS
						{
							return Install ? BootServices.InstallMultipleProtocolInterfaces(&ProtocolHandle, Argument..., static_cast<void*>(nullptr)) : BootServices.UninstallMultipleProtocolInterfaces(ProtocolHandle, Argument..., static_cast<void*>(nullptr));
						},
						Pairs
					);
				}
			);
		}

		std::array<EFI_GUID, sizeof...(T)> Guids{ Traits<T>::Guid... };
		std::tuple<std::unique_ptr<T>...> Owners;
		EFI_HANDLE ProtocolHandle{};
		bool Published{};
	};
}
