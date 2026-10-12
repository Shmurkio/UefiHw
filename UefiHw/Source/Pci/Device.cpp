#include <UefiHw/Pci/Device.hpp>
#include <UefiHw/Pci/Attributes.hpp>
#include <UefiHw/Io/Bar.hpp>
#include "State.hpp"
#include <utility>

namespace UefiHw::Pci
{
	Device::Device(
		ConfigurationSpace Space
	) : Context(std::make_shared<Detail::DeviceState>())
	{
		Detail::ValidateSpace(
			Space
		);

		Context->Space = Space;
	}

	Device::Device(
		EFI_HANDLE Controller,
		EFI_HANDLE Agent,
		ConfigurationSpace Space
	) : Device(Space)
	{
		Context->Relationship.emplace(
			Protocol::OpenByDriver<EFI_PCI_IO_PROTOCOL>(Controller, Agent, Controller)
		);
	}

	auto Device::TakeOver(
		EFI_HANDLE Controller,
		EFI_HANDLE Agent,
		ConfigurationSpace Space
	) -> Device
	{
		Device Result(
			Space
		);

		Result.Context->Relationship.emplace(
			Protocol::TakeOver<EFI_PCI_IO_PROTOCOL>(Controller, Agent)
		);

		return Result;
	}

	Device::~Device() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"PCI device cleanup"
			);
		}
	}

	Device::Device(
		Device&& Other
	) noexcept : Context(std::move(Other.Context))
	{
	}

	auto Device::IsOpen() const noexcept -> bool
	{
		return Context && Context->IsOpen();
	}

	auto Device::Close() -> void
	{
		if (!Context || !Context->Relationship)
		{
			return;
		}

		if (Context->Buffers || Context->AttributeOwner)
		{
			throw std::logic_error(
				"PCI device still owns DMA buffers or attribute scopes"
			);
		}

		std::visit(
			[](
				auto& Open
			)
			{
				Open.Close();
			},
			*Context->Relationship
		);

		Context->Relationship.reset();
	}

	auto Device::AccessConfiguration(
		bool Write,
		UINT32 Offset,
		std::size_t Width,
		void* Value
	) const -> void
	{
		if (!Context)
		{
			throw std::logic_error(
				"Moved PCI device"
			);
		}

		Detail::Configuration(
			Context->Interface(),
			Context->Space,
			Write,
			Offset,
			Width,
			Value
		);
	}

	auto Device::GetLocation() const -> Location
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		return Detail::GetLocation(
			Context->Interface()
		);
	}

	auto Device::GetIdentity() const -> Identity
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		return Detail::GetIdentity(
			Context->Interface()
		);
	}

	auto Device::Capabilities() const -> std::vector<Capability>
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		return Detail::Capabilities(
			Context->Interface(),
			Context->Space,
			false
		);
	}

	auto Device::ExtendedCapabilities() const -> std::vector<Capability>
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		return Detail::Capabilities(
			Context->Interface(),
			Context->Space,
			true
		);
	}

	auto Device::Bars() const -> std::vector<BarInfo>
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		return Detail::Bars(
			Context->Interface()
		);
	}

	auto Device::Registers(
		UINT8 Index
	) const -> Io::Bar
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		if (!Detail::GetBar(Context->Interface(), Index))
		{
			throw FirmwareError
			{
				ErrorStage::Firmware,
				EFI_UNSUPPORTED,
				"PCI BAR unavailable"
			};
		}
		return Io::Bar
		{
			Context,
			Index
		};
	}

	auto Device::Attributes() const -> UINT64
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		return Detail::Attributes(
			Context->Interface(),
			EfiPciIoAttributeOperationGet
		);
	}

	auto Device::SupportedAttributes() const -> UINT64
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		return Detail::Attributes(
			Context->Interface(),
			EfiPciIoAttributeOperationSupported
		);
	}

	auto Device::Flush() const -> void
	{
		if (!IsOpen())
		{
			throw std::logic_error(
				"Closed PCI device"
			);
		}

		Detail::Flush(
			Context->Interface()
		);
	}

	AttributeScope::AttributeScope(
		Device& Owner,
		UINT64 Enable,
		UINT64 Disable
	) : Context(Owner.Context), Mask(Enable | Disable)
	{
		if (!Context || !Owner.IsOpen() || Context->AttributeOwner)
		{
			throw std::logic_error(
				"Attribute scope needs an open device without another scope"
			);
		}

		if ((Enable & Disable) || !Mask)
		{
			throw std::invalid_argument(
				"Invalid attribute masks"
			);
		}

		auto& Interface = Context->Interface();

		const auto Supported = Detail::Attributes(
			Interface,
			EfiPciIoAttributeOperationSupported
		);

		if (Mask & ~Supported)
		{
			throw FirmwareError
			{
				ErrorStage::Firmware,
				EFI_UNSUPPORTED,
				"Unsupported PCI attributes"
			};
		}

		Original = Detail::Attributes(
			Interface,
			EfiPciIoAttributeOperationGet
		);

		Active = true;
		Context->AttributeOwner = true;

		try
		{
			[[maybe_unused]]
			const auto Status = Detail::Attributes(
				Interface,
				EfiPciIoAttributeOperationSet,
				(Original | Enable) & ~Disable
			);
		}
		catch (...)
		{
			try
			{
				Close();
			}
			catch (...)
			{
				FailCurrentCleanup(
					"PCI attributes rollback"
				);
			}

			throw;
		}
	}

	AttributeScope::~AttributeScope() noexcept
	{
		try
		{
			Close();
		}
		catch (...)
		{
			FailCurrentCleanup(
				"PCI attributes restoration"
			);
		}
	}

	AttributeScope::AttributeScope(
		AttributeScope&& Other
	) noexcept : Context(std::move(Other.Context)), Original(Other.Original), Mask(Other.Mask), Active(std::exchange(Other.Active, false))
	{
	}

	auto AttributeScope::Update(
		UINT64 Enable,
		UINT64 Disable
	) -> void
	{
		if (!Active)
		{
			throw std::logic_error(
				"Closed PCI attribute scope"
			);
		}

		if ((Enable & Disable) || ((Enable | Disable) & ~Mask))
		{
			throw std::invalid_argument(
				"PCI attribute update exceeds scope"
			);
		}

		auto& Interface = Context->Interface();

		const auto Current = Detail::Attributes(
			Interface,
			EfiPciIoAttributeOperationGet
		);

		[[maybe_unused]]
		const auto Status = Detail::Attributes(
			Interface,
			EfiPciIoAttributeOperationSet,
			(Current | Enable) & ~Disable
		);
	}

	auto AttributeScope::Close() -> void
	{
		if (!Active)
		{
			return;
		}

		auto& Interface = Context->Interface();

		const auto Current = Detail::Attributes(
			Interface,
			EfiPciIoAttributeOperationGet
		);

		[[maybe_unused]]
		const auto Status = Detail::Attributes(
			Interface,
			EfiPciIoAttributeOperationSet,
			(Current & ~Mask) | (Original & Mask)
		);

		Active = false;
		Context->AttributeOwner = false;
		Context.reset();
	}
}
