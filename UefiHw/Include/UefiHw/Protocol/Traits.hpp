#pragma once

#include <Uefi.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/SimpleFileSystem.h>

#include <concepts>

namespace UefiHw::Protocol
{
	template <typename T> struct Traits;

	template <> struct Traits<EFI_GRAPHICS_OUTPUT_PROTOCOL>
	{
		static inline constexpr EFI_GUID Guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	};

	template <> struct Traits<EFI_SIMPLE_FILE_SYSTEM_PROTOCOL>
	{
		static inline constexpr EFI_GUID Guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	};

	template <> struct Traits<EFI_PCI_IO_PROTOCOL>
	{
		static inline constexpr EFI_GUID Guid = EFI_PCI_IO_PROTOCOL_GUID;
	};

	template <typename T>
	concept RegisteredProtocol = requires { { Traits<T>::Guid } -> std::same_as<const EFI_GUID&>; };
}
