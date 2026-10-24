# Ke Hoach Tranh EDR Cho Beacon

Nguyen tac cot loi: **beacon phai vo hinh khi dang chay**. Bat ky thu gi co the dat trong loader hoac plugin thi KHONG dat trong beacon. Beacon giu toi gian — moi byte code them vao la be mat tan cong cho signature va memory scanner.

---

## DA HOAN THANH TRUOC DO

- [x] **Ma hoa vung nho khi ngu (Ekko)** — Khi beacon "ngu" (sleep), toan bo vung nho cua no duoc ma hoa bang RC4 thong qua chuoi ROP dung timer queue. Chi ho tro x64; x86 dung sleep binh thuong.
- [x] **Xoa dau van tay mac dinh** — Thay doi server header, trang 404, URI, User-Agent, port, ten pipe, thoi gian sleep/jitter, ten service de khong giong cau hinh mac dinh.

---

## SPRINT 1 — Gia co syscall (da hoan thanh truoc do)

**Van de:** Moi lan beacon goi ham ntdll (vi du NtOpenProcess, NtClose...), EDR co the dat hook tai diem goi do de theo doi.

### 1.1 Phan giai SSN kieu SysWhispers3

**SSN (System Service Number)** la so thu tu cua moi syscall trong bang cua Windows kernel. Moi ham Nt* trong ntdll co mot SSN rieng (vi du NtClose = 0x0F, NtOpenProcess = 0x26...). Khi goi syscall, CPU can biet SSN de biet goi ham nao trong kernel.

**Van de:** EDR hook cac ham Nt* trong ntdll, lam thay doi noi dung ham. Neu ta doc truc tiep SSN tu ham da bi hook, co the doc sai.

**Giai phap:** Sap xep tat ca ham `Zw*` (tuong duong `Nt*`) theo dia chi bo nho. Thu tu dia chi = thu tu SSN. Day la bat bien — EDR khong the thay doi thu tu dia chi cac export trong DLL.

```
Zw* exports sorted by address:
  ZwClose            -> address 0x7FF...100  -> SSN = 0 (vi tri dau)
  ZwOpenProcess      -> address 0x7FF...120  -> SSN = 1
  ZwQueryInfo...     -> address 0x7FF...140  -> SSN = 2
  ...
```

**Ket qua:** Ta co bang SSN chinh xac ma khong can doc noi dung ham (co the da bi hook).

### 1.2 Thuc thi syscall gian tiep (indirect syscall)

**Van de:** Neu beacon goi `syscall` truc tiep tu vung nho cua minh, return address tren stack se tro vao vung nho beacon — EDR thay return address khong nam trong DLL hop le -> phat hien.

**Giai phap:** Tim mot "gadget" `syscall; ret` (byte 0F 05 C3) trong phan .text cua ntdll. Thay vi goi syscall truc tiep, beacon nhay (JMP) den gadget nay. Khi do return address tren stack tro vao ntdll — hoan toan hop le.

```
Beacon code:
  mov r10, rcx          ; tham so 1 (quy uoc syscall)
  mov eax, SSN          ; so thu tu syscall
  jmp [ntdll_gadget]    ; nhay den 0F 05 C3 trong ntdll
                        ; -> return address = ntdll (hop le!)

Thay vi:
  mov r10, rcx
  mov eax, SSN
  syscall               ; return address = beacon memory (bi phat hien!)
  ret
```

### 1.3 Thay the tat ca loi goi ntdll truc tiep

Moi cho goi `ApiNt->NtClose(...)`, `ApiNt->NtWaitForSingleObject(...)` v.v. duoc chuyen sang dung `SysNtClose(...)`, `SysNtWaitForSingleObject(...)` — cac wrapper di qua indirect syscall.

---

## SPRINT 2 — Lam sach call stack (MOI THUC HIEN)

**Van de:** EDR co cac kernel callback (vi du PsSetCreateThreadNotifyRoutine) kiem tra call stack cua thread. Neu tat ca return address tren stack tro vao mot vung nho RWX (Read-Write-Execute) duy nhat — do la dau hieu ro rang cua malware.

