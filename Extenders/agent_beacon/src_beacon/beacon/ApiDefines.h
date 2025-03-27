#pragma once

#define HASH_LIB_NTDLL              0x19a59ec
#define HASH_LIB_KERNEL32           0x7b348614
#define HASH_LIB_IPHLPAPI           0x2d288345
#define HASH_LIB_ADVAPI32           0x721421e8
#define HASH_LIB_MSVCRT             0xb707534d


//ntdll
#define HASH_FUNC_NTCLOSE                            0xed9853bc
#define HASH_FUNC_NTCONTINUE                         0x3932454b
#define HASH_FUNC_NTFREEVIRTUALMEMORY                0x6130e328
#define HASH_FUNC_NTQUERYINFORMATIONPROCESS          0x68b3d2e1
#define HASH_FUNC_NTQUERYSYSTEMINFORMATION           0x91ef8a47
#define HASH_FUNC_NTOPENPROCESS                      0xf029bc37
#define HASH_FUNC_NTOPENPROCESSTOKEN                 0x845cc3b8
#define HASH_FUNC_NTOPENTHREADTOKEN                  0xf6f79cf1
#define HASH_FUNC_NTTERMINATETHREAD                  0x43b9dd27
#define HASH_FUNC_NTTERMINATEPROCESS                 0x9e28d66e
#define HASH_FUNC_RTLGETVERSION                      0xb28521fc
#define HASH_FUNC_RTLEXITUSERTHREAD                  0xa6320b07
#define HASH_FUNC_RTLEXITUSERPROCESS                 0x4fa6c04e
#define HASH_FUNC_RTLIPV4STRINGTOADDRESSA            0x87cc3a9a
#define HASH_FUNC_RTLRANDOMEX                        0x5b052214
#define HASH_FUNC_RTLNTSTATUSTODOSERROR              0x7701adaf
#define HASH_FUNC_NTFLUSHINSTRUCTIONCACHE            0x91a1659e

