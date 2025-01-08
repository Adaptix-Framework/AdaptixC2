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
#define HASH_FUNC_NTTERMINATETHREAD                  0x43b9dd27
#define HASH_FUNC_NTTERMINATEPROCESS                 0x9e28d66e
#define HASH_FUNC_RTLGETVERSION                      0xb28521fc
#define HASH_FUNC_RTLEXITUSERTHREAD                  0xa6320b07
#define HASH_FUNC_RTLEXITUSERPROCESS                 0x4fa6c04e
#define HASH_FUNC_RTLIPV4STRINGTOADDRESSA            0x87cc3a9a
#define HASH_FUNC_RTLRANDOMEX                        0x5b052214
#define HASH_FUNC_RTLNTSTATUSTODOSERROR              0x7701adaf

//kernel32
#define HASH_FUNC_COPYFILEA                          0x1cba2820
#define HASH_FUNC_CREATEDIRECTORYA                   0x4e15ef6e
#define HASH_FUNC_CREATEFILEA                        0x44701e19
#define HASH_FUNC_CREATEPIPE                         0xd38cc306
#define HASH_FUNC_CREATEPROCESSA                     0x352ef9d8
#define HASH_FUNC_DELETEFILEA                        0x75b1df38
#define HASH_FUNC_FINDCLOSE                          0x257f195b
#define HASH_FUNC_FINDFIRSTFILEA                     0x2ffa9aae
#define HASH_FUNC_FINDNEXTFILEA                      0xdacd2845
#define HASH_FUNC_GETACP                             0xa8455a98
#define HASH_FUNC_GETCOMPUTERNAMEEXA                 0x3bc15572
#define HASH_FUNC_GETCURRENTDIRECTORYA               0x9c466afd
#define HASH_FUNC_GETDRIVETYPEA                      0x4d986681
#define HASH_FUNC_GETEXITCODEPROCESS                 0xf714f658
#define HASH_FUNC_GETEXITCODETHREAD                  0xca4ca7d1
#define HASH_FUNC_GETFILESIZE                        0x5774353f
#define HASH_FUNC_GETFILEATTRIBUTESA                 0x4259a42c
#define HASH_FUNC_GETFULLPATHNAMEA                   0x3da055a6
#define HASH_FUNC_GETLOGICALDRIVES                   0xf20bc68c
#define HASH_FUNC_GETOEMCP                           0xd004d0d8
#define HASH_FUNC_K32GETMODULEBASENAMEA              0x9a48e677
#define HASH_FUNC_GETMODULEBASENAMEA                 0x56879f07
#define HASH_FUNC_GETMODULEHANDLEW                   0x7007130d
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
#define HASH_FUNC_WIDECHARTOMULTIBYTE                0x12d4f52d
#define HASH_FUNC_WRITEFILE                          0xd4a33cef

// iphlpapi
#define HASH_FUNC_GETADAPTERSINFO                    0xa1376764

// advapi32
#define HASH_FUNC_GETTOKENINFORMATION                0x49639a4b
#define HASH_FUNC_LOOKUPACCOUNTSIDA                  0x4be434ac

// msvcrt
#define HASH_FUNC_VSNPRINTF                          0xc4e4280e