### 2.1 Gia mao return address khi goi API

**Khi nao can:** Khi goi cac API nhay cam nhu `CreateThread`, `VirtualProtect`, `VirtualAlloc` — nhung ham nay kich hoat kernel callback, EDR se kiem tra call stack ngay luc do.

**Cach hoat dong:**

```
Binh thuong (de bi phat hien):
  Stack:  [return addr] -> beacon memory (0x00400000+)
  EDR thay: "Thread nay duoc tao tu vung nho la, khong phai DLL nao ca!" -> ALERT

Sau khi spoof (an toan):
  Stack:  [return addr] -> kernel32.dll (0x7FF...+)
  EDR thay: "Thread tao tu kernel32, binh thuong" -> OK
```

**Ky thuat cu the:**
1. Khi khoi tao, tim mot "gadget" `jmp rbx` (FF E3) trong kernel32.dll
2. Truoc khi goi API nhay cam:
   - Luu return address that vao bien toan cuc
   - Dat rbx = dia chi cua code phuc hoi (restore stub)
   - Thay [rsp] (return address tren stack) bang dia chi cua gadget `jmp rbx` trong kernel32
   - Dung JMP (khong phai CALL) de nhay den ham dich
3. Khi ham dich return -> nhay den gadget `jmp rbx` trong kernel32 -> nhay den restore stub -> quay ve caller that

**File lien quan:** `CallSpoof.cpp`, `CallSpoof.h`
**Noi ap dung:** `Boffer.cpp` — spoofed `CreateThread` khi tao thread cho BOF

### 2.2 Gia mao stack khi ngu (thread stack spoofing)

**Van de:** Ekko ma hoa vung nho beacon khi ngu — EDR khong the doc code. NHUNG return address tren stack cua thread van con, va chung tro vao vung nho da bi ma hoa (tro vao "rac") -> dau hieu bat thuong.

**Cach hoat dong:**
1. TRUOC khi Ekko ma hoa: quet toan bo stack cua thread
2. Tim tat ca gia tri tren stack tro vao vung nho beacon (imageBase -> imageBase + imageSize)
3. Luu lai cac gia tri goc (tren heap, khong bi Ekko ma hoa)
4. Thay bang dia chi hop le: `kernel32!BaseThreadInitThunk` va `ntdll!RtlUserThreadStart`
5. SAU khi Ekko giai ma: phuc hoi cac gia tri goc

```
Stack truoc khi spoof:
  [rsp+0x00] -> 0x004012A0 (beacon code - BI PHAT HIEN)
  [rsp+0x08] -> 0x00401100 (beacon code - BI PHAT HIEN)
  [rsp+0x10] -> 0x7FFB1234 (ntdll - OK)

Stack sau khi spoof (EDR kiem tra luc ngu):
  [rsp+0x00] -> 0x7FFB5678 (kernel32!BaseThreadInitThunk - AN TOAN)
  [rsp+0x08] -> 0x7FFC9ABC (ntdll!RtlUserThreadStart - AN TOAN)
  [rsp+0x10] -> 0x7FFB1234 (ntdll - OK)
```

**File lien quan:** `CallSpoof.cpp` (ham `SpoofThreadStack`, `RestoreThreadStack`)
**Noi goi:** `SleepObf.cpp` — truoc/sau ROP chain cua Ekko

---

## SPRINT 3 — Giam dau vet trong bo nho (MOI THUC HIEN)

**Van de:** Memory scanner (nhu pe-sieve, moneta, hoac EDR) co the dump process va phan tich bo nho de tim malware.

### 3.1 Xoa PE header sau khi khoi tao

**PE header** la cau truc dau tien cua moi file .exe/.dll trong bo nho Windows. No chua "MZ" magic, bang section, import/export... Memory scanner dung PE header de nhan dien vung nho la mot PE file — dau hieu cua shellcode/reflective DLL injection.

**Cach hoat dong:**
1. Khi beacon khoi tao, luu lai `imageBase` va `imageSize` (can cho Ekko ve sau)
2. Sau khi tat ca API da duoc resolve (ApiLoad hoan tat), xoa sach PE header:

