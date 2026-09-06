#include <efi.h>
#include <efilib.h>

// 1. Forward declaration du protocole
typedef struct _EFI_DHCP4_PROTOCOL EFI_DHCP4_PROTOCOL;
typedef struct _EFI_IP4_CONFIG2_PROTOCOL EFI_IP4_CONFIG2_PROTOCOL;
typedef struct _EFI_DNS4_PROTOCOL EFI_DNS4_PROTOCOL;
// 2. Énumérations (États et Événements)
typedef enum {
  Dhcp4Stopped     = 0x0,
  Dhcp4Init        = 0x1,
  Dhcp4Selecting   = 0x2,
  Dhcp4Requesting  = 0x3,
  Dhcp4Bound       = 0x4,
  Dhcp4Renewing    = 0x5,
  Dhcp4Rebinding   = 0x6,
  Dhcp4InitReboot  = 0x7,
  Dhcp4Rebooting   = 0x8
} EFI_DHCP4_STATE;

typedef enum {
  Dhcp4SendDiscover    = 0x01,
  Dhcp4RcvdOffer       = 0x02,
  Dhcp4SelectOffer     = 0x03,
  Dhcp4SendRequest     = 0x04,
  Dhcp4RcvdAck         = 0x05,
  Dhcp4RcvdNak         = 0x06,
  Dhcp4SendDecline     = 0x07,
  Dhcp4BoundCompleted  = 0x08,
  Dhcp4EnterRenewing   = 0x09,
  Dhcp4EnterRebinding  = 0x0a,
  Dhcp4AddressLost     = 0x0b,
  Dhcp4Fail            = 0x0c
} EFI_DHCP4_EVENT;

typedef enum {
  Ip4Config2DataTypeInterfaceInfo,
  Ip4Config2DataTypePolicy,
  Ip4Config2DataTypeManualAddress,
  Ip4Config2DataTypeGateway,
  Ip4Config2DataTypeDnsServer,
  Ip4Config2DataTypeMaximum
} EFI_IP4_CONFIG2_DATA_TYPE;

typedef enum {
  Ip4Config2PolicyStatic,
  Ip4Config2PolicyDhcp,
  Ip4Config2PolicyMax
} EFI_IP4_CONFIG2_POLICY;

// 3. Paquets DHCP
#pragma pack(1)
typedef struct {
  UINT8      OpCode;
  UINT8      Length;
  UINT8      Data[1];
} EFI_DHCP4_PACKET_OPTION;

typedef struct {
  UINT8              OpCode;
  UINT8              HwType;
  UINT8              HwAddrLen;
  UINT8              Hops;
  UINT32             Xid;
  UINT16             Seconds;
  UINT16             Reserved;
  EFI_IPv4_ADDRESS   ClientAddr;
  EFI_IPv4_ADDRESS   YourAddr;
  EFI_IPv4_ADDRESS   ServerAddr;
  EFI_IPv4_ADDRESS   GatewayAddr;
  UINT8              ClientHwAddr[16];
  CHAR8              ServerName[64];
  CHAR8              BootFileName[128];
} EFI_DHCP4_HEADER;

typedef struct {
  UINT32               Size;
  UINT32               Length;
  struct {
    EFI_DHCP4_HEADER   Header;
    UINT32             Magik;
    UINT8              Option[1];
  } Dhcp4;
} EFI_DHCP4_PACKET;
#pragma pack()

// 4. Callback
typedef EFI_STATUS (*EFI_DHCP4_CALLBACK)(
  IN EFI_DHCP4_PROTOCOL    *This,
  IN VOID                  *Context,
  IN EFI_DHCP4_STATE       CurrentState,
  IN EFI_DHCP4_EVENT       Dhcp4Event,
  IN EFI_DHCP4_PACKET      *Packet OPTIONAL,
  OUT EFI_DHCP4_PACKET     **NewPacket OPTIONAL
);

// 5. Configuration
typedef struct {
  UINT32                   DiscoverTryCount;
  UINT32                   *DiscoverTimeout;
  UINT32                   RequestTryCount;
  UINT32                   *RequestTimeout;
  EFI_IPv4_ADDRESS         ClientAddress;
  EFI_DHCP4_CALLBACK       Dhcp4Callback;
  VOID                     *CallbackContext;
  UINT32                   OptionCount;
  EFI_DHCP4_PACKET_OPTION  **OptionList;
} EFI_DHCP4_CONFIG_DATA;

