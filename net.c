#include <efi.h>
#include <efilib.h>
#include "net.h"
#include "func.h"
#include "display.h"
#include "memory.h"
#include "uefi_headers.h"

EFI_SERVICE_BINDING *TCP4ServiceBinding = NULL;
EFI_SERVICE_BINDING *Dns4ServiceBinding = NULL;
EFI_DNS4_PROTOCOL    *gDns4               = NULL;
EFI_HANDLE           gDns4ChildHandle     = NULL;
BOOLEAN Ready = FALSE;
static EFI_IPv4_ADDRESS DeviceAdress;
static EFI_IPv4_ADDRESS SubnetMask;
static EFI_IPv4_ADDRESS GatewayAdress;
static EFI_IPv4_ADDRESS DNSAdress[2]; //Max 
static UINTN DnsCount = 2;
EFI_DHCP4_MODE_DATA ModeData;

EFI_STATUS GetAdresses(EFI_IPv4_ADDRESS *Device, EFI_IPv4_ADDRESS *Mask, EFI_IPv4_ADDRESS *Gateway, EFI_IPv4_ADDRESS **DNS, UINTN* DNSCount) {
    if (!Ready) return EFI_NOT_READY;
    if (Device)   *Device   = DeviceAdress;
    if (Mask)     *Mask     = SubnetMask;
    if (Gateway)  *Gateway  = GatewayAdress;
    if (DNS)      *DNS      = DNSAdress;
    if (DNSCount) *DNSCount = DnsCount;
    return EFI_SUCCESS;
}


BOOLEAN ParseIPv4(CONST CHAR16 *Str, UINT8 Addr[4]) {
    if (!Str || !Addr) return FALSE;

    UINTN seg = 0;
    UINTN val = 0;
    BOOLEAN has_digits = FALSE;

    while (*Str != L'\0') {
        CHAR16 c = *Str;

        if (c >= L'0' && c <= L'9') {
            val = val * 10 + (c - L'0');
            if (val > 255) return FALSE; // Segment > 255 invalide
            has_digits = TRUE;
        } else if (c == L'.') {
            if (!has_digits || seg >= 3) return FALSE; // Syntaxe invalide ou trop de points
            Addr[seg++] = (UINT8)val;
            val = 0;
            has_digits = FALSE;
        } else {
            return FALSE; // Caractère non numérique invalide
        }
        Str++;
    }

    if (!has_digits || seg != 3) return FALSE; // Doit finir par un nombre et exactement 4 octets
    Addr[seg] = (UINT8)val;

    return TRUE;
}

EFI_STATUS GetDefaultDnsServer(OUT EFI_IPv4_ADDRESS *DnsServerIp) {
    EFI_STATUS Status;
    EFI_IP4_CONFIG2_PROTOCOL *Ip4Config2 = NULL;
    EFI_GUID Ip4Config2Guid = EFI_IP4_CONFIG2_PROTOCOL_GUID;

    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &Ip4Config2Guid, NULL, (VOID**)&Ip4Config2);
    if (EFI_ERROR(Status)) 
        return Status;
    
    UINTN DataSize = 0;
    Status = uefi_call_wrapper(
        Ip4Config2->GetData, 4,
        Ip4Config2,
        Ip4Config2DataTypeDnsServer,
        &DataSize,
        NULL
    );

    if (Status != EFI_BUFFER_TOO_SMALL || DataSize == 0)
        return EFI_NOT_FOUND;

    EFI_IPv4_ADDRESS *DnsList = NULL;
    DnsList = kmalloc(DataSize);
    if (!DnsList) return EFI_OUT_OF_RESOURCES;

    Status = uefi_call_wrapper(
        Ip4Config2->GetData, 4,
        Ip4Config2,
        Ip4Config2DataTypeDnsServer,
        &DataSize,
        DnsList
    );

    if (!EFI_ERROR(Status))
        CopyMem(DnsServerIp, &DnsList[0], sizeof(EFI_IPv4_ADDRESS));

    uefi_call_wrapper(BS->FreePool, 1, DnsList);
    return Status;
}

