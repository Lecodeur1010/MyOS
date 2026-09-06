#pragma once
#include <efi.h>
#include <efilib.h>
EFI_STATUS GetAdresses(EFI_IPv4_ADDRESS *Device, EFI_IPv4_ADDRESS *Mask, EFI_IPv4_ADDRESS *Gateway, EFI_IPv4_ADDRESS **DNS, UINTN* DNSCount);
BOOLEAN ParseIPv4(CONST CHAR16 *Str, UINT8 Addr[4]);
EFI_STATUS InitNet();
EFI_STATUS SendTCPRequest(UINTN* Time, EFI_IPv4_ADDRESS* Address, UINT16 Port, VOID* Payload, UINTN PayloadSize, VOID** Response, UINTN* ResponseSize);
EFI_STATUS ResolveHostName(IN CHAR16* HostName, OUT EFI_IPv4_ADDRESS* TargetIp);