// 6. Mode Data (Dépend de State, ConfigData et Packet)
typedef struct {
  EFI_DHCP4_STATE            State;
  EFI_DHCP4_CONFIG_DATA      ConfigData;
  EFI_IPv4_ADDRESS           ClientAddress;
  EFI_MAC_ADDRESS            ClientMacAddress;
  EFI_IPv4_ADDRESS           ServerAddress;
  EFI_IPv4_ADDRESS           RouterAddress;
  EFI_IPv4_ADDRESS           SubnetMask;
  UINT32                     LeaseTime;
  EFI_DHCP4_PACKET           *ReplyPacket;
} EFI_DHCP4_MODE_DATA;

// 7. Tokens et Points d'écoute
typedef struct {
  EFI_IPv4_ADDRESS       ListenAddress;
  EFI_IPv4_ADDRESS       SubnetMask;
  UINT16                 ListenPort;
} EFI_DHCP4_LISTEN_POINT;

typedef struct {
  EFI_STATUS                     Status;
  EFI_EVENT                      CompletionEvent;
  EFI_IPv4_ADDRESS               RemoteAddress;
  UINT16                         RemotePort;
  EFI_IPv4_ADDRESS               GatewayAddress;
  UINT32                         ListenPointCount;
  EFI_DHCP4_LISTEN_POINT         *ListenPoints;
  UINT32                         TimeoutValue;
  EFI_DHCP4_PACKET               *Packet;
  UINT32                         ResponseCount;
  EFI_DHCP4_PACKET               *ResponseList;
} EFI_DHCP4_TRANSMIT_RECEIVE_TOKEN;

typedef struct {
  EFI_IPv4_ADDRESS       Address;
  EFI_IPv4_ADDRESS       SubnetMask;
}   EFI_IP4_CONFIG2_MANUAL_ADDRESS;

// 8. Prototypes de fonctions membres
typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_GET_MODE_DATA)(
  IN EFI_DHCP4_PROTOCOL            *This,
  OUT EFI_DHCP4_MODE_DATA          *Dhcp4ModeData
);

typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_CONFIGURE)(
  IN EFI_DHCP4_PROTOCOL            *This,
  IN EFI_DHCP4_CONFIG_DATA         *Dhcp4CfgData OPTIONAL
);

typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_START)(
  IN EFI_DHCP4_PROTOCOL        *This,
  IN EFI_EVENT                 CompletionEvent OPTIONAL
);

typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_RENEW_REBIND)(
  IN EFI_DHCP4_PROTOCOL          *This,
  IN BOOLEAN                     RebindRequest,
  IN EFI_EVENT                   CompletionEvent OPTIONAL
);

typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_RELEASE)(
  IN EFI_DHCP4_PROTOCOL      *This
);

typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_STOP)(
  IN EFI_DHCP4_PROTOCOL      *This
);

typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_BUILD)(
  IN EFI_DHCP4_PROTOCOL        *This,
  IN EFI_DHCP4_PACKET          *SeedPacket,
  IN UINT32                    DeleteCount,
  IN UINT8                     *DeleteList OPTIONAL,
  IN UINT32                    AppendCount,
  IN EFI_DHCP4_PACKET_OPTION   *AppendList[] OPTIONAL,
  OUT EFI_DHCP4_PACKET         **NewPacket
);

typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_TRANSMIT_RECEIVE)(
  IN EFI_DHCP4_PROTOCOL                  *This,
  IN EFI_DHCP4_TRANSMIT_RECEIVE_TOKEN    *Token
);

typedef
EFI_STATUS
(EFIAPI *EFI_DHCP4_PARSE)(
  IN EFI_DHCP4_PROTOCOL            *This,
  IN EFI_DHCP4_PACKET              *Packet,
  IN OUT UINT32                    *OptionCount,
  IN OUT EFI_DHCP4_PACKET_OPTION   *PacketOptionList[] OPTIONAL
);

typedef
EFI_STATUS
(EFIAPI *EFI_IP4_CONFIG2_SET_DATA) (
  IN EFI_IP4_CONFIG2_PROTOCOL    *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE   DataType,
  IN UINTN                       DataSize,
  IN VOID                        *Data
  );