//kernel32
#define HASH_FUNC_CONNECTNAMEDPIPE                   0xda6c7d81
#define HASH_FUNC_COPYFILEA                          0x1cba2820
#define HASH_FUNC_CREATEDIRECTORYA                   0x4e15ef6e
#define HASH_FUNC_CREATEFILEA                        0x44701e19
#define HASH_FUNC_CREATENAMEDPIPEA                   0x375c5b8c
#define HASH_FUNC_CREATEPIPE                         0xd38cc306
#define HASH_FUNC_CREATEPROCESSA                     0x352ef9d8
#define HASH_FUNC_DELETEFILEA                        0x75b1df38
#define HASH_FUNC_DISCONNECTNAMEDPIPE                0x6d59f261
#define HASH_FUNC_FINDCLOSE                          0x257f195b
#define HASH_FUNC_FINDFIRSTFILEA                     0x2ffa9aae
#define HASH_FUNC_FINDNEXTFILEA                      0xdacd2845
#define HASH_FUNC_FREELIBRARY                        0x26ccae3b
#define HASH_FUNC_FLUSHFILEBUFFERS                   0xd60d6813
#define HASH_FUNC_GETACP                             0xa8455a98
#define HASH_FUNC_GETCOMPUTERNAMEEXA                 0x3bc15572
#define HASH_FUNC_GETCURRENTDIRECTORYA               0x9c466afd
#define HASH_FUNC_GETDRIVETYPEA                      0x4d986681
#define HASH_FUNC_GETEXITCODEPROCESS                 0xf714f658
#define HASH_FUNC_GETEXITCODETHREAD                  0xca4ca7d1
#define HASH_FUNC_GETFILESIZE                        0x5774353f
#define HASH_FUNC_GETFILEATTRIBUTESA                 0x4259a42c
#define HASH_FUNC_GETFULLPATHNAMEA                   0x3da055a6
#define HASH_FUNC_GETLASTERROR                       0xdbb35ee2
#define HASH_FUNC_GETLOGICALDRIVES                   0xf20bc68c
#define HASH_FUNC_GETOEMCP                           0xd004d0d8
#define HASH_FUNC_K32GETMODULEBASENAMEA              0x9a48e677
#define HASH_FUNC_GETMODULEBASENAMEA                 0x56879f07
#define HASH_FUNC_GETMODULEHANDLEA                   0x700712f7
#define HASH_FUNC_GETPROCADDRESS                     0x184f2ade
#define HASH_FUNC_GETTICKCOUNT                       0xfcdd8ab8
#define HASH_FUNC_GETTIMEZONEINFORMATION             0xac7885f5
#define HASH_FUNC_GETUSERNAMEA                       0x56f41f65
#define HASH_FUNC_HEAPALLOC                          0x90953b4d
#define HASH_FUNC_HEAPCREATE                         0xa84f7996
#define HASH_FUNC_HEAPDESTROY                        0xe1ed946c
#define HASH_FUNC_HEAPREALLOC                        0x1652b144
#define HASH_FUNC_HEAPFREE                           0x90075c24
#define HASH_FUNC_ISWOW64PROCESS                     0x5bf9b926
#define HASH_FUNC_LOADLIBRARYA                       0x1159d0fa
#define HASH_FUNC_LOCALALLOC                         0xaeff147a
#define HASH_FUNC_LOCALFREE                          0x14d443b1
#define HASH_FUNC_LOCALREALLOC                       0x769789b1
#define HASH_FUNC_MOVEFILEA                          0x48ccd21c
#define HASH_FUNC_MULTIBYTETOWIDECHAR                0xdcf711ed
#define HASH_FUNC_PEEKNAMEDPIPE                      0x79d7f37c
#define HASH_FUNC_READFILE                           0xc9c06180
#define HASH_FUNC_REMOVEDIRECTORYA                   0x4dada1a8
#define HASH_FUNC_RTLCAPTURECONTEXT                  0x626d2e2f
#define HASH_FUNC_SETCURRENTDIRECTORYA               0x2e1c9789
#define HASH_FUNC_SETNAMEDPIPEHANDLESTATE            0x89e25d30
#define HASH_FUNC_SLEEP                              0x5b4b729d
#define HASH_FUNC_VIRTUALALLOC                       0x63ce6376
#define HASH_FUNC_VIRTUALFREE                        0xbd37a32d
#define HASH_FUNC_WAITNAMEDPIPEA                     0x8a2ba58d
#define HASH_FUNC_WIDECHARTOMULTIBYTE                0x12d4f52d
#define HASH_FUNC_WRITEFILE                          0xd4a33cef

// iphlpapi
#define HASH_FUNC_GETADAPTERSINFO                    0xa1376764

// advapi32
#define HASH_FUNC_ALLOCATEANDINITIALIZESID           0xbf449b6e
#define HASH_FUNC_GETTOKENINFORMATION                0x49639a4b
#define HASH_FUNC_INITIALIZESECURITYDESCRIPTOR       0x529256ed
#define HASH_FUNC_IMPERSONATELOGGEDONUSER            0x77243019
#define HASH_FUNC_FREESID                            0x813c8686
#define HASH_FUNC_LOOKUPACCOUNTSIDA                  0x4be434ac
#define HASH_FUNC_REVERTTOSELF                       0xcce516a9
#define HASH_FUNC_SETTHREADTOKEN                     0x373ff89
#define HASH_FUNC_SETENTRIESINACLA                   0xa4379492
#define HASH_FUNC_SETSECURITYDESCRIPTORDACL          0x37dc047b
// msvcrt
#if defined(DEBUG)
#define HASH_FUNC_PRINTF                             0xbe293817
#endif
#define HASH_FUNC_VSNPRINTF                          0xc4e4280e

