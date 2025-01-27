#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <wchar.h>

#include "localip.h"

static void addNewIp(FFlist* list, const wchar_t* name, const char* addr, bool ipv6)
{
    FFLocalIpResult* ip = (FFLocalIpResult*) ffListAdd(list);

    int len = (int)wcslen(name);
    if(len > 0)
    {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, name, len, NULL, 0, NULL, NULL);
        ffStrbufInitA(&ip->name, (uint32_t)size_needed + 1);
        WideCharToMultiByte(CP_UTF8, 0, name, len, ip->name.chars, size_needed, NULL, NULL);
        ip->name.length = (uint32_t)size_needed;
        ip->name.chars[size_needed] = '\0';
    }
    else
    {
        ffStrbufInitS(&ip->name, "*");
    }

    ffStrbufInitS(&ip->addr, addr);
    ip->ipv6 = ipv6;
}

const char* ffDetectLocalIps(const FFinstance* instance, FFlist* results)
{
    IP_ADAPTER_ADDRESSES* adapter_addresses = NULL;

    // Start with a 16 KB buffer and resize if needed -
    // multiple attempts in case interfaces change while
    // we are in the middle of querying them.
    DWORD adapter_addresses_buffer_size = 16 * 1024;
    for (int attempts = 0; attempts != 3; ++attempts)
    {
        adapter_addresses = (IP_ADAPTER_ADDRESSES*)realloc(adapter_addresses, adapter_addresses_buffer_size);
        assert(adapter_addresses);

        DWORD error = GetAdaptersAddresses(
            AF_UNSPEC,
            GAA_FLAG_SKIP_ANYCAST |
            GAA_FLAG_SKIP_MULTICAST |
            GAA_FLAG_SKIP_DNS_SERVER |
            GAA_FLAG_SKIP_FRIENDLY_NAME,
            NULL,
            adapter_addresses,
            &adapter_addresses_buffer_size);

        if (error == ERROR_SUCCESS)
            break;
        else if (ERROR_BUFFER_OVERFLOW == error)
            continue;
        else
            return "GetAdaptersAddresses() failed";
    }

    // Iterate through all of the adapters
    for (IP_ADAPTER_ADDRESSES* adapter = adapter_addresses; adapter; adapter = adapter->Next)
    {
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK && !instance->config.localIpShowLoop)
            continue;

        for (IP_ADAPTER_UNICAST_ADDRESS* ifa = adapter->FirstUnicastAddress; ifa; ifa = ifa->Next)
        {
            if (ifa->Address.lpSockaddr->sa_family == AF_INET)
            {
                // IPv4
                if (!instance->config.localIpShowIpV4)
                    continue;

                SOCKADDR_IN* ipv4 = (SOCKADDR_IN*) ifa->Address.lpSockaddr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ipv4->sin_addr, addressBuffer, INET_ADDRSTRLEN);
                addNewIp(results, adapter->FriendlyName, addressBuffer, false);
            }
            else if (ifa->Address.lpSockaddr->sa_family == AF_INET6)
            {
                // IPv6
                if (!instance->config.localIpShowIpV6)
                    continue;

                SOCKADDR_IN6* ipv6 = (SOCKADDR_IN6*) ifa->Address.lpSockaddr;
                char addressBuffer[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &ipv6->sin6_addr, addressBuffer, INET6_ADDRSTRLEN);
                addNewIp(results, adapter->FriendlyName, addressBuffer, false);
            }
        }
    }

    free(adapter_addresses);
    return NULL;
}