typedef
EFI_STATUS
(EFIAPI *EFI_IP4_CONFIG2_GET_DATA) (
  IN EFI_IP4_CONFIG2_PROTOCOL    *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE   DataType,
  IN OUT UINTN                   *DataSize,
  IN VOID                        *Data OPTIONAL
  );

typedef
EFI_STATUS
(EFIAPI *EFI_IP4_CONFIG2_REGISTER_NOTIFY) (
  IN EFI_IP4_CONFIG2_PROTOCOL       *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE      DataType,
IN EFI_EVENT                        Event
);

typedef
EFI_STATUS
(EFIAPI *EFI_IP4_CONFIG2_UNREGISTER_NOTIFY) (
  IN EFI_IP4_CONFIG2_PROTOCOL          *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE         DataType,
  IN EFI_EVENT                         Event
  );


// 9. Structure finale du protocole
struct _EFI_DHCP4_PROTOCOL {
  EFI_DHCP4_GET_MODE_DATA        GetModeData;
  EFI_DHCP4_CONFIGURE            Configure;
  EFI_DHCP4_START                Start;
  EFI_DHCP4_RENEW_REBIND         RenewRebind;
  EFI_DHCP4_RELEASE              Release;
  EFI_DHCP4_STOP                 Stop;
  EFI_DHCP4_BUILD                Build;
  EFI_DHCP4_TRANSMIT_RECEIVE     TransmitReceive;
  EFI_DHCP4_PARSE                Parse;
};

typedef struct _EFI_IP4_CONFIG2_PROTOCOL {
  EFI_IP4_CONFIG2_SET_DATA             SetData;
  EFI_IP4_CONFIG2_GET_DATA             GetData;
  EFI_IP4_CONFIG2_REGISTER_NOTIFY      RegisterDataNotify;
  EFI_IP4_CONFIG2_UNREGISTER_NOTIFY    UnregisterDataNotify;
}   EFI_IP4_CONFIG2_PROTOCOL;

//--------------------DNS------------------------------------

typedef struct {
  UINT32             IpCount;
  EFI_IPv4_ADDRESS   *IpList;
}  DNS_HOST_TO_ADDR_DATA;

typedef struct {
 CHAR16                *HostName;
}  DNS_ADDR_TO_HOST_DATA;

typedef struct {
  CHAR8        *QName;
  UINT16       QType;
  UINT16       QClass;
  UINT32       TTL;
  UINT16       DataLength;
  CHAR8        *RData;
}  DNS_RESOURCE_RECORD;

typedef struct {
  UINTN                 RRCount;
  DNS_RESOURCE_RECORD  *RRList;
}   DNS_GENERAL_LOOKUP_DATA;

typedef struct {
  UINTN              DnsServerListCount;
  EFI_IPv4_ADDRESS   *DnsServerList;
  BOOLEAN            UseDefaultSetting;
  BOOLEAN            EnableDnsCache;
  UINT8              Protocol;
  EFI_IPv4_ADDRESS   StationIp;
  EFI_IPv4_ADDRESS   SubnetMask;
  UINT16             LocalPort;
  UINT32             RetryCount;
  UINT32             RetryInterval;
}  EFI_DNS4_CONFIG_DATA;

typedef struct {
  CHAR16               *HostName;
  EFI_IPv4_ADDRESS     *IpAddress;
   UINT32              Timeout;
}  EFI_DNS4_CACHE_ENTRY;


typedef struct {
  EFI_DNS4_CONFIG_DATA       DnsConfigData;
  UINT32                     DnsServerCount;
  EFI_IPv4_ADDRESS           *DnsServerList;
  UINT32                     DnsCacheCount;
  EFI_DNS4_CACHE_ENTRY       *DnsCacheList;
}  EFI_DNS4_MODE_DATA;

typedef struct {
  EFI_EVENT                    Event;
  EFI_STATUS                   Status;
  UINT32                       RetryCount;
  UINT32                       RetryInterval;
  union {
    DNS_HOST_TO_ADDR_DATA      *H2AData;
    DNS_ADDR_TO_HOST_DATA      *A2HData;
    DNS_GENERAL_LOOKUP_DATA    *GLookupData;
  }  RspData;
}   EFI_DNS4_COMPLETION_TOKEN;