// BOF
#define HASH_FUNC_BEACONDATAPARSE                    0x3a3f9b41
#define HASH_FUNC_BEACONDATAINT                      0xa3aad9b1
#define HASH_FUNC_BEACONDATASHORT                    0x3a79ae96
#define HASH_FUNC_BEACONDATALENGTH                   0x792460a8
#define HASH_FUNC_BEACONDATAEXTRACT                  0xaf9d1a81
#define HASH_FUNC_BEACONFORMATALLOC                  0xde6f0c40
#define HASH_FUNC_BEACONFORMATRESET                  0xdf9ef2b8
#define HASH_FUNC_BEACONFORMATAPPEND                 0xac9aff0d
#define HASH_FUNC_BEACONFORMATPRINTF                 0xcfb8e1e8
#define HASH_FUNC_BEACONFORMATTOSTRING               0x8350c50f
#define HASH_FUNC_BEACONFORMATFREE                   0x8aa15ab7
#define HASH_FUNC_BEACONFORMATINT                    0x8fd66460
#define HASH_FUNC_BEACONOUTPUT                       0xe1f90ffd
#define HASH_FUNC_BEACONPRINTF                       0xe411de3f
#define HASH_FUNC_BEACONUSETOKEN                     0x115b247a
#define HASH_FUNC_BEACONREVERTTOKEN                  0x84387705
#define HASH_FUNC_BEACONISADMIN                      0x4d34c8b1
#define HASH_FUNC_BEACONGETSPAWNTO                   0xc9df6b58
#define HASH_FUNC_BEACONINJECTPROCESS                0x2223da28
#define HASH_FUNC_BEACONINJECTTEMPORARYPROCESS       0xb7acd3ab
#define HASH_FUNC_BEACONSPAWNTEMPORARYPROCESS        0x3a613e77
#define HASH_FUNC_BEACONCLEANUPPROCESS               0x68ff8a73
#define HASH_FUNC_TOWIDECHAR                         0x99e43fee
#define HASH_FUNC_BEACONINFORMATION                  0xd9773b52
#define HASH_FUNC_BEACONADDVALUE                     0x175454f2
#define HASH_FUNC_BEACONGETVALUE                     0x132c1909
#define HASH_FUNC_BEACONREMOVEVALUE                  0xb772ea57
#define HASH_FUNC_LOADLIBRARYA                       0x1159d0fa
#define HASH_FUNC_GETPROCADDRESS                     0x184f2ade
#define HASH_FUNC_GETMODULEHANDLEA                   0x700712f7
#define HASH_FUNC_FREELIBRARY                        0x26ccae3b
#define HASH_FUNC___C_SPECIFIC_HANDLER               0x6d7af307

// wininet
#define HASH_FUNC_INTERNETOPENA                      0x4c383c80
#define HASH_FUNC_INTERNETCONNECTA                   0x575708d8
#define HASH_FUNC_HTTPOPENREQUESTA                   0x226c0d80
#define HASH_FUNC_HTTPSENDREQUESTA                   0xc2c06958
#define HASH_FUNC_INTERNETSETOPTIONA                 0x2e652253
#define HASH_FUNC_INTERNETQUERYOPTIONA               0x4f0bddbd
#define HASH_FUNC_HTTPQUERYINFOA                     0xd7775c67
#define HASH_FUNC_INTERNETQUERYDATAAVAILABLE         0x9ed7669e
#define HASH_FUNC_INTERNETCLOSEHANDLE                0xc0d1320f
#define HASH_FUNC_INTERNETREADFILE                   0xe64c229

// ws2_32
#define HASH_FUNC_WSASTARTUP                         0x512662e2
#define HASH_FUNC_WSACLEANUP                         0x6f1847d7
#define HASH_FUNC_SOCKET                             0xc4ef0f8d
#define HASH_FUNC_GETHOSTBYNAME                      0x9a3fe8fe
#define HASH_FUNC_IOCTLSOCKET                        0xb1dc75c8
#define HASH_FUNC_CONNECT                            0x93f5e60e
#define HASH_FUNC_WSAGETLASTERROR                    0x58a1e4d
#define HASH_FUNC_CLOSESOCKET                        0xf44c50c3
#define HASH_FUNC_SELECT                             0xc43ef024
#define HASH_FUNC___WSAFDISSET                       0x61b4503f
#define HASH_FUNC_SHUTDOWN                           0xccb8a380
#define HASH_FUNC_RECV                               0x6f5eb634
#define HASH_FUNC_RECVFROM                           0xcfb09288
#define HASH_FUNC_SEND                               0x6f5f43ee
#define HASH_FUNC_ACCEPT                             0x9a18f614
#define HASH_FUNC_BIND                               0x6f560281
#define HASH_FUNC_LISTEN                             0xb4374c73
#define HASH_FUNC_SENDTO                             0xc44006d1