EFI_STATUS ResolveHostName(IN CHAR16* HostName, OUT EFI_IPv4_ADDRESS* TargetIp) {
    if(!Ready) return EFI_NOT_READY;
    if (!gDns4) return EFI_NOT_READY;

    EFI_STATUS Status;
    EFI_DNS4_COMPLETION_TOKEN Token;
    ZeroMem(&Token, sizeof(EFI_DNS4_COMPLETION_TOKEN));

    // Création de l'événement de synchronisation
    Status = uefi_call_wrapper(BS->CreateEvent, 5, 0, TPL_NOTIFY, NULL, NULL, &Token.Event);
    if (EFI_ERROR(Status)) return Status;

    // Requête DNS (Utilise le cache global)
    Status = uefi_call_wrapper(gDns4->HostNameToIp, 3, gDns4, HostName, &Token);
    if (!EFI_ERROR(Status)) {
        UINTN Index;
        uefi_call_wrapper(BS->WaitForEvent, 3, 1, &Token.Event, &Index);
    }
    
    uefi_call_wrapper(BS->CloseEvent, 1, Token.Event);

    if (EFI_ERROR(Status) || EFI_ERROR(Token.Status)) {
        return EFI_ERROR(Status) ? Status : Token.Status;
    }

    // Récupération de l'adresse
    if (Token.RspData.H2AData != NULL && Token.RspData.H2AData->IpCount > 0) {
        CopyMem(TargetIp, &Token.RspData.H2AData->IpList[0], sizeof(EFI_IPv4_ADDRESS));
        
        uefi_call_wrapper(BS->FreePool, 1, Token.RspData.H2AData->IpList);
        uefi_call_wrapper(BS->FreePool, 1, Token.RspData.H2AData);
        return EFI_SUCCESS;
    }

    return EFI_NOT_FOUND;
}

EFI_STATUS StopAllDhcpInstances(VOID) {
    EFI_STATUS status;
    EFI_GUID Dhcp4Guid = EFI_DHCP4_PROTOCOL_GUID;
    UINTN HandleCount = 0;
    EFI_HANDLE *HandleBuffer = NULL;
    // 1. Récupérer toutes les instances DHCP4 existantes
    status = uefi_call_wrapper(BS->LocateHandleBuffer, 5,ByProtocol,&Dhcp4Guid,NULL,&HandleCount,&HandleBuffer);
    if (EFI_ERROR(status))
        return status;
    for (UINTN i = 0; i < HandleCount; i++) {
        EFI_DHCP4_PROTOCOL *Dhcp4 = NULL;
        // 2. Ouvrir le protocole pour agir dessus
        status = uefi_call_wrapper(BS->OpenProtocol, 6,HandleBuffer[i],&Dhcp4Guid,(VOID**)&Dhcp4,gImageHandle,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(status))
            continue;
        // 3. Stopper la machine a etats (interrompt Dhcp4Selecting, Dhcp4Requesting, etc.)
        status = uefi_call_wrapper(Dhcp4->Stop, 1, Dhcp4);
        // 4. Purger la configuration pour repasser imperativement a Dhcp4Stopped
        uefi_call_wrapper(Dhcp4->Configure, 2, Dhcp4, NULL);
        // Fermer le protocole
        uefi_call_wrapper(BS->CloseProtocol, 4, HandleBuffer[i], &Dhcp4Guid, gImageHandle, NULL);
    }
    // Libérer la mémoire du tampon de handles
    if (HandleBuffer != NULL) 
        uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);
    return EFI_SUCCESS;
}