typedef
EFI_STATUS
(EFIAPI *EFI_DNS4_GET_MODE_DATA)(
  IN EFI_DNS4_PROTOCOL             *This,
  OUT EFI_DNS4_MODE_DATA           *DnsModeData
  );

typedef
EFI_STATUS
(EFIAPI *EFI_DNS4_CONFIGURE)(
  IN EFI_DNS4_PROTOCOL         *This,
  IN EFI_DNS4_CONFIG_DATA      *DnsConfigData
);

typedef
EFI_STATUS
(EFIAPI *EFI_DNS4_HOST_NAME_TO_IP) (
  IN EFI_DNS4_PROTOCOL           *This,
  IN CHAR16                      *HostName,
  IN EFI_DNS4_COMPLETION_TOKEN   *Token
);

typedef
EFI_STATUS
(EFIAPI *EFI_DNS4_IP_TO_HOST_NAME) (
  IN EFI_DNS4_PROTOCOL             *This,
  IN EFI_IPv4_ADDRESS              IpAddress,
  IN EFI_DNS4_COMPLETION_TOKEN     *Token
);

typedef
EFI_STATUS
(EFIAPI *EFI_DNS4_GENERAL_LOOKUP) (
  IN EFI_DNS4_PROTOCOL             *This,
  IN CHAR8                         *QName,
  IN UINT16                        QType,
  IN UINT16                        QClass,
  IN EFI_DNS4_COMPLETION_TOKEN     *Token
);

 typedef
 EFI_STATUS
 (EFIAPI *EFI_DNS4_UPDATE_DNS_CACHE) (
   IN EFI_DNS4_PROTOCOL             *This,
   IN BOOLEAN                       DeleteFlag,
   IN BOOLEAN                       Override,
   IN EFI_DNS4_CACHE_ENTRY          DnsCacheEntry
);

typedef
EFI_STATUS
(EFIAPI *EFI_DNS4_POLL) (
 IN EFI_DNS4_PROTOCOL        *This
);

typedef
EFI_STATUS
(EFIAPI *EFI_DNS4_CANCEL) (
  IN EFI_DNS4_PROTOCOL             *This,
  IN EFI_DNS4_COMPLETION_TOKEN     *Token
);

typedef struct _EFI_DNS4_PROTOCOL {
  EFI_DNS4_GET_MODE_DATA       GetModeData;
  EFI_DNS4_CONFIGURE           Configure;
  EFI_DNS4_HOST_NAME_TO_IP     HostNameToIp;
  EFI_DNS4_IP_TO_HOST_NAME     IpToHostName;
  EFI_DNS4_GENERAL_LOOKUP      GeneralLookUp;
  EFI_DNS4_UPDATE_DNS_CACHE    UpdateDnsCache;
  EFI_DNS4_POLL                Poll;
  EFI_DNS4_CANCEL              Cancel;
}  EFI_DNS4_PROTOCOL;

#define EFI_DHCP4_SERVICE_BINDING_PROTOCOL_GUID {0x9d9a39d8,0xbd42,0x4a73,{0xa4,0xd5,0x8e,0xe9,0x4b,0xe1,0x13,0x80}}
#define EFI_DHCP4_PROTOCOL_GUID {0x8a219718,0x4ef5,0x4761,{0x91,0xc8,0xc0,0xf0,0x4b,0xda,0x9e,0x56}}
#define EFI_IP4_CONFIG2_PROTOCOL_GUID { 0x5b446ed1, 0xe30b, 0x4faa,{ 0x87, 0x1a, 0x36, 0x54, 0xec, 0xa3, 0x60, 0x80 }}
#define EFI_DNS4_SERVICE_BINDING_PROTOCOL_GUID { 0xb625b186, 0xe063, 0x44f7,{ 0x89, 0x5, 0x6a, 0x74, 0xdc, 0x6f, 0x52, 0xb4}}
#define EFI_DNS4_PROTOCOL_GUID { 0xae3d28cc, 0xe05b, 0x4fa1,{0xa0, 0x11, 0x7e, 0xb5, 0x5a, 0x3f, 0x14, 0x1 }}

#define Ip4Config2DataTypePolicy 1
#define Ip4Config2DataTypeManualAddress 2
#define Ip4Config2DataTypeGateway 3


typedef struct _EFI_DHCP4_PROTOCOL EFI_DHCP4_PROTOCOL;