```
Truoc khi stomp:
  [imageBase + 0x00]: 4D 5A 90 00 ...  ("MZ" - DOS header)
  [imageBase + 0xE0]: 50 45 00 00 ...  ("PE" - NT header)
  [imageBase + 0x1F8]: .text, .data ... (section table)
  -> pe-sieve: "TIM THAY PE TRONG BO NHO! NGHI NGO!"

Sau khi stomp:
  [imageBase + 0x00]: 00 00 00 00 ...  (tat ca la zero)
  -> pe-sieve: "Vung nho nay khong co gi dac biet"
```

**File lien quan:** `SleepObf.cpp` (ham `SaveImageInfo`, `StompPEHeaders`)
**Noi goi:** `MainAgent.cpp` — sau `ApiLoad()`, truoc vong lap chinh

### 3.2 Loai bo vung nho RWX

**RWX = Read + Write + Execute.** Trang nho vua ghi duoc vua chay duoc la dau hieu so 1 cua shellcode. Moi memory scanner deu flag RWX.

**Truoc day:** Ekko phuc hoi vung nho beacon ve `PAGE_EXECUTE_READWRITE` (RWX) sau khi giai ma.

**Bay gio:** Ekko phuc hoi theo 2 buoc:
1. Toan bo image -> `PAGE_EXECUTE_READ` (RX) — chi doc va chay, KHONG ghi
2. Rieng data section (.data, .bss) -> `PAGE_READWRITE` (RW) — doc va ghi (cho bien toan cuc)

```
Truoc (RWX - bi flag):
  [0x00400000 - 0x00410000] RWX  <- TOAN BO image
  Scanner: "RWX page! Shellcode detected!"

Sau (RX + RW - an toan):
  [0x00400000 - 0x00408000] R-X  <- .text (code)
  [0x00408000 - 0x00410000] RW-  <- .data (bien)
  Scanner: "Binh thuong, giong nhu DLL loaded"
```

**Chuoi ROP moi cua Ekko (7 buoc, truoc day la 6):**
```
100ms: VirtualProtect(image, RW)      — cho phep ghi de ma hoa
200ms: RC4 encrypt                     — ma hoa toan bo image
300ms: WaitForSingleObject(sleep)      — ngu
400ms: RC4 decrypt                     — giai ma
500ms: VirtualProtect(image, RX)       — code chi doc+chay (KHONG RWX!)
600ms: VirtualProtect(data, RW)        — data section ghi duoc [MOI]
700ms: SetEvent                        — danh thuc thread chinh
```

**File lien quan:** `SleepObf.cpp`

### 3.3 Ma hoa heap khi ngu

**Van de:** Ekko ma hoa image (code + global vars) khi ngu. NHUNG heap (bo nho cap phat dong boi LocalAlloc/malloc) KHONG nam trong image — no van doc duoc. Heap chua nhieu thong tin nhay cam: URL C2, session key, ten pipe, ket qua command...

**Cach hoat dong:**
1. Moi lan `MemAllocLocal()` cap phat bo nho, ghi lai con tro va kich thuoc vao linked list
2. Moi lan `MemFreeLocal()` giai phong, xoa khoi linked list
3. TRUOC khi Ekko ngu: XOR toan bo cac vung heap da theo doi voi key ngau nhien
4. SAU khi Ekko thuc: XOR lai (giai ma) de phuc hoi

```
Heap truoc khi ma hoa:
  [0x01A30000]: "https://c2.evil.com/api/beacon"  <- IOC!
  [0x01A40000]: "\\.\pipe\msagent_ab12"            <- IOC!
  [0x01A50000]: session_key_bytes...                <- Secrets!

Heap sau khi ma hoa (khi beacon dang ngu):
  [0x01A30000]: 7F 3B A2 19 CC 55 ...  <- Rac, khong doc duoc
  [0x01A40000]: B1 0E 44 D7 8A ...      <- Rac
  [0x01A50000]: 23 F1 88 9C ...          <- Rac
```

