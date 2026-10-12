#include <UefiHw/Driver/Registration.hpp>
#include <UefiHw/Firmware.hpp>
#include <UefiCpp/Execution.hpp>
#include <Protocol/LoadedImage.h>
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace UefiHw::Driver
{
	struct Registration::State final
	{
		struct Envelope final
		{
			EFI_DRIVER_BINDING_PROTOCOL Interface{};
			State* Owner{};
		};

		static_assert(
			std::is_standard_layout_v<Envelope>
		);

		static_assert(
			offsetof(Envelope, Interface) == 0
		);

		struct Entry final
		{
			EFI_HANDLE Handle{};
			std::unique_ptr<Controller> Instance;
			bool Started{};
		};

		Envelope Published{};
		std::unique_ptr<Binding> Implementation;
		std::vector<Entry> Controllers;
		std::atomic_flag CallbackActive{};
		bool Installed{};
		bool Closing{};
		bool UnloadInstalled{};
		EFI_HANDLE Image{};
		std::optional<FirmwareError> Failure;

		static inline constexpr EFI_GUID UnloadGuid = { 0x5621a50e, 0x842f, 0x48df, { 0xab, 0xf2, 0x95, 0xe4, 0x63, 0x13, 0x90, 0xbc } };

		auto RecordException() noexcept -> EFI_STATUS
		{
			const auto Status = CurrentExceptionStatus();

			try
			{
				throw;
			}
			catch (const FirmwareError& Error)
			{
				Failure = Error;
			}
			catch (...)
			{
				Failure.emplace(
					ErrorStage::Contract,
					Status,
					"Driver callback"
				);
			}

			return Status;
		}

		template <typename F>
		auto Dispatch(
			F&& Function
		) noexcept -> EFI_STATUS
		{
			if (CallbackActive.test_and_set(std::memory_order_acquire))
			{
				return EFI_NOT_READY;
			}

			struct Guard final
			{
				std::atomic_flag& Flag;

				~Guard() noexcept
				{
					Flag.clear(std::memory_order_release);
				}

			} Active { CallbackActive };

			try
			{
				return Function();
			}
			catch (...)
			{
				return RecordException();
			}
		}

		[[nodiscard]]
		auto Find(
			EFI_HANDLE Handle
		) -> std::vector<Entry>::iterator
		{
			return std::find_if(
				Controllers.begin(),
				Controllers.end(),
				[Handle](
					const Entry& Candidate
				) noexcept -> bool
				{
					return Candidate.Handle == Handle;
				}
			);
		}

		auto Start(
			EFI_HANDLE Handle,
			EFI_DEVICE_PATH_PROTOCOL* Path
		) -> EFI_STATUS
		{
			if (Closing)
			{
				return EFI_NOT_READY;
			}

			if (const auto Existing = Find(Handle); Existing != Controllers.end())
			{
				return Existing->Started ? EFI_ALREADY_STARTED : EFI_NOT_READY;
			}

			Controllers.reserve(
				Controllers.size() + 1
			);

			auto Instance = Implementation->CreateController(
				Published.Interface.DriverBindingHandle,
				Handle
			);

			if (!Instance)
			{
				ContractViolation(
					"CreateController returned null"
				);
			}

			Controllers.push_back(
				{
					Handle,
					std::move(Instance),
					false
				}
			);

			auto& Current = Controllers.back();
			EFI_STATUS Status{};

			try
			{
				Status = Current.Instance->Start(
					Path
				);
			}
			catch (...)
			{
				Status = RecordException();
			}

			if (!EFI_ERROR(Status))
			{
				Current.Started = true;
				return Status;
			}

			EFI_STATUS Rollback{};

			try
			{
				Rollback = Current.Instance->Stop();
			}
			catch (...)
			{
				Rollback = RecordException();
			}
			if (EFI_ERROR(Rollback))
			{
				return Rollback;
			}

			Controllers.pop_back();

			return Status;
		}

		auto Stop(
			EFI_HANDLE Handle,
			std::span<const EFI_HANDLE> Children
		) -> EFI_STATUS
		{
			const auto Found = Find(
				Handle
			);

			if (Found == Controllers.end())
			{
				return EFI_NOT_STARTED;
			}

			if (!Children.empty())
			{
				return Found->Instance->StopChildren(
					Children
				);
			}

			const auto Status = Found->Instance->Stop();

			if (!EFI_ERROR(Status))
			{
				Controllers.erase(
					Found
				);
			}

			return Status;
		}

		static auto EFIAPI SupportedThunk(
			EFI_DRIVER_BINDING_PROTOCOL* This,
			EFI_HANDLE Handle,
			EFI_DEVICE_PATH_PROTOCOL* Path
		) noexcept -> EFI_STATUS
		{
			return UefiCpp::FirmwareBoundary(
				[=]() noexcept -> EFI_STATUS
				{
					if (!This || !Handle)
					{
						return EFI_INVALID_PARAMETER;
					}

					auto& Self = *reinterpret_cast<Envelope*>(This)->Owner;

					return Self.Dispatch(
						[&]() -> EFI_STATUS
						{
							if (Self.Closing)
							{
								return EFI_NOT_READY;
							}

							return Self.Implementation->Supported(
								This->DriverBindingHandle,
								Handle,
								Path
							);
						}
					);
				}
			);
		}

		static auto EFIAPI StartThunk(
			EFI_DRIVER_BINDING_PROTOCOL* This,
			EFI_HANDLE Handle,
			EFI_DEVICE_PATH_PROTOCOL* Path
		) noexcept -> EFI_STATUS
		{
			return UefiCpp::FirmwareBoundary(
				[=]() noexcept -> EFI_STATUS
				{
					if (!This || !Handle)
					{
						return EFI_INVALID_PARAMETER;
					}

					auto& Self = *reinterpret_cast<Envelope*>(This)->Owner;

					return Self.Dispatch(
						[&]() -> EFI_STATUS
						{
							return Self.Start(
								Handle,
								Path
							);
						}
					);
				}
			);
		}

		static auto EFIAPI StopThunk(
			EFI_DRIVER_BINDING_PROTOCOL* This,
			EFI_HANDLE Handle,
			UINTN Count,
			EFI_HANDLE* Children
		) noexcept -> EFI_STATUS
		{
			return UefiCpp::FirmwareBoundary(
				[=]() noexcept -> EFI_STATUS
				{
					if (!This || !Handle || (Count && !Children))
					{
						return EFI_INVALID_PARAMETER;
					}

					auto& Self = *reinterpret_cast<Envelope*>(This)->Owner;

					return Self.Dispatch(
						[&]() -> EFI_STATUS
						{
							return Self.Stop(
								Handle,
								{
									Children,
									Count
								}
							);
						}
					);
				}
			);
		}

		auto Close() -> void
		{
			if (CallbackActive.test(std::memory_order_acquire))
			{
				throw std::logic_error(
					"Cannot close a driver inside an active binding callback"
				);
			}

			Closing = true;

			while (!Controllers.empty())
			{
				const auto Handle = Controllers.back().Handle;
				const auto WasStarted = Controllers.back().Started;

				const auto Status = FirmwareStatus(
					UefiCpp::Firmware::TryCall(
						[&](
							EFI_BOOT_SERVICES& BootServices
						) noexcept -> EFI_STATUS
						{
							return BootServices.DisconnectController(
								Handle,
								Published.Interface.DriverBindingHandle,
								nullptr
							);
						}
					),
					"DisconnectController"
				);

				if (EFI_ERROR(Status) && (WasStarted || Status != EFI_NOT_FOUND))
				{
					RequireSuccess(
						Status,
						"DisconnectController"
					);
				}

				if (Find(Handle) != Controllers.end())
				{
					RequireSuccess(
						Dispatch(
							[&]() -> EFI_STATUS
							{
								return Stop(
									Handle,
									{}
								);
							}
						),
						"Driver Stop during unload"
					);
				}
			}

			if (Installed)
			{
				EFI_GUID Guid = EFI_DRIVER_BINDING_PROTOCOL_GUID;

				RequireSuccess(
					UefiCpp::Firmware::TryCall(
						[&](
							EFI_BOOT_SERVICES& BootServices
						) noexcept -> EFI_STATUS
						{
							return BootServices.UninstallProtocolInterface(
								Published.Interface.DriverBindingHandle,
								&Guid,
								&Published.Interface
							);
						}
					),
					"Uninstall DriverBinding"
				);
				Installed = false;
			}
			if (UnloadInstalled)
			{
				EFI_GUID Guid = UnloadGuid;
				EFI_GUID LoadedGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

				RequireSuccess(
					UefiCpp::Firmware::TryCall(
						[&](
							EFI_BOOT_SERVICES& BootServices
						) noexcept -> EFI_STATUS
						{
							void* Raw{};

							const auto Status = BootServices.HandleProtocol(
								Image,
								&LoadedGuid,
								&Raw
							);

							if (EFI_ERROR(Status) || !Raw)
							{
								return EFI_ERROR(Status) ? Status : EFI_COMPROMISED_DATA;
							}
							 
							auto* Loaded = static_cast<EFI_LOADED_IMAGE_PROTOCOL*>(Raw);

							const auto Removed = BootServices.UninstallProtocolInterface(
								Image,
								&Guid,
								this
							);

							if (!EFI_ERROR(Removed) && Loaded->Unload == &UnloadThunk)
							{
								Loaded->Unload = nullptr;
							}

							return Removed;
						}
					),
					"Remove driver unload anchor"
				);

				UnloadInstalled = false;
			}
		}

		static auto EFIAPI UnloadThunk(
			EFI_HANDLE Image
		) noexcept -> EFI_STATUS
		{
			return UefiCpp::FirmwareBoundary(
				[Image]() noexcept -> EFI_STATUS
				{
					try
					{
						EFI_GUID Guid = UnloadGuid;
						void* Raw{};

						const auto Status = FirmwareStatus(
							UefiCpp::Firmware::TryCall(
								[&](
									EFI_BOOT_SERVICES& BootServices
								) noexcept -> EFI_STATUS
								{
									return BootServices.HandleProtocol(
										Image,
										&Guid,
										&Raw
									);
								}
							),
							"Find driver unload anchor"
						);

						if (Status == EFI_UNSUPPORTED)
						{
							return EFI_SUCCESS;
						}

						RequireSuccess(
							Status,
							"Find driver unload anchor"
						);

						if (!Raw)
						{
							ContractViolation(
								"Null driver unload anchor"
							);
						}

						auto& Self = *static_cast<State*>(Raw);

						try
						{
							Self.Close();
							return EFI_SUCCESS;
						}
						catch (...)
						{
							return Self.RecordException();
						}
					}
					catch (...)
					{
						return CurrentExceptionStatus();
					}
				}
			);
		}
	};

	Registration::Registration(
		EFI_HANDLE Image,
		std::unique_ptr<Binding> Implementation,
		UINT32 Version,
		EFI_HANDLE BindingHandle
	) : Context(std::make_unique<State>())
	{
		if (!Image || !Implementation)
		{
			throw std::invalid_argument(
				"Driver registration requires an image and implementation"
			);
		}

		Context->Image = Image;

		Context->Implementation = std::move(
			Implementation
		);

		Context->Published.Owner = Context.get();

		Context->Published.Interface =
		{
			&State::SupportedThunk,
			&State::StartThunk,
			&State::StopThunk,
			Version,
			Image,
			BindingHandle ? BindingHandle : Image
		};

		EFI_GUID Guid = EFI_DRIVER_BINDING_PROTOCOL_GUID;
		auto Handle = Context->Published.Interface.DriverBindingHandle;

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					return BootServices.InstallProtocolInterface(
						&Handle,
						&Guid,
						EFI_NATIVE_INTERFACE,
						&Context->Published.Interface
					);
				}
			),
			"Install DriverBinding"
		);

		Context->Installed = true;

		if (Handle != Context->Published.Interface.DriverBindingHandle)
		{
			FailCleanup(
				{
					ErrorStage::Contract,
					EFI_COMPROMISED_DATA,
					"DriverBinding handle changed"
				}
			);
		}
	}

	Registration::~Registration() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"Driver registration cleanup"
			);
		}
	}

	Registration::Registration(
		Registration&& Other
	) noexcept : Context(std::move(Other.Context))
	{
	}

	auto Registration::AttachUnload() -> void
	{
		if (!Context || !Context->Installed || Context->Closing)
		{
			throw std::logic_error(
				"Driver registration is closed"
			);
		}

		if (Context->UnloadInstalled)
		{
			return;
		}

		EFI_GUID LoadedGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
		EFI_GUID Guid = State::UnloadGuid;

		RequireSuccess(
			UefiCpp::Firmware::TryCall(
				[&](
					EFI_BOOT_SERVICES& BootServices
				) noexcept -> EFI_STATUS
				{
					void* Raw{};

					const auto Status = BootServices.HandleProtocol(
						Context->Image,
						&LoadedGuid,
						&Raw
					);

					if (EFI_ERROR(Status) || !Raw)
					{
						return EFI_ERROR(Status) ? Status : EFI_COMPROMISED_DATA;
					}

					auto* Loaded = static_cast<EFI_LOADED_IMAGE_PROTOCOL*>(Raw);

					if (Loaded->Unload)
					{
						return EFI_ALREADY_STARTED;
					}

					auto Image = Context->Image;

					const auto Installed = BootServices.InstallProtocolInterface(
						&Image,
						&Guid,
						EFI_NATIVE_INTERFACE,
						Context.get()
					);

					if (!EFI_ERROR(Installed))
					{
						Loaded->Unload = &State::UnloadThunk;
					}

					return Installed;
				}
			),
			"Attach driver unload"
		);

		Context->UnloadInstalled = true;
	}

	auto Registration::Close() -> void
	{
		if (Context)
		{
			Context->Close();
		}
	}

	auto Registration::Handle() const noexcept -> EFI_HANDLE
	{
		return Context && Context->Installed ? Context->Published.Interface.DriverBindingHandle : nullptr;
	}

	auto Registration::ControllerCount() const noexcept -> std::size_t
	{
		return Context ? Context->Controllers.size() : 0;
	}

	auto Registration::LastFailure() const -> std::optional<FirmwareError>
	{
		return Context ? Context->Failure : std::nullopt;
	}
}