EFI_STATUS ListDhcpInstances(VOID) {
    EFI_STATUS status;
    EFI_GUID Dhcp4Guid = EFI_DHCP4_PROTOCOL_GUID;
    UINTN HandleCount = 0;
    EFI_HANDLE *HandleBuffer = NULL;
    // 1. Lister tous les handles qui exposent le protocole DHCP4
    status = uefi_call_wrapper(BS->LocateHandleBuffer, 5,ByProtocol,&Dhcp4Guid,NULL,&HandleCount,&HandleBuffer);
    if (EFI_ERROR(status))
        return status;
    for (UINTN i = 0; i < HandleCount; i++) {
        EFI_DHCP4_PROTOCOL *Dhcp4 = NULL;
        // 2. Ouvrir le protocole en mode GET_PROTOCOL (simple lecture)
        status = uefi_call_wrapper(BS->OpenProtocol, 6,HandleBuffer[i],&Dhcp4Guid,(VOID**)&Dhcp4,gImageHandle,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(status))
            continue;
        // 3. Récupérer les informations de mode/état
        ZeroMem(&ModeData, sizeof(EFI_DHCP4_MODE_DATA));
        // Fermer le protocole après consultation
        uefi_call_wrapper(BS->CloseProtocol, 4, HandleBuffer[i], &Dhcp4Guid, gImageHandle, NULL);
    }
    // Libérer le tampon de handles
    if (HandleBuffer != NULL) 
        uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);
    StopAllDhcpInstances();
    return EFI_SUCCESS;
}

EFI_DHCP4_PACKET_OPTION* GetDhcpOption(EFI_DHCP4_PACKET *Packet, UINT8 OptionCode) {
    UINT32 Offset = 0;
    // Les options DHCP commencent après le Magic Cookie (4 octets)
    UINT8 *Options = Packet->Dhcp4.Option;
    UINT32 Length = Packet->Length - sizeof(EFI_DHCP4_HEADER) - 4;

    while (Offset < Length) {
        UINT8 Code = Options[Offset];
        if (Code == 255) break; // Pad/End option
        if (Code == 0) { Offset++; continue; } // Padding

        UINT8 OptionLen = Options[Offset + 1];
        if (Code == OptionCode) {
            return (EFI_DHCP4_PACKET_OPTION*)&Options[Offset];
        }
        Offset += 2 + OptionLen;
    }
    return NULL;
}

BOOLEAN IsMediaPresent(EFI_HANDLE ControllerHandle) {
    EFI_STATUS Status;
    EFI_SIMPLE_NETWORK_PROTOCOL *Snp = NULL;
    EFI_GUID SnpGuid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;

    // Ouvrir le protocole Simple Network sur le contrôleur
    Status = uefi_call_wrapper(
        BS->OpenProtocol, 6,
        ControllerHandle, &SnpGuid,
        (VOID**)&Snp, gImageHandle, NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );

    if (EFI_ERROR(Status) || Snp == NULL || Snp->Mode == NULL) {
        return FALSE;
    }

    // Si la carte supporte le MediaDetect, rafraîchir son état
    if (Snp->Mode->MediaPresentSupported) {
        // Optionnel : un GetStatus rafraîchit l'état auprès du contrôleur hardware
        UINT32 InterruptStatus = 0;
        VOID *TxBuf = NULL;
        uefi_call_wrapper(Snp->GetStatus, 3, Snp, &InterruptStatus, &TxBuf);
    }

    return Snp->Mode->MediaPresent;
}