**File lien quan:** `HeapCrypt.cpp`, `HeapCrypt.h`
**Noi tich hop:** `utils.cpp` (MemAllocLocal/MemFreeLocal), `SleepObf.cpp` (goi truoc/sau Ekko)

---

## SPRINT 4 — Lam mu telemetry (MOI THUC HIEN)

**Van de:** EDR thu thap du lieu telemetry tu trong process thong qua ETW (Event Tracing for Windows) va ntdll hooks. Can vo hieu hoa chung tu ben trong process cua beacon.

### 4.1 Patch EtwEventWrite

**ETW** la he thong event cua Windows. Khi beacon chay BOF (Beacon Object File) hoac load .NET assembly, cac su kien ETW duoc tao ra va EDR doc chung.

**EtwEventWrite** la ham trong ntdll tao cac event nay. Neu ta overwrite byte dau tien cua ham bang `ret` (0xC3), ham se return ngay ma khong lam gi — tat ca event bi "nuot".

**Cach hoat dong:**
1. TRUOC khi chay BOF: Luu 2 byte dau cua `EtwEventWrite`, ghi de bang `0xC3` (ret)
2. BOF chay — khong co ETW event nao duoc tao
3. SAU khi BOF xong: Phuc hoi 2 byte goc

```
EtwEventWrite binh thuong:
  0x7FFB12340000: 4C 8B DC ...  (mov r11, rsp — function prologue)
  -> Tao event, EDR doc duoc

EtwEventWrite da patch:
  0x7FFB12340000: C3 ...        (ret — return ngay lap tuc)
  -> Khong co event, EDR khong thay gi

Sau khi restore:
  0x7FFB12340000: 4C 8B DC ...  (phuc hoi lai binh thuong)
```

**Luu y:** Dung `NtProtectVirtualMemory` qua indirect syscall de thay doi permission cua trang nho ntdll (tranh bi hook khi goi VirtualProtect).

**File lien quan:** `EtwPatch.cpp`, `EtwPatch.h`
**Noi goi:** `Boffer.cpp` (async BOF), `bof_loader.cpp` (sync BOF)

### 4.2 Go hook ntdll co chon loc

**Van de:** Mot so EDR khong chi hook dau ham ma patch sau hon (thay doi nhieu hon chi 5 byte dau). Indirect syscall bypass hook tai dau ham, nhung khong giup neu EDR thay doi logic sau hon trong stub.

**Cach phat hien hook:** Kiem tra 4 byte dau cua ham Nt*:
```
Stub sach (khong bi hook):
  4C 8B D1    mov r10, rcx
  B8 xx xx    mov eax, SSN
  00 00
  -> Pattern nay la chuan, khong bi hook

Stub bi hook:
  E9 xx xx xx xx    jmp <EDR_hook_handler>
  hoac:
  FF 25 xx xx xx xx jmp [EDR_address]
  -> Byte dau khong phai 4C -> BI HOOK
```

**Cach go hook (cho tung ham, khong phai toan bo ntdll):**
1. Mo `\KnownDlls\ntdll.dll` bang `NtOpenSection` (ban sach tren disk, chua bi hook)
2. Map vao bo nho bang `NtMapViewOfSection` (chi doc, khong thay doi file)
3. Tim ham tuong ung trong ban sach (cung RVA offset)
4. Copy 32 byte sach ghi de len ham bi hook trong ntdll dang chay
5. Unmap ban sach

```
ntdll trong process (bi hook):
  NtOpenProcessToken: E9 12 34 56 78  (jmp EDR_handler)

\KnownDlls\ntdll.dll (sach):
  NtOpenProcessToken: 4C 8B D1 B8 26 00 00 00  (mov r10,rcx; mov eax,0x26)

Sau khi refresh:
  NtOpenProcessToken: 4C 8B D1 B8 26 00 00 00  (sach nhu ban goc!)
```

**Tai sao chi go tung ham?** Neu remap toan bo ntdll, se tao nhieu page fault -> EDR phat hien hanh vi bat thuong. Go tung ham chi tao 1 page fault — kho phat hien hon nhieu.

**File lien quan:** `NtdllRefresh.cpp`, `NtdllRefresh.h`
**Noi goi:** `MainAgent.cpp` — kiem tra va go hook cho `NtOpenProcessToken` va `NtQueryInformationProcess` khi khoi tao

---

## NGOAI PHAM VI (thuoc loader / plugin)

Nhung thu quan trong nhung KHONG dat trong beacon:

- AMSI bypass khi load (lam trong loader)
- Module stomping / reflective loading (lam trong loader)
- Process injection (lam trong plugin)
- Ma hoa shellcode / packer / crypter (lam trong loader)
- Persistence (lam trong plugin)
- Leo thang dac quyen (lam trong plugin)
- Go hook toan bo ntdll khi load (lam trong loader)
- Anti-sandbox / anti-debug (co the them neu can)

---

## Tong quan thay doi theo file

| File | Thay doi |
|------|----------|
| `CallSpoof.cpp/h` | MOI — Return address spoofing + Thread stack spoofing |
| `HeapCrypt.cpp/h` | MOI — Theo doi va ma hoa heap khi ngu |
| `EtwPatch.cpp/h` | MOI — Patch/restore EtwEventWrite quanh BOF |
| `NtdllRefresh.cpp/h` | MOI — Phat hien hook + go hook tung ham ntdll |
| `SleepObf.cpp/h` | VIET LAI — Them SaveImageInfo, StompPEHeaders, RWX->RX+RW, tich hop heap/stack spoof |
| `Syscalls.cpp/h` | SUA — 4 indirect syscall moi: NtOpenSection, NtMapViewOfSection, NtUnmapViewOfSection, NtProtectVirtualMemory |
| `ApiDefines.h` | SUA — 7 DJB2 hash moi cho API |
| `ApiLoader.h/cpp` | SUA — Resolve them NtFlushInstructionCache, EtwEventWrite, BaseThreadInitThunk, RtlUserThreadStart |
| `MainAgent.cpp` | SUA — Them init: SaveImageInfo -> CallSpoofInit -> HeapCryptInit -> go hook -> StompPEHeaders |
| `Boffer.cpp` | SUA — Spoofed CreateThread, ETW patch/restore quanh async BOF |
| `bof_loader.cpp` | SUA — ETW patch/restore quanh sync BOF |
| `utils.cpp` | SUA — MemAllocLocal/MemFreeLocal tich hop HeapTrack/HeapUntrack |

---

## Luong hoat dong tong the

```
[Khoi tao]
  ApiLoad()              -> Resolve tat ca API qua DJB2 hash
  SyscallsInit()         -> Tim gadget syscall;ret, phan giai SSN
  SaveImageInfo()        -> Luu imageBase, imageSize, data section range
  CallSpoofInit()        -> Tim gadget jmp rbx trong kernel32
  HeapCryptInit()        -> Bat dau theo doi heap
  Go hook ntdll          -> Kiem tra NtOpenProcessToken, NtQueryInformationProcess
  StompPEHeaders()       -> Xoa MZ/PE header

[Vong lap chinh]
  Exchange voi C2 server
  Xu ly command (BOF, download, v.v.)
    |-- Truoc BOF: EtwPatch() (patch EtwEventWrite)
    |-- Chay BOF
    |-- Sau BOF: EtwRestore() (phuc hoi EtwEventWrite)

[Ngu (Ekko Sleep)]
  HeapEncryptAll()       -> Ma hoa toan bo heap da theo doi
  SpoofThreadStack()     -> Thay return address tren stack bang kernel32/ntdll
  [ROP Chain]
    VirtualProtect(RW)   -> Cho phep ghi
    RC4 encrypt          -> Ma hoa image
    Sleep                -> Ngu
    RC4 decrypt          -> Giai ma image
    VirtualProtect(RX)   -> Code chi doc+chay
    VirtualProtect(RW)   -> Data section ghi duoc
    SetEvent             -> Danh thuc thread chinh
  RestoreThreadStack()   -> Phuc hoi return address goc
  HeapDecryptAll()       -> Giai ma heap
```