EFI_STATUS InitDHCP(EFI_HANDLE *OutControllerHandle) {
    ListDhcpInstances();
    EFI_STATUS status;

    EFI_GUID Dhcp4ServiceBindingGuid = EFI_DHCP4_SERVICE_BINDING_PROTOCOL_GUID;
    EFI_GUID Dhcp4Guid               = EFI_DHCP4_PROTOCOL_GUID;
    EFI_GUID Ip4Config2Guid          = EFI_IP4_CONFIG2_PROTOCOL_GUID;

    EFI_SERVICE_BINDING *Dhcp4SB = NULL;
    EFI_HANDLE DhcpChildHandle = NULL;
    EFI_DHCP4_PROTOCOL *Dhcp4 = NULL;
    EFI_IP4_CONFIG2_PROTOCOL *Ip4Config2 = NULL;

    // 1. Énumérer TOUS les ServiceBinding DHCP4
    UINTN HandleCount = 0;
    EFI_HANDLE *HandleBuffer = NULL;

    status = uefi_call_wrapper(
        BS->LocateHandleBuffer, 5,
        ByProtocol, &Dhcp4ServiceBindingGuid, NULL,
        &HandleCount, &HandleBuffer
    );

    if (EFI_ERROR(status) || HandleCount == 0)
        return status;

    EFI_HANDLE SelectedController = NULL;

    // 2. Chercher la carte avec le câble branché
    for (UINTN i = 0; i < HandleCount; i++) {
        if (IsMediaPresent(HandleBuffer[i])) {
            SelectedController = HandleBuffer[i];
            break;
        }
    }

    // Fallback si pas de détection
    if (SelectedController == NULL)
        SelectedController = HandleBuffer[0];

    // On retourne le handle choisi au niveau supérieur
    if (OutControllerHandle != NULL) {
        *OutControllerHandle = SelectedController;
    }

    // 3. Ouvrir le Service Binding sur CE contrôleur
    status = uefi_call_wrapper(
        BS->OpenProtocol, 6,
        SelectedController, &Dhcp4ServiceBindingGuid,
        (VOID**)&Dhcp4SB, gImageHandle, NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );

    if (HandleBuffer) {
        uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);
    }

    CHECK_STATUS(status,L"Can't open Dhcp4ServiceBinding : %r\n",TRUE,NOP,_s);

    // 3. Créer l'instance enfant
    status = uefi_call_wrapper(Dhcp4SB->CreateChild, 2, Dhcp4SB, &DhcpChildHandle);
    CHECK_STATUS(status,L"Can't create child DHCP : %r\n",TRUE,NOP,_s);

    // 3. Ouvrir le protocole DHCP4
    status = uefi_call_wrapper(
        BS->OpenProtocol, 6,
        DhcpChildHandle, &Dhcp4Guid,
        (VOID**)&Dhcp4, gImageHandle, NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    CHECK_STATUS(status,L"Can't open DHCP protocol : %r\n",TRUE,NOP,_s);
    // 4. Stopper et réinitialiser l'instance
    uefi_call_wrapper(Dhcp4->Stop, 1, Dhcp4);
    uefi_call_wrapper(Dhcp4->Configure, 2, Dhcp4, NULL);

    // 5. Configurer les paramètres DHCP
    EFI_DHCP4_CONFIG_DATA DhcpConfig;
    ZeroMem(&DhcpConfig, sizeof(EFI_DHCP4_CONFIG_DATA));

    UINT32 DiscoverTimeouts[3] = { 1, 2, 4 };
    UINT32 RequestTimeouts[3]  = { 1, 2, 4 };

    DhcpConfig.DiscoverTryCount = 3;
    DhcpConfig.DiscoverTimeout  = DiscoverTimeouts;
    DhcpConfig.RequestTryCount  = 3;
    DhcpConfig.RequestTimeout   = RequestTimeouts;
    DhcpConfig.ClientAddress.Addr[0] = 0; // 0.0.0.0
    UINT8 RequestedOptions[] = { 1, 3, 6 }; // 1=Subnet, 3=Router, 6=DNS
    
    EFI_DHCP4_PACKET_OPTION Option55;
    Option55.OpCode = 55;
    Option55.Length = sizeof(RequestedOptions);
    CopyMem(Option55.Data, RequestedOptions, sizeof(RequestedOptions));

    EFI_DHCP4_PACKET_OPTION *OptionList[1];
    OptionList[0] = &Option55;

    DhcpConfig.OptionCount = 1;
    DhcpConfig.OptionList  = OptionList;

    status = uefi_call_wrapper(Dhcp4->Configure, 2, Dhcp4, &DhcpConfig);
    CHECK_STATUS(status,L"Can't configure DHCP : %r\n",TRUE,NOP,_s);

    // 6. Démarrer la négociation DHCP
    status = uefi_call_wrapper(Dhcp4->Start, 2, Dhcp4, NULL);
    CHECK_STATUS(status,L"Can't start DHCP : %r\n",TRUE,NOP,_s);

    // 7. Vérification de l'IP obtenue
    ZeroMem(&ModeData, sizeof(EFI_DHCP4_MODE_DATA));
    status = uefi_call_wrapper(Dhcp4->GetModeData, 2, Dhcp4, &ModeData);

    if (!EFI_ERROR(status) && ModeData.State == Dhcp4Bound) {
        GatewayAdress=ModeData.RouterAddress;
        DeviceAdress=ModeData.ClientAddress;
        SubnetMask=ModeData.SubnetMask;
        // 8. Trouver Ip4Config2
        status = uefi_call_wrapper(BS->LocateProtocol, 3, &Ip4Config2Guid, NULL, (VOID**)&Ip4Config2);
        CHECK_STATUS(status, L"Can't find Ip4Config2Protocol : %r\n",TRUE,NOP,_s);

        // A. Basculer la politique de l'interface en Mode Statique
        EFI_IP4_CONFIG2_POLICY Policy = Ip4Config2PolicyStatic;
        status = uefi_call_wrapper(
            Ip4Config2->SetData, 4,
            Ip4Config2,
            Ip4Config2DataTypePolicy,
            sizeof(EFI_IP4_CONFIG2_POLICY),
            &Policy
        );

        CHECK_STATUS(status, L"Can't load Ip4Config2Protocol policy : %r\n",TRUE,NOP,_s);

        // B. Injecter la passerelle
        if (ModeData.RouterAddress.Addr[0] != 0) {
            EFI_IPv4_ADDRESS Gateway = ModeData.RouterAddress;
            status = uefi_call_wrapper(
                Ip4Config2->SetData, 4,
                Ip4Config2,
                Ip4Config2DataTypeGateway,
                sizeof(EFI_IPv4_ADDRESS),
                &Gateway
            );
            if (EFI_ERROR(status)) {
                CPrint(ActualConfig.Theme.Warning,L"Couldn't assign the gateway : %r\n", status);
            }
        }

        // C. Injecter l'adresse IP et le masque
        EFI_IP4_CONFIG2_MANUAL_ADDRESS ManualAddr;
        ZeroMem(&ManualAddr, sizeof(EFI_IP4_CONFIG2_MANUAL_ADDRESS));
        CopyMem(&ManualAddr.Address, &ModeData.ClientAddress, sizeof(EFI_IPv4_ADDRESS));
        CopyMem(&ManualAddr.SubnetMask, &ModeData.SubnetMask, sizeof(EFI_IPv4_ADDRESS));

        status = uefi_call_wrapper(
            Ip4Config2->SetData, 4,
            Ip4Config2,
            Ip4Config2DataTypeManualAddress,
            sizeof(EFI_IP4_CONFIG2_MANUAL_ADDRESS),
            &ManualAddr
        );

        CHECK_STATUS(status,L"Can't assign IP : %r\n",TRUE,NOP,_s);

        // D. Extraire et injecter le serveur DNS (DHCP Option 6)
        EFI_DHCP4_PACKET_OPTION *DnsOpt = GetDhcpOption(ModeData.ReplyPacket, 6);
        EFI_IPv4_ADDRESS DnsIp;

        if (DnsOpt != NULL && DnsOpt->Length >= 4)
            CopyMem(&DnsIp, DnsOpt->Data, sizeof(EFI_IPv4_ADDRESS));
        else 
            DnsIp = (EFI_IPv4_ADDRESS){{10, 0, 2, 3}};

        // On enregistre l'IP (trouvée ou fallback) dans Ip4Config2
        status = uefi_call_wrapper(
            Ip4Config2->SetData, 4,
            Ip4Config2,
            Ip4Config2DataTypeDnsServer,
            sizeof(EFI_IPv4_ADDRESS),
            &DnsIp
        );

    }

    return status;
}

EFI_STATUS InitDNS(EFI_HANDLE ControllerHandle) {
    EFI_STATUS status;
    EFI_HANDLE DnsChildHandle = NULL;
    EFI_GUID Dns4ServiceBindingGuid = EFI_DNS4_SERVICE_BINDING_PROTOCOL_GUID;
    EFI_GUID Dns4Guid = EFI_DNS4_PROTOCOL_GUID;

    // 1. Ouvrir le Service Binding DNS4 directement sur le contrôleur sélectionné
    status = uefi_call_wrapper(
        BS->OpenProtocol, 6,
        ControllerHandle, &Dns4ServiceBindingGuid,
        (VOID**)&Dns4ServiceBinding, gImageHandle, NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );

    CHECK_STATUS(status,L"Can't get DNS4 binding service : %r\n",TRUE,NOP,_s);

    // 2. Créer l'instance Child du client DNS
    status = uefi_call_wrapper(Dns4ServiceBinding->CreateChild, 2, Dns4ServiceBinding, &DnsChildHandle);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(BS->OpenProtocol, 6, DnsChildHandle, &Dns4Guid, (VOID**)&gDns4, gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status)) return status;

    // 3. Configurer le client DNS
    EFI_DNS4_CONFIG_DATA DnsConfig;
    ZeroMem(&DnsConfig, sizeof(EFI_DNS4_CONFIG_DATA));

    DnsConfig.UseDefaultSetting = TRUE;
    DnsConfig.DnsServerListCount = 2;
    status = GetDefaultDnsServer(DNSAdress);
    if (EFI_ERROR(status)) {
        CPrint(ActualConfig.Theme.Info,L"Can't find DHCP DNS (%r); defaulting to Google's one\n", status);
        DNSAdress[0] = (EFI_IPv4_ADDRESS){{8, 8, 8, 8}};
    }
    DNSAdress[1] = (EFI_IPv4_ADDRESS){{1, 1, 1, 1}};
    DnsConfig.DnsServerList = DNSAdress;
    DnsConfig.EnableDnsCache = TRUE;
    DnsConfig.Protocol = 0x11; // UDP

    status = uefi_call_wrapper(gDns4->Configure, 2, gDns4, &DnsConfig);
    CHECK_STATUS(status, L"Couldn't configure the DNS : %r",TRUE,NOP,_s);

    return EFI_SUCCESS;
}

EFI_STATUS InitNet(VOID) {
    EFI_STATUS status;
    EFI_HANDLE ActiveController = NULL;

    // 1. Initialiser le DHCP et récupérer le handle de la carte active
    status = InitDHCP(&ActiveController);
    CHECK_STATUS(status, L"Couldn't init DHCP : %r",TRUE,NOP,_s);


    status = InitDNS(ActiveController);
    CHECK_STATUS(status, L"Couldn't init DNS : %r",TRUE,NOP,_s);
    Ready = TRUE;
    return EFI_SUCCESS;
} 


EFI_STATUS SendTCPRequest(UINTN* Time, EFI_IPv4_ADDRESS* Address, UINT16 Port, VOID* Payload, UINTN PayloadSize, VOID** Response, UINTN* ResponseSize) {
    EFI_HANDLE ChildHandle = NULL;
    EFI_STATUS status;
    EFI_GUID Tcp4Guid = EFI_TCP4_PROTOCOL;
    EFI_TCP4 *TCP4 = NULL;
    UINTN Timeout = (Time && *Time > 0) ? *Time : 5000; // 5 secondes par défaut pour les gros downloads

    if (Port == 0 || !Address || !Response || !ResponseSize) return EFI_INVALID_PARAMETER;
    
    *Response = NULL;
    *ResponseSize = 0;

    // 1. Création du Child TCP4
    status = uefi_call_wrapper(TCP4ServiceBinding->CreateChild, 2, TCP4ServiceBinding, &ChildHandle);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(BS->OpenProtocol, 6, ChildHandle, &Tcp4Guid, (VOID**)&TCP4, gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(TCP4ServiceBinding->DestroyChild, 2, TCP4ServiceBinding, ChildHandle);
        return status;
    }

    // 2. Configuration du Socket
    EFI_TCP4_CONFIG_DATA Tcp4Config;
    ZeroMem(&Tcp4Config, sizeof(EFI_TCP4_CONFIG_DATA));
    Tcp4Config.TypeOfService = 0;
    Tcp4Config.TimeToLive = 255;
    Tcp4Config.AccessPoint.UseDefaultAddress = TRUE;
    CopyMem(&Tcp4Config.AccessPoint.RemoteAddress, Address, sizeof(EFI_IPv4_ADDRESS));
    Tcp4Config.AccessPoint.RemotePort = Port;
    Tcp4Config.AccessPoint.ActiveFlag = TRUE;

    status = uefi_call_wrapper(TCP4->Configure, 2, TCP4, &Tcp4Config);
    if (EFI_ERROR(status)) goto CleanupAndExit;

    // 3. Connexion TCP
    EFI_TCP4_CONNECTION_TOKEN ConnToken;
    ZeroMem(&ConnToken, sizeof(EFI_TCP4_CONNECTION_TOKEN));
    status = uefi_call_wrapper(BS->CreateEvent, 5, 0, TPL_CALLBACK, NULL, NULL, &ConnToken.CompletionToken.Event);
    if (EFI_ERROR(status)) goto CleanupAndExit;

    EFI_EVENT TimeoutEvent = NULL;
    status = uefi_call_wrapper(BS->CreateEvent, 5, EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TimeoutEvent);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->CloseEvent, 1, ConnToken.CompletionToken.Event);
        goto CleanupAndExit;
    }

    UINT64 Timeout100Ns = (UINT64)Timeout * 10000ULL; 
    uefi_call_wrapper(BS->SetTimer, 3, TimeoutEvent, TimerRelative, Timeout100Ns);

    status = uefi_call_wrapper(TCP4->Connect, 2, TCP4, &ConnToken);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->CloseEvent, 1, ConnToken.CompletionToken.Event);
        uefi_call_wrapper(BS->CloseEvent, 1, TimeoutEvent);
        goto CleanupAndExit;
    }

    EFI_EVENT WaitList[2] = { ConnToken.CompletionToken.Event, TimeoutEvent };
    UINTN EventIndex;
    uefi_call_wrapper(BS->WaitForEvent, 3, 2, WaitList, &EventIndex);
    
    uefi_call_wrapper(BS->SetTimer, 3, TimeoutEvent, TimerCancel, 0);
    uefi_call_wrapper(BS->CloseEvent, 1, TimeoutEvent);
    uefi_call_wrapper(BS->CloseEvent, 1, ConnToken.CompletionToken.Event);

    if (EventIndex == 1 || EFI_ERROR(ConnToken.CompletionToken.Status)) {
        status = (EventIndex == 1) ? EFI_TIMEOUT : ConnToken.CompletionToken.Status;
        CPrint(ActualConfig.Theme.Info,L"[NetDiag] Echec Connect Status: %r\n", status);
        goto CleanupAndExit;
    }

    CPrint(ActualConfig.Theme.Info,L"[NetDiag] TCP Connect OK !\n");
    UINT64 StartTSC = ReadTSC();

    // 4. ÉMISSION (Transmit)
    if (Payload && PayloadSize > 0) {
        EFI_TCP4_TRANSMIT_DATA TxData;
        ZeroMem(&TxData, sizeof(EFI_TCP4_TRANSMIT_DATA));
        TxData.Push = TRUE;
        TxData.DataLength = (UINT32)PayloadSize;
        TxData.FragmentCount = 1;
        TxData.FragmentTable[0].FragmentLength = (UINT32)PayloadSize;
        TxData.FragmentTable[0].FragmentBuffer = Payload;

        EFI_TCP4_IO_TOKEN TxToken;
        ZeroMem(&TxToken, sizeof(EFI_TCP4_IO_TOKEN));
        TxToken.Packet.TxData = &TxData;

        uefi_call_wrapper(BS->CreateEvent, 5, 0, TPL_CALLBACK, NULL, NULL, &TxToken.CompletionToken.Event);
        status = uefi_call_wrapper(TCP4->Transmit, 2, TCP4, &TxToken);
        
        if (!EFI_ERROR(status)) {
            uefi_call_wrapper(BS->WaitForEvent, 3, 1, &TxToken.CompletionToken.Event, &EventIndex);
            status = TxToken.CompletionToken.Status;
        }
        uefi_call_wrapper(BS->CloseEvent, 1, TxToken.CompletionToken.Event);
        
        if (EFI_ERROR(status)) goto CleanupAndExit;
    }

    // 5. RÉCEPTION BOUCLÉE (Accumulation progressive des paquets)
    #define CHUNK_SIZE 8192
    UINTN TotalReceived = 0;
    UINT8 *AccumulatedData = NULL;
    UINT8 TempChunk[CHUNK_SIZE];

    while (TRUE) {
        EFI_TCP4_RECEIVE_DATA RxData;
        ZeroMem(&RxData, sizeof(EFI_TCP4_RECEIVE_DATA));
        RxData.DataLength = CHUNK_SIZE;
        RxData.FragmentCount = 1;
        RxData.FragmentTable[0].FragmentLength = CHUNK_SIZE;
        RxData.FragmentTable[0].FragmentBuffer = (VOID*)TempChunk;

        EFI_TCP4_IO_TOKEN RxToken;
        ZeroMem(&RxToken, sizeof(EFI_TCP4_IO_TOKEN));
        RxToken.Packet.RxData = &RxData;

        uefi_call_wrapper(BS->CreateEvent, 5, 0, TPL_CALLBACK, NULL, NULL, &RxToken.CompletionToken.Event);
        uefi_call_wrapper(BS->CreateEvent, 5, EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TimeoutEvent);
        uefi_call_wrapper(BS->SetTimer, 3, TimeoutEvent, TimerRelative, Timeout100Ns);

        status = uefi_call_wrapper(TCP4->Receive, 2, TCP4, &RxToken);
        if (EFI_ERROR(status)) {
            uefi_call_wrapper(BS->CloseEvent, 1, TimeoutEvent);
            uefi_call_wrapper(BS->CloseEvent, 1, RxToken.CompletionToken.Event);
            break;
        }

        WaitList[0] = RxToken.CompletionToken.Event;
        WaitList[1] = TimeoutEvent;
        uefi_call_wrapper(BS->WaitForEvent, 3, 2, WaitList, &EventIndex);

        EFI_STATUS RxStatus = RxToken.CompletionToken.Status;

        uefi_call_wrapper(BS->SetTimer, 3, TimeoutEvent, TimerCancel, 0);
        uefi_call_wrapper(BS->CloseEvent, 1, TimeoutEvent);
        uefi_call_wrapper(BS->CloseEvent, 1, RxToken.CompletionToken.Event);

        // Si Timeout ou Erreur de socket (FIN / Reset)
        if (EventIndex == 1 || EFI_ERROR(RxStatus)) {
            // Un EFI_CONNECTION_FIN signifie simplement que le serveur a fini d'envoyer
            if (RxStatus == EFI_CONNECTION_FIN || EventIndex == 1) {
                status = EFI_SUCCESS;
            } else {
                status = RxStatus;
            }
            break;
        }

        // Récupération de la taille reçue lors de cet appel
        UINTN BytesRead = RxData.DataLength;
        if (BytesRead == 0) {
            // Aucun nouvel octet reçu, fin de flux
            break;
        }

        // Réallocation de la mémoire globale
        UINT8 *NewBuffer = kmalloc(TotalReceived + BytesRead);
        if (NewBuffer == NULL) {
            if (AccumulatedData) kfree(AccumulatedData);
            status = EFI_OUT_OF_RESOURCES;
            goto CleanupAndExit;
        }

        if (AccumulatedData != NULL) {
            CopyMem(NewBuffer, AccumulatedData, TotalReceived);
            kfree(AccumulatedData);
        }

        CopyMem(NewBuffer + TotalReceived, TempChunk, BytesRead);
        AccumulatedData = NewBuffer;
        TotalReceived += BytesRead;
    }

    if (TotalReceived > 0) {
        *Response = AccumulatedData;
        *ResponseSize = TotalReceived;
        status = EFI_SUCCESS;

        UINT64 EndTSC = ReadTSC();
        if (Time) {
            UINT64 TscPerMs = GetTSCFrequencyPerMs(); 
            *Time = (UINTN)((EndTSC - StartTSC) / TscPerMs);
        }
    } else if (!EFI_ERROR(status)) {
        status = EFI_NO_RESPONSE;
    }

CleanupAndExit:
    if (TCP4) {
        uefi_call_wrapper(TCP4->Configure, 2, TCP4, NULL); 
    }
    if (ChildHandle) {
        uefi_call_wrapper(TCP4ServiceBinding->DestroyChild, 2, TCP4ServiceBinding, ChildHandle);
    }
    return status;
}